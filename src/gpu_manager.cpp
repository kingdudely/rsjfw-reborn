#include "gpu_manager.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vulkan/vulkan.h>

namespace rsjfw {

GpuManager &GpuManager::instance() {
  static GpuManager instance;
  return instance;
}

std::vector<GpuInfo> GpuManager::discoverDevices() {
  std::vector<GpuInfo> gpus;

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "RSJFW GPU Discovery";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  VkInstance instance;
  if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
    std::cerr << "Failed to create Vulkan instance for GPU discovery"
              << std::endl;
    return gpus;
  }

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  if (deviceCount > 0) {
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto &device : devices) {
      VkPhysicalDeviceProperties properties;
      vkGetPhysicalDeviceProperties(device, &properties);

      GpuInfo info;
      info.name = properties.deviceName;
      info.vendorId = properties.vendorID;
      info.deviceId = properties.deviceID;
      info.driverVersion = properties.driverVersion;
      info.deviceType = properties.deviceType;
      info.pciBus = 0;
      info.pciSlot = 0;
      info.pciFunction = 0;

      gpus.push_back(info);
    }
  }

  vkDestroyInstance(instance, nullptr);

  // Sort logic: Discrete > Integrated > Virtual > CPU > Other
  std::sort(gpus.begin(), gpus.end(), [](const GpuInfo &a, const GpuInfo &b) {
    auto score = [](const GpuInfo &g) {
      if (g.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        return 4;
      if (g.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        return 3;
      if (g.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
        return 2;
      if (g.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        return 1;
      return 0;
    };
    return score(a) > score(b);
  });

  return gpus;
}

GpuInfo GpuManager::getBestDevice() {
  auto devices = discoverDevices();
  if (devices.empty())
    return {};
  return devices[0];
}

std::map<std::string, std::string> GpuManager::getEnvVars(const GpuInfo &gpu) {
  std::map<std::string, std::string> env;

  if (gpu.vendorId == 0x10de) { // NVIDIA
    env["__NV_PRIME_RENDER_OFFLOAD"] = "1";
    env["__GLX_VENDOR_LIBRARY_NAME"] = "nvidia";
  } else if (gpu.vendorId == 0x1002) { // AMD
    int cardIndex = -1;
    namespace fs = std::filesystem;

    try {
      if (fs::exists("/sys/class/drm")) {
        for (const auto &entry : fs::directory_iterator("/sys/class/drm")) {
          std::string filename = entry.path().filename().string();
          if (filename.find("card") == 0 &&
              filename.find("-") == std::string::npos &&
              filename.length() > 4 && std::isdigit(filename[4])) {

            std::ifstream vendorFile(entry.path() / "device/vendor");
            std::ifstream deviceFile(entry.path() / "device/device");
            std::string vStr, dStr;
            if (vendorFile >> vStr && deviceFile >> dStr) {
              uint32_t v = std::stoul(vStr, nullptr, 0);
              uint32_t d = std::stoul(dStr, nullptr, 0);

              if (v == gpu.vendorId && d == gpu.deviceId) {
                std::string num = filename.substr(4);
                cardIndex = std::stoi(num);
                break;
              }
            }
          }
        }
      }
    } catch (...) {
    }

    if (cardIndex != -1) {
      env["DRI_PRIME"] = std::to_string(cardIndex);
    }
  }

  return env;
}

} // namespace rsjfw
