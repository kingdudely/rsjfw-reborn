#ifndef DXVK_VIEW_H
#define DXVK_VIEW_H

#include "gui/view.h"
#include "downloader/github_client.h"
#include <string>
#include <vector>
#include <mutex>

namespace rsjfw {

class DxvkView : public View {
public:
    DxvkView();
    void render() override;
    const char* getName() const override { return "DXVK"; }

private:
    void refreshVersions(const std::string& repo);

    std::vector<downloader::GithubRelease> releases_;
    std::string lastRepo_;
    bool fetching_ = false;
    bool repoValid_ = true;
    std::mutex mtx_;
};

}

#endif
