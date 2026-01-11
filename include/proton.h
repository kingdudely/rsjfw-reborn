#ifndef PROTON_RUNNER_H
#define PROTON_RUNNER_H

#include "runner.h"
#include <filesystem>

namespace rsjfw {

class ProtonRunner : public Runner {
public:
    ProtonRunner(std::string protonRoot);

    bool configure(ProgressCb cb = nullptr) override;

    cmd::CmdResult runStudio(const std::string& versionGuid,
                             const std::vector<std::string>& args,
                             stream_buffer_t& outBuffer) override;

    bool runWine(const std::string& exe, const std::vector<std::string>& args, const std::string& taskName) override;

private:
    std::string protonRoot_;
    std::string protonScript_;
    std::string compatDataPath_;
};

}

#endif
