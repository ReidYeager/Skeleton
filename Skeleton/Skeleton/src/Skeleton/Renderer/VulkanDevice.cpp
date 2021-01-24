
#include "pch.h"
#include "VulkanDevice.h"

#include "Skeleton/Core/Common.h"

skeleton::VulkanDevice::VulkanDevice(
	VkPhysicalDevice _physical) : physicalDevice(_physical)
{

}

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

