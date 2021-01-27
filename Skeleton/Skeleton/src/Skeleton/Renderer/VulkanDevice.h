#pragma once

#include <vector>

#include "vulkan/vulkan.h"

#include "Skeleton/Renderer/VulkanDevice.h"

namespace skeleton
{

class VulkanDevice
{
//=================================================
// Variables
//=================================================
public:
	VkDevice logicalDevice;
	VkPhysicalDevice physicalDevice;

	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkQueue transferQueue;

	VkCommandPool transientPool;

	struct QueueFamilyIndices {
		uint32_t graphics;
		uint32_t present;
		uint32_t transfer;
	} queueIndices;

//=================================================
// Functions
//=================================================

public:
	VulkanDevice(
		VkPhysicalDevice _physical);
	~VulkanDevice();

	void CreateLogicalDevice(
		const std::vector<uint32_t> _queueIndices,
		const std::vector<const char*> _deviceExtensions,
		const std::vector<const char*> _deviceLayers);
	void Cleanup();

	// Commands
	//=================================================

	void CreateCommandPool(
		VkCommandPool& _pool,
		uint32_t _queueIndex,
		VkCommandPoolCreateFlags _flags = 0);

	VkCommandBuffer BeginSingleTimeCommand(	
		VkCommandPool& _pool);
	void EndSingleTimeCommand(
		VkCommandBuffer _command,
		VkCommandPool& _pool,
		VkQueue& _queue);

	// Operators
	//=================================================

	operator VkDevice() const { return logicalDevice; }

};

}

