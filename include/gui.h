#ifndef GUI_H
#define GUI_H

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <map>
#include <atomic>

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

private:
    GUI() = default;
    ~GUI() = default;

    void renderConfig();
    void renderLauncher();
    
    void renderGeneralTab();
    void renderRunnerTab();
    void renderWineConfig();
    void renderProtonConfig();
    void renderDxvkTab();
    void renderFFlagsTab();
    void renderEnvTab();
    void renderTroubleshootingTab();

    GLFWwindow* window_ = nullptr;
    Mode mode_ = MODE_CONFIG;
    bool running_ = false;
    
    float progress_ = 0.0f;
    std::string status_ = "Initializing...";
    std::mutex mutex_;

    unsigned int logoTexture_ = 0;
    int logoWidth_ = 0;
    int logoHeight_ = 0;
};

}

#endif
