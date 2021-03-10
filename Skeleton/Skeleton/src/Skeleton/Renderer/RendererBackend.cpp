
#include "pch.h"
#include "RendererBackend.h"
#include "Skeleton/Core/Common.h"

BufferManager::~BufferManager()
{
	uint32_t bufferCount = GetBufferCount();
	for (uint32_t i = 0; i < bufferCount; i++)
	{
		if (GetIndexBitMapAt(i))
		{
			vkFreeMemory(vulkanContext.device, m_memories[i], nullptr);
			vkDestroyBuffer(vulkanContext.device, m_buffers[i], nullptr);
		}
	}

	vkDestroyCommandPool(vulkanContext.device, transientPool, nullptr);
}

bool BufferManager::GetIndexBitMapAt(
	uint32_t _index)
{
	return (m_indexBitMap & (1i64 << _index));
}

void BufferManager::SetIndexBitMapAt(
	uint32_t _index,
	bool _value /*= true*/)
{
	if (_value)
	{
		m_indexBitMap |= (1i64 << _index);
	}
	else
	{
		m_indexBitMap &= ~(1i64 << _index);
	}
}

const VkBuffer* BufferManager::GetBuffer(
	uint32_t _index)
{
	if (GetIndexBitMapAt(_index))
	{
		return &m_buffers[_index];
	}
	else
	{
		return nullptr;
	}
}

const VkDeviceMemory* BufferManager::GetMemory(
	uint32_t _index)
{
	if (GetIndexBitMapAt(_index))
	{
		return &m_memories[_index];
	}
	else
	{
		return nullptr;
	}
}

void BufferManager::RemoveAtIndex(
	uint32_t _index)
{
	SetIndexBitMapAt(_index, false);
	vkFreeMemory(vulkanContext.device, m_memories[_index], nullptr);
	vkDestroyBuffer(vulkanContext.device, m_buffers[_index], nullptr);
}

uint32_t BufferManager::GetFirstAvailableIndex()
{
	uint32_t arraySize = static_cast<uint32_t>(m_buffers.size());
	uint32_t i = 0;
	while (i <= arraySize && i < INDEXBITMAPSIZE)
	{
		if (GetIndexBitMapAt(i) == 0)
		{
			return i;
		}

		i++;
	}
	SKL_LOG(SKL_ERROR, "Failed to find an available index");
	return -1;
}

uint32_t BufferManager::CreateAndFillBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	uint32_t stagingBufferIndex = CreateBuffer(
		stagingBuffer,
		stagingMemory,
		_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	FillBuffer(stagingMemory, _data, _size);

	uint32_t index = CreateBuffer(
		_buffer,
		_memory,
		_size,
		_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CopyBuffer(stagingBuffer, _buffer, _size);

	RemoveAtIndex(stagingBufferIndex);
	return index;
}

uint32_t BufferManager::CreateBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage,
	VkMemoryPropertyFlags _memProperties)
{
	uint32_t index = GetFirstAvailableIndex();
	SetIndexBitMapAt(index);

	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.usage = _usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = _size;

	VkBuffer tmpBuffer;
	SKL_ASSERT_VK(
		vkCreateBuffer(vulkanContext.device, &createInfo, nullptr, &tmpBuffer),
		"Failed to create vert buffer");

	if (index < static_cast<uint32_t>(m_buffers.size()))
	{
		m_buffers[index] = tmpBuffer;
	}
	else
	{
		m_buffers.push_back(tmpBuffer);
	}
	_buffer = m_buffers[index];

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(vulkanContext.device, _buffer, &memReq);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memReq.memoryTypeBits,
		_memProperties);

	VkDeviceMemory tmpMemory;
	SKL_ASSERT_VK(
		vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &tmpMemory),
		"Failed to allocate vert memory");

	if (index < static_cast<uint32_t>(m_memories.size()))
	{
		m_memories[index] = tmpMemory;
	}
	else
	{
		m_memories.push_back(tmpMemory);
	}
	_memory = m_memories[index];

	vkBindBufferMemory(vulkanContext.device, _buffer, _memory, 0);

	return index;
}

uint32_t BufferManager::CreateBuffer(
	VkDeviceSize _size,
	VkBufferUsageFlags _usage,
	VkMemoryPropertyFlags _memProperties)
{
	uint32_t index = GetFirstAvailableIndex();
	SetIndexBitMapAt(index);

	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.usage = _usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = _size;

	VkBuffer tmpBuffer;
	SKL_ASSERT_VK(
		vkCreateBuffer(vulkanContext.device, &createInfo, nullptr, &tmpBuffer),
		"Failed to create vert buffer");

	if (index < static_cast<uint32_t>(m_buffers.size()))
	{
		m_buffers[index] = tmpBuffer;
	}
	else
	{
		m_buffers.push_back(tmpBuffer);
	}

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(vulkanContext.device, m_buffers[index], &memReq);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memReq.memoryTypeBits,
		_memProperties);

	VkDeviceMemory tmpMemory;
	SKL_ASSERT_VK(
		vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &tmpMemory),
		"Failed to allocate vert memory");

	if (index < static_cast<uint32_t>(m_memories.size()))
	{
		m_memories[index] = tmpMemory;
	}
	else
	{
		m_memories.push_back(tmpMemory);
	}

	vkBindBufferMemory(vulkanContext.device, m_buffers[index], m_memories[index], 0);

	return index;
}

void BufferManager::FillBuffer(
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size)
{
	void* tmpData;
	vkMapMemory(vulkanContext.device, _memory, 0, _size, 0, &tmpData);
	memcpy(tmpData, _data, static_cast<size_t>(_size));
	vkUnmapMemory(vulkanContext.device, _memory);
}

void BufferManager::CopyBuffer(
	VkBuffer _src,
	VkBuffer _dst,
	VkDeviceSize _size)
{
	VkCommandBuffer copyCommand = BeginSingleTimeCommand(transientPool);

	VkBufferCopy region = {};
	region.size = _size;
	region.dstOffset = 0;
	region.srcOffset = 0;

	vkCmdCopyBuffer(copyCommand, _src, _dst, 1, &region);

	EndSingleTimeCommand(copyCommand, transientPool, vulkanContext.transferQueue);
}

uint32_t BufferManager::FindMemoryType(
	uint32_t _mask,
	VkMemoryPropertyFlags _flags)
{
	const VkPhysicalDeviceMemoryProperties& props = vulkanContext.gpu.memProperties;

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


SklVulkanContext_t vulkanContext;

void SklVulkanContext_t::Cleanup()
{
	SKL_PRINT("Vulkan Context", "Cleanup =================================================");
	for (uint32_t i = 0; i < parProgs.size(); i++)
	{
		vkDestroyPipeline(device, parProgs[i].pipeline, nullptr);
		vkDestroyPipelineLayout(device, parProgs[i].pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, parProgs[i].descriptorSetLayout, nullptr);
	}

	for (auto& rend : renderables)
	{
		rend.Cleanup();
	}

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyDevice(device, nullptr);
}

void sklRenderable_t::Cleanup()
{
	//for (const auto& b : buffers)
		//{
		//	vkDestroyBuffer(vulkanContext.device, b->buffer, nullptr);
		//	vkFreeMemory(vulkanContext.device, b->memory, nullptr);
		//}

	for (const auto& i : images)
	{
		vkFreeMemory(vulkanContext.device, i->memory, nullptr);
		vkDestroySampler(vulkanContext.device, i->sampler, nullptr);
		vkDestroyImageView(vulkanContext.device, i->view, nullptr);
		vkDestroyImage(vulkanContext.device, i->image, nullptr);
	}
}

//=================================================
// Renderer Backend
//=================================================

void SklRendererBackend::CreateInstance()
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

void SklRendererBackend::CreateDevice()
{
	// Select a physical device
	uint32_t queueIndices[3];
	VkPhysicalDevice physicalDevice;
	ChoosePhysicalDevice(physicalDevice, queueIndices[0], queueIndices[1], queueIndices[2]);

	SKL_PRINT(
		SKL_DEBUG,
		"Queue indices :--: Graphics: %u :--: Present: %u :--: Transfer: %u",
		queueIndices[0],
		queueIndices[1],
		queueIndices[2]);

	vulkanContext.gpu.device = physicalDevice;

	vulkanContext.graphicsIdx = queueIndices[0];
	vulkanContext.presentIdx = queueIndices[1];
	vulkanContext.transferIdx = queueIndices[2];

	CreateLogicalDevice(
		{ queueIndices[0], queueIndices[1], queueIndices[2] },
		deviceExtensions,
		validationLayer);

	// Fill GPU details
	//=================================================
	VkPhysicalDevice& pysDevice = vulkanContext.gpu.device;
	SklPhysicalDeviceInfo_t& pdInfo = vulkanContext.gpu;

	uint32_t propertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(pysDevice, &propertyCount, nullptr);
	pdInfo.queueFamilyProperties.resize(propertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pysDevice, &propertyCount, pdInfo.queueFamilyProperties.data());

	vkGetPhysicalDeviceProperties(pysDevice, &pdInfo.properties);

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(pysDevice, nullptr, &extensionCount, nullptr);
	pdInfo.extensionProperties.resize(extensionCount);
	vkEnumerateDeviceExtensionProperties(pysDevice, nullptr, &extensionCount, pdInfo.extensionProperties.data());

	vkGetPhysicalDeviceFeatures(pysDevice, &pdInfo.features);
	vkGetPhysicalDeviceMemoryProperties(pysDevice, &pdInfo.memProperties);

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(pysDevice, surface, &presentModeCount, nullptr);
	pdInfo.presentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(pysDevice, surface, &presentModeCount, pdInfo.presentModes.data());

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pysDevice, surface, &pdInfo.surfaceCapabilities);

	uint32_t surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(pysDevice, surface, &surfaceFormatCount, nullptr);
	pdInfo.surfaceFormats.resize(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(pysDevice, surface, &surfaceFormatCount, pdInfo.surfaceFormats.data());
}

void SklRendererBackend::ChoosePhysicalDevice(
	VkPhysicalDevice& _selectedDevice,
	uint32_t& _graphicsIndex,
	uint32_t& _presentIndex,
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
		vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extensionCount, physicalDeviceExt.data());

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
			if (_graphicsIndex == _presentIndex || _graphicsIndex == _transferIndex || _presentIndex == _transferIndex)
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

uint32_t SklRendererBackend::GetQueueIndex(
	std::vector<VkQueueFamilyProperties>& _queues,
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

uint32_t SklRendererBackend::GetPresentIndex(
	const VkPhysicalDevice* _device,
	uint32_t _queuePropertyCount,
	uint32_t _graphicsIndex)
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
