
#include <vector>

#include "vulkan/vulkan.h"

#include "Skeleton/Core/Common.h"
#include "Skeleton/Renderer/ParProgs.h"
#include "Skeleton/Core/Mesh.h"

// TODO : Clean this file up

#ifndef SKL_RENDER_BACKEND_H
#define SKL_RENDER_BACKEND_H

struct SklPhysicalDeviceInfo_t
{
	VkPhysicalDevice						device;
	VkPhysicalDeviceProperties				properties;
	VkPhysicalDeviceMemoryProperties		memProperties;
	VkPhysicalDeviceFeatures				features;
	VkSurfaceCapabilitiesKHR				surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR>			surfaceFormats;
	std::vector<VkPresentModeKHR>			presentModes;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	std::vector<VkExtensionProperties>		extensionProperties;
};

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

	std::vector<skeleton::shader_t> shaders;
	std::vector<skeleton::parProg_t> parProgs;

	VkExtent2D renderExtent;
	VkRenderPass renderPass;

	std::vector<skeleton::mesh_t> meshes;

	void Cleanup();
};

extern SklVulkanContext_t vulkanContext;

//=================================================
// Commands
//=================================================

inline void CreateCommandPool(
	VkCommandPool& _pool,
	uint32_t _queueIndex,
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

inline VkCommandBuffer BeginSingleTimeCommand(
	VkCommandPool& _pool)
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

inline void EndSingleTimeCommand(
	VkCommandBuffer _command,
	VkCommandPool& _pool,
	VkQueue& _queue)
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


//=================================================
// Create logical device
//=================================================

inline void CreateLogicalDevice(
	const std::vector<uint32_t> _queueIndices,
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

	//CreateCommandPool(transientPool, queueIndices.transfer, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

class RenderBackend
{

};

#endif // RENDERER_BACKEND_H

