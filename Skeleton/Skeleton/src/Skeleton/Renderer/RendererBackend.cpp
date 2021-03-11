
#include "pch.h"
#include "RendererBackend.h"
#include "Skeleton/Core/Common.h"

BufferManager::~BufferManager()
{
	vkDeviceWaitIdle(vulkanContext.device);

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

SklRendererBackend::SklRendererBackend(
	SDL_Window* _window,
	const std::vector<const char*>& _extraExtensions) : window(_window)
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

SklRendererBackend::~SklRendererBackend()
{
	vkDeviceWaitIdle(vulkanContext.device);

	vkDestroyDescriptorPool(vulkanContext.device, descriptorPool, nullptr);

	CleanupRenderComponents();

	vkDestroyCommandPool(vulkanContext.device, graphicsCommandPool, nullptr);

	vulkanContext.Cleanup();
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

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

void SklRendererBackend::CreateCommandPool()
{
	SklCreateCommandPool(graphicsCommandPool, vulkanContext.graphicsIdx);
}

//=================================================
// Renderer
//=================================================

void SklRendererBackend::InitializeRenderComponents()
{
	CreateRenderComponents();

	CreateDescriptorPool();
	CreateSyncObjects();
	CreateCommandBuffers();
}

void SklRendererBackend::CreateRenderComponents()
{
	CreateSwapchain();
	CreateRenderpass();
	CreateDepthImage();
	CreateFramebuffers();
}

void SklRendererBackend::CleanupRenderComponents()
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
	vkDestroyImage(vulkanContext.device, depthImage, nullptr);
	vkFreeMemory(vulkanContext.device, depthMemory, nullptr);
	vkDestroyImageView(vulkanContext.device, depthImageView, nullptr);

	// Destroy Renderpass
	//vkDestroyRenderPass(vulkanContext.device, renderpass)

	// Destroy Swapchain
	for (const auto& view : swapchainImageViews)
	{
		vkDestroyImageView(vulkanContext.device, view, nullptr);
	}
	vkDestroySwapchainKHR(vulkanContext.device, swapchain, nullptr);
}

void SklRendererBackend::RecreateRenderComponents()
{
	vkDeviceWaitIdle(vulkanContext.device);

	CleanupRenderComponents();
	CreateRenderComponents();
}

//=================================================
// Renderer Create Functions
//=================================================

void SklRendererBackend::CreateSwapchain()
{
	// Find the best surface format
	VkSurfaceFormatKHR formatInfo = vulkanContext.gpu.surfaceFormats[0];
	for (const auto& p : vulkanContext.gpu.surfaceFormats)
	{
		if (p.format == VK_FORMAT_R32G32B32A32_SFLOAT && p.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT)
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
		SDL_GetWindowSize(window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));
		extent.width = glm::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = glm::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
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
		swapchainImageViews[i] = CreateImageView(swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImages[i]);
	}
}

void SklRendererBackend::CreateRenderpass()
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

void SklRendererBackend::CreateDepthImage()
{
	VkFormat depthFormat = FindDepthFormat();
	CreateImage(
		vulkanContext.renderExtent.width,
		vulkanContext.renderExtent.height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage,
		depthMemory);

	depthImageView = CreateImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImage);
}

void SklRendererBackend::CreateFramebuffers()
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
		VkImageView attachments[] = { swapchainImageViews[i], depthImageView };
		createInfo.attachmentCount = 2;
		createInfo.pAttachments = attachments;

		SKL_ASSERT_VK(
			vkCreateFramebuffer(vulkanContext.device, &createInfo, nullptr, &frameBuffers[i]),
			"Failed to create a framebuffer");
	}
}

void SklRendererBackend::CreateDescriptorPool()
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

void SklRendererBackend::CreateSyncObjects()
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
			vkCreateFence(vulkanContext.device, &fenceInfo, nullptr, &flightFences[i]),
			"Failed to create sync fence");
		SKL_ASSERT_VK(
			vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
			"Failed to create image semaphore");
		SKL_ASSERT_VK(
			vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]),
			"Failed to create render semaphore");
	}
}

void SklRendererBackend::CreateCommandBuffers()
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

// Renderer
//=================================================

void SklRendererBackend::CreateImage(
	uint32_t _width,
	uint32_t _height,
	VkFormat _format,
	VkImageTiling _tiling,
	VkImageUsageFlags _usage,
	VkMemoryPropertyFlags _memFlags,
	VkImage& _image,
	VkDeviceMemory& _memory)
{
	VkImageCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.extent.width = _width;
	createInfo.extent.height = _height;
	createInfo.extent.depth = 1;
	createInfo.mipLevels = 1;
	createInfo.arrayLayers = 1;
	createInfo.format = _format;
	createInfo.tiling = _tiling;
	createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	createInfo.usage = _usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	createInfo.flags = 0;

	SKL_ASSERT_VK(
		vkCreateImage(vulkanContext.device, &createInfo, nullptr, &_image),
		"Failed to create texture image");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vulkanContext.device, _image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = bufferManager->FindMemoryType(
		memRequirements.memoryTypeBits,
		_memFlags);

	SKL_ASSERT_VK(
		vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &_memory),
		"Failed to allocate texture memory");

	vkBindImageMemory(vulkanContext.device, _image, _memory, 0);
}

VkImageView SklRendererBackend::CreateImageView(
	const VkFormat _format,
	VkImageAspectFlags _aspect,
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

VkFormat SklRendererBackend::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat SklRendererBackend::FindSupportedFormat(
	const std::vector<VkFormat>& _candidates,
	VkImageTiling _tiling,
	VkFormatFeatureFlags _features)
{
	for (VkFormat format : _candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(vulkanContext.gpu.device, format, &properties);

		if (_tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & _features) == _features)
			return format;
		else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & _features) == _features)
			return format;
	}
	throw std::runtime_error("Failed to find a suitable format");
}

