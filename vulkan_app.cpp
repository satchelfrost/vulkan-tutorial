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
  createRenderPass();
  createGraphicsPipelineLayout();
  createGraphicsPipeline();
  createFrameBuffers();
  createCommandPool();
  createCommandBuffer();
  createSyncObjects();
}

void VulkanApp::cleanup() {
  vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
  vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
  vkDestroyFence(device_, inFlightFence_, nullptr);
  vkDestroyCommandPool(device_, commandPool_, nullptr);
  for (auto framebuffer : swapChainFramebuffers_)
    vkDestroyFramebuffer(device_, framebuffer, nullptr);
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
  vkDestroyRenderPass(device_, renderPass_, nullptr);
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
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    drawFrame();
  }

  vkDeviceWaitIdle(device_);
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

void VulkanApp::createGraphicsPipeline() {
  // Create shader modules
  auto vertShaderCode = readFile("shaders/vert.spv");
  auto fragShaderCode = readFile("shaders/frag.spv");
  std::cout << "vert shader byte code size: " << vertShaderCode.size() << "\n";
  std::cout << "frag shader byte code size: " << fragShaderCode.size() << "\n";
  VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, device_);
  VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, device_);
  VkPipelineShaderStageCreateInfo vertShaderCI{};
  vertShaderCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderCI.module = vertShaderModule;
  vertShaderCI.pName = "main";
  VkPipelineShaderStageCreateInfo fragShaderCI{};
  fragShaderCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderCI.module = fragShaderModule;
  fragShaderCI.pName = "main";
  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderCI, fragShaderCI};

  // Dynamic states
  std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  // Vertex Input
  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;

  // Input Assembly
  VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // Viewport State
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y= 0.0f;
  viewport.width = (float)swapChainExtent_.width;
  viewport.height = (float)swapChainExtent_.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapChainExtent_;
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  // Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp = 0.0f;
  rasterizer.depthBiasSlopeFactor = 0.0f;

  // Multisampling
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f;
  multisampling.pSampleMask = nullptr;
  multisampling.alphaToCoverageEnable = VK_FALSE;
  multisampling.alphaToOneEnable = VK_FALSE;

  // Colorblending
  VkPipelineColorBlendAttachmentState colorBlendAttachment{};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_FALSE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  // Create the pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = nullptr;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout_;
  pipelineInfo.renderPass = renderPass_;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
  pipelineInfo.basePipelineIndex = -1;

  VkResult result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);
  successCheck(result, "Failed to create graphics pipeline");

  // Destroy shader modules
  vkDestroyShaderModule(device_, vertShaderModule, nullptr);
  vkDestroyShaderModule(device_, fragShaderModule, nullptr);
}

void VulkanApp::createGraphicsPipelineLayout() {
  VkPipelineLayoutCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  createInfo.setLayoutCount = 0;
  createInfo.pSetLayouts = nullptr;
  createInfo.pushConstantRangeCount = 0;
  createInfo.pPushConstantRanges = nullptr;

  VkResult result = vkCreatePipelineLayout(device_, &createInfo, nullptr, &pipelineLayout_);
  successCheck(result, "Failed to create pipeline layout");
}

void VulkanApp::createRenderPass() {
  VkAttachmentDescription colorAttachment{};
  colorAttachment.format = swapChainImageFormat_;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef{};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo{};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VkResult result = vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_);
  successCheck(result, "Failed to create render pass");
}

void VulkanApp::createFrameBuffers() {
  swapChainFramebuffers_.resize(swapChainImageViews_.size());
  for (size_t i = 0; i < swapChainImageViews_.size(); i++) {
    VkImageView attachments[] = {swapChainImageViews_[i]};

    VkFramebufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    createInfo.renderPass = renderPass_;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = attachments;
    createInfo.width = swapChainExtent_.width;
    createInfo.height = swapChainExtent_.height;
    createInfo.layers = 1;

    VkResult result = vkCreateFramebuffer(device_, &createInfo, nullptr, &swapChainFramebuffers_[i]);
    successCheck(result, "Failed to create framebuffer");
  }
}

void VulkanApp::createCommandPool() {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice_, surface_);
  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

  VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
  successCheck(result, "Failed to create command pool");
}

void VulkanApp::createCommandBuffer() {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool_;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkResult result = vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_);
  successCheck(result, "Failed to allocate command buffers");
}

void VulkanApp::drawFrame() {
  vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
  vkResetFences(device_, 1, &inFlightFence_);

  uint32_t imageIndex;
  vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);

  vkResetCommandBuffer(commandBuffer_, 0);
  VkFramebuffer framebuffer = swapChainFramebuffers_[imageIndex];
  recordCommandBuffer(commandBuffer_, framebuffer, swapChainExtent_, renderPass_, pipeline_); 

  VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &imageAvailableSemaphore_; // TEST
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submitInfo.pWaitDstStageMask = &waitStage; // TEST
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer_;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &renderFinishedSemaphore_; // TEST

  VkResult result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_);
  successCheck(result, "Failed to submit draw command buffer");

  VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &renderFinishedSemaphore_; // TEST
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapChain_; // TEST
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pResults = nullptr;

  vkQueuePresentKHR(presentQueue_, &presentInfo);
}

void VulkanApp::createSyncObjects() {
  VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_);
  successCheck(result, "Failed to create imave available semaphore");
  result = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_);
  successCheck(result, "Failed to create render finsihed semaphore");

  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

  // Set this flag so that the fence will not block on the first call to drawFrame()
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  result = vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_);
  successCheck(result, "Failed to create in flight fence");
}