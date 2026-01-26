#define VKROOTS_IMPLEMENTATION
#include "vkroots.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>

namespace RSJFW {

namespace {
std::mutex g_logMutex;

void Log(const char *fmt, ...) {
  const char *enableLogging = std::getenv("RSJFW_LAYER_LOGGING");
  if (!enableLogging || std::strcmp(enableLogging, "1") != 0)
    return;

  std::lock_guard<std::mutex> lock(g_logMutex);
  va_list args;
  va_start(args, fmt);

  fprintf(stdout, "[RSJFW Layer] ");
  vfprintf(stdout, fmt, args);
  fprintf(stdout, "\n");
  fflush(stdout);

  va_end(args);
}

std::optional<VkPresentModeKHR> GetDesiredPresentMode() {
  const char *envPresentMode = std::getenv("RSJFW_LAYER_PRESENT_MODE");
  if (envPresentMode) {
    if (std::strcmp(envPresentMode, "MAILBOX") == 0)
      return VK_PRESENT_MODE_MAILBOX_KHR;
    if (std::strcmp(envPresentMode, "FIFO") == 0)
      return VK_PRESENT_MODE_FIFO_KHR;
    if (std::strcmp(envPresentMode, "IMMEDIATE") == 0)
      return VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (std::strcmp(envPresentMode, "FIFO_RELAXED") == 0)
      return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  }
  return std::nullopt;
}

const char *GetErrorStrategy() {
  static const char *strategy = []() {
    const char *envStrategy = std::getenv("RSJFW_LAYER_ERROR_STRATEGY");
    if (envStrategy && std::strcmp(envStrategy, "SEAMLESS") == 0) {
      return "SEAMLESS";
    }
    return "SAFE";
  }();
  return strategy;
}
} // namespace

class VkDeviceOverrides {
public:
  static VkResult
  CreateSwapchainKHR(const vkroots::VkDeviceDispatch &dispatch, VkDevice device,
                     const VkSwapchainCreateInfoKHR *pCreateInfo,
                     const VkAllocationCallbacks *pAllocator,
                     VkSwapchainKHR *pSwapchain) {
    if (!pCreateInfo) {
      return dispatch.CreateSwapchainKHR(device, pCreateInfo, pAllocator,
                                         pSwapchain);
    }

    VkSwapchainCreateInfoKHR modifiedInfo = *pCreateInfo;

    Log("Creating Swapchain: %ux%u, minImageCount: %u, presentMode: %u",
        pCreateInfo->imageExtent.width, pCreateInfo->imageExtent.height,
        pCreateInfo->minImageCount, pCreateInfo->presentMode);

    // this somewhat improves the stability of flickering
    const char *forceTripleEnv =
        std::getenv("RSJFW_LAYER_FORCE_TRIPLE_BUFFERING");
    bool shouldForceTriple =
        !forceTripleEnv || std::strcmp(forceTripleEnv, "0") != 0;

    if (shouldForceTriple && pCreateInfo->imageExtent.width > 256 &&
        pCreateInfo->imageExtent.height > 256) {
      if (modifiedInfo.minImageCount < 3u) {
        Log("  Forcing minImageCount to 3 (was %u)",
            pCreateInfo->minImageCount);
        modifiedInfo.minImageCount = 3u;
      }
    }

    // fifo or mailbox (fifo better)
    auto desiredMode = GetDesiredPresentMode();
    if (desiredMode) {
      Log("  Overriding presentMode to %u", (uint32_t)*desiredMode);
      modifiedInfo.presentMode = *desiredMode;
    }

    // better capture with obs
    modifiedInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    return dispatch.CreateSwapchainKHR(device, &modifiedInfo, pAllocator,
                                       pSwapchain);
  }

  static void DestroySwapchainKHR(const vkroots::VkDeviceDispatch &dispatch,
                                  VkDevice device, VkSwapchainKHR swapchain,
                                  const VkAllocationCallbacks *pAllocator) {
    dispatch.DestroySwapchainKHR(device, swapchain, pAllocator);
  }

  static VkResult QueuePresentKHR(const vkroots::VkQueueDispatch &dispatch,
                                  VkQueue queue,
                                  const VkPresentInfoKHR *pPresentInfo) {
    VkResult result = dispatch.QueuePresentKHR(queue, pPresentInfo);

    // look ik this is eerily familiar but man its the only fucking way for
    // roblox to recreate its swapchain and no way im forcing a manual swapchain
    // recreation
    if (result == VK_ERROR_OUT_OF_DATE_KHR &&
        std::strcmp(GetErrorStrategy(), "SAFE") == 0) {
      return VK_ERROR_SURFACE_LOST_KHR;
    }

    return result;
  }

  static VkResult AcquireNextImageKHR(const vkroots::VkDeviceDispatch &dispatch,
                                      VkDevice device, VkSwapchainKHR swapchain,
                                      uint64_t timeout, VkSemaphore semaphore,
                                      VkFence fence, uint32_t *pImageIndex) {
    VkResult result = dispatch.AcquireNextImageKHR(
        device, swapchain, timeout, semaphore, fence, pImageIndex);

    // surface lost so roblox does not kill itself
    if (result == VK_ERROR_OUT_OF_DATE_KHR &&
        std::strcmp(GetErrorStrategy(), "SAFE") == 0) {
      return VK_ERROR_SURFACE_LOST_KHR;
    }

    return result;
  }
};

} // namespace RSJFW

VKROOTS_DEFINE_LAYER_INTERFACES(vkroots::NoOverrides, RSJFW::VkDeviceOverrides);
