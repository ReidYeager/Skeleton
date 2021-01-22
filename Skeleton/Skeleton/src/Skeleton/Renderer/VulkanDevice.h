#pragma once

#include <vector>

#include "vulkan/vulkan.h"

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
	VulkanDevice(VkPhysicalDevice _physical) : physicalDevice(_physical) {}
	~VulkanDevice();

	void CreateLogicalDevice(
		const std::vector<uint32_t> _queueIndices,
		const std::vector<const char*> _deviceExtensions,
		const std::vector<const char*> _deviceLayers);
	void Cleanup();

	// Buffers
	//=================================================

	void CreateAndFillBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		const void* _data,
		VkDeviceSize size,
		VkBufferUsageFlags _usage);

	void CreateBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		VkDeviceSize _size,
		VkBufferUsageFlags _usage,
		VkMemoryPropertyFlags _memProperties);

	void FillBuffer(
		VkDeviceMemory& memory,
		const void* _data,
		VkDeviceSize _size);

	void CopyBuffer(
		VkBuffer _src,
		VkBuffer _dst,
		VkDeviceSize _size);

	uint32_t FindMemoryType(
		uint32_t _mask,
		VkMemoryPropertyFlags _flags);

	// Pools
	//=================================================

	void CreateCommandPool(
		VkCommandPool& _pool,
		uint32_t _queueIndex,
		VkCommandPoolCreateFlags _flags = 0);

	// Operators
	//=================================================

	operator VkDevice() const { return logicalDevice; }

};

}

