#pragma once

#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"

namespace skeleton
{
class Renderer
{
//////////////////////////////////////////////////////////////////////////
// Variables
//////////////////////////////////////////////////////////////////////////
private:
	// TODO : Move all SDL stuff to its own file
	SDL_Window* window;

	VkInstance instance;
	VkSurfaceKHR surface;
	VkDevice device;
	VkPhysicalDevice physicalDevice;

	VkQueue graphicsQueue;
	VkQueue presentQueue;
	VkQueue transferQueue;

//////////////////////////////////////////////////////////////////////////
// Functions
//////////////////////////////////////////////////////////////////////////
public:
// Initialization & Cleanup //////////////////////////////////////////////

	// Initializes the renderer in its entirety
	Renderer();
	// Cleans up all vulkan objects
	~Renderer();

// Runtime ///////////////////////////////////////////////////////////////

	void RenderFrame();

protected:
// Create Renderer ///////////////////////////////////////////////////////

	void CreateRenderer();
	void CleanupRenderer();
	void RecreateRenderer();

// Initializers //////////////////////////////////////////////////////////

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

// Create Renderer Functions /////////////////////////////////////////////

	void CreateSwapchain();
	void CreateRenderpass();
	void CreatePipelineLayout();
	void CreatePipeline();
	void CreateDepthImage();
	void CreateFrameBuffer();

	void RecordCommandBuffers();

// Helpers ///////////////////////////////////////////////////////////////

	// Returns the first instance of a queue with the input flags
	uint32_t GetQueueIndex(
		std::vector<VkQueueFamilyProperties>& _queues,
		VkQueueFlags _flags);

	// Returns the first instance of a presentation queue
	uint32_t GetPresentIndex(
		const VkPhysicalDevice* _device,
		uint32_t _queuePropertyCount,
		uint32_t _graphicsIndex);

}; // Renderer
} // namespace skeleton

