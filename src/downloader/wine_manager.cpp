#include "downloader/wine_manager.h"
#include "downloader/github_client.h"
#include "http.h"
#include "logger.h"
#include "path_manager.h"
#include "zip_util.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

namespace rsjfw::downloader {

namespace fs = std::filesystem;
using json = nlohmann::json;

WineManager &WineManager::instance() {
  static WineManager inst;
  return inst;
}

WineManager::WineManager() {
  auto &pm = PathManager::instance();
  wineDir_ = pm.wine();
  downloadsDir_ = pm.root() / "downloads";
  fs::create_directories(downloadsDir_);
}

std::vector<std::string> WineManager::fetchVersions(const std::string &repo) {
  LOG_DEBUG("Fetching Wine versions for %s", repo.c_str());
  auto releases = GithubClient::fetchReleases(repo);
  std::vector<std::string> tags;
  for (const auto &r : releases)
    tags.push_back(r.tag);
  return tags;
}

bool WineManager::installVersion(const std::string &repo,
                                 const std::string &tag,
                                 const std::string &assetName, ProgressCb cb) {
  LOG_INFO("Provisioning Wine/Proton %s (%s)", repo.c_str(), tag.c_str());
  auto releases = GithubClient::fetchReleases(repo);
  const GithubRelease *target = nullptr;

  if (tag == "latest" && !releases.empty())
    target = &releases[0];
  else {
    for (const auto &r : releases) {
      if (r.tag == tag) {
        target = &r;
        break;
      }
    }
  }

  if (!target) {
    LOG_ERROR("Runner version not found: %s @ %s", repo.c_str(), tag.c_str());
    return false;
  }

  const GithubAsset *asset = nullptr;
  if (!assetName.empty()) {
    for (const auto &a : target->assets) {
      if (a.name == assetName) {
        asset = &a;
        break;
      }
    }
  } else {
    for (const char *ext : {".tar.xz", ".tar.gz", ".tar"}) {
      for (const auto &a : target->assets) {
        if (a.name.length() >= strlen(ext) &&
            a.name.compare(a.name.length() - strlen(ext), strlen(ext), ext) ==
                0) {
          if (a.name.find(".asc") != std::string::npos)
            continue;
          if (a.name.find(".sha256") != std::string::npos)
            continue;
          asset = &a;
          goto found_asset;
        }
      }
    }
  }

found_asset:;
  if (!asset) {
    LOG_ERROR("No suitable wine asset found in %s", target->tag.c_str());
    return false;
  }

  LOG_DEBUG("Selected asset: %s (%zu bytes)", asset->name.c_str(), asset->size);
  if (cb)
    cb(0.0f, "downloading " + asset->name + "...");

  fs::path dlPath = downloadsDir_ / asset->name;
  if (!HTTP::download(
          asset->url, dlPath.string(),
          makeSubProgress(0.0f, 0.7f, "downloading runner archive...", cb))) {
    LOG_ERROR("Download failed for %s", asset->url.c_str());
    return false;
  }

  LOG_DEBUG("Extracting %s to %s", dlPath.c_str(), wineDir_.c_str());
  if (cb)
    cb(0.7f, "extracting archive...");

  std::set<fs::path> existing;
  fs::create_directories(wineDir_);
  for (const auto &p : fs::directory_iterator(wineDir_))
    existing.insert(p);

  if (!ZipUtil::extract(dlPath.string(), wineDir_.string(),
                        [&](float p, std::string s) {
                          if (cb)
                            cb(0.7f + (p * 0.3f), s);
                        })) {
    LOG_ERROR("Extract failed for %s", dlPath.c_str());
    return false;
  }

  fs::path newRoot;
  for (const auto &p : fs::directory_iterator(wineDir_)) {
    if (existing.find(p) == existing.end() && fs::is_directory(p)) {
      if (fs::exists(p.path() / "bin" / "wine") ||
          fs::exists(p.path() / "files" / "bin" / "wine") ||
          fs::exists(p.path() / "proton")) {
        newRoot = p;
        break;
      }
    }
  }

  if (newRoot.empty()) {
    LOG_ERROR("Extraction completed but no valid Wine/Proton root found in %s. "
              "Assuming we did extract?",
              wineDir_.c_str());
    newRoot = wineDir_;
  }

  LOG_INFO("Successfully installed to %s", newRoot.c_str());

  json meta;
  meta["repo"] = repo;
  meta["tag"] = target->tag;
  meta["asset"] = asset->name;
  std::ofstream(newRoot / "rsjfw_meta.json") << meta.dump(4);

  fs::remove(dlPath);
  if (cb)
    cb(1.0f, "installed " + target->tag);
  return true;
}

std::vector<InstalledWine> WineManager::getInstalledVersions() {
  std::vector<InstalledWine> list;
  if (!fs::exists(wineDir_))
    return list;

  for (const auto &e : fs::directory_iterator(wineDir_)) {
    if (!e.is_directory())
      continue;
    fs::path bin = e.path() / "bin" / "wine";
    if (!fs::exists(bin))
      bin = e.path() / "files" / "bin" / "wine";
    if (fs::exists(bin)) {
      InstalledWine w;
      w.name = e.path().filename().string();
      w.path = e.path().string();
      fs::path m = e.path() / "rsjfw_meta.json";
      if (fs::exists(m)) {
        try {
          auto j = json::parse(std::ifstream(m));
          w.repo = j.value("repo", "");
          w.tag = j.value("tag", "");
          w.asset = j.value("asset", "");
        } catch (...) {
        }
      }
      list.push_back(w);
    }
  }
  return list;
}

bool WineManager::deleteVersion(const std::string &path) {
  LOG_DEBUG("Deleting runner at %s", path.c_str());
  if (fs::exists(path)) {
    fs::remove_all(path);
    return true;
  }
  return false;
}

} // namespace rsjfw::downloader
