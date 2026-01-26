#include "os/cmd.h"
#include "logger.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace rsjfw::cmd {

std::set<pid_t> Command::activePids_;
std::mutex Command::pidsMtx_;
bool Command::shuttingDown_ = false;

void Command::registerPid(pid_t pid) {
  if (pid <= 0)
    return;
  std::lock_guard lock(pidsMtx_);
  
  if (shuttingDown_) {
      LOG_WARN("Process spawned during shutdown, terminating immediately: %d", pid);
      // Try to kill process group if it exists, otherwise kill process
      if (::kill(-pid, SIGKILL) != 0) {
          ::kill(pid, SIGKILL);
      }
      return;
  }

  activePids_.insert(pid);
}

void Command::unregisterPid(pid_t pid) {
  if (pid <= 0) return;
  std::lock_guard lock(pidsMtx_);
  if (activePids_.find(pid) != activePids_.end()) {
      activePids_.erase(pid);
  }
}

static void readLoop(int fd, stream_buffer_t &buffer) {
  char buf[4096];
  while (true) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if (n <= 0)
      break;
    buffer.append({buf, static_cast<size_t>(n)});
  }
  close(fd);
}

CmdResult Command::runAsync(const std::string &exe,
                            const std::vector<std::string> &args,
                            const Options &opts, stream_buffer_t *buffer) {
  std::string fullCmd = "";
  for (const auto &[k, v] : opts.env)
    fullCmd += k + "=" + v + " ";
  fullCmd += exe;
  for (const auto &a : args)
    fullCmd += " " + a;
  LOG_DEBUG("[cmd] %s", fullCmd.c_str());

  int outPipe[2], errPipe[2];
  if (buffer) {
    if (pipe(outPipe) != 0 || pipe(errPipe) != 0)
      return {-1, -1};
  }

  pid_t pid = fork();
  if (pid == 0) {
    // New process group for easier cleanup
    setpgid(0, 0);

    if (!opts.cwd.empty())
      chdir(opts.cwd.c_str());
    for (const auto &[k, v] : opts.env)
      setenv(k.c_str(), v.c_str(), 1);

    if (buffer) {
      dup2(outPipe[1], STDOUT_FILENO);
      dup2(opts.mergeStdoutStderr ? outPipe[1] : errPipe[1], STDERR_FILENO);
      close(outPipe[0]);
      close(outPipe[1]);
      close(errPipe[0]);
      close(errPipe[1]);
    }

    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(exe.c_str()));
    for (const auto &a : args)
      argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);

    execvp(exe.c_str(), argv.data());
    _exit(127);
  }

  registerPid(pid);

  if (buffer) {
    close(outPipe[1]);
    close(errPipe[1]);
    std::thread([pid, fd = outPipe[0], buffer]() {
      readLoop(fd, *buffer);
      int status;
      waitpid(pid, &status, 0);
      Command::unregisterPid(pid);
    }).detach();

    if (!opts.mergeStdoutStderr) {
      std::thread([fd = errPipe[0], buffer]() {
        readLoop(fd, *buffer);
      }).detach();
    }
  } else {
    std::thread([pid]() {
      int status;
      waitpid(pid, &status, 0);
      Command::unregisterPid(pid);
    }).detach();
  }

  return {pid, -1};
}

CmdResult Command::runSync(const std::string &exe,
                           const std::vector<std::string> &args,
                           const Options &opts, stream_buffer_t *buffer) {
  std::string fullCmd = "";
  for (const auto &[k, v] : opts.env) {
      // Escape for display in log
      std::string val = v;
      size_t pos = 0;
      while ((pos = val.find('"', pos)) != std::string::npos) {
          val.replace(pos, 1, "\\\"");
          pos += 2;
      }
      fullCmd += k + "=\"" + val + "\" ";
  }
  fullCmd += exe;
  for (const auto &a : args)
    fullCmd += " " + a;
  LOG_DEBUG("[cmd] %s", fullCmd.c_str());

  int outPipe[2], errPipe[2];
  if (buffer) {
    if (pipe(outPipe) != 0 || pipe(errPipe) != 0)
      return {-1, -1};
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Create new process group for easier cleanup of children
    setpgid(0, 0);
    
    if (!opts.cwd.empty())
      chdir(opts.cwd.c_str());
    for (const auto &[k, v] : opts.env)
      setenv(k.c_str(), v.c_str(), 1);

    if (buffer) {
      if (opts.mergeStdoutStderr) {
          dup2(outPipe[1], STDOUT_FILENO);
          dup2(outPipe[1], STDERR_FILENO);
      } else {
          dup2(outPipe[1], STDOUT_FILENO);
          dup2(errPipe[1], STDERR_FILENO);
      }
      close(outPipe[0]);
      close(outPipe[1]);
      close(errPipe[0]);
      close(errPipe[1]);
    }

    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(exe.c_str()));
    for (const auto &a : args)
      argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);

    execvp(exe.c_str(), argv.data());
    _exit(127);
  }

  registerPid(pid);

  if (buffer) {
    close(outPipe[1]);
    close(errPipe[1]);

    int exitCode = -1;

    fcntl(outPipe[0], F_SETFL, O_NONBLOCK);
    fcntl(errPipe[0], F_SETFL, O_NONBLOCK);

    struct pollfd pfds[2];
    pfds[0].fd = outPipe[0];
    pfds[0].events = POLLIN;
    pfds[1].fd = errPipe[0];
    pfds[1].events = POLLIN;

    bool outOpen = true;
    bool errOpen = !opts.mergeStdoutStderr;
    if (opts.mergeStdoutStderr)
      close(errPipe[0]);

    while (outOpen || errOpen) {
      {
          std::lock_guard lock(pidsMtx_);
          if (shuttingDown_) break;
      }
      int status = 0;
      int wres = waitpid(pid, &status, WNOHANG);
      if (wres > 0 || (wres == -1 && errno == ECHILD)) {
        if (wres > 0) {
            if (WIFEXITED(status))
              exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
              exitCode = 128 + WTERMSIG(status);
        } else {
            // ECHILD means process is gone, assume killed?
            exitCode = -1; 
        }

        // Child exited, do one last non-blocking drain of the pipes
        for (int i = 0; i < 2; i++) {
          if (i == 1 && opts.mergeStdoutStderr) continue;
          if (pfds[i].fd < 0) continue;
          
          int flags = fcntl(pfds[i].fd, F_GETFL, 0);
          fcntl(pfds[i].fd, F_SETFL, flags | O_NONBLOCK);
          char buf[8192];
          while (true) {
            ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
            if (n > 0 && buffer) buffer->append({buf, static_cast<size_t>(n)});
            else break;
          }
        }
        break;
      } else if (wres == -1) {
          LOG_ERROR("waitpid failed: %s", strerror(errno));
          // Don't break? Or break? If waitpid fails with other than ECHILD (e.g. EINTR), we should retry?
          // But WNOHANG should not EINTR often.
          // If we break, we might leave pipes open?
      }

      int ret = poll(pfds, 2, 50);
      if (ret > 0) {
        for (int i = 0; i < 2; i++) {
          if (pfds[i].revents & POLLIN) {
            char buf[8192];
            ssize_t n = read(pfds[i].fd, buf, sizeof(buf));
            if (n > 0) {
               if (buffer) buffer->append({buf, static_cast<size_t>(n)});
            } else if (n == 0) {
              if (i == 0) outOpen = false;
              else errOpen = false;
            }
          } else if (pfds[i].revents & (POLLHUP | POLLERR)) {
            if (i == 0) outOpen = false;
            else errOpen = false;
          }
        }
      }
    }

    if (outOpen)
      close(outPipe[0]);
    if (errOpen)
      close(errPipe[0]);

    if (exitCode == -1) {
      bool shouldReturn = false;
      {
           std::lock_guard lock(pidsMtx_);
           if (shuttingDown_) shouldReturn = true;
      }
      if (shouldReturn) {
           unregisterPid(pid);
           return {pid, -1};
      }

      int status = 0;
      if (waitpid(pid, &status, 0) != -1) {
        if (WIFEXITED(status))
          exitCode = WEXITSTATUS(status);
        else if (WIFSIGNALED(status))
          exitCode = 128 + WTERMSIG(status);
      }
    }

    unregisterPid(pid);
    return {pid, exitCode};
  } else {
    int status = 0;
    waitpid(pid, &status, 0);
    int exitCode = -1;
    if (WIFEXITED(status))
      exitCode = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
      exitCode = 128 + WTERMSIG(status);

    unregisterPid(pid);
    return {pid, exitCode};
  }
}

void Command::kill(pid_t pid, bool force) {
  if (pid <= 0)
    return;
  
  LOG_DEBUG("Killing PID %d (force=%d)", pid, force);
  // Kill the entire process group.
  ::kill(-pid, SIGTERM);
  
  if (force) {
      // Wait up to 300ms for process to exit
      for (int i = 0; i < 30; ++i) {
          usleep(10000); // 10ms
          if (::kill(-pid, 0) != 0 && errno == ESRCH) {
              LOG_DEBUG("PID %d exited gracefully", pid);
              return;
          }
      }
      
      LOG_DEBUG("PID %d still alive, sending SIGKILL", pid);
      ::kill(-pid, SIGKILL);
  }
}

void Command::killAll() {
  std::vector<pid_t> pidsToKill;
  {
      std::lock_guard lock(pidsMtx_);
      shuttingDown_ = true;
      LOG_INFO("Killing %zu active commands...", activePids_.size());
      pidsToKill.assign(activePids_.begin(), activePids_.end());
  }
  
  // First pass: SIGTERM to all groups (active and historic)
  for (pid_t pid : pidsToKill) {
    if (::kill(-pid, SIGTERM) != 0) {
        ::kill(pid, SIGTERM);
    }
  }

  // Give them a moment to exit
  usleep(100000); // 100ms

  // Second pass: SIGKILL to all groups
  for (pid_t pid : pidsToKill) {
      if (::kill(-pid, SIGKILL) != 0) {
          int err = errno;
          if (err != ESRCH) { // Ignore if process is already gone
              if (::kill(pid, SIGKILL) != 0) {
                 if (errno != ESRCH) LOG_ERROR("Failed to kill process %d: %s", pid, strerror(errno));
              } else {
                 LOG_DEBUG("Killed process %d (single)", pid);
              }
          }
      } else {
          LOG_DEBUG("Killed process group %d", pid);
      }
  }
  
  {
      std::lock_guard lock(pidsMtx_);
      activePids_.clear();
  }
}

} // namespace rsjfw::cmd
