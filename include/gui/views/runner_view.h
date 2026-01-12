#ifndef RUNNER_VIEW_H
#define RUNNER_VIEW_H

#include "gui/view.h"
#include "downloader/github_client.h"
#include <string>
#include <vector>
#include <future>
#include <mutex>

namespace rsjfw {

class RunnerView : public View {
public:
    RunnerView();
    void render() override;
    const char* getName() const override { return "Runner"; }

private:
    void renderWineConfig();
    void renderProtonConfig();
    void refreshVersions(const std::string& repo, bool isProton);

    std::vector<downloader::GithubRelease> wineReleases_;
    std::vector<downloader::GithubRelease> protonReleases_;
    std::string lastWineRepo_;
    std::string lastProtonRepo_;
    bool fetchingWine_ = false;
    bool fetchingProton_ = false;
    bool wineRepoValid_ = true;
    bool protonRepoValid_ = true;
    
    std::mutex mtx_;
};

}

#endif
