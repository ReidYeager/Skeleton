#pragma once

#include <stdio.h>

#include "vulkan/vulkan.h"

namespace skeleton
{

#define SKL_DEBUG "SKL_DEBUG"
#define SKL_ERROR "SKL_ERROR"
#define SKL_ERROR_VK "SKL_ERROR_VK"

inline const char* VulkanResultToString(
	VkResult _result)
{
	#define ETS(x) case x: return #x;
	
	switch (_result)
	{
		ETS(VK_SUCCESS);
		ETS(VK_NOT_READY);
		ETS(VK_TIMEOUT);
		ETS(VK_EVENT_SET);
		ETS(VK_EVENT_RESET);
		ETS(VK_INCOMPLETE);
		ETS(VK_ERROR_OUT_OF_HOST_MEMORY);
		ETS(VK_ERROR_OUT_OF_DEVICE_MEMORY);
		ETS(VK_ERROR_INITIALIZATION_FAILED);
		ETS(VK_ERROR_DEVICE_LOST);
		ETS(VK_ERROR_MEMORY_MAP_FAILED);
		ETS(VK_ERROR_LAYER_NOT_PRESENT);
		ETS(VK_ERROR_EXTENSION_NOT_PRESENT);
		ETS(VK_ERROR_FEATURE_NOT_PRESENT);
		ETS(VK_ERROR_INCOMPATIBLE_DRIVER);
		ETS(VK_ERROR_TOO_MANY_OBJECTS);
		ETS(VK_ERROR_FORMAT_NOT_SUPPORTED);
		ETS(VK_ERROR_FRAGMENTED_POOL);
		ETS(VK_ERROR_UNKNOWN);
		ETS(VK_ERROR_OUT_OF_POOL_MEMORY);
		ETS(VK_ERROR_INVALID_EXTERNAL_HANDLE);
		ETS(VK_ERROR_FRAGMENTATION);
		ETS(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
		ETS(VK_ERROR_SURFACE_LOST_KHR);
		ETS(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
		ETS(VK_SUBOPTIMAL_KHR);
		ETS(VK_ERROR_OUT_OF_DATE_KHR);
		ETS(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
		ETS(VK_ERROR_VALIDATION_FAILED_EXT);
		ETS(VK_ERROR_INVALID_SHADER_NV);
		ETS(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
		ETS(VK_ERROR_NOT_PERMITTED_EXT);
		ETS(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
		ETS(VK_THREAD_IDLE_KHR);
		ETS(VK_THREAD_DONE_KHR);
		ETS(VK_OPERATION_DEFERRED_KHR);
		ETS(VK_OPERATION_NOT_DEFERRED_KHR);
		ETS(VK_PIPELINE_COMPILE_REQUIRED_EXT);
		ETS(VK_RESULT_MAX_ENUM);
	default: return "Invalid vkResult";
	}

	#undef ETS
}

#define SKL_PRINT(category, message, ...)	\
{											\
	printf("%s :--: ", category);			\
	printf(message, __VA_ARGS__);			\
	printf("\n");							\
}

#define SKL_PRINT_SLIM(message, ...)	\
{										\
	printf(message, __VA_ARGS__);		\
	printf("\n");						\
}

#define SKL_LOG(category, message, ...)					\
{														\
	SKL_PRINT(category, message, __VA_ARGS__);			\
	SKL_PRINT_SLIM("\t%s :--: %d", __FILE__, __LINE__);	\
}

#define SKL_ASSERT_VK(vkFunction, errorMessage, ...)		\
{															\
	VkResult vkAssertResult = vkFunction;					\
	if (vkAssertResult != VK_SUCCESS)						\
	{														\
		SKL_PRINT(SKL_ERROR_VK, errorMessage, __VA_ARGS__);	\
		throw skeleton::VulkanResultToString(vkAssertResult);			\
	}														\
}

} // namespace skeleton

