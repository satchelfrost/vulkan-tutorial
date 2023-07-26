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
  window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}

void VulkanApp::initVulkan() {
  createVulkanInstance();
  setupDebugMessenger();
  createSurface();
  pickPhysicalDevice();
  createLogicalDevice();
}

void VulkanApp::cleanup() {
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
  successCheck(result, "Failed to create instance!");

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

  // Get queues here?
  vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
  vkGetDeviceQueue(device_, indices.presentFamily.value(), 0, &presentQueue_);
}

void VulkanApp::createSurface() {
  VkResult result = glfwCreateWindowSurface(instance_, window_, nullptr, &surface_);
  successCheck(result, "Failed to create window surface");
}