#include "prefix.h"
#include "logger.h"
#include "orchestrator.h"
#include "config.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace rsjfw {

namespace fs = std::filesystem;

Prefix::Prefix(const std::string &root, const std::string &install)
    : rootDir_(root), installDir_(install), registry_(root) {
  fs::create_directories(rootDir_);
}

void Prefix::setExecutor(const std::string &binary,
                         const std::vector<std::string> &preArgs) {
  executorBinary_ = binary;
  executorPreArgs_ = preArgs;
}

void Prefix::setWrapper(const std::vector<std::string> &wrapper) {
  wrapperArgs_ = wrapper;
}

void Prefix::setEnvironment(const std::map<std::string, std::string> &env) {
  baseEnv_ = env;
}

std::string Prefix::resolveBinary(const std::string &binary) const {
  if (!executorBinary_.empty())
    return executorBinary_;
  fs::path root(installDir_);
  std::vector<fs::path> searchPaths = {root / binary, root / "bin" / binary,
                                       root / "files" / "bin" / binary};
  for (const auto &p : searchPaths) {
    if (fs::exists(p)) {
      LOG_DEBUG("Resolved %s -> %s", binary.c_str(), p.c_str());
      return p.string();
    }
  }
  LOG_WARN("Could not resolve %s in %s", binary.c_str(), installDir_.c_str());
  return binary;
}

void Prefix::addLibPaths(cmd::Options &opts) const {
  if (!executorBinary_.empty())
    return;
  std::string libPath = "";
  if (char *env_p = std::getenv("LD_LIBRARY_PATH"))
    libPath = env_p;
  fs::path r(installDir_);
  std::vector<std::string> libDirs = {"lib",
                                      "lib64",
                                      "lib/wine",
                                      "lib64/wine",
                                      "lib/wine/x86_64-unix",
                                      "lib/wine/i386-unix",
                                      "files/lib",
                                      "files/lib64",
                                      "files/lib/x86_64-linux-gnu",
                                      "files/lib/i386-linux-gnu",
                                      "files/lib/wine/x86_64-unix",
                                      "files/lib/wine/i386-unix"};
  for (const auto &d : libDirs) {
    auto p = r / d;
    if (fs::exists(p)) {
      if (!libPath.empty())
        libPath = ":" + libPath;
      libPath = p.string() + libPath;
    }
  }
  if (!libPath.empty())
    opts.env["LD_LIBRARY_PATH"] = libPath;
}

static std::vector<std::string> splitArgs(const std::string& s) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string item;
    while (ss >> item) res.push_back(item);
    return res;
}

void Prefix::wrapCommand(const std::string& binary, const std::vector<std::string>& args, std::string& outBinary, std::vector<std::string>& outArgs) {
    auto& gen = Config::instance().getGeneral();
    std::vector<std::string> wrappers;
    
    if (gen.enableGamemode) {
        wrappers.push_back("gamemoderun");
        auto gArgs = splitArgs(gen.gamemodeArgs);
        wrappers.insert(wrappers.end(), gArgs.begin(), gArgs.end());
    }
    
    if (gen.enableGamescope) {
        wrappers.push_back("gamescope");
        auto gArgs = splitArgs(gen.gamescopeArgs);
        wrappers.insert(wrappers.end(), gArgs.begin(), gArgs.end());
        wrappers.push_back("--");
    }

    for (const auto& l : gen.customLaunchers) {
        if (l.enabled && !l.command.empty()) {
            wrappers.push_back(l.command);
            auto lArgs = splitArgs(l.args);
            wrappers.insert(wrappers.end(), lArgs.begin(), lArgs.end());
        }
    }

    if (wrappers.empty()) {
        outBinary = binary;
        outArgs = args;
    } else {
        outBinary = wrappers[0];
        outArgs.assign(wrappers.begin() + 1, wrappers.end());
        outArgs.push_back(binary);
        outArgs.insert(outArgs.end(), args.begin(), args.end());
    }
}

bool Prefix::init(ProgressCb cb) {
  if (cb)
    cb(0.1f, "verifying prefix...");
  if (!fs::exists(fs::path(rootDir_) / "system.reg")) {
    if (cb)
      cb(0.3f, "bootstrapping wine prefix...");
    cmd::Options opts;
    opts.env = baseEnv_;
    opts.env["WINEPREFIX"] = rootDir_;
    opts.env["WINEARCH"] = "win64";
    opts.env["WINEDEBUG"] = Orchestrator::instance().isWineDebugEnabled()
                                ? "err+all,warn+all,fixme+all"
                                : "-all";
    addLibPaths(opts);
    
    std::string baseBinary;
    std::vector<std::string> baseArgs = wrapperArgs_;
    
    if (executorBinary_.empty()) {
      baseBinary = resolveBinary("wineboot");
      baseArgs.push_back("-u");
    } else {
      baseBinary = executorBinary_;
      baseArgs.insert(baseArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
      if (baseBinary.find("proton") != std::string::npos)
        baseArgs.push_back("/usr/bin/true");
      else {
        baseArgs.push_back("wineboot");
        baseArgs.push_back("-u");
      }
    }

    std::string finalBinary;
    std::vector<std::string> finalArgs;
    wrapCommand(baseBinary, baseArgs, finalBinary, finalArgs);

    auto res = cmd::Command::runSync(finalBinary, finalArgs, opts);
    if (res.exitCode != 0 && res.exitCode != 120)
      return false;

    waitForExit();
  }
  if (cb)
    cb(1.0f, "prefix ready.");
  return true;
}

bool Prefix::wine(const std::string &exe, const std::vector<std::string> &args,
                  std::function<void(const std::string &)> onOutput,
                  const std::string &cwd, bool wait,
                  const std::map<std::string, std::string> &extraEnv) {
  cmd::Options opts;
  opts.env = baseEnv_;
  for (const auto &[k, v] : extraEnv)
    opts.env[k] = v;
  opts.env["WINEPREFIX"] = rootDir_;
  opts.env["WINEARCH"] = "win64";
  if (!cwd.empty())
    opts.cwd = cwd;
  addLibPaths(opts);

  std::string baseBinary;
  std::vector<std::string> baseArgs = wrapperArgs_;
  
  if (executorBinary_.empty()) {
    baseBinary = resolveBinary("wine");
    baseArgs.push_back(exe);
    baseArgs.insert(baseArgs.end(), args.begin(), args.end());
  } else {
    baseBinary = executorBinary_;
    baseArgs.insert(baseArgs.end(), executorPreArgs_.begin(), executorPreArgs_.end());
    baseArgs.push_back(exe);
    baseArgs.insert(baseArgs.end(), args.begin(), args.end());
  }

  std::string finalBinary;
  std::vector<std::string> finalArgs;
  wrapCommand(baseBinary, baseArgs, finalBinary, finalArgs);

  if (!wait) {
    cmd::Command::runAsync(finalBinary, finalArgs, opts, nullptr);
    return true;
  }

  stream_buffer_t buf;
  buf.connect([&](const std::string_view chunk) {
    if (onOutput)
      onOutput(std::string(chunk));
  });
  auto res =
      cmd::Command::runSync(finalBinary, finalArgs, opts, onOutput ? &buf : nullptr);
  return res.exitCode == 0;
}

std::pair<std::string, std::vector<std::string>>
Prefix::getWineserverCmd(const std::string &arg) const {
  std::vector<std::string> args = wrapperArgs_;
  std::string binary;

  std::string directServer = "";
  {
    fs::path root(installDir_);
    std::vector<fs::path> searchPaths = {root / "wineserver",
                                         root / "bin" / "wineserver",
                                         root / "files" / "bin" / "wineserver"};
    for (const auto &p : searchPaths) {
      if (fs::exists(p)) {
        directServer = p.string();
        break;
      }
    }
  }

  if (!directServer.empty()) {
    binary = directServer;
    args.push_back(arg);
  } else if (executorBinary_.empty()) {
    binary = resolveBinary("wineserver");
    args.push_back(arg);
  } else {
    binary = executorBinary_;
    args.insert(args.end(), executorPreArgs_.begin(), executorPreArgs_.end());
    args.push_back("wineserver");
    args.push_back(arg);
  }
  return {binary, args};
}

bool Prefix::kill() {
  cmd::Options opts;
  opts.env = baseEnv_;
  opts.env["WINEPREFIX"] = rootDir_;

  auto [binary, args] = getWineserverCmd("-k");
  auto res = cmd::Command::runSync(binary, args, opts);
  return res.exitCode == 0;
}

bool Prefix::waitForExit() {
  cmd::Options opts;
  opts.env = baseEnv_;
  opts.env["WINEPREFIX"] = rootDir_;

  auto [binary, args] = getWineserverCmd("-w");

  auto future = std::async(std::launch::async, [binary, args, opts]() {
    return cmd::Command::runSync(binary, args, opts);
  });

  if (future.wait_for(std::chrono::seconds(60)) ==
      std::future_status::timeout) {
    LOG_WARN("Timeout waiting for prefix to close, forcing kill...");
    kill();
    return false;
  }

  auto res = future.get();
  return res.exitCode == 0;
}

void Prefix::registryAdd(const std::string &k, const std::string &n,
                         const std::string &v, const std::string &t) {
  registry_.add(k, n, v, t);
}

bool Prefix::registryCommit() { return registry_.commit(); }

std::optional<std::string> Prefix::registryQuery(const std::string &key,
                                                 const std::string &name) {
  return registry_.query(key, name);
}

bool Prefix::installDxvk(const std::string &dxvkRoot) {
  fs::path root(dxvkRoot);
  if (!fs::exists(root))
    return false;
  LOG_INFO("injecting dxvk into prefix...");
  fs::path sys32 = fs::path(rootDir_) / "drive_c" / "windows" / "system32";
  fs::path syswow = fs::path(rootDir_) / "drive_c" / "windows" / "syswow64";
  auto installDlls = [&](fs::path src, fs::path dst) {
    if (!fs::exists(src) || !fs::exists(dst))
      return;
    for (const auto &entry : fs::directory_iterator(src)) {
      if (entry.path().extension() == ".dll") {
        fs::path target = dst / entry.path().filename();
        try {
          if (fs::exists(target))
            fs::remove(target);
          fs::copy_file(entry.path(), target);
          registryAdd("HKCU\\Software\\Wine\\DllOverrides",
                      entry.path().stem().string(), "native", "REG_SZ");
        } catch (...) {
        }
      }
    }
  };
  if (fs::exists(root / "x64")) {
    installDlls(root / "x64", sys32);
    installDlls(root / "x32", syswow);
  } else if (fs::exists(root / "x86_64-windows")) {
    installDlls(root / "x86_64-windows", sys32);
    installDlls(root / "i386-windows", syswow);
  }
  return registryCommit();
}

} // namespace rsjfw
