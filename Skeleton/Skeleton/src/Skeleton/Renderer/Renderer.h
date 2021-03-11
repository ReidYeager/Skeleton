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

	struct MVPMatrices {
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	} mvp;

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

	void CreateDescriptorSet(
		parProg_t& _prog,
		sklRenderable_t& _renderable);

	void CreateModelBuffers();
	void RecordCommandBuffers();

protected:
	// Initializers
	//=================================================

	// Pre-renderer creation

	// Post-renderer creation

	// Create Renderer Functions
	//=================================================

	// Helpers
	//=================================================

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

}; // Renderer

