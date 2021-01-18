
#include "pch.h"
#include <set>
#include <string>

#include "glm/glm.hpp"

#include "Renderer.h"
#include "../Core/Common.h"
#include "../Core/AssetLoader.h"

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
	CreateCommandPools();

	CreateRenderer();

	CreateSyncObjects();
	CreateCommandBuffers();
	RecordCommandBuffers();

	SKL_PRINT(End Init, "-------------------------------------------------");

	bool stayOpen = true;
	SDL_Event e;

	while (stayOpen)
	{
		RenderFrame();

		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				stayOpen = false;
			}
		}
	}

	vkDeviceWaitIdle(device);
}

// Cleans up all vulkan objects
skeleton::Renderer::~Renderer()
{
	for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
	{
		vkDestroyFence(device, flightFences[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device, renderCompleteSemaphores[i], nullptr);
	}

	CleanupRenderer();

	vkDestroyCommandPool(device, graphicsPool, nullptr);

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);

	SDL_DestroyWindow(window);
}

//////////////////////////////////////////////////////////////////////////
// RenderFrame
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::RenderFrame()
{
	vkWaitForFences(device, 1, &flightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (imageIsInFlightFences[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(device, 1, &imageIsInFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
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

	vkResetFences(device, 1, &flightFences[currentFrame]);

	SKL_ASSERT_VK(
		vkQueueSubmit(graphicsQueue, 1, &submitInfo, flightFences[currentFrame]),
		"Failed to submit draw command");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(presentQueue, &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FLIGHT_IMAGE_COUNT;
}

//////////////////////////////////////////////////////////////////////////
// CreateRenderer
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::CreateRenderer()
{
	CreateSwapchain();
	CreateRenderpass();

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
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}

	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyRenderPass(device, renderpass, nullptr);

	for (const auto& view : swapchainImageViews)
	{
		vkDestroyImageView(device, view, nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void skeleton::Renderer::RecreateRenderer()
{
	vkDeviceWaitIdle(device);
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
	uint32_t queueCount = 3;
	uint32_t queueIndices[3];
	ChoosePhysicalDevice(physicalDevice, queueIndices[0], queueIndices[1], queueIndices[2]);
	graphicsQueueIndex = queueIndices[0];
	presentQueueIndex = queueIndices[1];
	transferQueueIndex = queueIndices[2];

	SKL_PRINT(
		SKL_DEBUG,
		"Queue indices :--: Graphics: %u :--: Present: %u :--: Transfer: %u",
		queueIndices[0],
		queueIndices[1],
		queueIndices[2]);

	// Create the logical device
	VkPhysicalDeviceFeatures enabledFeatures = {};

	const float priority = 1.f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueCount);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[i].queueFamilyIndex = queueIndices[i];
		queueCreateInfos[i].queueCount = 1;
		queueCreateInfos[i].pQueuePriorities = &priority;
	}

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &enabledFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayer.size());
	createInfo.ppEnabledLayerNames = validationLayer.data();
	createInfo.queueCreateInfoCount = queueCount;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	SKL_ASSERT_VK(
		vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
		"Failed to create logical device");

	vkGetDeviceQueue(device, queueIndices[0], 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueIndices[1], 0, &presentQueue);
	vkGetDeviceQueue(device, queueIndices[2], 0, &transferQueue);
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
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = graphicsQueueIndex;

	SKL_ASSERT_VK(
		vkCreateCommandPool(device, &createInfo, nullptr, &graphicsPool),
		"Failed to create graphics command pool");
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
			vkCreateFence(device, &fenceInfo, nullptr, &flightFences[i]),
			"Failed to create sync fence");
		SKL_ASSERT_VK(
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
			"Failed to create image semaphore");
		SKL_ASSERT_VK(
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]),
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
		vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
		"Failed to allocate command buffers");
}

//////////////////////////////////////////////////////////////////////////
// CreateRenderer Functions
//////////////////////////////////////////////////////////////////////////

void skeleton::Renderer::CreateSwapchain()
{
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(physicalDevice, &props);
	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(physicalDevice, &features);

	// Find the best format
	uint32_t surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());

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
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

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
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
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
	if (graphicsQueueIndex != presentQueueIndex)
	{
		uint32_t sharedIndices[] = {graphicsQueueIndex, presentQueueIndex};
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
		vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain),
		"Failed to create swapchain");

	swapchainFormat = formatInfo.format;
	swapchainExtent = extent;

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

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
		vkCreateRenderPass(device, &creteInfo, nullptr, &renderpass),
		"Failed to create renderpass");
}

void skeleton::Renderer::CreatePipelineLayout()
{
	//Pipeline Layout/////////////////////////////////////////////////////////
	VkPipelineLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 0;
	layoutInfo.pSetLayouts = nullptr;
	layoutInfo.pushConstantRangeCount = 0;
	layoutInfo.pPushConstantRanges = nullptr;

	SKL_ASSERT_VK(
		vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout),
		"Failed to create pipeline layout");
}

void skeleton::Renderer::CreatePipeline()
{
	//Viewport State//////////////////////////////////////////////////////////
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

	//Vert Input State////////////////////////////////////////////////////////
	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateInfo.pVertexAttributeDescriptions = nullptr;
	vertexInputStateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateInfo.pVertexBindingDescriptions = nullptr;

	//Input Assembly//////////////////////////////////////////////////////////
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = {};
	inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

	//Rasterizer//////////////////////////////////////////////////////////////
	VkPipelineRasterizationStateCreateInfo rasterStateInfo = {};
	rasterStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateInfo.rasterizerDiscardEnable = VK_TRUE;
	rasterStateInfo.lineWidth = 1.f;
	rasterStateInfo.depthBiasEnable = VK_FALSE;
	rasterStateInfo.depthClampEnable = VK_FALSE;
	rasterStateInfo.rasterizerDiscardEnable = VK_FALSE;

	//Multisample State///////////////////////////////////////////////////////
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {};
	multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateInfo.sampleShadingEnable = VK_FALSE;

	//Depth State/////////////////////////////////////////////////////////////
	VkPipelineDepthStencilStateCreateInfo depthStateInfo = {};

	//Color Blend State///////////////////////////////////////////////////////
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendStateInfo = {};
	blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateInfo.logicOpEnable = VK_FALSE;
	blendStateInfo.attachmentCount = 1;
	blendStateInfo.pAttachments = &blendAttachmentState;

	//Dynamic states//////////////////////////////////////////////////////////
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 0;
	dynamicStateInfo.pDynamicStates = nullptr;

	//Shader modules//////////////////////////////////////////////////////////
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

	//Pipeline////////////////////////////////////////////////////////////////
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
		vkCreateGraphicsPipelines(device, nullptr, 1, &createInfo, nullptr, &pipeline),
		"Failed to create graphics pipeline");

	vkDestroyShaderModule(device, vertModule, nullptr);
	vkDestroyShaderModule(device, fragModule, nullptr);
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

	uint32_t imageCount = swapchainImageViews.size();
	frameBuffers.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		createInfo.pAttachments = &swapchainImageViews[i];

		SKL_ASSERT_VK(
			vkCreateFramebuffer(device, &createInfo, nullptr, &frameBuffers[i]),
			"Failed to create a framebuffer");
	}
}

void skeleton::Renderer::RecordCommandBuffers()
{
	uint32_t comandCount = static_cast<uint32_t>(commandBuffers.size());
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkClearValue clearValues[1] = {};
	clearValues[0].color = {0.0f, 1.0f, 0.0f, 1.0f};
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
		vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		SKL_ASSERT_VK(
			vkEndCommandBuffer(commandBuffers[i]),
			"Failed to end command buffer");
	}
}

//////////////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////////////

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

//VkImage skeleton::Renderer::CreateImage()
//{
//	VkImageCreateInfo createInfo = {};
//	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//	createInfo.arrayLayers = 1;
//	createInfo.extent = _extent;
//	createInfo.format = _format;
//	createInfo.imageType = VK_IMAGE_TYPE_2D;
//	createInfo.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//	createInfo.mipLevels = 1;
//	createInfo.pQueueFamilyIndices = nullptr;
//	createInfo.queueFamilyIndexCount = 1;
//
//	VkImage tmpImage;
//	SKL_ASSERT_VK(
//		vkCreateImage(device, &createInfo, nullptr, &tmpImage),
//		"Failed to create image");
//	return tmpImage;
//}

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
		vkCreateImageView(device, &createInfo, nullptr, &tmpView),
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
		vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &tmpModule),
		"Failed to create shader module");

	return tmpModule;
}


