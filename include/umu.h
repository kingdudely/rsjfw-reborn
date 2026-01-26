#ifndef RSJFW_UMU_H
#define RSJFW_UMU_H

#include "runner.h"

namespace rsjfw {

class UmuRunner : public Runner {
public:
    UmuRunner(std::shared_ptr<Prefix> prefix, const std::string& protonRootPath);
    ~UmuRunner() override = default;

    bool configure(ProgressCb cb = nullptr) override;
    
    cmd::CmdResult runStudio(const std::string& versionGuid,
                             const std::vector<std::string>& args,
                             stream_buffer_t& outBuffer) override;

    bool runWine(const std::string& exe, const std::vector<std::string>& args, const std::string& taskName) override;
    std::string resolveWindowsPath(const std::string& unixPath) override;

    std::map<std::string, std::string> getBaseEnv() override;
};

}

#endif
