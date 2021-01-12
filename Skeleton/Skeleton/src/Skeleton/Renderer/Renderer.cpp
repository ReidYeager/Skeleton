
#include "pch.h"
#include <stdio.h>

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
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
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

}

void skeleton::Renderer::CleanupRenderer()
{

}

void skeleton::Renderer::RecreateRenderer()
{

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
	std::vector<const char*> instanceExtensions(sdlExtensionCount);
	SDL_Vulkan_GetInstanceExtensions(window, &sdlExtensionCount, instanceExtensions.data());
	instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	std::vector<const char*> instanceLayers;
	instanceLayers.push_back("VK_LAYER_KHRONOS_validation");

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
	createInfo.enabledLayerCount = static_cast<uint32_t>(instanceLayers.size());
	createInfo.ppEnabledLayerNames = instanceLayers.data();

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

	SKL_PRINT(
		SKL_DEBUG,
		"Queue indices :--: Graphics: %u :--: Present: %u :--: Transfer: %u",
		queueIndices[0],
		queueIndices[1],
		queueIndices[2]);

	// Create the logical device
	VkPhysicalDeviceFeatures enabledFeatures = {};
	const char* enabledLayers = "VK_LAYER_KHRONOS_validation";

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
	createInfo.enabledExtensionCount = 0;
	createInfo.ppEnabledExtensionNames = nullptr;
	createInfo.enabledLayerCount = 1;
	createInfo.ppEnabledLayerNames = &enabledLayers;
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
	std::vector<VkQueueFamilyProperties> queueProperties;

	for (const auto& pdevice : physDevices)
	{
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, nullptr);
		queueProperties.resize(propertyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &propertyCount, queueProperties.data());

		_graphicsIndex = GetQueueIndex(queueProperties, VK_QUEUE_GRAPHICS_BIT);
		_transferIndex = GetQueueIndex(queueProperties, VK_QUEUE_TRANSFER_BIT);
		_presentIndex = GetPresentIndex(&pdevice, propertyCount, _graphicsIndex);

		if (_graphicsIndex != -1 &&
			_presentIndex != -1 &&
			_transferIndex != -1)
		{
			_selectedDevice = pdevice;
			return;
		}

		queueProperties.clear();
	}
}

void skeleton::Renderer::CreateCommandPools()
{

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

}

void skeleton::Renderer::CreateRenderpass()
{

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

void skeleton::Renderer::CreateFrameBuffer()
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

