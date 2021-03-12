
#include <vector>

#include "vulkan/vulkan.h"
#include "sdl/SDL.h"
#include "sdl/SDL_vulkan.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Skeleton/Renderer/RendererBackend.h"
#include "Skeleton/Core/Camera.h"

#ifndef SKELETON_RENDERER_RENDERER_H
#define SKELETON_RENDERER_RENDERER_H

class Renderer
{
  //=================================================
  // Variables
  //=================================================
public:
  SklRendererBackend* backend;
  BufferManager* bufferManager;

  // Transformation matrices for objects
  struct MVPMatrices {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
  } mvp;

  // TODO : Move to an Object class
  VkBuffer mvpBuffer;
  VkDeviceMemory mvpMemory;

  // TODO : Move to the Main Application
  Camera cam;

  //=================================================
  // Functions
  //=================================================
public:
  // Initialization & Cleanup
  //=================================================

  // Creates the RendererBackend and BufferManager
  Renderer(const std::vector<const char*>& _extraExtensions, SDL_Window* _window);
  // Cleans up the RendererBackend
  ~Renderer();

  // RendererBackend
  //=================================================

  // Initializes the RendererBackend
  void CreateRenderer();
  // Cleans up the RendererBackend
  void CleanupRenderer();
  // Recreates the RendererBackend
  void RecreateRenderer();

  // Runtime
  //=================================================

  // Handles all rendering processes
  // Fetches the next image and places in the rendering and presentation queues
  void RenderFrame();

  // Defines buffers and images for a shaderProgram's bindings
  void CreateDescriptorSet(shaderProgram_t& _prog, sklRenderable_t& _renderable);

  // TODO : Remove when Objects have individual MVP buffers/push-constants
  // Creates a universal MVP buffer
  void CreateModelBuffers();
  // Records rendering information into commandbuffers
  void RecordCommandBuffers();

protected:
  // Helpers
  //=================================================

  // Loads image from file to a texture
  void CreateTextureImage(const char* _directory, VkImage& _image, VkImageView& _view,
                          VkDeviceMemory& _memory, VkSampler& _sampler);

  // Creates a generic imageSampler
  VkSampler CreateSampler();

  // Changes the ImageLayout of an image via a commandBuffer
  void TransitionImageLayout(VkImage _iamge, VkFormat _format,
                             VkImageLayout _oldLayout, VkImageLayout _newLayout);

  // Copies a VkBuffer to a VkImage
  void CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width, uint32_t _height);

}; // Renderer

#endif // !SKELETON_RENDERER_RENDERER_H

