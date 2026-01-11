#include "gui.h"
#include "config.h"
#include "logo_data.h"
#include "downloader/wine_manager.h"
#include "downloader/dxvk_manager.h"
#include "downloader/roblox_manager.h"
#include "downloader/github_client.h"
#include "orchestrator.h"
#include "path_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <ImGuiFileDialog.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <future>
#include <filesystem>

namespace rsjfw {

namespace fs = std::filesystem;
using json = nlohmann::json;

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

static bool InputTextString(const char* label, std::string& str) {
    char buf[256];
    strncpy(buf, str.c_str(), 255);
    buf[255] = '\0';
    if (ImGui::InputText(label, buf, 256)) {
        str = std::string(buf);
        return true;
    }
    return false;
}

GUI& GUI::instance() {
    static GUI inst;
    return inst;
}

bool GUI::init() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return false;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    window_ = glfwCreateWindow(800, 600, "RSJFW", NULL, NULL);
    if (!window_) return false;

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.86f, 0.08f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.96f, 0.18f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.76f, 0.02f, 0.18f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.86f, 0.08f, 0.24f, 0.50f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    
    colors[ImGuiCol_TitleBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.86f, 0.08f, 0.24f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 0.75f);

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    int w, h, c;
    unsigned char* data = stbi_load_from_memory(assets_logo_png, assets_logo_png_len, &w, &h, &c, 4);
    if (data) {
        glGenTextures(1, &logoTexture_);
        glBindTexture(GL_TEXTURE_2D, logoTexture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        logoWidth_ = w;
        logoHeight_ = h;
        stbi_image_free(data);
    }

    return true;
}

void GUI::setMode(Mode mode) {
    mode_ = mode;
    if (mode_ == MODE_LAUNCHER) {
        glfwSetWindowSize(window_, 500, 300);
        glfwSetWindowTitle(window_, "RSJFW Launcher");
    } else {
        glfwSetWindowSize(window_, 900, 600);
        glfwSetWindowTitle(window_, "RSJFW Configuration");
    }
    
    const GLFWvidmode* modeVid = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int w, h;
    glfwGetWindowSize(window_, &w, &h);
    glfwSetWindowPos(window_, (modeVid->width - w) / 2, (modeVid->height - h) / 2);
    
    glfwShowWindow(window_);
}

void GUI::setProgress(float progress, const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
    status_ = status;
}

void GUI::run() {
    running_ = true;
    while (!glfwWindowShouldClose(window_) && running_) {
        glfwPollEvents();

        if (mode_ == MODE_LAUNCHER) {
            auto state = Orchestrator::instance().getState();
            if (state == LauncherState::RUNNING) {
                shutdown();
                break;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int w, h;
        glfwGetWindowSize(window_, &w, &h);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
        
        ImGui::Begin("Main", nullptr, flags);

        if (mode_ == MODE_CONFIG) renderConfig();
        else renderLauncher();

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }

    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
    }
}

void GUI::shutdown() {
    running_ = false;
    if (window_) {
        glfwSetWindowShouldClose(window_, 1);
    }
}

void GUI::renderConfig() {
    static int tab = 0;
    
    ImGui::BeginChild("Sidebar", ImVec2(150, 0), true);
    if (ImGui::Button("General", ImVec2(130, 30))) tab = 0;
    if (ImGui::Button("Runner", ImVec2(130, 30))) tab = 1;
    if (ImGui::Button("DXVK", ImVec2(130, 30))) tab = 2;
    if (ImGui::Button("FFlags", ImVec2(130, 30))) tab = 3;
    if (ImGui::Button("Env Vars", ImVec2(130, 30))) tab = 4;
    if (ImGui::Button("Troubleshooting", ImVec2(130, 30))) tab = 5;
    
    ImGui::Dummy(ImVec2(0, 20));
    if (ImGui::Button("Save & Exit", ImVec2(130, 30))) {
        Config::instance().save();
        shutdown();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Content", ImVec2(0, 0), true);
    switch (tab) {
        case 0: renderGeneralTab(); break;
        case 1: renderRunnerTab(); break;
        case 2: renderDxvkTab(); break;
        case 3: renderFFlagsTab(); break;
        case 4: renderEnvTab(); break;
        case 5: renderTroubleshootingTab(); break;
    }
    ImGui::EndChild();
}

void GUI::renderGeneralTab() {
    auto& gen = Config::instance().getGeneral();
    
    ImGui::Text("General Settings");
    ImGui::Separator();
    
    const char* renderers[] = { "Automatic", "D3D11", "Vulkan", "OpenGL" };
    int currentRenderer = 0;
    for (int i = 0; i < 4; i++) {
        if (gen.renderer == renderers[i]) {
            currentRenderer = i;
            break;
        }
    }
    if (ImGui::Combo("Renderer", &currentRenderer, renderers, 4)) {
        gen.renderer = renderers[currentRenderer];
    }
    
    ImGui::InputInt("Target FPS", &gen.targetFps);
    
    if (ImGui::BeginCombo("Lighting", gen.lightingTechnology.c_str())) {
        if (ImGui::Selectable("Default", gen.lightingTechnology == "Default")) gen.lightingTechnology = "Default";
        if (ImGui::Selectable("Future", gen.lightingTechnology == "Future")) gen.lightingTechnology = "Future";
        if (ImGui::Selectable("ShadowMap", gen.lightingTechnology == "ShadowMap")) gen.lightingTechnology = "ShadowMap";
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Use DXVK", &gen.dxvk);
    ImGui::Checkbox("Dark Mode", &gen.darkMode);

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Desktop Mode");
    ImGui::Separator();
    ImGui::Checkbox("Virtual Desktop Mode", &gen.desktopMode);
    ImGui::Checkbox("Multi-Desktop (Unique Session)", &gen.multipleDesktops);
    InputTextString("Resolution (WxH)", gen.desktopResolution);
}

void GUI::renderRunnerTab() {
    auto& gen = Config::instance().getGeneral();
    
    ImGui::Text("Runner Configuration");
    ImGui::Separator();
    
    const char* runners[] = { "Wine", "Proton" };
    int currentRunner = (gen.runnerType == "Proton") ? 1 : 0;
    if (ImGui::Combo("Runner Type", &currentRunner, runners, 2)) {
        gen.runnerType = runners[currentRunner];
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Launch Wrapper");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Command to prepend (e.g. 'gamemoderun' or 'gamescope')");
    
    InputTextString("##Wrapper", gen.launchWrapper);
    
    ImGui::SameLine();
    if (ImGui::Button("Gamemode")) gen.launchWrapper = "gamemoderun";
    ImGui::SameLine();
    if (ImGui::Button("Gamescope")) gen.launchWrapper = "gamescope -W 1920 -H 1080 -r 144 --";
    
    ImGui::Checkbox("Enable MangoHud", &gen.enableMangoHud);
    ImGui::SameLine();
    ImGui::Checkbox("Enable WebView2", &gen.enableWebView2);
    
    ImGui::Dummy(ImVec2(0, 20));
    
    if (gen.runnerType == "Proton") renderProtonConfig();
    else renderWineConfig();
}

void GUI::renderWineConfig() {
    auto& src = Config::instance().getGeneral().wineSource;
    ImGui::Text("Wine Settings");
    ImGui::Separator();

    static bool mode = src.useCustomRoot;
    if (ImGui::RadioButton("Use GitHub Repository", !mode)) { mode = false; src.useCustomRoot = false; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Use Static Path", mode)) { mode = true; src.useCustomRoot = true; }

    ImGui::Dummy(ImVec2(0, 10));

    if (!mode) {
        const char* presets[] = { "vinegarhq/wine-builds" };
        static int currentPreset = -1;
        
        if (ImGui::Combo("Presets", &currentPreset, presets, 1)) {
            src.repo = presets[currentPreset];
        }

        static std::string lastRepo = "";
        static bool repoValid = false;
        static std::vector<std::string> versions;
        static std::future<void> fetchFuture;
        static bool fetching = false;

        InputTextString("GitHub Repo", src.repo);

        if (src.repo != lastRepo && !fetching) {
            lastRepo = src.repo;
            fetching = true;
            versions.clear();
            fetchFuture = std::async(std::launch::async, [this, &src]() {
                if (downloader::GithubClient::isValidRepo(src.repo)) {
                    repoValid = true;
                    auto rels = downloader::GithubClient::fetchReleases(src.repo);
                    for (const auto& r : rels) versions.push_back(r.tag);
                } else {
                    repoValid = false;
                }
                fetching = false;
            });
        }

        if (fetching) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Checking...");
        } else if (!repoValid) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid Repo!");
        } else {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid");
        }

        if (ImGui::BeginCombo("Version", src.version.c_str())) {
            if (ImGui::Selectable("latest", src.version == "latest")) src.version = "latest";
            for (const auto& v : versions) {
                if (ImGui::Selectable(v.c_str(), src.version == v)) src.version = v;
            }
            ImGui::EndCombo();
        }
    } else {
        InputTextString("Path", src.customRootPath);
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            ImGuiFileDialog::Instance()->OpenDialog("ChooseDir", "Choose Directory", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseDir", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                src.customRootPath = ImGuiFileDialog::Instance()->GetFilePathName();
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Current Active Root:");
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", src.installedRoot.empty() ? "None" : src.installedRoot.c_str());
    
    if (ImGui::Button("Force Update / Reinstall")) {
        src.installedRoot = "";
        Config::instance().save();
    }
}

void GUI::renderProtonConfig() {
    auto& src = Config::instance().getGeneral().protonSource;
    ImGui::Text("Proton Settings");
    ImGui::Separator();

    static bool mode = src.useCustomRoot;
    if (ImGui::RadioButton("Use GitHub Repository", !mode)) { mode = false; src.useCustomRoot = false; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Use Static Path", mode)) { mode = true; src.useCustomRoot = true; }

    ImGui::Dummy(ImVec2(0, 10));

    if (!mode) {
        const char* presets[] = { "GloriousEggroll/proton-ge-custom" };
        static int currentPreset = -1;
        
        if (ImGui::Combo("Presets", &currentPreset, presets, 1)) {
            src.repo = presets[currentPreset];
        }

        static std::string lastRepo = "";
        static bool repoValid = false;
        static std::vector<std::string> versions;
        static std::future<void> fetchFuture;
        static bool fetching = false;

        InputTextString("GitHub Repo", src.repo);

        if (src.repo != lastRepo && !fetching) {
            lastRepo = src.repo;
            fetching = true;
            versions.clear();
            fetchFuture = std::async(std::launch::async, [this, &src]() {
                if (downloader::GithubClient::isValidRepo(src.repo)) {
                    repoValid = true;
                    auto rels = downloader::GithubClient::fetchReleases(src.repo);
                    for (const auto& r : rels) versions.push_back(r.tag);
                } else {
                    repoValid = false;
                }
                fetching = false;
            });
        }

        if (fetching) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Checking...");
        } else if (!repoValid) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid Repo!");
        } else {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid");
        }

        if (ImGui::BeginCombo("Version", src.version.c_str())) {
            if (ImGui::Selectable("latest", src.version == "latest")) src.version = "latest";
            for (const auto& v : versions) {
                if (ImGui::Selectable(v.c_str(), src.version == v)) src.version = v;
            }
            ImGui::EndCombo();
        }
    } else {
        InputTextString("Path", src.customRootPath);
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            ImGuiFileDialog::Instance()->OpenDialog("ChooseDirProton", "Choose Directory", nullptr, ".", 1, nullptr, ImGuiFileDialogFlags_Modal);
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseDirProton", ImGuiWindowFlags_NoCollapse, ImVec2(400, 600))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                src.customRootPath = ImGuiFileDialog::Instance()->GetFilePathName();
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Current Active Root:");
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", src.installedRoot.empty() ? "None" : src.installedRoot.c_str());
    
    if (ImGui::Button("Force Update / Reinstall")) {
        src.installedRoot = "";
        Config::instance().save();
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Environment Toggles");
    ImGui::Separator();
    auto& gen = Config::instance().getGeneral();
    ImGui::Checkbox("Enable Fsync", &gen.enableFsync);
    ImGui::SameLine();
    ImGui::Checkbox("Enable Esync", &gen.enableEsync);
}

void GUI::renderDxvkTab() {
    auto& src = Config::instance().getGeneral().dxvkSource;
    ImGui::Text("DXVK Configuration");
    ImGui::Separator();
    
    static std::string lastRepo = "";
    static bool repoValid = false;
    static std::vector<std::string> versions;
    static std::future<void> fetchFuture;
    static bool fetching = false;

    InputTextString("GitHub Repo", src.repo);

    if (src.repo != lastRepo && !fetching) {
        lastRepo = src.repo;
        fetching = true;
        versions.clear();
        fetchFuture = std::async(std::launch::async, [this, &src]() {
            if (downloader::GithubClient::isValidRepo(src.repo)) {
                repoValid = true;
                auto rels = downloader::GithubClient::fetchReleases(src.repo);
                for (const auto& r : rels) versions.push_back(r.tag);
            } else {
                repoValid = false;
            }
            fetching = false;
        });
    }

    if (fetching) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Checking...");
    } else if (!repoValid) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid Repo!");
    } else {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Valid");
    }

    if (ImGui::BeginCombo("Version", src.version.c_str())) {
        if (ImGui::Selectable("latest", src.version == "latest")) src.version = "latest";
        for (const auto& v : versions) {
            if (ImGui::Selectable(v.c_str(), src.version == v)) src.version = v;
        }
        ImGui::EndCombo();
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Text("Current Local Root:");
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", src.installedRoot.empty() ? "None" : src.installedRoot.c_str());
    
    if (ImGui::Button("Force Reinstall DXVK")) {
        src.installedRoot = "";
        Config::instance().save();
    }
}

void GUI::renderFFlagsTab() {
    ImGui::Text("Fast Flags Editor");
    ImGui::Separator();

    if (ImGui::Button("FPS Unlock (999)")) {
        Config::instance().getFFlags()["DFIntTaskSchedulerTargetFps"] = 999;
    }
    ImGui::SameLine();
    if (ImGui::Button("Quality Preset")) {
        Config::instance().getFFlags()["FFlagDebugForceFutureIsBrightPhase3"] = true;
        Config::instance().getGeneral().renderer = "Vulkan";
        Config::instance().getGeneral().lightingTechnology = "Future";
    }
    ImGui::SameLine();
    if (ImGui::Button("Performance Preset")) {
        Config::instance().getFFlags()["FFlagDebugForceFutureIsBrightPhase3"] = false;
        Config::instance().getGeneral().renderer = "Vulkan";
        Config::instance().getGeneral().lightingTechnology = "ShadowMap";
    }

    ImGui::Separator();
    
    auto& fflags = Config::instance().getFFlags();
    
    static char keyBuf[256] = "";
    static char valBuf[256] = "";
    
    ImGui::InputText("Flag Name", keyBuf, 256);
    ImGui::InputText("Value (JSON)", valBuf, 256);
    if (ImGui::Button("Add/Update")) {
        try {
            std::string k(keyBuf);
            if (!k.empty()) {
                json v = json::parse(valBuf);
                fflags[k] = v;
                memset(keyBuf, 0, 256);
                memset(valBuf, 0, 256);
            }
        } catch(...) {}
    }
    
    ImGui::Separator();
    
    if (ImGui::BeginTable("FFlags", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Value");
        ImGui::TableHeadersRow();
        
        auto it = fflags.begin();
        while (it != fflags.end()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", it->first.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", it->second.dump().c_str());
            it++;
        }
        ImGui::EndTable();
    }
}

void GUI::renderEnvTab() {
    auto& env = Config::instance().getGeneral().customEnv;
    ImGui::Text("Environment Variables");
    ImGui::Separator();
    
    static char keyBuf[256] = "";
    static char valBuf[256] = "";
    
    ImGui::InputText("Variable", keyBuf, 256);
    ImGui::InputText("Value", valBuf, 256);
    if (ImGui::Button("Set")) {
        std::string k(keyBuf);
        if (!k.empty()) {
            env[k] = std::string(valBuf);
            memset(keyBuf, 0, 256);
            memset(valBuf, 0, 256);
        }
    }
    
    ImGui::Separator();
    for (const auto& [k, v] : env) {
        ImGui::Text("%s = %s", k.c_str(), v.c_str());
    }
}

void GUI::renderTroubleshootingTab() {
    auto& gen = Config::instance().getGeneral();
    ImGui::Text("Maintenance Actions");
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Warning: Some actions are destructive!");
    ImGui::Separator();
    
    if (ImGui::Button("Wipe Prefix", ImVec2(200, 30))) {
        ImGui::OpenPopup("Confirm Wipe");
    }
    
    if (ImGui::BeginPopupModal("Confirm Wipe", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete the entire prefix?");
        ImGui::Text("All installed components will be removed.");
        
        if (ImGui::Button("Yes, Delete", ImVec2(120, 0))) {
            try {
                auto& pm = PathManager::instance();
                if (gen.runnerType == "Proton") {
                    fs::remove_all(pm.root() / "proton_data");
                } else {
                    fs::remove_all(pm.prefix());
                }
            } catch(...) {}
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::Button("Force Reconfigure", ImVec2(200, 30))) {
        try {
            auto& pm = PathManager::instance();
            if (gen.runnerType == "Proton") {
                fs::remove(pm.root() / "proton_data" / ".rsjfw_proton_provisioned");
            } else {
                fs::remove(pm.prefix() / ".rsjfw_provisioned");
            }
        } catch(...) {}
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Triggers dependency install on next launch");
    
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::Button("Clear Shader Cache", ImVec2(200, 30))) {
        try {
            auto& pm = PathManager::instance();
            for (auto& entry : fs::recursive_directory_iterator(pm.root())) {
                if (entry.path().extension() == ".dxvk-cache") {
                    fs::remove(entry.path());
                }
            }
        } catch(...) {}
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::Button("Clear Downloads", ImVec2(200, 30))) {
        try {
            fs::remove_all(PathManager::instance().cache());
            fs::create_directories(PathManager::instance().cache());
        } catch(...) {}
    }
    
    ImGui::Dummy(ImVec2(0, 10));
    
    if (ImGui::Button("Reset FFlags", ImVec2(200, 30))) {
        try {
            auto& rbx = downloader::RobloxManager::instance();
            std::string guid = rbx.getLatestVersionGUID();
            if (!guid.empty()) {
                fs::path settings = PathManager::instance().versions() / guid / "ClientSettings" / "ClientAppSettings.json";
                if (fs::exists(settings)) fs::remove(settings);
            }
        } catch(...) {}
    }
}

void GUI::renderLauncher() {
    float w = ImGui::GetWindowWidth();
    float h = ImGui::GetWindowHeight();
    
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float barH = 30.0f;
    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 barPos = ImVec2(winPos.x, winPos.y + h - barH);
    
    draw->AddRectFilled(barPos, ImVec2(barPos.x + w, barPos.y + barH), IM_COL32(30, 30, 30, 255));
    
    std::string s;
    float p;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        s = status_;
        p = progress_;
    }
    
    static float smoothP = 0.0f;
    smoothP += (p - smoothP) * ImGui::GetIO().DeltaTime * 5.0f;
    
    if (smoothP > 0.0f) {
        float fillW = w * smoothP;
        draw->AddRectFilled(barPos, ImVec2(barPos.x + fillW, barPos.y + barH), IM_COL32(220, 20, 60, 255));
    }
    
    if (logoTexture_) {
        float size = 150.0f;
        float aspect = (float)logoWidth_ / (float)logoHeight_;
        float lw = size * aspect;
        
        float availableH = h - barH;
        float yPos = (availableH - size) * 0.5f; 
        
        ImGui::SetCursorPos(ImVec2((w - lw) * 0.5f, yPos));
        ImGui::Image((void*)(intptr_t)logoTexture_, ImVec2(lw, size));
    }
    
    ImGui::SetCursorPos(ImVec2(10, h - barH + (barH - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::Text("%s", s.c_str());
    
    std::string pct = std::to_string((int)(smoothP * 100)) + "%";
    ImVec2 pctSize = ImGui::CalcTextSize(pct.c_str());
    ImGui::SetCursorPos(ImVec2(w - pctSize.x - 10, h - barH + (barH - pctSize.y) * 0.5f));
    ImGui::Text("%s", pct.c_str());
}

}
