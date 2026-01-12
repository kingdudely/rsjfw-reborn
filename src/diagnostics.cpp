#include "diagnostics.h"
#include "path_manager.h"
#include "logger.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <unistd.h>

namespace rsjfw {

namespace fs = std::filesystem;

Diagnostics& Diagnostics::instance() {
    static Diagnostics inst;
    return inst;
}

int Diagnostics::failureCount() const {
    int count = 0;
    for (const auto& r : results_) {
        if (!r.second.ok) count++;
    }
    return count;
}

void Diagnostics::runChecks() {
    std::lock_guard<std::mutex> lock(mtx_);
    results_.clear();
    
    checkDependencies();
    checkDesktopFile();
    checkProtocolHandler();
}

void Diagnostics::fixIssue(const std::string& name, std::function<void(float, std::string)> progressCb) {
    for (auto& r : results_) {
        if (r.first == name && r.second.fixable && r.second.fixAction) {
            r.second.fixAction(progressCb);
            return;
        }
    }
}

void Diagnostics::checkDependencies() {
    std::vector<std::string> deps = {"cabextract", "curl", "tar", "unzip"};
    std::vector<std::string> missing;
    
    for (const auto& dep : deps) {
        if (system(("which " + dep + " > /dev/null 2>&1").c_str()) != 0) {
            missing.push_back(dep);
        }
    }
    
    HealthStatus s;
    s.ok = missing.empty();
    s.category = "System";
    if (s.ok) {
        s.message = "All core dependencies found";
    } else {
        s.message = "Missing dependencies";
        s.detail = "Missing: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            s.detail += missing[i] + (i == missing.size() - 1 ? "" : ", ");
        }
        s.fixable = false; 
    }
    results_.push_back({"Core Dependencies", s});
}

void Diagnostics::checkDesktopFile() {
    auto& pm = PathManager::instance();
    const char* homeEnv = getenv("HOME");
    if (!homeEnv) return;
    
    fs::path home = homeEnv;
    fs::path desktopPath = home / ".local/share/applications/rsjfw.desktop";
    fs::path currentExe = pm.executablePath();
    
    bool exists = fs::exists(desktopPath);
    bool valid = false;
    
    if (exists) {
        std::ifstream f(desktopPath);
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("Exec=") == 0) {
                if (line.find(currentExe.string()) != std::string::npos) {
                    valid = true;
                }
                break;
            }
        }
    }
    
    HealthStatus s;
    s.ok = exists && valid;
    s.category = "Integration";
    s.message = s.ok ? "Desktop entry valid" : (exists ? "Desktop entry stale" : "Desktop entry missing");
    s.fixable = true;
    s.fixAction = [desktopPath, currentExe](std::function<void(float, std::string)> cb) {
        cb(0.1f, "Generating desktop file...");
        fs::create_directories(desktopPath.parent_path());
        std::ofstream f(desktopPath);
        f << "[Desktop Entry]\n"
          << "Name=RSJFW\n"
          << "Exec=" << currentExe.string() << " launch %u\n"
          << "Type=Application\n"
          << "Terminal=false\n"
          << "Icon=rsjfw\n"
          << "MimeType=x-scheme-handler/roblox-player;x-scheme-handler/roblox-studio;\n"
          << "Categories=Game;\n"
          << "Actions=Configure;\n\n"
          << "[Desktop Action Configure]\n"
          << "Name=Configure RSJFW\n"
          << "Exec=" << currentExe.string() << " config\n";
        f.close();
        
        system("update-desktop-database ~/.local/share/applications > /dev/null 2>&1");
        cb(1.0f, "Done");
    };
    
    results_.push_back({"Desktop Entry", s});
}

void Diagnostics::checkProtocolHandler() {
    bool playerOk = system("xdg-mime query default x-scheme-handler/roblox-player | grep rsjfw > /dev/null 2>&1") == 0;
    bool studioOk = system("xdg-mime query default x-scheme-handler/roblox-studio | grep rsjfw > /dev/null 2>&1") == 0;
    
    HealthStatus s;
    s.ok = playerOk && studioOk;
    s.category = "Integration";
    s.message = s.ok ? "Protocol handlers registered" : "Protocol handlers missing/incorrect";
    s.fixable = true;
    s.fixAction = [](std::function<void(float, std::string)> cb) {
        cb(0.5f, "Registering protocol handlers...");
        system("xdg-mime default rsjfw.desktop x-scheme-handler/roblox-player > /dev/null 2>&1");
        system("xdg-mime default rsjfw.desktop x-scheme-handler/roblox-studio > /dev/null 2>&1");
        cb(1.0f, "Done");
    };
    
    results_.push_back({"Protocol Handlers", s});
}

}
