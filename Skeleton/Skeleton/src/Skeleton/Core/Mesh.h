
#ifndef SKELETON_CORE_MESH_H
#define SKELETON_CORE_MESH_H 1

#include <vector>

#include "skeleton/core/vertex.h"

// Indexed mesh
struct mesh_t
{
  std::vector<vertex_t> verticies;
  std::vector<uint32_t> indices;

  const VkBuffer* vertexBuffer;
  const VkBuffer* indexBuffer;

  uint32_t vertexBufferIndex;
  uint32_t indexBufferIndex;

};

#endif // !SKELETON_CORE_MESH_H

