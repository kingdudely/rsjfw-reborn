#ifndef HOME_VIEW_H
#define HOME_VIEW_H

#include "gui/view.h"
#include "credential_manager.h"
#include <vector>
#include <string>
#include <atomic>
#include <mutex>

namespace rsjfw {

class HomeView : public View {
public:
    HomeView();
    void render() override;
    const char* getName() const override { return "Home"; }

private:
    std::vector<RobloxUser> cachedUsers_;
    std::atomic<bool> refreshing_{false};
    std::mutex usersMtx_;
    void refreshUsers();
};


}

#endif
