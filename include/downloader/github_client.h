#ifndef RSJFW_DOWNLOADER_GITHUB_CLIENT_H
#define RSJFW_DOWNLOADER_GITHUB_CLIENT_H

#include <string>
#include <vector>
#include <optional>
#include "github_types.h"

namespace rsjfw::downloader {

class GithubClient {
public:
    static std::vector<GithubRelease> fetchReleases(const std::string& repo);
    static std::optional<GithubRelease> fetchLatest(const std::string& repo);
    static bool isValidRepo(const std::string& repo);
};

}

#endif
