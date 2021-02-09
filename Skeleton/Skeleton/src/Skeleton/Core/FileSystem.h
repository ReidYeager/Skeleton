#pragma once

#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "Common.h"

namespace skeleton::tools
{

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

inline void DestroyImageFile(void* _data)
{
	stbi_image_free(_data);
}

} // namespace skeleton

