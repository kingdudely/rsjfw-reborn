#ifndef GUI_H
#define GUI_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "gui/view.h"

struct GLFWwindow;

namespace rsjfw {

class GUI {
public:
    static GUI& instance();

    bool init();
    void run();
    void shutdown();

    enum Mode {
        MODE_CONFIG,
        MODE_LAUNCHER
    };
    void setMode(Mode mode);

    void setProgress(float progress, const std::string& status);
    
    bool isRunning() const { return running_; }

    void renderTopbar();
    
    unsigned int getLogoTexture() const { return logoTexture_; }
    unsigned int getWideLogoTexture() const { return wideLogoTexture_; }
    int getWideLogoWidth() const { return wideLogoWidth_; }
    int getWideLogoHeight() const { return wideLogoHeight_; }

private:
    GUI();
    ~GUI() = default;

    void renderConfig();
    void renderLauncher();
    
    std::vector<std::unique_ptr<View>> views_;
    int currentTab_ = 0;
    
    float indicatorY_ = 0.0f;
    float targetIndicatorY_ = 0.0f;

    GLFWwindow* window_ = nullptr;
    unsigned int logoTexture_ = 0;
    unsigned int wideLogoTexture_ = 0;
    int wideLogoWidth_ = 0, wideLogoHeight_ = 0;

    std::atomic<bool> running_{false};
    Mode mode_ = MODE_CONFIG;
    
    float progress_ = 0.0f;
    std::string status_;
    std::mutex mutex_;
};

}

#endif
