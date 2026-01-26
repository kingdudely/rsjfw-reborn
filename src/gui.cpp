#include "gui.h"
#include "config.h"
#include "diagnostics.h"
#include "font_icons.h"
#include "font_roboto.h"
#include "gui/icons.h"
#include "logger.h"
#include "logo_data.h"
#include "logo_wide_data.h"
#include "orchestrator.h"
#include "path_manager.h"

#include "gui/views/diagnostics_view.h"
#include "gui/views/dxvk_view.h"
#include "gui/views/env_view.h"
#include "gui/views/fflags_view.h"
#include "gui/views/general_view.h"
#include "gui/views/home_view.h"
#include "gui/views/runner_view.h"
#include "gui/views/troubleshoot_view.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

namespace rsjfw {

static std::map<int, float> tabHoverStates;

static void glfw_error_callback(int error, const char *description) {
  LOG_ERROR("GLFW Error %d: %s", error, description);
}

GUI &GUI::instance() {
  static GUI inst;
  return inst;
}

GUI::GUI() {
  views_.push_back(std::make_unique<HomeView>());
  views_.push_back(std::make_unique<GeneralView>());
  views_.push_back(std::make_unique<RunnerView>());
  views_.push_back(std::make_unique<DxvkView>());
  views_.push_back(std::make_unique<FFlagsView>());
  views_.push_back(std::make_unique<EnvView>());
  views_.push_back(std::make_unique<DiagnosticsView>());
  views_.push_back(std::make_unique<TroubleshootView>());
}

bool GUI::init() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return false;

  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

  window_ = glfwCreateWindow(1200, 750, "RSJFW", NULL, NULL);
  if (!window_)
    return false;

  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();

  ImFontConfig font_cfg;
  font_cfg.FontDataOwnedByAtlas = false;
  io.Fonts->AddFontFromMemoryTTF(_tmp_Roboto_Medium_ttf,
                                 _tmp_Roboto_Medium_ttf_len, 18.0f, &font_cfg);

  ImFontConfig icons_config;
  icons_config.MergeMode = true;
  icons_config.PixelSnapH = true;
  icons_config.FontDataOwnedByAtlas = false;
  static const ImWchar icons_ranges[] = {0xf000, 0xf8ff, 0};
  io.Fonts->AddFontFromMemoryTTF(_tmp_fa_solid_900_ttf,
                                 _tmp_fa_solid_900_ttf_len, 16.0f,
                                 &icons_config, icons_ranges);

  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowRounding = 0.0f;
  style.ChildRounding = 0.0f;
  style.FrameRounding = 0.0f;
  style.GrabRounding = 0.0f;
  style.PopupRounding = 0.0f;
  style.TabRounding = 0.0f;
  style.ScrollbarRounding = 0.0f;

  style.ItemSpacing = ImVec2(0, 0);
  style.WindowPadding = ImVec2(0, 0);

  ImVec4 *colors = style.Colors;
  colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.70f, 0.05f, 0.15f, 1.00f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.90f, 0.10f, 0.25f, 1.00f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.50f, 0.02f, 0.10f, 1.00f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.70f, 0.05f, 0.15f, 0.50f);
  colors[ImGuiCol_Border] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  int w, h, c;
  unsigned char *data = stbi_load_from_memory(
      assets_logo_png, assets_logo_png_len, &w, &h, &c, 4);
  if (data) {
    glGenTextures(1, &logoTexture_);
    glBindTexture(GL_TEXTURE_2D, logoTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    stbi_image_free(data);
  }

  data = stbi_load_from_memory(assets_logo_wide_png, assets_logo_wide_png_len,
                               &w, &h, &c, 4);
  if (data) {
    glGenTextures(1, &wideLogoTexture_);
    glBindTexture(GL_TEXTURE_2D, wideLogoTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 data);
    wideLogoWidth_ = w;
    wideLogoHeight_ = h;
    stbi_image_free(data);
  }

  return true;
}

void GUI::setMode(Mode mode) {
  mode_ = mode;
  if (mode_ == MODE_LAUNCHER) {
    glfwSetWindowSize(window_, 900, 450);
    glfwSetWindowTitle(window_, "RSJFW Launcher");
  } else {
    glfwSetWindowSize(window_, 1200, 750);
    glfwSetWindowTitle(window_, "RSJFW Dashboard");
  }
}

void GUI::setProgress(float progress, const std::string &status) {
  std::lock_guard<std::mutex> lock(mutex_);
  progress_ = progress;
  status_ = status;
}

void GUI::run() {
  LOG_INFO("GUI::run() called, mode: %d", mode_);
  auto &cfg = Config::instance().getGeneral();

  if (mode_ == MODE_LAUNCHER && cfg.hideLauncher) {
    LOG_INFO("Launcher hidden by configuration");
    glfwHideWindow(window_);
  } else {
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();
    if (monitor) {
      const GLFWvidmode *modeVid = glfwGetVideoMode(monitor);
      if (modeVid) {
        int w, h;
        glfwGetWindowSize(window_, &w, &h);
        glfwSetWindowPos(window_, (modeVid->width - w) / 2,
                         (modeVid->height - h) / 2);
      }
    } else {
      LOG_WARN("No primary monitor detected, using default position");
      glfwSetWindowPos(window_, 100, 100);
    }
    glfwShowWindow(window_);
    glfwPollEvents();
    LOG_INFO("Window shown");
  }

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

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::Begin("Main", nullptr, flags);
    if (mode_ == MODE_CONFIG)
      renderConfig();
    else
      renderLauncher();
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
  if (window_)
    glfwSetWindowShouldClose(window_, 1);
}

void GUI::renderConfig() {
  float winHeight = ImGui::GetWindowHeight();
  float winWidth = ImGui::GetWindowWidth();
  float dt = ImGui::GetIO().DeltaTime;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  ImGui::BeginChild("Sidebar", ImVec2(240, 0), true,
                    ImGuiWindowFlags_NoScrollbar);

  if (logoTexture_) {
    float s = 60.0f;
    ImGui::SetCursorPos(ImVec2(90, 30));
    ImGui::Image((void *)(intptr_t)logoTexture_, ImVec2(s, s));
  }
  ImGui::SetCursorPosY(110);

  float itemHeight = 55.0f;
  float startY = ImGui::GetCursorPosY();

  targetIndicatorY_ = startY + (currentTab_ * itemHeight);
  indicatorY_ += (targetIndicatorY_ - indicatorY_) * dt * 15.0f;

  ImVec2 p = ImGui::GetCursorScreenPos();
  ImDrawList *dl = ImGui::GetWindowDrawList();

  dl->AddRectFilled(
      ImVec2(p.x, p.y + (indicatorY_ - startY)),
      ImVec2(p.x + 240, p.y + (indicatorY_ - startY) + itemHeight),
      IM_COL32(180, 10, 40, 255));

  dl->AddRectFilled(ImVec2(p.x, p.y + (indicatorY_ - startY)),
                    ImVec2(p.x + 4, p.y + (indicatorY_ - startY) + itemHeight),
                    IM_COL32(255, 40, 70, 255));

  const char *icons[] = {ICON_FA_HOUSE,         ICON_FA_GEARS, ICON_FA_ROCKET,
                         ICON_FA_DESKTOP,       ICON_FA_FLAG,  ICON_FA_SLIDERS,
                         ICON_FA_SHIELD_HALVED, ICON_FA_WRENCH};

  for (int i = 0; i < (int)views_.size(); ++i) {
    ImGui::PushID(i);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));

    if (ImGui::Button("##tab", ImVec2(240, itemHeight))) {
      currentTab_ = i;
    }

    bool hovered = ImGui::IsItemHovered();
    if (tabHoverStates.find(i) == tabHoverStates.end())
      tabHoverStates[i] = 0.0f;

    float targetHover = hovered ? 1.0f : 0.0f;
    tabHoverStates[i] += (targetHover - tabHoverStates[i]) * dt * 10.0f;

    ImGui::PopStyleColor(3);

    ImVec2 rectMin = ImGui::GetItemRectMin();

    if (tabHoverStates[i] > 0.01f) {
      float lineWidth = 120.0f * tabHoverStates[i];
      dl->AddLine(ImVec2(rectMin.x + 65, rectMin.y + 40),
                  ImVec2(rectMin.x + 65 + lineWidth, rectMin.y + 40),
                  IM_COL32(255, 40, 70, 255 * tabHoverStates[i]), 2.0f);
    }

    bool selected = (currentTab_ == i);
    ImU32 textCol =
        selected ? IM_COL32(255, 255, 255, 255) : IM_COL32(180, 180, 180, 255);
    if (hovered)
      textCol = IM_COL32(255, 255, 255, 255);

    const char *icon = (i < 8) ? icons[i] : "?";

    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::GetWindowDrawList()->AddText(ImVec2(rectMin.x + 30, rectMin.y + 18),
                                        textCol, icon);
    ImGui::GetWindowDrawList()->AddText(ImVec2(rectMin.x + 65, rectMin.y + 18),
                                        textCol, views_[i]->getName());
    ImGui::PopFont();

    ImGui::PopID();
  }

  float bottomHeight = 60.0f;
  ImGui::SetCursorPosY(winHeight - bottomHeight);

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.05f, 0.15f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.80f, 0.10f, 0.20f, 1.0f));
  if (ImGui::Button("save & exit", ImVec2(120, bottomHeight))) {
    Config::instance().save();
    shutdown();
  }
  ImGui::PopStyleColor(2);

  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
  if (ImGui::Button("discard", ImVec2(120, bottomHeight))) {
    shutdown();
  }
  ImGui::PopStyleColor(2);

  ImGui::EndChild();
  ImGui::PopStyleColor();
  ImGui::PopStyleVar();

  ImGui::SameLine();

  ImGui::BeginGroup();
  renderTopbar();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.00f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(40, 40));
  ImGui::BeginChild("Content", ImVec2(0, 0), false,
                    ImGuiWindowFlags_NoScrollbar);

  ImGui::Dummy(ImVec2(0, 10));

  if (currentTab_ >= 0 && currentTab_ < (int)views_.size()) {
    views_[currentTab_]->render();
  }

  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::PopStyleColor();
  ImGui::EndGroup();
}

void GUI::renderTopbar() {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));

  ImGui::BeginChild("Topbar", ImVec2(0, 60), false);
  ImGui::SetCursorPos(ImVec2(30, (60 - ImGui::GetTextLineHeight()) * 0.5f));

  ImGui::Text("%s", views_[currentTab_]->getName());

  ImGui::SameLine(ImGui::GetWindowWidth() - 250);
  ImGui::TextDisabled("rsjfw-reborn v1.1.0");

  ImGui::SameLine(ImGui::GetWindowWidth() - 100);
  ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s online",
                     ICON_FA_CIRCLE_CHECK);

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void GUI::renderLauncher() {
  float w = ImGui::GetWindowWidth();
  float h = ImGui::GetWindowHeight();
  float dt = ImGui::GetIO().DeltaTime;

  std::string s;
  float p;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    s = status_;
    p = progress_;
  }

  auto state = Orchestrator::instance().getState();
  std::string stateStr = "";
  switch (state) {
  case LauncherState::BOOTSTRAPPING:
    stateStr = "bootstrapping environment";
    break;
  case LauncherState::DOWNLOADING_ROBLOX:
    stateStr = "fetching roblox studio";
    break;
  case LauncherState::PREPARING_WINE: {
    auto rt = Config::instance().getGeneral().runnerType;
    if (rt == "UMU")
      stateStr = "preparing umu";
    else
      stateStr = "preparing " + rt;
    break;
  }
  case LauncherState::INSTALLING_DXVK:
    stateStr = "optimizing graphics";
    break;
  case LauncherState::APPLYING_CONFIG:
    stateStr = "applying settings";
    break;
  case LauncherState::LAUNCHING_STUDIO:
    stateStr = "launching studio";
    break;
  case LauncherState::ERROR:
    stateStr = "critical failure";
    break;
  default:
    stateStr = "ready";
    break;
  }

  static float smoothP = 0.0f;
  static LauncherState lastState = LauncherState::IDLE;
  static std::string lastStateStr = "";
  static float textAnim = 1.0f;

  if (state != lastState) {
    if (lastState == LauncherState::BOOTSTRAPPING)
      lastStateStr = "bootstrapping environment";
    else if (lastState == LauncherState::DOWNLOADING_ROBLOX)
      lastStateStr = "fetching roblox studio";
    else if (lastState == LauncherState::PREPARING_WINE)
      lastStateStr = "preparing compatibility layer";
    else if (lastState == LauncherState::INSTALLING_DXVK)
      lastStateStr = "optimizing graphics";
    else if (lastState == LauncherState::APPLYING_CONFIG)
      lastStateStr = "applying settings";
    else if (lastState == LauncherState::LAUNCHING_STUDIO)
      lastStateStr = "launching studio";
    else if (lastState == LauncherState::ERROR)
      lastStateStr = "critical failure";
    else
      lastStateStr = "ready";

    lastState = state;
    smoothP = 0.0f;
    textAnim = 0.0f;
  }

  textAnim = std::min(1.0f, textAnim + dt * 2.0f);
  smoothP += (p - smoothP) * dt * 4.0f;

  if (wideLogoTexture_) {
    float winAspect = w / h;
    float imgAspect = (float)wideLogoWidth_ / (float)wideLogoHeight_;
    float lw, lh;

    if (winAspect > imgAspect) {
      lw = w;
      lh = w / imgAspect;
    } else {
      lh = h;
      lw = h * imgAspect;
    }

    ImGui::SetCursorPos(ImVec2((w - lw) * 0.5f, (h - lh) * 0.5f));
    ImGui::Image((void *)(intptr_t)wideLogoTexture_, ImVec2(lw, lh));
  }

  float barH = 50.0f;
  ImDrawList *draw = ImGui::GetWindowDrawList();
  ImVec2 barPos = ImVec2(0, h - barH);

  draw->AddRectFilled(barPos, ImVec2(w, h), IM_COL32(25, 25, 25, 255));

  if (state == LauncherState::ERROR) {
    draw->AddRectFilled(barPos, ImVec2(w, h), IM_COL32(180, 10, 10, 255));
  } else if (smoothP > 0.001f) {
    draw->AddRectFilled(barPos, ImVec2(w * smoothP, h),
                        IM_COL32(180, 10, 40, 255));
  }

  ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

  float ease = 1.0f - std::pow(1.0f - textAnim, 3.0f);

  if (textAnim < 1.0f && !lastStateStr.empty() && lastStateStr != stateStr) {
    ImU32 prevColor = IM_COL32(200, 200, 200, (int)((1.0f - ease) * 255));
    ImVec2 prevStatePos = ImVec2(20 + ease * 100.0f, h - barH + 5);
    draw->AddText(prevStatePos, prevColor, lastStateStr.c_str());
  }

  ImU32 curColor = (state == LauncherState::ERROR)
                       ? IM_COL32(255, 200, 200, (int)(ease * 255))
                       : IM_COL32(255, 255, 255, (int)(ease * 255));
  ImVec2 curStatePos = ImVec2(-50.0f + ease * 70.0f, h - barH + 5);
  draw->AddText(curStatePos, curColor, stateStr.c_str());

  ImVec2 textPos = ImVec2(20, h - 25);
  draw->AddText(textPos,
                (state == LauncherState::ERROR) ? IM_COL32(255, 200, 200, 255)
                                                : IM_COL32(255, 255, 255, 255),
                s.c_str());

  if (state != LauncherState::ERROR) {
    std::string pct = std::to_string((int)(smoothP * 100)) + "%";
    ImVec2 pctSize = ImGui::CalcTextSize(pct.c_str());
    ImVec2 pctPos =
        ImVec2(w - pctSize.x - 20,
               h - barH + (barH - ImGui::GetTextLineHeight()) * 0.5f);
    draw->AddText(pctPos, IM_COL32(255, 255, 255, 255), pct.c_str());
  } else {
    std::string help = "check rsjfw.log";
    ImVec2 helpSize = ImGui::CalcTextSize(help.c_str());
    draw->AddText(ImVec2(w - helpSize.x - 20, h - barH + 5),
                  IM_COL32(255, 255, 255, 255), help.c_str());
  }

  ImGui::PopFont();
}

} // namespace rsjfw
