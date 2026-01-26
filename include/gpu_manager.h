#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <vulkan/vulkan.h>

namespace rsjfw {

struct GpuInfo {
    std::string name;
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t driverVersion;
    VkPhysicalDeviceType deviceType;
    uint32_t pciBus;
    uint32_t pciSlot;
    uint32_t pciFunction;
};

class GpuManager {
public:
    static GpuManager& instance();

    GpuManager(const GpuManager&) = delete;
    GpuManager& operator=(const GpuManager&) = delete;

    std::vector<GpuInfo> discoverDevices();
    GpuInfo getBestDevice();
    std::map<std::string, std::string> getEnvVars(const GpuInfo& gpu);

private:
    GpuManager() = default;
};

} // namespace rsjfw
