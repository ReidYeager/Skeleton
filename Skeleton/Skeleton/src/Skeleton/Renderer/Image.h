
#ifndef SKELETON_RENDERER_IMAGE_H
#define SKELETON_RENDERER_IMAGE_H 1

#include "vulkan/vulkan.h"

// Stores all information Vulkan needs to render an image
struct sklImage_t
{
  VkImage         image;
  VkDeviceMemory  memory;
  VkImageView     view;
  VkSampler       sampler;
  VkFormat        format;
};

struct sklTexture_t
{
  const char* directory;
  uint32_t imageIndex;

  sklTexture_t(const char* _dir) : directory(_dir) {}
};

#endif // !SKELETON_RENDERER_SKL_IMAGE_H

