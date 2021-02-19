
#pragma once

#include <vector>
#include "vulkan/vulkan.h"
#include "Skeleton/Renderer/Image.h"

namespace skeleton
{
class Renderable
{
//=================================================
// Variables
//=================================================
public:
	std::vector<VkBuffer> buffers;
	std::vector<skeleton::ImageInfo> images;


//=================================================
// Functions
//=================================================
public:
	

};
} // namespace skeleton

