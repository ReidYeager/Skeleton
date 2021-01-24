
#include "pch.h"
#include <set>
#include <string>
#include <chrono>

#include "Renderer.h"
#include "Skeleton/Core/Common.h"
#include "Skeleton/Core/AssetLoader.h"

#include "Skeleton/Core/Vertex.h"

const std::vector<skeleton::Vertex> verts = {
	{{-0.25f, -0.5f, 1.0f}, {0.87843137255f, 0.54509803922f, 0.07843137255f}},
	{{ 0.25f,  0.5f, 1.0f}, {0.07843137255f, 0.87843137255f, 0.54509803922f}},
	{{-0.75f,  0.5f, 1.0f}, {0.54509803922f, 0.07843137255f, 0.87843137255f}},
	{{ 0.75f, -0.5f, 1.0f}, {1.f, 1.f, 1.f}},

	{{ 0.25f, -0.5f, 0.5f}, {1.f, 0.f, 0.f}},
	{{-0.25f,  0.5f, 0.5f}, {0.f, 1.f, 0.f}},
	{{ 0.75f,  0.5f, 0.5f}, {0.f, 0.f, 1.f}},
	{{-0.75f, -0.5f, 0.5f}, {1.f, 1.f, 1.f}}
};

const std::vector<uint32_t> indices = {
	0, 1, 2,
	0, 3, 1,

	4, 6, 5,
	4, 5, 7
};

//////////////////////////////////////////////////////////////////////////
// Initialize & Cleanup
//////////////////////////////////////////////////////////////////////////

// Initializes the renderer in its entirety
skeleton::Renderer::Renderer()
{
	uint32_t sdlFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS;
	window = SDL_CreateWindow(
		"Skeleton Application",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		800,
		600,
		sdlFlags);

	CreateInstance();
	CreateDevice();
	bufferManager = new BufferManager(device);
	CreateCommandPools();

	CreateDescriptorSetLayout();

	CreateRenderer();

	CreateModelBuffers();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateSyncObjects();
	CreateCommandBuffers();
	RecordCommandBuffers();

	SKL_PRINT(End Init, "-------------------------------------------------");

	bool stayOpen = true;
	SDL_Event e;

	static auto startTime = std::chrono::high_resolution_clock::now();

	while (stayOpen)
	{
		auto curTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();

		mvp.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
		mvp.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
		mvp.proj = glm::perspective(glm::radians(45.f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.f);
		mvp.proj[1][1] *= -1;

		bufferManager->FillBuffer(mvpMemory, &mvp, sizeof(mvp));

		RenderFrame();

		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				stayOpen = false;
			}
		}
	}

	vkDeviceWaitIdle(*device);
}

// Cleans up all vulkan objects
skeleton::Renderer::~Renderer()
{
	for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
	{
		vkDestroyFence(*device, flightFences[i], nullptr);
		vkDestroySemaphore(*device, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(*device, renderCompleteSemaphores[i], nullptr);
	}

	vkDestroyDescriptorSetLayout(*device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(*device, descriptorPool, nullptr);

	CleanupRenderer();

	vkDestroyCommandPool(*device, graphicsPool, nullptr);

	delete(bufferManager);
	delete(device);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	SDL_DestroyWindow(window);
}

//////////////////////////////////////////////////////////////////////////
// RenderFrame
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::RenderFrame()
{
	vkWaitForFences(*device, 1, &flightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(*device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (imageIsInFlightFences[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(*device, 1, &imageIsInFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
	}
	imageIsInFlightFences[imageIndex] = flightFences[currentFrame];

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];

	vkResetFences(*device, 1, &flightFences[currentFrame]);

	SKL_ASSERT_VK(
		vkQueueSubmit(device->graphicsQueue, 1, &submitInfo, flightFences[currentFrame]),
		"Failed to submit draw command");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(device->presentQueue, &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FLIGHT_IMAGE_COUNT;
}

void skeleton::Renderer::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding mvpBinding = {};
	mvpBinding.binding = 0;
	mvpBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpBinding.descriptorCount = 1;
	mvpBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	mvpBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = 1;
	createInfo.pBindings = &mvpBinding;

	SKL_ASSERT_VK(
		vkCreateDescriptorSetLayout(*device, &createInfo, nullptr, &descriptorSetLayout),
		"Failed to create descriptor set layout");
}

void skeleton::Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 1;
	createInfo.pPoolSizes = &poolSize;
	createInfo.maxSets = 1;

	SKL_ASSERT_VK(
		vkCreateDescriptorPool(*device, &createInfo, nullptr, &descriptorPool),
		"Failed to create descriptor pool");
}

void skeleton::Renderer::CreateDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &descriptorSetLayout;

	SKL_ASSERT_VK(
		vkAllocateDescriptorSets(*device, &allocInfo, &descriptorSet),
		"Failed to allocate descriptor set");

	// 
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = mvpBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet descWrite = {};
	descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrite.dstSet = descriptorSet;
	descWrite.dstBinding = 0;
	descWrite.dstArrayElement = 0;
	descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descWrite.descriptorCount = 1;
	descWrite.pBufferInfo = &bufferInfo;
	descWrite.pImageInfo = nullptr;
	descWrite.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(*device, 1, &descWrite, 0, nullptr);

}

//////////////////////////////////////////////////////////////////////////
// CreateRenderer
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::CreateRenderer()
{
	CreateSwapchain();
	CreateRenderpass();

	CreateDepthImage();

	CreatePipelineLayout();
	CreatePipeline();

	CreateDepthImage();
	CreateFrameBuffers();

	//RecordCommandBuffers();
}

void skeleton::Renderer::CleanupRenderer()
{
	for (uint32_t i = 0; i < (uint32_t)frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(*device, frameBuffers[i], nullptr);
	}

	vkDestroyPipeline(*device, pipeline, nullptr);
	vkDestroyPipelineLayout(*device, pipelineLayout, nullptr);
	vkDestroyRenderPass(*device, renderpass, nullptr);

	for (const auto& view : swapchainImageViews)
	{
		vkDestroyImageView(*device, view, nullptr);
	}

	vkDestroySwapchainKHR(*device, swapchain, nullptr);
}

void skeleton::Renderer::RecreateRenderer()
{
	vkDeviceWaitIdle(*device);
	CleanupRenderer();
	CreateRenderer();
}

//////////////////////////////////////////////////////////////////////////
// Initialization
//////////////////////////////////////////////////////////////////////////

// Creates the vkInstance & vkSurface
void skeleton::Renderer::CreateInstance()
{
	// Fill instance extensions & layers
	uint32_t sdlExtensionCount;
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, nullptr);
	std::vector<const char*> sdlExtensions(sdlExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, sdlExtensions.data());
	for (const auto& sdlExt : sdlExtensions)
	{
		instanceExtensions.push_back(sdlExt);
	}

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

// Gets a physical device & creates the vkDevice
void skeleton::Renderer::CreateDevice()
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

	device = new VulkanDevice(physicalDevice);

	device->queueIndices.graphics = queueIndices[0];
	device->queueIndices.present  = queueIndices[1];
	device->queueIndices.transfer = queueIndices[2];

	device->CreateLogicalDevice(
		{queueIndices[0], queueIndices[1], queueIndices[2]},
		deviceExtensions,
		validationLayer);

}

// Determines the best physical device for rendering
void skeleton::Renderer::ChoosePhysicalDevice(
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

		for (const auto& ext : physicalDeviceExt)
		{
			requiredExtensionSet.erase(ext.extensionName);
		}

		_graphicsIndex = GetQueueIndex(queueProperties, VK_QUEUE_GRAPHICS_BIT);
		_transferIndex = GetQueueIndex(queueProperties, VK_QUEUE_TRANSFER_BIT);
		_presentIndex = GetPresentIndex(&pdevice, propertyCount, _graphicsIndex);

		if (
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

void skeleton::Renderer::CreateCommandPools()
{
	device->CreateCommandPool(graphicsPool, device->queueIndices.graphics);
}

void skeleton::Renderer::CreateSyncObjects()
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
			vkCreateFence(*device, &fenceInfo, nullptr, &flightFences[i]),
			"Failed to create sync fence");
		SKL_ASSERT_VK(
			vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
			"Failed to create image semaphore");
		SKL_ASSERT_VK(
			vkCreateSemaphore(*device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]),
			"Failed to create render semaphore");
	}
}

void skeleton::Renderer::CreateCommandBuffers()
{
	uint32_t bufferCount = static_cast<uint32_t>(swapchainImages.size());
	commandBuffers.resize(bufferCount);

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = bufferCount;
	allocInfo.commandPool = graphicsPool;

	SKL_ASSERT_VK(
		vkAllocateCommandBuffers(*device, &allocInfo, commandBuffers.data()),
		"Failed to allocate command buffers");
}

//////////////////////////////////////////////////////////////////////////
// CreateRenderer Functions
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::CreateSwapchain()
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(device->physicalDevice, &props);
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(device->physicalDevice, &features);

	// Find the best format
	uint32_t surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &surfaceFormatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());

	VkSurfaceFormatKHR formatInfo = surfaceFormats[0];
	for (const auto& p : surfaceFormats)
	{
		if (p.format == VK_FORMAT_R32G32B32A32_SFLOAT && p.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT)
		{
			formatInfo = p;
			break;
		}
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->physicalDevice, surface, &presentModeCount, presentModes.data());

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& p : presentModes)
	{
		if (p == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = p;
			break;
		}
	}

	// Get the device's extent
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->physicalDevice, surface, &capabilities);
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
	if (device->queueIndices.graphics != device->queueIndices.present)
	{
		uint32_t sharedIndices[] = { device->queueIndices.graphics, device->queueIndices.present };
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
		vkCreateSwapchainKHR(*device, &createInfo, nullptr, &swapchain),
		"Failed to create swapchain");

	swapchainFormat = formatInfo.format;
	swapchainExtent = extent;

	vkGetSwapchainImagesKHR(*device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(*device, swapchain, &imageCount, swapchainImages.data());

	swapchainImageViews.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++)
	{
		swapchainImageViews[i] = CreateImageView(swapchainFormat, swapchainImages[i]);
	}
}

void skeleton::Renderer::CreateRenderpass()
{
	VkAttachmentDescription colorDesc = {};
	colorDesc.format = swapchainFormat;
	colorDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	colorDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	//colorDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentReference colorRef;
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkRenderPassCreateInfo creteInfo = {};
	creteInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	creteInfo.attachmentCount = 1;
	creteInfo.pAttachments = &colorDesc;
	creteInfo.subpassCount = 1;
	creteInfo.pSubpasses = &subpass;

	SKL_ASSERT_VK(
		vkCreateRenderPass(*device, &creteInfo, nullptr, &renderpass),
		"Failed to create renderpass");
}

void skeleton::Renderer::CreatePipelineLayout()
{
	//Pipeline Layout/////////////////////////////////////////////////////////
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 0;
	layoutInfo.pPushConstantRanges = nullptr;

	SKL_ASSERT_VK(
		vkCreatePipelineLayout(*device, &layoutInfo, nullptr, &pipelineLayout),
		"Failed to create pipeline layout");
}

void skeleton::Renderer::CreatePipeline()
{
	// Viewport State
	//=================================================
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)swapchainExtent.width;
	viewport.height = (float)swapchainExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissor = {};
	scissor.extent = swapchainExtent;
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;

	// Vert Input State
	//=================================================
	const auto vertexInputBindingDesc = skeleton::Vertex::GetBindingDescription();
	const auto vertexInputAttribDescs = skeleton::Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttribDescs.size());
	vertexInputStateInfo.pVertexAttributeDescriptions = vertexInputAttribDescs.data();
	vertexInputStateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateInfo.pVertexBindingDescriptions = &vertexInputBindingDesc;

	// Input Assembly
	//=================================================
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = {};
	inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

	// Rasterizer
	//=================================================
	VkPipelineRasterizationStateCreateInfo rasterStateInfo = {};
	rasterStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateInfo.rasterizerDiscardEnable = VK_TRUE;
	rasterStateInfo.lineWidth = 1.f;
	rasterStateInfo.depthBiasEnable = VK_FALSE;
	rasterStateInfo.depthClampEnable = VK_FALSE;
	rasterStateInfo.rasterizerDiscardEnable = VK_FALSE;

	// Multisample State
	//=================================================
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {};
	multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateInfo.sampleShadingEnable = VK_FALSE;

	// Depth State
	//=================================================
	VkPipelineDepthStencilStateCreateInfo depthStateInfo = {};
	depthStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStateInfo.depthTestEnable = VK_TRUE;
	depthStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;

	// Color Blend State
	//=================================================
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendStateInfo = {};
	blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateInfo.logicOpEnable = VK_FALSE;
	blendStateInfo.attachmentCount = 1;
	blendStateInfo.pAttachments = &blendAttachmentState;

	// Dynamic states
	//=================================================
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 0;
	dynamicStateInfo.pDynamicStates = nullptr;

	// Shader modules
	//=================================================
	VkShaderModule vertModule = CreateShaderModule("res\\default_vert.spv");
	VkShaderModule fragModule = CreateShaderModule("res\\default_frag.spv");

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Pipeline
	//=================================================
	VkGraphicsPipelineCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pViewportState = &viewportStateInfo;
	createInfo.pVertexInputState = &vertexInputStateInfo;
	createInfo.pInputAssemblyState = &inputAssemblyStateInfo;
	createInfo.pRasterizationState = &rasterStateInfo;
	createInfo.pMultisampleState = &multisampleStateInfo;
	createInfo.pDepthStencilState = nullptr; //&depthStateInfo;
	createInfo.pColorBlendState = &blendStateInfo;
	createInfo.pDynamicState = &dynamicStateInfo;

	createInfo.stageCount = 2;
	createInfo.pStages = shaderStages;

	createInfo.layout = pipelineLayout;
	createInfo.renderPass = renderpass;

	SKL_ASSERT_VK(
		vkCreateGraphicsPipelines(*device, nullptr, 1, &createInfo, nullptr, &pipeline),
		"Failed to create graphics pipeline");

	vkDestroyShaderModule(*device, vertModule, nullptr);
	vkDestroyShaderModule(*device, fragModule, nullptr);
}

void skeleton::Renderer::CreateDepthImage()
{

}

void skeleton::Renderer::CreateFrameBuffers()
{
	VkFramebufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = renderpass;
	createInfo.layers = 1;
	createInfo.attachmentCount = 1;
	createInfo.width = swapchainExtent.width;
	createInfo.height = swapchainExtent.height;

	uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());
	frameBuffers.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		createInfo.pAttachments = &swapchainImageViews[i];

		SKL_ASSERT_VK(
			vkCreateFramebuffer(*device, &createInfo, nullptr, &frameBuffers[i]),
			"Failed to create a framebuffer");
	}
}

void skeleton::Renderer::RecordCommandBuffers()
{
	uint32_t comandCount = static_cast<uint32_t>(commandBuffers.size());
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkClearValue clearValues[1] = {};
	clearValues[0].color = {0.20784313725f, 0.21568627451f, 0.21568627451f, 1.0f};
	//clearValues[1].depthStencil = {1, 0};

	VkRenderPassBeginInfo rpBeginInfo = {};
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = renderpass;
	rpBeginInfo.clearValueCount = 1;
	rpBeginInfo.pClearValues = clearValues;
	rpBeginInfo.renderArea.extent = swapchainExtent;
	rpBeginInfo.renderArea.offset = {0, 0};

	for (uint32_t i = 0; i < comandCount; i++)
	{
		rpBeginInfo.framebuffer = frameBuffers[i];

		SKL_ASSERT_VK(
			vkBeginCommandBuffer(commandBuffers[i], &beginInfo),
			"Failed to begin a command buffer");

		vkCmdBeginRenderPass(commandBuffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		VkDeviceSize offset[] = {0};
		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertBuffer, offset);
		vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);


		vkCmdEndRenderPass(commandBuffers[i]);

		SKL_ASSERT_VK(
			vkEndCommandBuffer(commandBuffers[i]),
			"Failed to end command buffer");
	}
}

//=================================================
// Helpers
//=================================================

// Returns the first instance of a queue with the input flags
uint32_t skeleton::Renderer::GetQueueIndex(
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

// Returns the first instance of a presentation queue
uint32_t skeleton::Renderer::GetPresentIndex(
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

void skeleton::Renderer::CreateImage(
	const VkExtent2D _extent,
	const VkFormat _format,
	const VkImageLayout _layout,
	VkImage& _image,
	VkImageView& _view)
{
	VkImageCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	createInfo.arrayLayers = 1;
	createInfo.extent.width = _extent.width;
	createInfo.extent.height = _extent.height;
	createInfo.format = _format;
	createInfo.imageType = VK_IMAGE_TYPE_2D;
	createInfo.initialLayout = _layout;
	createInfo.mipLevels = 1;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.queueFamilyIndexCount = 1;

	SKL_ASSERT_VK(
		vkCreateImage(*device, &createInfo, nullptr, &_image),
		"Failed to create image");

	_view = CreateImageView(_format, _image);
}

VkImageView skeleton::Renderer::CreateImageView(
	const VkFormat _format,
	const VkImage& _image)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.subresourceRange.levelCount   = 1;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.layerCount     = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.image = _image;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.format = _format;

	VkImageView tmpView;
	SKL_ASSERT_VK(
		vkCreateImageView(*device, &createInfo, nullptr, &tmpView),
		"Failed to create image view");

	return tmpView;
}

VkShaderModule skeleton::Renderer::CreateShaderModule(
	const char* _directory)
{
	std::vector<char> shaderSource = skeleton::tools::LoadFileAsString(_directory);
	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = shaderSource.size();
	moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSource.data());
	VkShaderModule tmpModule;
	SKL_ASSERT_VK(
		vkCreateShaderModule(*device, &moduleCreateInfo, nullptr, &tmpModule),
		"Failed to create shader module");

	return tmpModule;
}

void skeleton::Renderer::CreateModelBuffers()
{
	// Create vertex buffer
	bufferManager->CreateAndFillBuffer(
		vertBuffer,
		vertMemory,
		verts.data(),
		sizeof(verts[0]) * verts.size(),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	// Create index buffer
	bufferManager->CreateAndFillBuffer(
		indexBuffer,
		indexMemory,
		indices.data(),
		sizeof(indices[0]) * indices.size(),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	// Create uniform buffer
	VkDeviceSize mvpSize = sizeof(MVPMatrices);
	bufferManager->CreateBuffer(
		mvpBuffer,
		mvpMemory,
		mvpSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

