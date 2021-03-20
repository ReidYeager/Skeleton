
#ifndef SKELETON_RENDERER_VULKAN_CONTEXT_H
#define SKELETON_RENDERER_VULKAN_CONTEXT_H 1

#include "vulkan/vulkan.h"

#include "skeleton/renderer/shader_program.h"
#include "skeleton/core/mesh.h"
#include "skeleton/core/debug_tools.h"
#include "skeleton/renderer/image.h"

// A buffer and its device memory
struct sklBuffer_t
{
  VkBuffer buffer;
  VkDeviceMemory memory;
};

// A bound object with all information needed for rendering
struct sklRenderable_t
{
  mesh_t mesh;
  uint32_t shaderProgramIndex;
  std::vector<sklBuffer_t*> buffers;
  std::vector<sklImage_t*> images;

  // TODO : CreateBuffer()/CreateImage()
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

  VkCommandPool graphicsCommandPool;

  std::vector<shader_t> shaders;
  std::vector<shaderProgram_t> shaderPrograms;

  VkExtent2D renderExtent;
  VkRenderPass renderPass;

  std::vector<sklRenderable_t> renderables;

  // Destroys all attached Vulkan components
  void Cleanup();

  // Handles all setup for recording a commandBuffer to be executed once
  VkCommandBuffer BeginSingleTimeCommand(VkCommandPool& _pool)
  {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = _pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer singleCommand;
    SKL_ASSERT_VK(
      vkAllocateCommandBuffers(device, &allocInfo, &singleCommand),
      "Failed to create transient command buffer");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(singleCommand, &beginInfo);

    return singleCommand;
  }

  // Handles the execution and destruction of a commandBuffer
  void EndSingleTimeCommand(VkCommandBuffer _command, VkCommandPool& _pool, VkQueue& _queue)
  {
    vkEndCommandBuffer(_command);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_command;

    vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_queue);

    vkFreeCommandBuffers(device, _pool, 1, &_command);
  }
};

// Global SklVulkanContext_t for arbitrary use throughout the program
extern SklVulkanContext_t vulkanContext;

#endif // !SKELETON_RENDERER_VULKAN_CONTEXT_H

