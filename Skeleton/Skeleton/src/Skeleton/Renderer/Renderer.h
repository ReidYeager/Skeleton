#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "VulkanDevice.h"
#include "BufferManager.h"

namespace skeleton
{
class Renderer
{
//=================================================
// Variables
//=================================================
private:
	// TODO : Move all SDL stuff to its own file
	SDL_Window* window;

	VkInstance instance;
	VkSurfaceKHR surface;
	std::vector<const char*> validationLayer    = { "VK_LAYER_KHRONOS_validation" };
	std::vector<const char*> instanceExtensions = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };
	std::vector<const char*> deviceExtensions   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VulkanDevice* device;
	BufferManager* bufferManager;

	VkCommandPool graphicsPool;

	VkFormat swapchainFormat;
	VkExtent2D swapchainExtent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkImage textureImage;
	VkDeviceMemory textureMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;

	VkImage depthImage;
	VkDeviceMemory depthMemory;
	VkImageView depthImageView;

	struct MVPMatrices {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	} mvp;
	VkDescriptorSetLayout descriptorSetLayout;
	VkBuffer mvpBuffer;
	VkDeviceMemory mvpMemory;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	VkRenderPass renderpass;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	std::vector<VkFramebuffer> frameBuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	#define MAX_FLIGHT_IMAGE_COUNT 3
	std::vector<VkFence> imageIsInFlightFences;
	std::vector<VkFence> flightFences;
	std::vector<VkSemaphore> renderCompleteSemaphores;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	uint32_t currentFrame = 0;

	VkBuffer vertBuffer;
	VkDeviceMemory vertMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexMemory;


//=================================================
// Functions
//=================================================
public:
	// Initialization & Cleanup
	//=================================================

	// Initializes the renderer in its entirety
	Renderer();
	// Cleans up all vulkan objects
	~Renderer();

	// Runtime
	//=================================================

	void RenderFrame();

	void CreateDescriptorSetLayout();
	void CreateDescriptorPool();
	void CreateDescriptorSet();

protected:
	// Create Renderer
	//=================================================

	void CreateRenderer();
	void CleanupRenderer();
	void RecreateRenderer();

	// Initializers
	//=================================================

	// Pre-renderer creation

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
	void CreateCommandPools();

	// Post-renderer creation

	void CreateSyncObjects();
	void CreateCommandBuffers();

	// Create Renderer Functions
	//=================================================

	void CreateSwapchain();
	void CreateRenderpass();
	void CreatePipelineLayout();
	void CreatePipeline();
	void CreateDepthImage();
	void CreateFrameBuffers();

	void RecordCommandBuffers();

	// Helpers
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

	VkImageView CreateImageView(
		const VkFormat _format,
		VkImageAspectFlags _aspect,
		const VkImage& _image);

	VkShaderModule CreateShaderModule(
		const char* _directory);

	void CreateModelBuffers();

	void CreateImage(
		uint32_t _width,
		uint32_t _height,
		VkFormat _format,
		VkImageTiling _tiling,
		VkImageUsageFlags _usage,
		VkMemoryPropertyFlags _memFlags,
		VkImage& _image,
		VkDeviceMemory& _memory);

	void CreateTextureImage(
		const char* _directory);

	void CreateSampler();

	void TransitionImageLayout(
		VkImage _iamge,
		VkFormat _format,
		VkImageLayout _old,
		VkImageLayout _new);

	void CopyBufferToImage(
		VkBuffer _buffer,
		VkImage _image,
		uint32_t _width,
		uint32_t _height);

	VkFormat FindDepthFormat();

	VkFormat FindSupportedFormat(
		const std::vector<VkFormat>& _candidates,
		VkImageTiling _tiling,
		VkFormatFeatureFlags _features);

}; // Renderer
} // namespace skeleton

