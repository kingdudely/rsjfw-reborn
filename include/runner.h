#ifndef RUNNER_H
#define RUNNER_H

#include "prefix.h"
#include "os/cmd.h"
#include "streambuf.h"
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace rsjfw
{
    class Runner {
    public:
        Runner(std::shared_ptr<Prefix> prefix, const std::string& wineBinDir)
            : prefix_(prefix), wineBinDir_(wineBinDir) {}

        virtual ~Runner() = default;

        static std::unique_ptr<Runner> createWineRunner(const std::string& wineRootPath);
        static std::unique_ptr<Runner> createProtonRunner(const std::string& protonRootPath);
        static std::unique_ptr<Runner> createUmuRunner(const std::string& protonRootPath);

        virtual bool configure(ProgressCb cb = nullptr) = 0;

        virtual cmd::CmdResult runStudio(const std::string& versionGuid,
                                         const std::vector<std::string>& args,
                                         stream_buffer_t& outBuffer) = 0;

        virtual bool runWine(const std::string& exe, const std::vector<std::string>& args, const std::string& taskName) = 0;
        virtual std::string resolveWindowsPath(const std::string& unixPath) = 0;

        std::shared_ptr<Prefix> getPrefix() const { return prefix_; }
        virtual std::map<std::string, std::string> getBaseEnv();

    protected:
        std::shared_ptr<Prefix> prefix_;
        std::string wineBinDir_;
        std::map<std::string, std::string> env_;

        void addBaseEnv();
        bool provisionCommonDependencies(ProgressCb cb);
    };
}

#endif
