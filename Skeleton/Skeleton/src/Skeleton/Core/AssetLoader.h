#pragma once

#include <fstream>
#include <vector>

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

} // namespace skeleton

