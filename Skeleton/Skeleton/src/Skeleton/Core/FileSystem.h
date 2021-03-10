#pragma once

#include <fstream>
#include <vector>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tinyobjloader/tiny_obj_loader.h"

#include "Common.h"
#include "Skeleton/Renderer/RendererBackend.h"

inline std::vector<char> LoadFile(
	const char* _directory)
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

inline void* LoadImageFile(
	const char* _directory,
	int& _width,
	int& _height)
{
	int channels;
	stbi_uc* img = stbi_load(_directory, &_width, &_height, &channels, STBI_rgb_alpha);
	assert(img != nullptr);
	return img;
}

inline void DestroyImageFile(
	void* _data)
{
	stbi_image_free(_data);
}

inline mesh_t LoadMesh(
	const char* _directory,
	BufferManager* _bm)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;
	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, _directory))
	{
		SKL_LOG("FileSystem", "Failed to load OBJ\n\ttinyObj warning: %s\n\ttinyObj error: %s", warn.c_str(), err.c_str());
		return {};
	}

	mesh_t mesh;
	std::unordered_map<vertex_t, uint32_t> vertMap = {};

	for (const auto& shape : shapes)
	{
		for (const auto& index : shape.mesh.indices)
		{
			vertex_t vert = {};

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

	SKL_PRINT("Mesh", "%zd unique verts", mesh.verticies.size());

	_bm->CreateAndFillBuffer(
		mesh.vertexBuffer,
		mesh.vertexBufferMemory,
		mesh.verticies.data(),
		mesh.verticies.size() * sizeof(mesh.verticies[0]),
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	_bm->CreateAndFillBuffer(
		mesh.indexBuffer,
		mesh.indexBufferMemory,
		mesh.indices.data(),
		mesh.indices.size() * sizeof(mesh.indices[0]),
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	return mesh;
}


