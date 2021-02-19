#pragma once

#include "vulkan/vulkan.h"

namespace skeleton
{
	struct ImageInfo
	{
		VkImage			image;
		VkDeviceMemory	memory;
		VkImageView		view;
		VkSampler		sampler;
	};
} // namespace skeleton

