
#include "pch.h"
#include <set>
#include <string>
#include <chrono>

#include "stb/stb_image.h"

#include "Renderer.h"
#include "Skeleton/Core/Common.h"
#include "Skeleton/Core/AssetLoader.h"
#include "Skeleton/Core/Vertex.h"

const std::vector<skeleton::Vertex> verts = {
	{{-0.5f,  0.5f, 0.0f}, {0.87843137255f, 0.54509803922f, 0.07843137255f}, {0.0f, 0.0f}},
	{{ 0.5f,  0.5f, 0.0f}, {0.07843137255f, 0.87843137255f, 0.54509803922f}, {1.0f, 0.0f}},
	{{ 0.5f, -0.5f, 0.0f}, {0.54509803922f, 0.07843137255f, 0.87843137255f}, {1.0f, 1.0f}},
	{{-0.5f, -0.5f, 0.0f}, {1.f, 1.f, 1.f}, {0.0f, 1.0f}},
	//{{-0.5f, -0.5f, -2.0f}, {0.87843137255f, 0.54509803922f, 0.07843137255f}, {0.0f, 1.0f}},
	//{{ 0.5f, -0.5f, -2.0f}, {0.07843137255f, 0.87843137255f, 0.54509803922f}, {1.0f, 1.0f}},
	//{{ 0.5f,  0.5f, -2.0f}, {0.54509803922f, 0.07843137255f, 0.87843137255f}, {1.0f, 0.0f}},
	//{{-0.5f,  0.5f, -2.0f}, {1.f, 1.f, 1.f}, {0.0f, 0.0f}},

	{{ -0.25f,  0.5f, 0.5f}, {1.f, 0.f, 0.f}, {0.0f, 0.0f}},
	{{  0.75f,  0.5f, 0.5f}, {0.f, 1.f, 0.f}, {1.0f, 0.0f}},
	{{  0.25f, -0.5f, 0.5f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f}},
	{{ -0.75f, -0.5f, 0.5f}, {1.f, 1.f, 1.f}, {0.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
	2, 1, 0,
	0, 3, 2,

	6, 5, 4,
	4, 7, 6
};

//////////////////////////////////////////////////////////////////////////
// Initialize & Cleanup
//////////////////////////////////////////////////////////////////////////

// Initializes the renderer in its entirety
skeleton::Renderer::Renderer()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		SKL_LOG("SDL ERROR", "%s", SDL_GetError());
		throw "SDL failure";
	}

	uint32_t sdlFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN | SDL_WINDOW_INPUT_FOCUS;
	window = SDL_CreateWindow(
		"Skeleton Application",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		800,
		600,
		sdlFlags);
	SDL_SetRelativeMouseMode(SDL_TRUE);
	SDL_SetWindowGrab(window, SDL_TRUE);

	CreateInstance();
	CreateDevice();
	bufferManager = new BufferManager(device);
	CreateCommandPools();

	CreateDescriptorSetLayout();

	CreateRenderer();

	CreateTextureImage("res/TestImage.png");
	CreateModelBuffers();
	CreateDescriptorPool();
	CreateDescriptorSet();
	CreateSyncObjects();
	CreateCommandBuffers();
	RecordCommandBuffers();

	SKL_PRINT("End Init", "-------------------------------------------------");

	bool stayOpen = true;
	SDL_Event e;

	static auto startTime = std::chrono::high_resolution_clock::now();

	float camSpeed = 2.f;
	float mouseSensativity = 0.1f;
	float pitch = 0.f, yaw = -90.f;

	glm::vec3 camFront = {0, 0, -1};
	glm::vec3 camPos = {0, 0, 3};

	auto prevTime = std::chrono::high_resolution_clock::now();

	while (stayOpen)
	{
		auto curTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(curTime - prevTime).count();

		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				stayOpen = false;
			}
			if (e.type == SDL_MOUSEMOTION)
			{
				yaw += e.motion.xrel * mouseSensativity;
				pitch -= e.motion.yrel * mouseSensativity;
				glm::clamp(pitch, -89.f, 89.f);

				camFront.x = (float)(glm::cos(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));
				camFront.y = (float)glm::sin(glm::radians(pitch));
				camFront.z = (float)(glm::sin(glm::radians(yaw)) * glm::cos(glm::radians(pitch)));
				camFront = glm::normalize(camFront);
			}
		}

		int count = 0;
		const Uint8* keys = SDL_GetKeyboardState(&count);
		if (keys[SDL_SCANCODE_W])
			camPos += camFront * camSpeed * deltaTime;
		if (keys[SDL_SCANCODE_S])
			camPos -= camFront * camSpeed * deltaTime;
		if (keys[SDL_SCANCODE_D])
			camPos += glm::normalize(glm::cross(camFront, { 0.f, 1.f, 0.f })) * camSpeed * deltaTime;
		if (keys[SDL_SCANCODE_A])
			camPos -= glm::normalize(glm::cross(camFront, { 0.f, 1.f, 0.f })) * camSpeed * deltaTime;
		if (keys[SDL_SCANCODE_E])
			camPos += glm::vec3(0.f, 1.f, 0.f) * camSpeed * deltaTime;
		if (keys[SDL_SCANCODE_Q])
			camPos -= glm::vec3(0.f, 1.f, 0.f) * camSpeed * deltaTime;

		mvp.model = glm::mat4(1.0f);
		//mvp.model = glm::rotate(glm::mat4(1.f), glm::radians(0.f), glm::vec3(0.f, 0.f, 1.f));
		mvp.view = glm::lookAt(camPos, camPos + camFront, glm::vec3(0.f, 1.f, 0.f));
		//mvp.view = glm::mat4(1.0f);
		mvp.proj = glm::perspective(glm::radians(45.f), swapchainExtent.width / (float)swapchainExtent.height, 0.1f, 10.f);
		mvp.proj[1][1] *= -1;

		bufferManager->FillBuffer(mvpMemory, &mvp, sizeof(mvp));

		RenderFrame();


		prevTime = curTime;
	}

	vkDeviceWaitIdle(*device);
}

// Cleans up all vulkan objects
skeleton::Renderer::~Renderer()
{
	vkFreeMemory(*device, textureMemory, nullptr);
	vkDestroySampler(*device, textureSampler, nullptr);
	vkDestroyImageView(*device, textureImageView, nullptr);
	vkDestroyImage(*device, textureImage, nullptr);

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
	VkDescriptorSetLayoutBinding mvpBufferBinding = {};
	mvpBufferBinding.binding = 0;
	mvpBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	mvpBufferBinding.descriptorCount = 1;
	mvpBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	mvpBufferBinding.pImmutableSamplers = nullptr;
	VkDescriptorSetLayoutBinding textureBinding = {};
	textureBinding.binding = 1;
	textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBinding.descriptorCount = 1;
	textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	textureBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding bindings[] = {mvpBufferBinding, textureBinding};
	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = 2;
	createInfo.pBindings = bindings;

	SKL_ASSERT_VK(
		vkCreateDescriptorSetLayout(*device, &createInfo, nullptr, &descriptorSetLayout),
		"Failed to create descriptor set layout");
}

void skeleton::Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 2;
	createInfo.pPoolSizes = poolSizes;
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

	// mvp buffer
	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = mvpBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	// Texture sampler
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.sampler = textureSampler;
	imageInfo.imageView = textureImageView;

	VkWriteDescriptorSet descWrites[2] = {};
	descWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrites[0].dstSet = descriptorSet;
	descWrites[0].dstBinding = 0;
	descWrites[0].dstArrayElement = 0;
	descWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descWrites[0].descriptorCount = 1;
	descWrites[0].pBufferInfo = &bufferInfo;
	descWrites[0].pImageInfo = nullptr;
	descWrites[0].pTexelBufferView = nullptr;

	descWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrites[1].dstSet = descriptorSet;
	descWrites[1].dstBinding = 1;
	descWrites[1].dstArrayElement = 0;
	descWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descWrites[1].descriptorCount = 1;
	descWrites[1].pBufferInfo = nullptr;
	descWrites[1].pImageInfo = &imageInfo;
	descWrites[1].pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(*device, 2, descWrites, 0, nullptr);

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

	CreateFrameBuffers();

	//RecordCommandBuffers();
}

void skeleton::Renderer::CleanupRenderer()
{
	for (uint32_t i = 0; i < (uint32_t)frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(*device, frameBuffers[i], nullptr);
	}

	SKL_PRINT_SLIM("Destory Depth ------------------------------");
	vkDestroyImage(*device, depthImage, nullptr);
	vkFreeMemory(*device, depthMemory, nullptr);
	vkDestroyImageView(*device, depthImageView, nullptr);

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
		swapchainImageViews[i] = CreateImageView(swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImages[i]);
	}
}

void skeleton::Renderer::CreateRenderpass()
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

	VkAttachmentDescription attachments[] = {colorDesc, depthDesc};
	VkRenderPassCreateInfo creteInfo = {};
	creteInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	creteInfo.attachmentCount = 2;
	creteInfo.pAttachments = attachments;
	creteInfo.subpassCount = 1;
	creteInfo.pSubpasses = &subpass;
	creteInfo.dependencyCount = 1;
	creteInfo.pDependencies = &dependency;

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
	//rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateInfo.cullMode = VK_CULL_MODE_NONE;
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
	depthStateInfo.depthWriteEnable = VK_TRUE;
	depthStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStateInfo.depthBoundsTestEnable = VK_FALSE;

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
	createInfo.pDepthStencilState = &depthStateInfo;
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
	VkFormat depthFormat = FindDepthFormat();
	CreateImage(
		swapchainExtent.width,
		swapchainExtent.height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage,
		depthMemory);

	depthImageView = CreateImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImage);
}

void skeleton::Renderer::CreateFrameBuffers()
{
	VkFramebufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = renderpass;
	createInfo.layers = 1;
	createInfo.width = swapchainExtent.width;
	createInfo.height = swapchainExtent.height;

	uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());
	frameBuffers.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		VkImageView attachments[] = {swapchainImageViews[i], depthImageView};
		createInfo.attachmentCount = 2;
		createInfo.pAttachments = attachments;

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

	VkClearValue clearValues[2] = {};
	clearValues[0].color = {0.20784313725f, 0.21568627451f, 0.21568627451f, 1.0f};
	clearValues[1].depthStencil = {1, 0};

	VkRenderPassBeginInfo rpBeginInfo = {};
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = renderpass;
	rpBeginInfo.clearValueCount = 2;
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

VkImageView skeleton::Renderer::CreateImageView(
	VkFormat _format,
	VkImageAspectFlags _aspect,
	const VkImage& _image)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.subresourceRange.levelCount   = 1;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.layerCount     = 1;
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

void skeleton::Renderer::CreateImage(
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
		vkCreateImage(*device, &createInfo, nullptr, &_image),
		"Failed to create texture image");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(*device, _image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = bufferManager->FindMemoryType(
		memRequirements.memoryTypeBits,
		_memFlags);

	SKL_ASSERT_VK(
		vkAllocateMemory(*device, &allocInfo, nullptr, &_memory),
		"Failed to allocate texture memory");

	vkBindImageMemory(*device, _image, _memory, 0);
}

void skeleton::Renderer::CreateTextureImage(
	const char* _directory)
{
	// Image loading
	//=================================================
	int width, height, channels;
	stbi_uc* img = stbi_load(_directory, &width, &height, &channels, STBI_rgb_alpha);
	VkDeviceSize size = width * height * 4;

	assert(img != nullptr);

	// Staging buffer
	//=================================================

	VkDeviceMemory stagingMemory;
	VkBuffer stagingBuffer;

	uint32_t stagingIndex =	bufferManager->CreateBuffer(
		stagingBuffer,
		stagingMemory,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	bufferManager->FillBuffer(stagingMemory, img, size);

	stbi_image_free(img);

	// Image creation
	//=================================================

	CreateImage(
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		textureImage,
		textureMemory);

	TransitionImageLayout(
		textureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(
		stagingBuffer,
		textureImage,
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height));
	TransitionImageLayout(
		textureImage,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	bufferManager->RemoveAtIndex(stagingIndex);

	textureImageView = CreateImageView(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, textureImage);

	CreateSampler();
}

void skeleton::Renderer::CreateSampler()
{
	VkSamplerCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	VkPhysicalDeviceProperties physicalProps = {};
	vkGetPhysicalDeviceProperties(device->physicalDevice, &physicalProps);
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.0f;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 0.0f;

	// TODO : Make this conditional based on the physicalDevice's capabilities
	// (Includes physical device selection)
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.maxAnisotropy = physicalProps.limits.maxSamplerAnisotropy;

	SKL_ASSERT_VK(
		vkCreateSampler(*device, &createInfo, nullptr, &textureSampler),
		"Failed to create texture sampler");
}

void skeleton::Renderer::TransitionImageLayout(
	VkImage _iamge,
	VkFormat _format,
	VkImageLayout _old,
	VkImageLayout _new)
{
	VkCommandBuffer transitionCommand = device->BeginSingleTimeCommand(graphicsPool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = _old;
	barrier.newLayout = _new;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = _iamge;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;

	VkPipelineStageFlagBits srcStage;
	VkPipelineStageFlagBits dstStage;


	if (_old == VK_IMAGE_LAYOUT_UNDEFINED && _new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		SKL_LOG(SKL_ERROR, "Not a handled image transition");
	}

	vkCmdPipelineBarrier(transitionCommand, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	device->EndSingleTimeCommand(transitionCommand, graphicsPool, device->graphicsQueue);
}

void skeleton::Renderer::CopyBufferToImage(
	VkBuffer _buffer,
	VkImage _image,
	uint32_t _width,
	uint32_t _height)
{
	VkCommandBuffer command = device->BeginSingleTimeCommand(device->transientPool);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.baseArrayLayer = 0;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {_width, _height, 1};

	vkCmdCopyBufferToImage(
		command,
		_buffer,
		_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	device->EndSingleTimeCommand(command, device->transientPool, device->transferQueue);
}

VkFormat skeleton::Renderer::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat skeleton::Renderer::FindSupportedFormat(
	const std::vector<VkFormat>& _candidates,
	VkImageTiling _tiling,
	VkFormatFeatureFlags _features)
{
	for (VkFormat format : _candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &properties);

		if (_tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & _features) == _features)
			return format;
		else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & _features) == _features)
			return format;
	}
	throw std::runtime_error("Failed to find a suitable format");
}

