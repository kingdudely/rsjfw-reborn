#ifndef HOME_VIEW_H
#define HOME_VIEW_H

#include "gui/view.h"
#include "diagnostics.h"
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
    std::atomic<float> fixProgress_{-1.0f};
    std::string fixStatus_;
    std::mutex statusMtx_;
};

}

#endif
