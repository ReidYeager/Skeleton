#pragma once

#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "Common.h"

namespace skeleton::tools
{

inline std::vector<char> LoadFileAsString(
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

//inline void LoadImage(
//	const char* _directory)
//{
//	int width, height, channels;
//	stbi_uc* img = stbi_load(_directory, &width, &height, &channels, STBI_rgb_alpha);
//	VkDeviceSize size = width * height * 4;
//
//	SKL_PRINT_SLIM("%p", img);
//	assert(img == nullptr);
//}

} // namespace skeleton

