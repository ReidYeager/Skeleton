
#include "vulkan/vulkan.h"

#ifndef SKELETON_CORE_RENDERER_IMAGE_H
#define SKELETON_CORE_RENDERER_IMAGE_H

// Stores all information Vulkan needs to render an image
struct ImageInfo
{
  VkImage			image;
  VkDeviceMemory	memory;
  VkImageView		view;
  VkSampler		sampler;
};

#endif // !SKELETON_CORE_RENDERER_IMAGE_H

