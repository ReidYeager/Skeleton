#pragma once

#include <stdio.h>

#include "vulkan/vulkan.h"

namespace skeleton
{
enum SKLDebugPrintCategories
{
	SKL_DEBUG,
	SKL_ERROR,
	SKL_ERROR_VK
};

#define SKL_PRINT(category, message, ...)	\
{											\
	printf("%s :--: ", #category);			\
	printf(message, __VA_ARGS__);			\
	printf("\n");							\
}

#define SKL_ASSERT_VK(vkFunction, errorMessage, ...)		\
{															\
	VkResult vkAssertResult = vkFunction;					\
	if (vkAssertResult != VK_SUCCESS)						\
	{														\
		SKL_PRINT(SKL_ERROR_VK, errorMessage, __VA_ARGS__);	\
		throw "Throwing SKL_ERROR_VK";						\
	}														\
}
} // namespace skeleton

