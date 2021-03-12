
#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

#include "Skeleton/Core/DebugTools.h"
#include "Skeleton/Core/Time.h"
#include "Skeleton/Renderer/ShaderProgram.h"
#include "Skeleton/Core/Mesh.h"

#ifndef SKELETON_RENDERER_RENDERER_BACKEND_H
#define SKELETON_RENDERER_RENDERER_BACKEND_H

// A buffer and its device memory
struct sklBuffer_t
{
  VkBuffer buffer;
  VkDeviceMemory memory;
};

// An image and its device memory and sampling information
struct sklImage_t
{
  VkImage image;
  VkImageView view;
  VkDeviceMemory memory;
  VkSampler sampler;
};

// A bound object with all information needed for rendering
struct sklRenderable_t
{
  mesh_t mesh;
  uint32_t parProgIndex;
  std::vector<sklBuffer_t*> buffers;
  std::vector<sklImage_t*> images;

  // TODO : Create buffer/image function

  // Destroys all attached images
  void Cleanup();
};

// Holds all information Vulkan needs when working with a GPU
struct SklPhysicalDeviceInfo_t
{
  VkPhysicalDevice                      device;
  VkPhysicalDeviceProperties            properties;
  VkPhysicalDeviceMemoryProperties      memProperties;
  VkPhysicalDeviceFeatures              features;
  VkSurfaceCapabilitiesKHR              surfaceCapabilities;
  std::vector<VkSurfaceFormatKHR>       surfaceFormats;
  std::vector<VkPresentModeKHR>         presentModes;
  std::vector<VkQueueFamilyProperties>  queueFamilyProperties;
  std::vector<VkExtensionProperties>    extensionProperties;
};

// Holds all information needed throughout Vulkan's execution
struct SklVulkanContext_t
{
  SklPhysicalDeviceInfo_t gpu;
  VkDevice device;

  uint32_t graphicsIdx;
  uint32_t presentIdx;
  uint32_t transferIdx;
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkQueue transferQueue;

  std::vector<shader_t> shaders;
  std::vector<shaderProgram_t> parProgs;

  VkExtent2D renderExtent;
  VkRenderPass renderPass;

  std::vector<sklRenderable_t> renderables;

  // Destroys all attached Vulkan components
  void Cleanup();
};

// Holds the MVP matrix of a camera and all the objects it should render
struct sklView_t
{
  glm::mat4 mvpMatrix;                       // This view's Model-View-Projection matrix
  std::vector<sklRenderable_t> renderables;  // Objects this view can see
};

// Global SklVulkanContext_t for arbitrary use throughout the program
extern SklVulkanContext_t vulkanContext;

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

// Handles all setup for recording a commandBuffer to be executed once
inline VkCommandBuffer BeginSingleTimeCommand(VkCommandPool& _pool)
{
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = _pool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer singleCommand;
  SKL_ASSERT_VK(
    vkAllocateCommandBuffers(vulkanContext.device, &allocInfo, &singleCommand),
    "Failed to create transient command buffer");

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(singleCommand, &beginInfo);

  return singleCommand;
}

// Handles the execution and destruction of a commandBuffer
inline void EndSingleTimeCommand(VkCommandBuffer _command, VkCommandPool& _pool, VkQueue& _queue)
{
  vkEndCommandBuffer(_command);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &_command;

  vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(_queue);

  vkFreeCommandBuffers(vulkanContext.device, _pool, 1, &_command);
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

  vkGetDeviceQueue(vulkanContext.device, vulkanContext.graphicsIdx, 0, &vulkanContext.graphicsQueue);
  vkGetDeviceQueue(vulkanContext.device, vulkanContext.presentIdx, 0, &vulkanContext.presentQueue);
  vkGetDeviceQueue(vulkanContext.device, vulkanContext.transferIdx, 0, &vulkanContext.transferQueue);
}

// TODO : Handle buffer memory better
// Handles the life of VkBuffers and their memory
class BufferManager
{
  //=================================================
  // Variables
  //=================================================
private:
  uint64_t m_indexBitMap = 0;
  const uint32_t INDEXBITMAPSIZE = 64;
  std::vector<VkBuffer> m_buffers;
  std::vector<VkDeviceMemory> m_memories;

public:
  VkCommandPool transientPool; // Used for single-time commands

  //=================================================
  // Functions
  //=================================================
public:
  // Initializes the transientPool
  BufferManager()
  {
    SklCreateCommandPool(transientPool, vulkanContext.transferIdx);
  }
  // Destroys all buffers and frees their memory
  ~BufferManager();

  // Retrieves the status of a buffer
  bool GetIndexBitMapAt(uint32_t _index);
  // Sets the status of a buffer
  void SetIndexBitMapAt(uint32_t _index, bool _value = true);

  // Retrieves the number of active buffers
  uint32_t GetBufferCount() { return static_cast<uint32_t>(m_buffers.size()); }

  // Retrieves a specific buffer, returns nullptr if one is not found
  const VkBuffer* GetBuffer(uint32_t _index);
  // Retrieves a specific buffer's memory, returns nullptr if none is not found
  const VkDeviceMemory* GetMemory(uint32_t _index);
  // Destroys a specific buffer and frees its memory
  void RemoveAtIndex(uint32_t _index);
  // Retrieves the index of the first available buffer slot
  uint32_t GetFirstAvailableIndex();

  // Creates a new buffer and fills its memory
  uint32_t CreateAndFillBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory, const void* _data,
                               VkDeviceSize size, VkBufferUsageFlags _usage);

  // Creates a new buffer and allocates its memory, returns the buffer and memory
  uint32_t CreateBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory, VkDeviceSize _size,
                        VkBufferUsageFlags _usage, VkMemoryPropertyFlags _memProperties);
  // Creates a new buffer and allocates its memory
  uint32_t CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage,
                        VkMemoryPropertyFlags _memProperties);

  // Copies data into deviceMemory
  void FillBuffer(VkDeviceMemory& memory, const void* _data, VkDeviceSize _size);
  // Copies the data from a source buffer into another
  void CopyBuffer(VkBuffer _src, VkBuffer _dst, VkDeviceSize _size);
  // Finds the first memory property that fits input criteria
  uint32_t FindMemoryType(uint32_t _mask, VkMemoryPropertyFlags _flags);

}; // BufferManager


// Handles Vulkan initialization and destruction
class SklRendererBackend
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

  VkImage depthImage;
  VkImageView depthImageView;
  VkDeviceMemory depthMemory;

  VkDescriptorPool descriptorPool;

  // Synchronization
  #define MAX_FLIGHT_IMAGE_COUNT 3
  std::vector<VkFence> imageIsInFlightFences;
  std::vector<VkFence> flightFences;
  std::vector<VkSemaphore> renderCompleteSemaphores;
  std::vector<VkSemaphore> imageAvailableSemaphores;
  uint32_t currentFrame = 0;

  VkCommandPool graphicsCommandPool;
  std::vector<VkCommandBuffer> commandBuffers;

public:
  //=================================================
  // Initialization
  //=================================================

  // Initializes surface independent components and selects a physical device
  SklRendererBackend(SDL_Window* _window, const std::vector<const char*>& _extraExtensions);
  // Destroys all attached Vulkan components
  ~SklRendererBackend();

  // Creates the vkInstance & vkSurface
  void CreateInstance();
  // Gets a physical device & creates the vkDevice
  void CreateDevice();
  // Determines the best physical device for rendering
  void ChoosePhysicalDevice(VkPhysicalDevice& _selectedDevice, uint32_t& _graphicsIndex,
                            uint32_t& _presentIndex, uint32_t& _transferIndex);
  // Creates a command pool for graphics
  void CreateCommandPool();

  //=================================================
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


  //=================================================
  // Helpers
  //=================================================

  // Initialization
  //=================================================

  // Returns the first instance of a queue with the input flags
  uint32_t GetQueueIndex(std::vector<VkQueueFamilyProperties>& _queues, VkQueueFlags _flags);

  // Returns the first instance of a presentation queue
  uint32_t GetPresentIndex(const VkPhysicalDevice* _device, uint32_t _queuePropertyCount,
                           uint32_t _graphicsIndex);

  // Renderer
  //=================================================

  // Creates and binds an image and its memory
  void CreateImage(uint32_t _width, uint32_t _height, VkFormat _format, VkImageTiling _tiling,
                   VkImageUsageFlags _usage, VkMemoryPropertyFlags _memFlags, VkImage& _image,
                   VkDeviceMemory& _memory);

  // TODO : Add view creation settings (Mip levels, multi-layer, etc.)
  // Creates a basic imageView
  VkImageView CreateImageView(const VkFormat _format, VkImageAspectFlags _aspect,
                              const VkImage& _image);

  // Find the first supported format for a depth image
  VkFormat FindDepthFormat();
  // Find the first specified format supported
  VkFormat FindSupportedFormat(const std::vector<VkFormat>& _candidates, VkImageTiling _tiling,
                               VkFormatFeatureFlags _features);

};

#endif // SKELETON_RENDERER_RENDERER_BACKEND_H

