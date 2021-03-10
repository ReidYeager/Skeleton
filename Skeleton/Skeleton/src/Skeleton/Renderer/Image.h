#pragma once

#include "vulkan/vulkan.h"

struct ImageInfo
{
	VkImage			image;
	VkDeviceMemory	memory;
	VkImageView		view;
	VkSampler		sampler;
};

