
#ifndef SKELETON_RENDERER_RENDER_BACKEND_H
#define SKELETON_RENDERER_RENDER_BACKEND_H 1

#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

#include "skeleton/renderer/vulkan_context.h"
#include "skeleton/core/debug_tools.h"
#include "skeleton/core/time.h"
#include "skeleton/renderer/shader_program.h"
#include "skeleton/renderer/resource_managers.h"
#include "skeleton/core/mesh.h"

// Holds the MVP matrix of a camera and all the objects it should render
struct sklView_t
{
  glm::mat4 mvpMatrix;                       // This view's Model-View-Projection matrix
  std::vector<sklRenderable_t> renderables;  // Objects this view can see
};

// Creates a command pool
inline void SklCreateCommandPool(VkCommandPool& _pool, uint32_t _queueIndex,
                                 VkCommandPoolCreateFlags _flags = 0)
{
  VkCommandPoolCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex = _queueIndex;
  createInfo.flags = _flags;
  SKL_ASSERT_VK(
    vkCreateCommandPool(vulkanContext.device, &createInfo, nullptr, &_pool),
    "Failed to create command pool with flags %u", _flags);
}

// Creates a logical device based on the selected GPU in the vulkanContext
inline void CreateLogicalDevice(const std::vector<uint32_t> _queueIndices,
                                const std::vector<const char*> _deviceExtensions,
                                const std::vector<const char*> _deviceLayers)
{
  VkPhysicalDeviceFeatures enabledFeatures = {};
  enabledFeatures.samplerAnisotropy = VK_TRUE;
  uint32_t queueCount = static_cast<uint32_t>(_queueIndices.size());

  const float priority = 1.f;
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueCount);
  for (uint32_t i = 0; i < queueCount; i++)
  {
    queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfos[i].queueFamilyIndex = _queueIndices[i];
    queueCreateInfos[i].queueCount = 1;
    queueCreateInfos[i].pQueuePriorities = &priority;
  }

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pEnabledFeatures = &enabledFeatures;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = _deviceExtensions.data();
  createInfo.enabledLayerCount = static_cast<uint32_t>(_deviceLayers.size());
  createInfo.ppEnabledLayerNames = _deviceLayers.data();
  createInfo.queueCreateInfoCount = queueCount;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();

  SKL_ASSERT_VK(
    vkCreateDevice(vulkanContext.gpu.device, &createInfo, nullptr, &vulkanContext.device),
    "Failed to create logical device");

  vkGetDeviceQueue(vulkanContext.device, vulkanContext.graphicsIdx, 0,
                   &vulkanContext.graphicsQueue);
  vkGetDeviceQueue(vulkanContext.device, vulkanContext.presentIdx, 0,
                   &vulkanContext.presentQueue);
  vkGetDeviceQueue(vulkanContext.device, vulkanContext.transferIdx, 0,
                   &vulkanContext.transferQueue);
}

// Handles Vulkan initialization and destruction
class SklRenderBackend
{
private:
  std::vector<const char*> validationLayer = { "VK_LAYER_KHRONOS_validation" };
  std::vector<const char*> instanceExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
  std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

public:
  // TODO : Find a way to remove SDL window from Create(instance/swapchain)
  SDL_Window* window;
  BufferManager* bufferManager;

  VkInstance instance;
  VkSurfaceKHR surface;

  VkSwapchainKHR swapchain;
  VkFormat swapchainFormat;
  std::vector<VkImage> swapchainImages;
  std::vector<VkImageView> swapchainImageViews;
  std::vector<VkFramebuffer> frameBuffers;

  sklImage_t* depthImage;

  VkDescriptorPool descriptorPool;

  // Synchronization
  #define MAX_FLIGHT_IMAGE_COUNT 3
  std::vector<VkFence> imageIsInFlightFences;
  std::vector<VkFence> flightFences;
  std::vector<VkSemaphore> renderCompleteSemaphores;
  std::vector<VkSemaphore> imageAvailableSemaphores;
  uint32_t currentFrame = 0;

  std::vector<VkCommandBuffer> commandBuffers;

public:
  // Initialization
  //=================================================

  // Initializes surface independent components and selects a physical device
  SklRenderBackend(SDL_Window* _window, const std::vector<const char*>& _extraExtensions);
  // Destroys all attached Vulkan components
  ~SklRenderBackend();

  // Creates the vkInstance & vkSurface
  void CreateInstance();
  // Gets a physical device & creates the vkDevice
  void CreateDevice();
  // Determines the best physical device for rendering
  void ChoosePhysicalDevice(VkPhysicalDevice& _selectedDevice, uint32_t& _graphicsIndex,
                            uint32_t& _presentIndex, uint32_t& _transferIndex);
  // Creates a command pool for graphics
  void CreateCommandPool();

  // Renderer
  //=================================================

  // One-time creation of all components required for rendering
  void InitializeRenderComponents();
  // Creates all components required for rendering dependent on the swapchain
  void CreateRenderComponents();
  // Destroys all components required for rendering dependent on the swapchain
  void CleanupRenderComponents();
  // Destroys and recreates all components required for rendering dependent on the swapchain
  void RecreateRenderComponents();

  // Creates the swapchain, retrieves its images, and creates their views
  void CreateSwapchain();
  // Creates a generic Renderpass
  void CreateRenderpass();
  // Creates a generic DepthImage
  void CreateDepthImage();
  // Creates a set of Framebuffers
  void CreateFramebuffers();

  // Creates a generic shader descriptor pool
  void CreateDescriptorPool();
  // Creates the fences and semaphores required for GPU/CPU synchronization
  void CreateSyncObjects();
  // Allocates CommandBuffers for recording
  void CreateCommandBuffers();

  // Helpers
  //=================================================

  // Returns the first instance of a queue with the input flags
  uint32_t GetQueueIndex(std::vector<VkQueueFamilyProperties>& _queues, VkQueueFlags _flags);
  // Returns the first instance of a presentation queue
  uint32_t GetPresentIndex(const VkPhysicalDevice* _device, uint32_t _queuePropertyCount,
                           uint32_t _graphicsIndex);
  // Creates and binds an image and its memory
  uint32_t CreateImage(uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling,
                       VkImageUsageFlags _usage, VkMemoryPropertyFlags _memFlags);
  // Find the first supported format for a depth image
  VkFormat FindDepthFormat();
  // Find the first specified format supported
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling,
                               VkFormatFeatureFlags _features);

};

#endif // SKELETON_RENDERER_RENDERER_BACKEND_H

