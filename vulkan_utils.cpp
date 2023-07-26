#include "vulkan_utils.h"
#include <stdexcept>
#include <iostream>
#include <set>

void successCheck(VkResult result, const char* onFailureMsg) {
  if (result != VK_SUCCESS)
    throw std::runtime_error(onFailureMsg);
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugUtilsMessengerEXT* pDebugMessenger) { 
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func)
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  else
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger,
                                       const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr)
    func(instance, messenger, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData) {
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
  }
  return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL createInstancedebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData) {
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::cerr << "Instance validation layer: " << pCallbackData->pMessage << std::endl;
  }
  return VK_FALSE;
}

void logRequiredExtensions(const std::vector<const char*> requiredExtensions) {
  std::cout << "Extensions Required:\n";
  std::cout << "--------------------\n";
  for (size_t i = 0; i < requiredExtensions.size(); i++)
    std::cout  << i + 1 << ") " << requiredExtensions[i] << '\n';
  std::cout << "\n";
}

void logAvailableInstanceExtensions(const std::vector<VkExtensionProperties>& extensions,
                                    const std::vector<const char*> requiredExtensions) {
  std::cout << "Available Extensions:\n";
  std::cout << "--------------------\n";
  for (size_t i = 0; i < extensions.size(); i++) {
    std::string availableAndRequired = "";
    for (auto req : requiredExtensions)
      if (req == std::string(extensions[i].extensionName))
        availableAndRequired = " <--- Required"; 
    std::cout << i + 1 << ") " << extensions[i].extensionName << availableAndRequired << '\n';
  }
  std::cout << "\n";
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, bool createInstance) {
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  if (createInstance)
    createInfo.pfnUserCallback = createInstancedebugCallback;
  else
    createInfo.pfnUserCallback = debugCallback;
  createInfo.pUserData = nullptr;
}

bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& requiredExts) {
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  /* We could go into detail in terms of different devices and not choose a device until we are satisfied.
     For example, the tutorial mentions that we can rate devices and store them in a multimap (score -> device),
     so that we can choose the one with the highest score at the end.
     For now I'm just going to choose the first device with some basic properties and features. */
  bool basic = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;

  basic &= checkDeviceExtensionSupport(device, requiredExts);

  return basic && findQueueFamilies(device, surface).isComplete();
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices indices;
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  // Here we want the queue family to support graphics and the ability to present to a surface
  int i = 0;
  for (const auto& queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indices.graphicsFamily = i;

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport)
      indices.presentFamily = i;


    if (indices.isComplete())
      break;

    i++;
  }
  return indices;
}

void BeginningOfMsg(std::string of) {
  std::cout << "******BEGIN " << of << "******\n";
}

void EndOfMsg(std::string of) {
  std::cout << "******END " << of << "******\n";
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& requiredExts) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
  std::set<std::string> requiredExtensions(requiredExts.begin(), requiredExts.end());
  for (const auto& extension : availableExtensions)
    requiredExtensions.erase(extension.extensionName);
  return requiredExtensions.empty();
}