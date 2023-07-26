#pragma once

#include "vulkan_utils.h"
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <optional>

class VulkanApp {
private:
  GLFWwindow* window_;
  VkInstance instance_;
  VkDebugUtilsMessengerEXT debugMessenger_;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;
  const std::vector<const char*> requestedValidationLayers_ = {"VK_LAYER_KHRONOS_validation"};
  const std::vector<const char*> deviceExts_ = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  const bool enableValidationLayers_ = true;
  VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
  VkDevice device_;
  VkQueue graphicsQueue_;
  VkQueue presentQueue_;
  VkSurfaceKHR surface_;

public:
  void run();

private:
  void initWindow();
  void initVulkan();
  void cleanup();
  void createVulkanInstance();
  bool checkValidationLayerSupport();
  std::vector<const char*> getRequiredExtensions();
  void setupDebugMessenger();
  void mainLoop();
  void pickPhysicalDevice();
  void createLogicalDevice();
  void createSurface();
};