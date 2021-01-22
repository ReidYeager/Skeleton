
#include "pch.h"
#include "VulkanDevice.h"

#include "Skeleton/Core/Common.h"

skeleton::VulkanDevice::~VulkanDevice()
{
	Cleanup();
}

void skeleton::VulkanDevice::CreateLogicalDevice(
	const std::vector<uint32_t> _queueIndices,
	const std::vector<const char*> _deviceExtensions,
	const std::vector<const char*> _deviceLayers)
{
	VkPhysicalDeviceFeatures enabledFeatures = {};
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
		vkCreateDevice(physicalDevice, &createInfo, nullptr, &logicalDevice),
		"Failed to create logical device");

	vkGetDeviceQueue(logicalDevice, _queueIndices[0], 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, _queueIndices[1], 0, &presentQueue);
	vkGetDeviceQueue(logicalDevice, _queueIndices[2], 0, &transferQueue);

	CreateCommandPool(transientPool, queueIndices.transfer, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

void skeleton::VulkanDevice::Cleanup()
{
	vkDestroyCommandPool(logicalDevice, transientPool, nullptr);
	vkDestroyDevice(logicalDevice, nullptr);
}

//=================================================
// Buffers
//=================================================

void skeleton::VulkanDevice::CreateAndFillBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	CreateBuffer(
		stagingBuffer,
		stagingMemory,
		_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	FillBuffer(stagingMemory, _data, _size);

	CreateBuffer(
		_buffer,
		_memory,
		_size,
		_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CopyBuffer(stagingBuffer, _buffer, _size);

	vkFreeMemory(logicalDevice, stagingMemory, nullptr);
	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
}

void skeleton::VulkanDevice::CreateBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage,
	VkMemoryPropertyFlags _memProperties)
{
	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.usage = _usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = _size;

	SKL_ASSERT_VK(
		vkCreateBuffer(logicalDevice, &createInfo, nullptr, &_buffer),
		"Failed to create vert buffer");

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(logicalDevice, _buffer, &memReq);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memReq.memoryTypeBits,
		_memProperties);

	SKL_ASSERT_VK(
		vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &_memory),
		"Failed to allocate vert memory");

	vkBindBufferMemory(logicalDevice, _buffer, _memory, 0);
}

void skeleton::VulkanDevice::FillBuffer(
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size)
{
	void* tmpData;
	vkMapMemory(logicalDevice, _memory, 0, _size, 0, &tmpData);
	memcpy(tmpData, _data, static_cast<size_t>(_size));
	vkUnmapMemory(logicalDevice, _memory);
}

void skeleton::VulkanDevice::CopyBuffer(
	VkBuffer _src,
	VkBuffer _dst,
	VkDeviceSize _size)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = transientPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer transferCommand;
	SKL_ASSERT_VK(
		vkAllocateCommandBuffers(logicalDevice, &allocInfo, &transferCommand),
		"Failed to create transient command buffer");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(transferCommand, &beginInfo);

	VkBufferCopy region = {};
	region.size = _size;
	region.dstOffset = 0;
	region.srcOffset = 0;

	vkCmdCopyBuffer(transferCommand, _src, _dst, 1, &region);

	vkEndCommandBuffer(transferCommand);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommand;

	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	vkFreeCommandBuffers(logicalDevice, transientPool, 1, &transferCommand);
}

uint32_t skeleton::VulkanDevice::FindMemoryType(
	uint32_t _mask,
	VkMemoryPropertyFlags _flags)
{
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &props);

	for (uint32_t i = 0; i < props.memoryTypeCount; i++)
	{
		if (_mask & (1 << i) && (props.memoryTypes[i].propertyFlags & _flags) == _flags)
		{
			return i;
		}
	}

	SKL_ASSERT_VK(
		VK_ERROR_UNKNOWN,
		"Failed to find a suitable memory type");
	return 0;
}

//=================================================
// Pools
//=================================================

void skeleton::VulkanDevice::CreateCommandPool(
	VkCommandPool& _pool,
	uint32_t _queueIndex,
	VkCommandPoolCreateFlags _flags /*= 0*/)
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = _queueIndex;
	createInfo.flags = _flags;
	SKL_ASSERT_VK(
		vkCreateCommandPool(logicalDevice, &createInfo, nullptr, &_pool),
		"Failed to create command pool with flags %u", _flags);
}

