#ifndef WINE_H
#define WINE_H

#include "runner.h"
#include <filesystem>

namespace rsjfw {

  class WineRunner : public Runner {
  public:
    WineRunner(std::shared_ptr<Prefix> prefix, const std::string& wineBinDir);

    bool configure(ProgressCb cb = nullptr) override;

    cmd::CmdResult runStudio(const std::string& versionGuid,
                             const std::vector<std::string>& args,
                             stream_buffer_t& outBuffer) override;

    bool runWine(const std::string& exe, const std::vector<std::string>& args, const std::string& taskName) override;
    std::string resolveWindowsPath(const std::string& unixPath) override;
    std::map<std::string, std::string> getBaseEnv() override;

  private:
    std::string getWineBinary() const;
  };
}

#endif