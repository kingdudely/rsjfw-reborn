#ifndef OS_CMD_H
#define OS_CMD_H

#include "../streambuf.h"
#include <string>
#include <vector>
#include <map>
#include <sys/types.h>

namespace rsjfw::cmd {

    struct CmdResult {
        pid_t pid = -1;
        int exitCode = -1;
    };

    struct Options {
        std::string cwd;
        std::map<std::string, std::string> env;
        bool mergeStdoutStderr = true;
    };

    class Command {
    public:
        static CmdResult runAsync(
            const std::string &exe,
            const std::vector<std::string> &args,
            const Options &opts,
            stream_buffer_t *buffer = nullptr);

        static CmdResult runSync(
            const std::string &exe,
            const std::vector<std::string> &args,
            const Options &opts,
            stream_buffer_t *buffer = nullptr);

        static void kill(pid_t pid, bool force = false);
    };
}

#endif
