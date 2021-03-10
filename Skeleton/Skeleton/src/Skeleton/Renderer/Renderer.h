#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Skeleton/Renderer/RendererBackend.h"
#include "Skeleton/Core/Camera.h"

class Renderer
{
//=================================================
// Variables
//=================================================
public:
	SklRendererBackend* backend;
	BufferManager* bufferManager;

	VkCommandPool graphicsPool;

	VkFormat swapchainFormat;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;

	VkImage depthImage;
	VkDeviceMemory depthMemory;
	VkImageView depthImageView;

	struct MVPMatrices {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	} mvp;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;

	uint32_t m_boundParProg = 0;

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

	// Move out of renderer
	//=================================================
	VkBuffer mvpBuffer;
	VkDeviceMemory mvpMemory;

	Camera cam;

//=================================================
// Functions
//=================================================
public:
	// Initialization & Cleanup
	//=================================================

	// Initializes the renderer in its entirety
	Renderer(
		const std::vector<const char*>& _extraExtensions,
		SDL_Window* _window);
	// Cleans up all vulkan objects
	~Renderer();

	// Create Renderer
	//=================================================

	void CreateRenderer();
	void CleanupRenderer();
	void RecreateRenderer();

	// Runtime
	//=================================================

	void RenderFrame();

	void CreateDescriptorPool();
	void CreateDescriptorSet(
		parProg_t& _prog,
		sklRenderable_t& _renderable);

	void CreateModelBuffers();
	void RecordCommandBuffers();

protected:
	// Initializers
	//=================================================

	// Pre-renderer creation

	void CreateCommandPools();

	// Post-renderer creation

	void CreateSyncObjects();
	void CreateCommandBuffers();

	// Create Renderer Functions
	//=================================================

	void CreateSwapchain();
	void CreateRenderpass();
	void CreateDepthImage();
	void CreateFrameBuffers();

	// Helpers
	//=================================================

	VkImageView CreateImageView(
		const VkFormat _format,
		VkImageAspectFlags _aspect,
		const VkImage& _image);

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
		const char* _directory,
		VkImage& _image,
		VkImageView& _view,
		VkDeviceMemory& _memory,
		VkSampler& _sampler);

	VkSampler CreateSampler();

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

