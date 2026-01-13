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
    static auto lastCheck = std::chrono::steady_clock::now() - std::chrono::hours(1);
    auto now = std::chrono::steady_clock::now();
    
    // If results already exist and checked recently, skip in non-GUI mode
    // (In config mode results will be cleared by the UI if requested)
    if (!results_.empty() && std::chrono::duration_cast<std::chrono::seconds>(now - lastCheck).count() < 30) {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    results_.clear();
    
    checkDependencies();
    checkDesktopFile();
    checkProtocolHandler();
    
    lastCheck = now;
}

void Diagnostics::fixIssue(const std::string& name, std::function<void(float, std::string)> progressCb) {
    for (auto& r : results_) {
        if (r.first == name && r.second.fixable && r.second.fixAction) {
            r.second.isFixing = true;
            r.second.fixProgress = 0.0f;
            r.second.fixStatus = "starting fix...";
            
            auto internalCb = [&](float p, std::string s) {
                r.second.fixProgress = p;
                r.second.fixStatus = s;
                if (progressCb) progressCb(p, s);
            };

            r.second.fixAction(internalCb);
            r.second.isFixing = false;
            runChecks();
            return;
        }
    }
}

void Diagnostics::checkDependencies() {
    std::vector<std::string> deps = {"cabextract", "curl", "tar", "unzip"};
    std::vector<std::string> missing;
    
    std::string pathEnv = getenv("PATH");
    std::vector<std::string> paths;
    std::stringstream ss(pathEnv);
    std::string item;
    while (std::getline(ss, item, ':')) {
        paths.push_back(item);
    }

    for (const auto& dep : deps) {
        bool found = false;
        for (const auto& p : paths) {
            if (fs::exists(fs::path(p) / dep)) {
                found = true;
                break;
            }
        }
        if (!found) missing.push_back(dep);
    }
    
    HealthStatus s;
    s.ok = missing.empty();
    s.category = "System";
    if (s.ok) {
        s.message = "all core dependencies found";
    } else {
        s.message = "missing dependencies";
        s.failReason = "some required system tools are not installed on your host.";
        s.detail = "missing: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            s.detail += missing[i] + (i == missing.size() - 1 ? "" : ", ");
        }
        s.fixable = false; 
    }
    results_.push_back({"core dependencies", s});
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
    s.message = s.ok ? "desktop entry valid" : (exists ? "desktop entry stale" : "desktop entry missing");
    if (!s.ok) {
        s.failReason = exists ? "the desktop entry points to an old or different executable path." : "no desktop entry found for rsjfw.";
    }
    s.fixable = true;
    s.fixAction = [desktopPath, currentExe](std::function<void(float, std::string)> cb) {
        cb(0.1f, "generating desktop file...");
        fs::create_directories(desktopPath.parent_path());
        
        auto& pm = PathManager::instance();
        fs::path iconDir = fs::path(getenv("HOME")) / ".local/share/icons";
        fs::create_directories(iconDir);
        fs::path iconPath = iconDir / "rsjfw.png";
        
        fs::path sourceIcon = currentExe.parent_path().parent_path() / "assets" / "logo.png";
        if (fs::exists(sourceIcon)) {
            fs::copy_file(sourceIcon, iconPath, fs::copy_options::overwrite_existing);
        }

        std::ofstream f(desktopPath);
        f << "[Desktop Entry]\n"
          << "Name=rsjfw\n"
          << "Exec=" << currentExe.string() << " launch %u\n"
          << "Type=Application\n"
          << "Terminal=false\n"
          << "Icon=" << iconPath.string() << "\n"
          << "MimeType=x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;application/x-roblox-rbxl;application/x-roblox-rbxlx;\n"
          << "Categories=Development;Game;\n"
          << "Actions=Configure;\n\n"
          << "[Desktop Action Configure]\n"
          << "Name=configure rsjfw\n"
          << "Exec=" << currentExe.string() << " config\n";
        f.close();
        
        system("update-desktop-database ~/.local/share/applications > /dev/null 2>&1");
        
        fs::path mimeDir = fs::path(getenv("HOME")) / ".local/share/mime/packages";
        fs::create_directories(mimeDir);
        std::ofstream mf(mimeDir / "rsjfw.xml");
        mf << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
           << "  <mime-type type=\"application/x-roblox-rbxl\">\n"
           << "    <comment>roblox place file</comment>\n"
           << "    <glob pattern=\"*.rbxl\"/>\n"
           << "  </mime-type>\n"
           << "  <mime-type type=\"application/x-roblox-rbxlx\">\n"
           << "    <comment>roblox xml place file</comment>\n"
           << "    <glob pattern=\"*.rbxlx\"/>\n"
           << "  </mime-type>\n"
           << "</mime-info>\n";
        mf.close();
        system("update-mime-database ~/.local/share/mime > /dev/null 2>&1");
        
        cb(1.0f, "done");
    };
    
    results_.push_back({"desktop entry", s});
}

void Diagnostics::checkProtocolHandler() {
    bool studioOk = system("xdg-mime query default x-scheme-handler/roblox-studio | grep rsjfw > /dev/null 2>&1") == 0;
    bool authOk = system("xdg-mime query default x-scheme-handler/roblox-studio-auth | grep rsjfw > /dev/null 2>&1") == 0;
    bool rbxlOk = system("xdg-mime query default application/x-roblox-rbxl | grep rsjfw > /dev/null 2>&1") == 0;
    
    HealthStatus s;
    s.ok = studioOk && authOk && rbxlOk;
    s.category = "Integration";
    s.message = s.ok ? "protocol handlers registered" : "protocol handlers missing/incorrect";
    if (!s.ok) {
        s.failReason = "roblox-studio or rbxl file associations are not set to rsjfw.";
    }
    s.fixable = true;
    s.fixAction = [](std::function<void(float, std::string)> cb) {
        cb(0.5f, "registering protocol handlers...");
        system("xdg-mime default rsjfw.desktop x-scheme-handler/roblox-studio > /dev/null 2>&1");
        system("xdg-mime default rsjfw.desktop x-scheme-handler/roblox-studio-auth > /dev/null 2>&1");
        system("xdg-mime default rsjfw.desktop application/x-roblox-rbxl > /dev/null 2>&1");
        system("xdg-mime default rsjfw.desktop application/x-roblox-rbxlx > /dev/null 2>&1");
        cb(1.0f, "done");
    };
    
    results_.push_back({"protocol handlers", s});
}

}
