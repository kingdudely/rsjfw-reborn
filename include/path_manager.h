#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>
#include <unistd.h>

namespace rsjfw {

class PathManager {
public:
  static PathManager &instance() {
    static PathManager inst;
    return inst;
  }

  void init() {
    std::filesystem::create_directories(root_);
    std::filesystem::create_directories(cache());
    std::filesystem::create_directories(versions());
    std::filesystem::create_directories(prefix());
    std::filesystem::create_directories(wine());
  }

  std::filesystem::path root() const { return root_; }
  std::filesystem::path cache() const { return root_ / "cache"; }
  std::filesystem::path versions() const { return root_ / "versions"; }
  std::filesystem::path prefix() const { return root_ / "prefix"; }
  std::filesystem::path wine() const { return root_ / "wine"; }

  std::filesystem::path executablePath() const {
    char buf[1024];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len != -1) {
      buf[len] = '\0';
      return std::filesystem::path(buf);
    }
    return "";
  }

  std::vector<std::filesystem::path> all_versions() const {
    std::vector<std::filesystem::path> allVers{};
    for (const auto &e : std::filesystem::directory_iterator(versions()))
      allVers.push_back(e);
    return allVers;
  }

private:
  PathManager() {
    const char *home = getenv("HOME");
    root_ = std::filesystem::path(home ? home : ".") / ".rsjfw";
  }
  std::filesystem::path root_;
};

}

#endif
