#pragma once

#include <vector>
#include "Skeleton/Core/Vertex.h"

struct mesh_t
{
	std::vector<vertex_t> verticies;
	std::vector<uint32_t> indices;

	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;

	void Cleanup(VkDevice& _device)
	{
		vkDestroyBuffer(_device, vertexBuffer, nullptr);
		vkFreeMemory(_device, vertexBufferMemory, nullptr);
		vkDestroyBuffer(_device, indexBuffer, nullptr);
		vkFreeMemory(_device, indexBufferMemory, nullptr);
	}
};


