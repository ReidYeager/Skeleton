
#include <vector>

#include "vulkan/vulkan.h"

#include "Skeleton/Core/Common.h"
#include "Skeleton/Renderer/ParProgs.h"
#include "Skeleton/Core/Mesh.h"

// TODO : Clean this file up

#ifndef SKL_RENDER_BACKEND_H
#define SKL_RENDER_BACKEND_H

struct sklBuffer_t
{
	VkBuffer buffer;
	VkDeviceMemory memory;
};

struct sklImage_t
{
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkSampler sampler;
};

struct sklRenderable_t
{
	mesh_t mesh;
	uint32_t parProgIndex;
	std::vector<sklBuffer_t*> buffers;
	std::vector<sklImage_t*> images;

	// TODO : Create buffer/image function

	void Cleanup();
};

struct SklPhysicalDeviceInfo_t
{
	VkPhysicalDevice						device;
	VkPhysicalDeviceProperties				properties;
	VkPhysicalDeviceMemoryProperties		memProperties;
	VkPhysicalDeviceFeatures				features;
	VkSurfaceCapabilitiesKHR				surfaceCapabilities;
	std::vector<VkSurfaceFormatKHR>			surfaceFormats;
	std::vector<VkPresentModeKHR>			presentModes;
	std::vector<VkQueueFamilyProperties>	queueFamilyProperties;
	std::vector<VkExtensionProperties>		extensionProperties;
};

struct SklVulkanContext_t
{
	SklPhysicalDeviceInfo_t gpu;
	VkDevice device;

	uint32_t graphicsIdx;
	uint32_t presentIdx;
	uint32_t transferIdx;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkQueue transferQueue;

	std::vector<shader_t> shaders;
	std::vector<parProg_t> parProgs;

	VkExtent2D renderExtent;
	VkRenderPass renderPass;

	std::vector<sklRenderable_t> renderables;

	void Cleanup();
};

struct sklView_t
{
	glm::mat4 mvpMatrix;						// This view's MVP
	std::vector<sklRenderable_t> renderables;	// Objects this view can see
};

extern SklVulkanContext_t vulkanContext;

//=================================================
// Commands
//=================================================

inline void SklCreateCommandPool(
	VkCommandPool& _pool,
	uint32_t _queueIndex,
	VkCommandPoolCreateFlags _flags = 0)
{
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = _queueIndex;
	createInfo.flags = _flags;
	SKL_ASSERT_VK(
		vkCreateCommandPool(vulkanContext.device, &createInfo, nullptr, &_pool),
		"Failed to create command pool with flags %u", _flags);
}

inline VkCommandBuffer BeginSingleTimeCommand(
	VkCommandPool& _pool)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = _pool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer singleCommand;
	SKL_ASSERT_VK(
		vkAllocateCommandBuffers(vulkanContext.device, &allocInfo, &singleCommand),
		"Failed to create transient command buffer");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(singleCommand, &beginInfo);

	return singleCommand;
}

inline void EndSingleTimeCommand(
	VkCommandBuffer _command,
	VkCommandPool& _pool,
	VkQueue& _queue)
{
	vkEndCommandBuffer(_command);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &_command;

	vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(_queue);

	vkFreeCommandBuffers(vulkanContext.device, _pool, 1, &_command);
}


//=================================================
// Create logical device
//=================================================

inline void CreateLogicalDevice(
	const std::vector<uint32_t> _queueIndices,
	const std::vector<const char*> _deviceExtensions,
	const std::vector<const char*> _deviceLayers)
{
	VkPhysicalDeviceFeatures enabledFeatures = {};
	enabledFeatures.samplerAnisotropy = VK_TRUE;
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
		vkCreateDevice(vulkanContext.gpu.device, &createInfo, nullptr, &vulkanContext.device),
		"Failed to create logical device");

	vkGetDeviceQueue(vulkanContext.device, vulkanContext.graphicsIdx, 0, &vulkanContext.graphicsQueue);
	vkGetDeviceQueue(vulkanContext.device, vulkanContext.presentIdx, 0, &vulkanContext.presentQueue);
	vkGetDeviceQueue(vulkanContext.device, vulkanContext.transferIdx, 0, &vulkanContext.transferQueue);

	//CreateCommandPool(transientPool, queueIndices.transfer, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
}

// TODO : Better handle buffer memory
class BufferManager
{
	//=================================================
	// Variables
	//=================================================
private:
	// TODO : Upgrade this to allow more buffers
	uint64_t m_indexBitMap = 0;
	#define INDEXBITMAPSIZE 64
	std::vector<VkBuffer> m_buffers;
	std::vector<VkDeviceMemory> m_memories;

public:
	VkCommandPool transientPool;

	//=================================================
	// Functions
	//=================================================
public:
	BufferManager()
	{
		SklCreateCommandPool(transientPool, vulkanContext.transferIdx);
	}
	~BufferManager();

	bool GetIndexBitMapAt(uint32_t _index);
	void SetIndexBitMapAt(uint32_t _index, bool _value = true);

	uint32_t GetBufferCount() { return static_cast<uint32_t>(m_buffers.size()); }

	const VkBuffer* GetBuffer(uint32_t _index);
	const VkDeviceMemory* GetMemory(uint32_t _index);
	void RemoveAtIndex(uint32_t _index);
	uint32_t GetFirstAvailableIndex();

	uint32_t CreateAndFillBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		const void* _data,
		VkDeviceSize size,
		VkBufferUsageFlags _usage);

	uint32_t CreateBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		VkDeviceSize _size,
		VkBufferUsageFlags _usage,
		VkMemoryPropertyFlags _memProperties);

	uint32_t CreateBuffer(
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

}; // BufferManager


#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

//=================================================
// Renderer Backend
//=================================================

// Initialization and drawing
class SklRendererBackend
{
private:
	std::vector<const char*> validationLayer = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> instanceExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

public:
	// TODO : Find a way to remove SDL window from Create(instance/swapchain)
	SDL_Window* window;
	BufferManager* bufferManager;

	VkInstance instance;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	VkFormat swapchainFormat;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkFramebuffer> frameBuffers;

	VkImage depthImage;
	VkImageView depthImageView;
	VkDeviceMemory depthMemory;

	VkDescriptorPool descriptorPool;

	// Synchronization
	#define MAX_FLIGHT_IMAGE_COUNT 3
	std::vector<VkFence> imageIsInFlightFences;
	std::vector<VkFence> flightFences;
	std::vector<VkSemaphore> renderCompleteSemaphores;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	uint32_t currentFrame = 0;

	VkCommandPool graphicsCommandPool;
	std::vector<VkCommandBuffer> commandBuffers;

public:
	//=================================================
	// Initialization
	//=================================================

	SklRendererBackend(
		SDL_Window* _window,
		const std::vector<const char*>& _extraExtensions);

	~SklRendererBackend();

	// Creates the vkInstance & vkSurface
	void CreateInstance();
	// Gets a physical device & creates the vkDevice
	void CreateDevice();
	// Determines the best physical device for rendering
	void ChoosePhysicalDevice(
		VkPhysicalDevice& _selectedDevice,
		uint32_t& _graphicsIndex,
		uint32_t& _presentIndex,
		uint32_t& _transferIndex);
	void CreateCommandPool();

	//=================================================
	// Renderer
	//=================================================

	void InitializeRenderComponents();
	void CreateRenderComponents();
	void CleanupRenderComponents();
	void RecreateRenderComponents();

	void CreateSwapchain();
	void CreateRenderpass();
	void CreateDepthImage();
	void CreateFramebuffers();

	void CreateDescriptorPool();
	void CreateSyncObjects();
	void CreateCommandBuffers();


	//=================================================
	// Helpers
	//=================================================
	
	// Initialization
	//=================================================

	// Returns the first instance of a queue with the input flags
	uint32_t GetQueueIndex(
		std::vector<VkQueueFamilyProperties>& _queues,
		VkQueueFlags _flags);

	// Returns the first instance of a presentation queue
	uint32_t GetPresentIndex(
		const VkPhysicalDevice* _device,
		uint32_t _queuePropertyCount,
		uint32_t _graphicsIndex);

	// Renderer
	//=================================================

	void CreateImage(
		uint32_t _width,
		uint32_t _height,
		VkFormat _format,
		VkImageTiling _tiling,
		VkImageUsageFlags _usage,
		VkMemoryPropertyFlags _memFlags,
		VkImage& _image,
		VkDeviceMemory& _memory);

	VkImageView CreateImageView(
		const VkFormat _format,
		VkImageAspectFlags _aspect,
		const VkImage& _image);

	VkFormat FindDepthFormat();

	VkFormat FindSupportedFormat(
		const std::vector<VkFormat>& _candidates,
		VkImageTiling _tiling,
		VkFormatFeatureFlags _features);

};

#endif // RENDERER_BACKEND_H

