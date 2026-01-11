#include "os/cmd.h"
#include <cstring>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <fcntl.h>
#include <iostream>
#include <poll.h>

namespace rsjfw::cmd {

static void readLoop(int fd, stream_buffer_t &buffer) {
    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        buffer.append({buf, static_cast<size_t>(n)});
    }
    close(fd);
}

CmdResult Command::runAsync(const std::string &exe,
                            const std::vector<std::string> &args,
                            const Options &opts, stream_buffer_t *buffer) {
    int outPipe[2], errPipe[2];
    if (buffer) {
        if (pipe(outPipe) != 0 || pipe(errPipe) != 0) return {-1, -1};
    }

    pid_t pid = fork();
    if (pid == 0) {
        int nullFd = open("/dev/null", O_RDWR);
        if (nullFd != -1) {
            dup2(nullFd, STDIN_FILENO);
            if (!buffer) {
                dup2(nullFd, STDOUT_FILENO);
                dup2(nullFd, STDERR_FILENO);
            }
            close(nullFd);
        }

        if (!opts.cwd.empty()) chdir(opts.cwd.c_str());
        for (const auto &[k, v] : opts.env) setenv(k.c_str(), v.c_str(), 1);

        if (buffer) {
            dup2(outPipe[1], STDOUT_FILENO);
            dup2(opts.mergeStdoutStderr ? outPipe[1] : errPipe[1], STDERR_FILENO);
            close(outPipe[0]); close(outPipe[1]);
            close(errPipe[0]); close(errPipe[1]);
        }

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(exe.c_str()));
        for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);

        execvp(exe.c_str(), argv.data());
        _exit(127);
    }

    if (buffer) {
        close(outPipe[1]);
        close(errPipe[1]);
        std::thread(readLoop, outPipe[0], std::ref(*buffer)).detach();
        if (!opts.mergeStdoutStderr)
            std::thread(readLoop, errPipe[0], std::ref(*buffer)).detach();
    }

    return {pid, -1};
}

CmdResult Command::runSync(const std::string &exe,
                           const std::vector<std::string> &args,
                           const Options &opts, stream_buffer_t *buffer) {
    int outPipe[2], errPipe[2];
    if (pipe(outPipe) != 0 || pipe(errPipe) != 0) return {-1, -1};

    pid_t pid = fork();
    if (pid == 0) {
        int nullFd = open("/dev/null", O_RDWR);
        if (nullFd != -1) {
            dup2(nullFd, STDIN_FILENO);
            close(nullFd);
        }

        if (!opts.cwd.empty()) chdir(opts.cwd.c_str());
        for (const auto &[k, v] : opts.env) setenv(k.c_str(), v.c_str(), 1);

        dup2(outPipe[1], STDOUT_FILENO);
        dup2(opts.mergeStdoutStderr ? outPipe[1] : errPipe[1], STDERR_FILENO);
        close(outPipe[0]); close(outPipe[1]);
        close(errPipe[0]); close(errPipe[1]);

        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(exe.c_str()));
        for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);

        execvp(exe.c_str(), argv.data());
        _exit(127);
    }

    close(outPipe[1]);
    close(errPipe[1]);

    int exitCode = -1;

    if (buffer) {
        fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
        fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

        struct pollfd pfds[2];
        pfds[0].fd = outPipe[0];
        pfds[0].events = POLLIN;
        pfds[1].fd = errPipe[0];
        pfds[1].events = POLLIN;

        bool outOpen = true;
        bool errOpen = !opts.mergeStdoutStderr;
        if (opts.mergeStdoutStderr) close(errPipe[0]);

        while (outOpen || errOpen) {
            int ret = poll(pfds, 2, 100);
            if (ret > 0) {
                for (int i = 0; i < 2; i++) {
                    if (pfds[i].revents & POLLIN) {
                        char buf[8192];
                        ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
                        if (n > 0) buffer->append({buf, static_cast<size_t>(n)});
                        else if (n == 0) {
                            if (i == 0) outOpen = false;
                            else errOpen = false;
                        }
                    } else if (pfds[i].revents & (POLLHUP | POLLERR)) {
                        if (i == 0) outOpen = false;
                        else errOpen = false;
                    }
                }
            }

            int status = 0;
            if (waitpid(pid, &status, WNOHANG) != 0) {
                if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) exitCode = 128 + WTERMSIG(status);
                
                for(int j=0; j<10; j++) {
                    char buf[8192];
                    bool readAnything = false;
                    if (outOpen) {
                        ssize_t n = read(outPipe[0], buf, sizeof(buf));
                        if (n > 0) { buffer->append({buf, static_cast<size_t>(n)}); readAnything = true; }
                        else outOpen = false;
                    }
                    if (errOpen) {
                        ssize_t n = read(errPipe[0], buf, sizeof(buf));
                        if (n > 0) { buffer->append({buf, static_cast<size_t>(n)}); readAnything = true; }
                        else errOpen = false;
                    }
                    if (!readAnything) break;
                    usleep(1000);
                }
                break;
            }
        }

        if (outOpen) close(outPipe[0]);
        if (errOpen) close(errPipe[0]);
    } else {
        close(outPipe[0]);
        close(errPipe[0]);
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status)) exitCode = 128 + WTERMSIG(status);
    }

    return {pid, exitCode};
}

void Command::kill(pid_t pid, bool force) {
    if (pid <= 0) return;
    ::kill(pid, SIGTERM);
    if (force) {
        usleep(300000);
        ::kill(pid, SIGKILL);
    }
}

}
