
#include "pch.h"
#include <set>
#include <string>

#include "glm/glm.hpp"

#include "Renderer.h"
#include "../Core/Common.h"

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

	bool stayOpen = true;
	SDL_Event e;

	while (stayOpen)
	{
		while (SDL_PollEvent(&e) != 0)
		{
			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
			{
				stayOpen = false;
			}
		}
	}
}

// Cleans up all vulkan objects
skeleton::Renderer::~Renderer()
{
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

	RecordCommandBuffers();
}

void skeleton::Renderer::CleanupRenderer()
{
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

	for (const auto& pdevice : physDevices)
	{
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueProperties(propertyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, queueProperties.data());

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
			_selectedDevice = pdevice;
			return;
		}
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
	
}

void skeleton::Renderer::CreateCommandBuffers()
{

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
	//VkRenderPassCreateInfo createInfo = {};
	//createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
}

void skeleton::Renderer::CreatePipelineLayout()
{

}

void skeleton::Renderer::CreatePipeline()
{

}

void skeleton::Renderer::CreateDepthImage()
{

}

void skeleton::Renderer::CreateFrameBuffers()
{

}

void skeleton::Renderer::RecordCommandBuffers()
{

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

