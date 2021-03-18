
#include "pch.h"
#include "render_backend.h"

#include "skeleton/core/debug_tools.h"
#include "skeleton/core/time.h"

SklVulkanContext_t vulkanContext;

void SklVulkanContext_t::Cleanup()
{
  SKL_PRINT("Vulkan Context", "Cleanup =================================================");
  for (uint32_t i = 0; i < shaderPrograms.size(); i++)
  {
    vkDestroyPipeline(device, shaderPrograms[i].pipeline, nullptr);
    vkDestroyPipelineLayout(device, shaderPrograms[i].pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, shaderPrograms[i].descriptorSetLayout, nullptr);
  }

  ImageManager::Cleanup();
  //BufferManager::Cleanup();

  vulkanContext.renderables.clear();
  vulkanContext.gpu.queueFamilyProperties.clear();
  vulkanContext.gpu.extensionProperties.clear();
  vulkanContext.gpu.presentModes.clear();
  vulkanContext.gpu.surfaceFormats.clear();

  vkDestroyRenderPass(device, renderPass, nullptr);
  vkDestroyDevice(device, nullptr);
}

//=================================================
// Renderer Backend
//=================================================

SklRenderBackend::SklRenderBackend(SDL_Window* _window,
                                       const std::vector<const char*>& _extraExtensions)
                                       : window(_window)
{
  // Include the extra extensions specified
  for (const char* additionalExtension : _extraExtensions)
  {
    instanceExtensions.push_back(additionalExtension);
  }
  CreateInstance();
  CreateDevice();
  CreateCommandPool();
  bufferManager = new BufferManager();
}

SklRenderBackend::~SklRenderBackend()
{
  vkDeviceWaitIdle(vulkanContext.device);

  vkDestroyDescriptorPool(vulkanContext.device, descriptorPool, nullptr);

  CleanupRenderComponents();

  vkDestroyCommandPool(vulkanContext.device, graphicsCommandPool, nullptr);

  vulkanContext.Cleanup();
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
}

void SklRenderBackend::CreateInstance()
{
  // Basic application metadata
  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.apiVersion = VK_API_VERSION_1_2;
  appInfo.pEngineName = "Skeleton Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.pApplicationName = "Test Skeleton Application";
  appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);

  // Create the instance
  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
  createInfo.ppEnabledExtensionNames = instanceExtensions.data();
  createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayer.size());
  createInfo.ppEnabledLayerNames = validationLayer.data();

  SKL_ASSERT_VK(
      vkCreateInstance(&createInfo, nullptr, &instance),
      "Failed to create vkInstance");

  // Create the surface
  SDL_Vulkan_CreateSurface(window, instance, &surface);
}

void SklRenderBackend::CreateDevice()
{
  // Select a physical device
  uint32_t queueIndices[3];
  VkPhysicalDevice physicalDevice;
  ChoosePhysicalDevice(physicalDevice, queueIndices[0], queueIndices[1], queueIndices[2]);

  SKL_PRINT(SKL_DEBUG, "Queue indices :--: Graphics: %u :--: Present: %u :--: Transfer: %u",
            queueIndices[0], queueIndices[1], queueIndices[2]);

  vulkanContext.gpu.device = physicalDevice;

  vulkanContext.graphicsIdx = queueIndices[0];
  vulkanContext.presentIdx = queueIndices[1];
  vulkanContext.transferIdx = queueIndices[2];

  CreateLogicalDevice({ queueIndices[0], queueIndices[1], queueIndices[2] }, deviceExtensions,
                      validationLayer);

  // Fill GPU details
  //=================================================
  VkPhysicalDevice& pysDevice = vulkanContext.gpu.device;
  SklPhysicalDeviceInfo_t& pdInfo = vulkanContext.gpu;

  uint32_t propertyCount;
  vkGetPhysicalDeviceQueueFamilyProperties(pysDevice, &propertyCount, nullptr);
  pdInfo.queueFamilyProperties.resize(propertyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(pysDevice, &propertyCount,
                                           pdInfo.queueFamilyProperties.data());

  vkGetPhysicalDeviceProperties(pysDevice, &pdInfo.properties);

  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(pysDevice, nullptr, &extensionCount, nullptr);
  pdInfo.extensionProperties.resize(extensionCount);
  vkEnumerateDeviceExtensionProperties(pysDevice, nullptr, &extensionCount,
                                       pdInfo.extensionProperties.data());

  vkGetPhysicalDeviceFeatures(pysDevice, &pdInfo.features);
  vkGetPhysicalDeviceMemoryProperties(pysDevice, &pdInfo.memProperties);

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(pysDevice, surface, &presentModeCount, nullptr);
  pdInfo.presentModes.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(pysDevice, surface, &presentModeCount,
                                            pdInfo.presentModes.data());

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pysDevice, surface, &pdInfo.surfaceCapabilities);

  uint32_t surfaceFormatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(pysDevice, surface, &surfaceFormatCount, nullptr);
  pdInfo.surfaceFormats.resize(surfaceFormatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(pysDevice, surface, &surfaceFormatCount,
                                       pdInfo.surfaceFormats.data());
}

void SklRenderBackend::ChoosePhysicalDevice(VkPhysicalDevice& _selectedDevice,
                                              uint32_t& _graphicsIndex, uint32_t& _presentIndex,
                                              uint32_t& _transferIndex)
{
  // Get all available physical devices
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  std::vector<VkPhysicalDevice> physDevices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, physDevices.data());

  uint32_t propertyCount;

  struct BestGPU
  {
    VkPhysicalDevice device;
    uint32_t graphicsIndex;
    uint32_t presentIndex;
    uint32_t transferIndex;
  } bestFit;

  for (const auto& pdevice : physDevices)
  {
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, queueProperties.data());

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pdevice, &props);

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> physicalDeviceExt(extensionCount);
    vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount,
                                         physicalDeviceExt.data());

    std::set<std::string> requiredExtensionSet(deviceExtensions.begin(), deviceExtensions.end());

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(pdevice, &features);

    for (const auto& ext : physicalDeviceExt)
    {
      requiredExtensionSet.erase(ext.extensionName);
    }

    _graphicsIndex = GetQueueIndex(queueProperties, VK_QUEUE_GRAPHICS_BIT);
    _transferIndex = GetQueueIndex(queueProperties, VK_QUEUE_TRANSFER_BIT);
    _presentIndex = GetPresentIndex(&pdevice, propertyCount, _graphicsIndex);

    if (
      features.samplerAnisotropy &&
      requiredExtensionSet.empty() &&
      _graphicsIndex != -1 &&
      _presentIndex != -1 &&
      _transferIndex != -1)
    {
      if (_graphicsIndex == _presentIndex || _graphicsIndex == _transferIndex
          || _presentIndex == _transferIndex)
      {
        bestFit.device = pdevice;
        bestFit.graphicsIndex = _graphicsIndex;
        bestFit.presentIndex = _presentIndex;
        bestFit.transferIndex = _transferIndex;
      }
      else
      {
        _selectedDevice = pdevice;
        return;
      }
    }
  }

  if (bestFit.device != VK_NULL_HANDLE)
  {
    _selectedDevice = bestFit.device;
    _graphicsIndex = bestFit.graphicsIndex;
    _presentIndex = bestFit.presentIndex;
    _transferIndex = bestFit.transferIndex;
    return;
  }

  SKL_ASSERT_VK(VK_ERROR_UNKNOWN, "Failed to find a suitable physical device");
}

void SklRenderBackend::CreateCommandPool()
{
  SklCreateCommandPool(graphicsCommandPool, vulkanContext.graphicsIdx);
}

//=================================================
// Renderer
//=================================================

void SklRenderBackend::InitializeRenderComponents()
{
  CreateRenderComponents();

  CreateDescriptorPool();
  CreateSyncObjects();
  CreateCommandBuffers();
}

void SklRenderBackend::CreateRenderComponents()
{
  CreateSwapchain();
  CreateRenderpass();
  CreateDepthImage();
  CreateFramebuffers();
}

void SklRenderBackend::CleanupRenderComponents()
{
  // Destroy Sync Objects
  for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
  {
    vkDestroyFence(vulkanContext.device, flightFences[i], nullptr);
    vkDestroySemaphore(vulkanContext.device, imageAvailableSemaphores[i], nullptr);
    vkDestroySemaphore(vulkanContext.device, renderCompleteSemaphores[i], nullptr);
  }

  // Destroy Framebuffers
  for (uint32_t i = 0; i < (uint32_t)frameBuffers.size(); i++)
  {
    vkDestroyFramebuffer(vulkanContext.device, frameBuffers[i], nullptr);
  }

  // Destroy Depth Image
  //vkDestroyImage(vulkanContext.device, depthImage, nullptr);
  //vkFreeMemory(vulkanContext.device, depthMemory, nullptr);
  //vkDestroyImageView(vulkanContext.device, depthImageView, nullptr);

  // Destroy Renderpass
  //vkDestroyRenderPass(vulkanContext.device, Renderpass)

  // Destroy Swapchain
  for (const auto& view : swapchainImageViews)
  {
    vkDestroyImageView(vulkanContext.device, view, nullptr);
  }
  vkDestroySwapchainKHR(vulkanContext.device, swapchain, nullptr);
}

void SklRenderBackend::RecreateRenderComponents()
{
  vkDeviceWaitIdle(vulkanContext.device);

  CleanupRenderComponents();
  CreateRenderComponents();
}

//=================================================
// Renderer Create Functions
//=================================================

void SklRenderBackend::CreateSwapchain()
{
  // Find the best surface format
  VkSurfaceFormatKHR formatInfo = vulkanContext.gpu.surfaceFormats[0];
  for (const auto& p : vulkanContext.gpu.surfaceFormats)
  {
    if (p.format == VK_FORMAT_R32G32B32A32_SFLOAT
        && p.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT)
    {
      formatInfo = p;
      break;
    }
  }

  // Find the best present mode
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (const auto& p : vulkanContext.gpu.presentModes)
  {
    if (p == VK_PRESENT_MODE_MAILBOX_KHR)
    {
      presentMode = p;
      break;
    }
  }

  // Get the device's extent
  VkSurfaceCapabilitiesKHR& capabilities = vulkanContext.gpu.surfaceCapabilities;
  VkExtent2D extent;
  if (capabilities.currentExtent.width != UINT32_MAX)
  {
    extent = capabilities.currentExtent;
  }
  else
  {
    uint32_t width, height;
    SDL_GetWindowSize(window, reinterpret_cast<int*>(&width),
                      reinterpret_cast<int*>(&height));
    extent.width = glm::clamp(width, capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = glm::clamp(height, capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);
  }

  // Choose an image count
  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
  {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.clipped = VK_TRUE;
  createInfo.imageArrayLayers = 1;
  createInfo.imageFormat = formatInfo.format;
  createInfo.imageColorSpace = formatInfo.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.minImageCount = imageCount;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (vulkanContext.graphicsIdx != vulkanContext.presentIdx)
  {
    uint32_t sharedIndices[] = { vulkanContext.graphicsIdx, vulkanContext.presentIdx };
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = sharedIndices;
  }
  else
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  createInfo.presentMode = presentMode;

  SKL_ASSERT_VK(
      vkCreateSwapchainKHR(vulkanContext.device, &createInfo, nullptr, &swapchain),
      "Failed to create swapchain");

  swapchainFormat = formatInfo.format;
  vulkanContext.renderExtent = extent;

  vkGetSwapchainImagesKHR(vulkanContext.device, swapchain, &imageCount, nullptr);
  swapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(vulkanContext.device, swapchain, &imageCount, swapchainImages.data());

  swapchainImageViews.resize(imageCount);
  for (uint32_t i = 0; i < imageCount; i++)
  {
    swapchainImageViews[i] = CreateImageView(swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT,
                                             swapchainImages[i]);
  }
}

void SklRenderBackend::CreateRenderpass()
{
  VkAttachmentDescription colorDesc = {};
  colorDesc.format = swapchainFormat;
  colorDesc.samples = VK_SAMPLE_COUNT_1_BIT;
  colorDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  colorDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

  VkAttachmentReference colorRef;
  colorRef.attachment = 0;
  colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthDesc = {};
  depthDesc.format = FindDepthFormat();
  depthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
  depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depthRef = {};
  depthRef.attachment = 1;
  depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {};
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorRef;
  subpass.pDepthStencilAttachment = &depthRef;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

  VkSubpassDependency dependency = {};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[] = { colorDesc, depthDesc };
  VkRenderPassCreateInfo creteInfo = {};
  creteInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  creteInfo.attachmentCount = 2;
  creteInfo.pAttachments = attachments;
  creteInfo.subpassCount = 1;
  creteInfo.pSubpasses = &subpass;
  creteInfo.dependencyCount = 1;
  creteInfo.pDependencies = &dependency;

  SKL_ASSERT_VK(
      vkCreateRenderPass(vulkanContext.device, &creteInfo, nullptr, &vulkanContext.renderPass),
      "Failed to create renderpass");
}

void SklRenderBackend::CreateDepthImage()
{
  VkFormat depthFormat = FindDepthFormat();
  uint32_t i = CreateImage(vulkanContext.renderExtent.width, vulkanContext.renderExtent.height,
                           depthFormat, VK_IMAGE_TILING_OPTIMAL,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  depthImage = ImageManager::images[i];
  depthImage->view = CreateImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImage->image);
}

void SklRenderBackend::CreateFramebuffers()
{
  VkFramebufferCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  createInfo.renderPass = vulkanContext.renderPass;
  createInfo.layers = 1;
  createInfo.width = vulkanContext.renderExtent.width;
  createInfo.height = vulkanContext.renderExtent.height;

  uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());
  frameBuffers.resize(imageCount);

  for (uint32_t i = 0; i < imageCount; i++)
  {
    VkImageView attachments[] = { swapchainImageViews[i], depthImage->view };
    createInfo.attachmentCount = 2;
    createInfo.pAttachments = attachments;

    SKL_ASSERT_VK(
        vkCreateFramebuffer(vulkanContext.device, &createInfo, nullptr, &frameBuffers[i]),
        "Failed to create a framebuffer");
  }
}

void SklRenderBackend::CreateDescriptorPool()
{
  VkDescriptorPoolSize poolSizes[2] = {};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = 3;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = 6;

  VkDescriptorPoolCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  createInfo.poolSizeCount = 2;
  createInfo.pPoolSizes = poolSizes;
  createInfo.maxSets = 3;

  SKL_ASSERT_VK(
      vkCreateDescriptorPool(vulkanContext.device, &createInfo, nullptr, &descriptorPool),
      "Failed to create descriptor pool");
}

void SklRenderBackend::CreateSyncObjects()
{
  imageIsInFlightFences.resize(swapchainImages.size(), VK_NULL_HANDLE);
  flightFences.resize(MAX_FLIGHT_IMAGE_COUNT);
  imageAvailableSemaphores.resize(MAX_FLIGHT_IMAGE_COUNT);
  renderCompleteSemaphores.resize(MAX_FLIGHT_IMAGE_COUNT);

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
  {
    SKL_ASSERT_VK(
        vkCreateFence(vulkanContext.device, &fenceInfo, nullptr,
                      &flightFences[i]),
        "Failed to create sync fence");
    SKL_ASSERT_VK(
        vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr,
                          &imageAvailableSemaphores[i]),
        "Failed to create image semaphore");
    SKL_ASSERT_VK(
        vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr,
                          &renderCompleteSemaphores[i]),
        "Failed to create render semaphore");
  }
}

void SklRenderBackend::CreateCommandBuffers()
{
  uint32_t bufferCount = static_cast<uint32_t>(swapchainImages.size());
  commandBuffers.resize(bufferCount);

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = bufferCount;
  allocInfo.commandPool = graphicsCommandPool;

  SKL_ASSERT_VK(
      vkAllocateCommandBuffers(vulkanContext.device, &allocInfo, commandBuffers.data()),
      "Failed to allocate command buffers");
}

//=================================================
// Helpers
//=================================================

// Initialization
//=================================================

uint32_t SklRenderBackend::GetQueueIndex(std::vector<VkQueueFamilyProperties>& _queues,
                                           VkQueueFlags _flags)
{
  uint32_t i = 0;
  uint32_t bestfit = -1;

  for (const auto& props : _queues)
  {
    if ((props.queueFlags & _flags) == _flags)
    {
      // Attempt to avoid queues that share with Graphics
      if ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 || (_flags & VK_QUEUE_GRAPHICS_BIT))
      {
        return i;
      }
      else
      {
        bestfit = i;
      }
    }

    i++;
  }

  // Returns bestfit (-1 if no bestfit was found)
  return bestfit;
}

uint32_t SklRenderBackend::GetPresentIndex(const VkPhysicalDevice* _device,
                                             uint32_t _queuePropertyCount, uint32_t _graphicsIndex)
{
  uint32_t bestfit = -1;
  VkBool32 supported;


  for (uint32_t i = 0; i < _queuePropertyCount; i++)
  {
    vkGetPhysicalDeviceSurfaceSupportKHR(*_device, i, surface, &supported);
    if (supported)
    {
      // Attempt to avoid queues that share with Graphics
      if (i != _graphicsIndex)
      {
        return i;
      }
      else
      {
        bestfit = i;
      }
    }
  }

  // Returns bestfit (-1 if no bestfit was found)
  return bestfit;
}

// Renderer
//=================================================

uint32_t SklRenderBackend::CreateImage(uint32_t _width, uint32_t _height, VkFormat _format,
                                       VkImageTiling _tiling, VkImageUsageFlags _usage,
                                       VkMemoryPropertyFlags _memFlags)
{
  return ImageManager::CreateImage(_width, _height, _format, _tiling, _usage, _memFlags);
}

VkImageView SklRenderBackend::CreateImageView(const VkFormat _format, VkImageAspectFlags _aspect,
                                              const VkImage& _image)
{
  VkImageViewCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.baseMipLevel = 0;
  createInfo.subresourceRange.layerCount = 1;
  createInfo.subresourceRange.baseArrayLayer = 0;
  createInfo.subresourceRange.aspectMask = _aspect;
  createInfo.image = _image;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.format = _format;

  VkImageView tmpView;
  SKL_ASSERT_VK(
      vkCreateImageView(vulkanContext.device, &createInfo, nullptr, &tmpView),
      "Failed to create image view");

  return tmpView;
}

VkFormat SklRenderBackend::FindDepthFormat()
{
  return FindSupportedFormat(
      { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
      VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat SklRenderBackend::FindSupportedFormat(const std::vector<VkFormat>& _candidates,
                                                 VkImageTiling _tiling,
                                                 VkFormatFeatureFlags _features)
{
  for (VkFormat format : _candidates)
  {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(vulkanContext.gpu.device, format, &properties);

    if (_tiling == VK_IMAGE_TILING_LINEAR
        && (properties.linearTilingFeatures & _features) == _features)
      return format;
    else if (_tiling == VK_IMAGE_TILING_OPTIMAL
             && (properties.optimalTilingFeatures & _features) == _features)
      return format;
  }
  throw std::runtime_error("Failed to find a suitable format");
}

