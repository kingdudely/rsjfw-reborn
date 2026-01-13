#include "downloader/github_client.h"
#include "http.h"
#include "logger.h"
#include <nlohmann/json.hpp>

namespace rsjfw::downloader {

using json = nlohmann::json;

std::vector<GithubRelease> GithubClient::fetchReleases(const std::string& repo) {
    std::vector<GithubRelease> releases;
    std::string url = "https://api.github.com/repos/" + repo + "/releases";
    try {
        std::string resp = HTTP::get(url);
        auto j = json::parse(resp);
        if (!j.is_array()) return releases;

        for (const auto& rel : j) {
            GithubRelease r;
            r.tag = rel.value("tag_name", "");
            r.name = rel.value("name", "");
            r.htmlUrl = rel.value("html_url", "");
            r.prerelease = rel.value("prerelease", false);

            if (rel.contains("assets")) {
                for (const auto& asset : rel["assets"]) {
                    GithubAsset a;
                    a.name = asset.value("name", "");
                    a.url = asset.value("browser_download_url", "");
                    a.size = asset.value("size", 0);
                    r.assets.push_back(a);
                }
            }
            releases.push_back(r);
        }
    } catch (...) {}
    return releases;
}

std::optional<GithubRelease> GithubClient::fetchLatest(const std::string& repo) {
    auto releases = fetchReleases(repo);
    if (releases.empty()) return std::nullopt;
    return releases[0];
}

bool GithubClient::isValidRepo(const std::string& repo) {
    std::string url = "https://api.github.com/repos/" + repo;
    try {
        std::string resp = HTTP::get(url);
        auto j = json::parse(resp);
        return j.contains("id");
    } catch (...) {
        return false;
    }
}

}
