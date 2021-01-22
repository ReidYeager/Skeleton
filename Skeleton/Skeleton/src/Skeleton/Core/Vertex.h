#pragma once

#include <vector>

#include "glm/glm.hpp"
#include "vulkan/vulkan.h"

namespace skeleton
{

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription desc = {};
		desc.stride = sizeof(Vertex);
		desc.binding = 0;
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
	{
		std::vector<VkVertexInputAttributeDescription> attribs(2);
		attribs[0].binding = 0;
		attribs[0].location = 0;
		attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribs[0].offset = offsetof(Vertex, position);
		attribs[1].binding = 0;
		attribs[1].location = 1;
		attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attribs[1].offset = offsetof(Vertex, color);

		return attribs;
	}
}; // Vertex

} // namespace skeleton

