#ifndef RSJFW_PREFIX_H
#define RSJFW_PREFIX_H

#include "os/cmd.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <map>

namespace rsjfw {

using ProgressCb = std::function<void(float, std::string)>;

struct RegistryEntry {
  std::string key;
  std::string valueName;
  std::string value;
  std::string type;
};

class Prefix {
public:
  Prefix(const std::string &rootDir, const std::string &installDir);
  ~Prefix() = default;

  void setExecutor(const std::string& binary, const std::vector<std::string>& preArgs);
  void setWrapper(const std::vector<std::string>& wrapper);
  void setEnvironment(const std::map<std::string, std::string>& env);

  bool init(ProgressCb cb = nullptr);
  bool kill();

  bool wine(const std::string &exe, const std::vector<std::string> &args,
            std::function<void(const std::string &)> onOutput = nullptr,
            const std::string &cwd = "", bool wait = true,
            const std::map<std::string, std::string>& env = {});

  void registryAdd(const std::string &key, const std::string &name,
                   const std::string &val, const std::string &type = "REG_SZ");

  bool registryCommit();

  std::optional<std::string> registryQuery(const std::string &key,
                                           const std::string &name);

  bool installDxvk(const std::string& dxvkRoot);

  std::string getPath() const { return rootDir_; }
  std::string getInstallDir() const { return installDir_; }

private:
  std::string rootDir_;
  std::string installDir_;
  std::vector<RegistryEntry> pendingReg_;
  
  std::string executorBinary_;
  std::vector<std::string> executorPreArgs_;
  std::vector<std::string> wrapperArgs_;
  std::map<std::string, std::string> baseEnv_;

  std::string resolveBinary(const std::string &binary) const;
  std::string generateRegFile() const;
  void addLibPaths(cmd::Options& opts) const;
};

}

#endif
