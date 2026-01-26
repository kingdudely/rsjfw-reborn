#include "umu.h"
#include "config.h"
#include "logger.h"
#include "os/cmd.h"
#include "path_manager.h"
#include <filesystem>

namespace rsjfw {

namespace fs = std::filesystem;

UmuRunner::UmuRunner(std::shared_ptr<Prefix> prefix,
                     const std::string &protonRootPath)
    : Runner(prefix, protonRootPath) {
  prefix_->setEnvironment(getBaseEnv());
}

bool UmuRunner::configure(ProgressCb cb) {
  LOG_INFO("Configuring UMU environment...");

  auto pm = PathManager::instance();
  fs::create_directories(pm.umu());

  if (cb)
    cb(0.1f, "verifying prefix...");

  prefix_->setEnvironment(getBaseEnv());

  if (!prefix_->init(cb)) {
    LOG_ERROR("UMU prefix initialization failed");
    return false;
  }

  return provisionCommonDependencies(cb);
}

cmd::CmdResult UmuRunner::runStudio(const std::string &versionGuid,
                                    const std::vector<std::string> &args,
                                    stream_buffer_t &outBuffer) {
  auto &gen = Config::instance().getGeneral();
  fs::path exePath = fs::path(PathManager::instance().versions()) /
                     versionGuid / "RobloxStudioBeta.exe";

  if (!fs::exists(exePath)) {
    LOG_ERROR("Studio executable not found: %s", exePath.c_str());
    return {-1, 1};
  }

  cmd::Options opts;
  opts.env = getBaseEnv();

  std::string bin = "umu-run";
  std::vector<std::string> finalArgs = {exePath.string()};
  finalArgs.insert(finalArgs.end(), args.begin(), args.end());

  std::string wrappedBin;
  std::vector<std::string> wrappedArgs;
  prefix_->wrapCommand(bin, finalArgs, wrappedBin, wrappedArgs);

  // umu leaves these open and they make the roblox studio exe hang forever
  // until they die
  cmd::Command::runSync("killall", {"CrGpuMain"}, {});
  cmd::Command::runSync("killall", {"CrBrowserMain"}, {});

  LOG_INFO("Launching Studio via UMU...");
  return cmd::Command::runSync(wrappedBin, wrappedArgs, opts, &outBuffer);
}

bool UmuRunner::runWine(const std::string &exe,
                        const std::vector<std::string> &args,
                        const std::string &taskName) {
  cmd::Options opts;
  opts.env = getBaseEnv();

  std::vector<std::string> fullArgs = {exe};
  fullArgs.insert(fullArgs.end(), args.begin(), args.end());

  auto res = cmd::Command::runSync("umu-run", fullArgs, opts);
  return res.exitCode == 0;
}

std::string UmuRunner::resolveWindowsPath(const std::string &unixPath) {
  return "Z:" + unixPath;
}

std::map<std::string, std::string> UmuRunner::getBaseEnv() {
  auto res = Runner::getBaseEnv();
  auto &gen = Config::instance().getGeneral();

  res["WINEPREFIX"] = prefix_->getPath();

  if (!wineBinDir_.empty() && wineBinDir_ != "GE-Proton") {
    res["PROTONPATH"] = wineBinDir_;
  } else {
    // "GE-Proton" tells umu to download its own
    res["PROTONPATH"] = "GE-Proton";
  }

  res["GAMEID"] = gen.umuId;
  res["STORE"] = "none";
  res["STEAM_COMPAT_DATA_PATH"] =
      fs::path(prefix_->getPath()).parent_path().string();

  return res;
}

} // namespace rsjfw
