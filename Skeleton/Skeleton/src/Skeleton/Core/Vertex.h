
#ifndef SKELETON_CORE_VERTEX_H
#define SKELETON_CORE_VERTEX_H 1

#include <vector>

#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include "vulkan/vulkan.h"

// Per-vertex information used to build and render meshes
struct vertex_t
{
  glm::vec3 position;
  glm::vec2 uv;
  glm::vec3 normal;

  // Defines the size of a vertex for Vulkan
  static VkVertexInputBindingDescription GetBindingDescription()
  {
    VkVertexInputBindingDescription desc = {};
    desc.stride = sizeof(vertex_t);
    desc.binding = 0;
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return desc;
  }

  // Defines the layout of vertex information for Vulkan
  static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions()
  {
    std::vector<VkVertexInputAttributeDescription> attribs(3);
    // Position
    attribs[0].binding = 0;
    attribs[0].location = 0;
    attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[0].offset = offsetof(vertex_t, position);
    // UV
    attribs[1].binding = 0;
    attribs[1].location = 1;
    attribs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribs[1].offset = offsetof(vertex_t, uv);
    // Normal
    attribs[2].binding = 0;
    attribs[2].location = 2;
    attribs[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribs[2].offset = offsetof(vertex_t, normal);

    return attribs;
  }

  // Required for hash mapping
  // Compares the attributes of other against itself
  bool operator==(const vertex_t& other) const
  {
    return position == other.position && normal == other.normal && uv == other.uv;
  }
}; // Vertex

// TODO : Make a better hash function http://www.azillionmonkeys.com/qed/hash.html
// Used to map vertices into an unordered array during mesh building
namespace std {
  template<> struct hash<vertex_t> {
    size_t operator()(vertex_t const& vertex) const {
      return ((hash<glm::vec3>()(vertex.position) ^
        (hash<glm::vec2>()(vertex.uv) << 1)) >> 1) ^
        (hash<glm::vec3>()(vertex.normal) << 1);
    }
  };
}

#endif // !SKELETON_CORE_VERTEX_H

