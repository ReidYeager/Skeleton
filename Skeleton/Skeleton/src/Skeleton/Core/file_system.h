
#ifndef SKELETON_CORE_FILE_SYSTEM_H
#define SKELETON_CORE_FILE_SYSTEM_H 1

#include <fstream>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#include "skeleton/core/debug_tools.h"
#include "skeleton/core/time.h"
#include "skeleton/renderer/render_backend.h"

// Loads a file's binary as a char array
inline std::vector<char> LoadFile(const char* _directory)
{
  std::ifstream inFile;
  inFile.open(_directory, std::ios::ate | std::ios::binary);
  if (!inFile)
  {
    SKL_LOG(SKL_ERROR, "Failed to load file \"%s\"", _directory);
    return {};
  }

  size_t fileSize = inFile.tellg();
  inFile.seekg(0);

  std::vector<char> finalString(fileSize);
  inFile.read(finalString.data(), fileSize);

  inFile.close();
  return finalString;
}

// Loads an image via stbi
inline void* LoadImageFile(const char* _directory, int& _width, int& _height)
{
  int channels;
  stbi_uc* img = stbi_load(_directory, &_width, &_height, &channels, STBI_rgb_alpha);
  assert(img != nullptr);
  return img;
}

// Destroys a stbi image
inline void DestroyImageFile(void* _data)
{
  stbi_image_free(_data);
}

// Loads a .obj file and converts it to a skl_Mesh
inline mesh_t LoadMesh(const char* _directory, BufferManager* _bufferManager)
{
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string loadWarnings, loadErrors;

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &loadWarnings, &loadErrors, _directory))
  {
    SKL_LOG("FileSystem", "Failed to load OBJ\n\ttinyObj warning: %s\n\ttinyObj error: %s",
            loadWarnings.c_str(), loadErrors.c_str());
    return {};
  }

  mesh_t mesh;
  std::unordered_map<vertex_t, uint32_t> vertMap = {};

  // Construct the vertex and index arrays
  vertex_t vert = {};
  for (const auto& shape : shapes)
  {
    for (const auto& index : shape.mesh.indices)
    {
      vert.position = {
        attrib.vertices[3 * index.vertex_index + 0],
        attrib.vertices[3 * index.vertex_index + 1],
        attrib.vertices[3 * index.vertex_index + 2]
      };

      vert.uv = {
        attrib.texcoords[2 * index.texcoord_index + 0],
        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
      };

      vert.normal = {
        attrib.normals[3 * index.normal_index + 0],
        attrib.normals[3 * index.normal_index + 1],
        attrib.normals[3 * index.normal_index + 2]
      };

      if (vertMap.count(vert) == 0)
      {
        vertMap[vert] = (uint32_t)mesh.verticies.size();
        mesh.verticies.push_back(vert);
      }
      mesh.indices.push_back(vertMap[vert]);
    }
  }

  mesh.vertexBufferIndex = _bufferManager->CreateAndFillBuffer(mesh.verticies.data(),
      mesh.verticies.size() * sizeof(mesh.verticies[0]), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  mesh.indexBufferIndex = _bufferManager->CreateAndFillBuffer(mesh.indices.data(),
      mesh.indices.size() * sizeof(mesh.indices[0]), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  mesh.vertexBuffer = _bufferManager->GetBuffer(mesh.vertexBufferIndex);
  mesh.indexBuffer = _bufferManager->GetBuffer(mesh.indexBufferIndex);

  SKL_PRINT("Mesh", "%zd unique verts -- %u, %u", mesh.verticies.size(), mesh.vertexBufferIndex, mesh.indexBufferIndex);

  return mesh;
}

#endif // !SKELETON_CORE_FILE_SYSTEM_H


