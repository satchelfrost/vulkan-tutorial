#include "vulkan_app.h"
#include <set>

void VulkanApp::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanup();
}

void VulkanApp::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  window_ = glfwCreateWindow(width, height, "Vulkan", nullptr, nullptr);
}

void VulkanApp::initVulkan() {
  createVulkanInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
  retrieveQueues();
  createSwapChain();
  retrieveSwapChainImages();
  createImageViews();
}

void VulkanApp::cleanup() {
  for (auto imageView : swapChainImageViews_)
    vkDestroyImageView(device_, imageView, nullptr);
  vkDestroySwapchainKHR(device_, swapChain_, nullptr);
  vkDestroyDevice(device_, nullptr);
  if (enableValidationLayers_) {
    DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
    EndOfMsg("Validation Log");
  }
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkDestroyInstance(instance_, nullptr);
  glfwDestroyWindow(window_);
  glfwTerminate();
}

void VulkanApp::createVulkanInstance() {
  // Check for validation layers requested
  if (enableValidationLayers_ && !checkValidationLayerSupport()) {
    throw std::runtime_error("Validation layers requested, but not available");
  }

  // Optional application information
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Hello Triangle";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.pEngineName = "RevoVR";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // Non-optional information for extensions and validation layers
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  // Get the necessary extensions for glfw
  auto requiredExtensions = getRequiredExtensions();
  
  // Continue populating createInfo
  createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
  createInfo.ppEnabledExtensionNames = requiredExtensions.data();
  logRequiredExtensions(requiredExtensions);
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  if (enableValidationLayers_) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(requestedValidationLayers_.size());
    createInfo.ppEnabledLayerNames = requestedValidationLayers_.data();

    populateDebugMessengerCreateInfo(debugCreateInfo, true);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  // Enumerate optional extensions
  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
  logAvailableInstanceExtensions(extensions, requiredExtensions);

  if (enableValidationLayers_)
    BeginningOfMsg("Instance Validation Log");

  VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
  successCheck(result, "Failed to create instance");

  if (enableValidationLayers_)
    EndOfMsg("Instance Validation Log");
}

bool VulkanApp::checkValidationLayerSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  std::cout << "Checking for validation layers\n";
  std::cout << "------------------------------\n";
  for (const char* layerName : requestedValidationLayers_) {
    bool layerFound = false;
    for (const auto& layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        std::cout << "Requested validation layer " << layerName << " was found\n\n";
        break;
      }
    }

    if (!layerFound) {
      std::cout << "Requested validation layer " << layerName << " was not found\n\n";
      return false;
    }
  }
  return true;
}

std::vector<const char*> VulkanApp::getRequiredExtensions() {
  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  std::vector<const char*> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

  if (enableValidationLayers_)
    requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  
  return requiredExtensions;
}


void VulkanApp::setupDebugMessenger() {
  if (!enableValidationLayers_)
    return;

  BeginningOfMsg("Validation Log");
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  populateDebugMessengerCreateInfo(createInfo, false);

  successCheck(CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_),
               "Failed to set up debug messenger");
}

void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_))
    glfwPollEvents();
}

void VulkanApp::pickPhysicalDevice() {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
  if (deviceCount == 0)
    throw std::runtime_error("Failed to find GPUs with Vulkan support");
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());
  for (const auto& device : devices) {
    if (isDeviceSuitable(device, surface_, deviceExts_)) {
      physicalDevice_ = device;
      break;
    }
  }
  if (physicalDevice_ == VK_NULL_HANDLE)
    throw std::runtime_error("Failed to find suitable GPU");
}

void VulkanApp::createLogicalDevice() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice_, surface_);
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  // Create the queues
  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  // Create the device
  VkPhysicalDeviceFeatures deviceFeatures{};
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pEnabledFeatures = &deviceFeatures;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExts_.size());
  createInfo.ppEnabledExtensionNames = deviceExts_.data();

  VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
  successCheck(result, "Failed to create logical device");
}

void VulkanApp::createSurface() {
  VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
  successCheck(result, "Failed to create window surface");
}

void VulkanApp::createSwapChain() {
  SwapChainSupportDetails details = querySwapChainSupport(physicalDevice_, surface_);
  VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(details.formats);
  VkPresentModeKHR presentMode = chooseSwapPresentMode(details.presentModes);
  swapChainExtent_ = chooseSwapExtent(details.capabilities, window_);
  swapChainImageFormat_ = surfaceFormat.format;

  // Recommended to request at least one more image than minimum in case of waiting to acquire another image
  uint32_t imageCount = details.capabilities.minImageCount + 1;

  if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
    imageCount = details.capabilities.maxImageCount;

  // Create the swap chain
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface_;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageExtent = swapChainExtent_;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  QueueFamilyIndices indices = findQueueFamilies(physicalDevice_, surface_);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = details.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  VkResult result = vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_);
  successCheck(result, "Failed to create swap chain");
}

void VulkanApp::retrieveQueues() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice_, surface_);
  vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

void VulkanApp::retrieveSwapChainImages() {
  uint32_t imageCount = 0;
  vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
  swapChainImages_.resize(imageCount);
  vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());
}

void VulkanApp::createImageViews() {
  swapChainImageViews_.resize(swapChainImages_.size());
  for (size_t i = 0; i < swapChainImages_.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = swapChainImages_[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapChainImageFormat_;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]);
    successCheck(result, "Failed to create image views");
  }
}
