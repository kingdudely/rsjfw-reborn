#ifndef RSJFW_DOWNLOADER_GITHUB_TYPES_H
#define RSJFW_DOWNLOADER_GITHUB_TYPES_H

#include <string>
#include <vector>

namespace rsjfw::downloader {

struct GithubAsset {
    std::string name;
    std::string url;
    size_t size;
};

struct GithubRelease {
    std::string tag;
    std::string name;
    std::string htmlUrl;
    bool prerelease;
    std::vector<GithubAsset> assets;
};

}

#endif
