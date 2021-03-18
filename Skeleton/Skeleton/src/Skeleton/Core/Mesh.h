
#include <vector>

#include "skeleton/core/vertex.h"

#ifndef SKELETON_CORE_MESH_H
#define SKELETON_CORE_MESH_H 1

// Indexed mesh
struct mesh_t
{
  std::vector<vertex_t> verticies;
  std::vector<uint32_t> indices;

  VkBuffer vertexBuffer;
  VkDeviceMemory vertexBufferMemory;
  VkBuffer indexBuffer;
  VkDeviceMemory indexBufferMemory;

  // TODO : Utilize a BufferManager for mesh data
  // Frees the memory associated with this mesh
  void Cleanup(VkDevice& _device)
  {
    vkDestroyBuffer(_device, vertexBuffer, nullptr);
    vkFreeMemory(_device, vertexBufferMemory, nullptr);
    vkDestroyBuffer(_device, indexBuffer, nullptr);
    vkFreeMemory(_device, indexBufferMemory, nullptr);
  }
};

#endif // !SKELETON_CORE_MESH_H

