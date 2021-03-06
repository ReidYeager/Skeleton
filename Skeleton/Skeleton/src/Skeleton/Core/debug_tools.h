
#ifndef SKELETON_CORE_DEBUG_TOOLS_H
#define SKELETON_CORE_DEBUG_TOOLS_H 1

#include <stdio.h>

#include "vulkan/vulkan.h"

// TODO : Replace this file with a more permanent debug/logging system

#define SKL_DEBUG "SKL_DEBUG"
#define SKL_ERROR "SKL_ERROR"
#define SKL_ERROR_VK "SKL_ERROR_VK"

// Converts a VkResult enum to its string
inline const char* VulkanResultToString(VkResult _result)
{
  #define RTS(x) case x: return #x;

  switch (_result)
  {
    RTS(VK_SUCCESS);
    RTS(VK_NOT_READY);
    RTS(VK_TIMEOUT);
    RTS(VK_EVENT_SET);
    RTS(VK_EVENT_RESET);
    RTS(VK_INCOMPLETE);
    RTS(VK_ERROR_OUT_OF_HOST_MEMORY);
    RTS(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    RTS(VK_ERROR_INITIALIZATION_FAILED);
    RTS(VK_ERROR_DEVICE_LOST);
    RTS(VK_ERROR_MEMORY_MAP_FAILED);
    RTS(VK_ERROR_LAYER_NOT_PRESENT);
    RTS(VK_ERROR_EXTENSION_NOT_PRESENT);
    RTS(VK_ERROR_FEATURE_NOT_PRESENT);
    RTS(VK_ERROR_INCOMPATIBLE_DRIVER);
    RTS(VK_ERROR_TOO_MANY_OBJECTS);
    RTS(VK_ERROR_FORMAT_NOT_SUPPORTED);
    RTS(VK_ERROR_FRAGMENTED_POOL);
    RTS(VK_ERROR_UNKNOWN);
    RTS(VK_ERROR_OUT_OF_POOL_MEMORY);
    RTS(VK_ERROR_INVALID_EXTERNAL_HANDLE);
    RTS(VK_ERROR_FRAGMENTATION);
    RTS(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
    RTS(VK_ERROR_SURFACE_LOST_KHR);
    RTS(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
    RTS(VK_SUBOPTIMAL_KHR);
    RTS(VK_ERROR_OUT_OF_DATE_KHR);
    RTS(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
    RTS(VK_ERROR_VALIDATION_FAILED_EXT);
    RTS(VK_ERROR_INVALID_SHADER_NV);
    RTS(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
    RTS(VK_ERROR_NOT_PERMITTED_EXT);
    RTS(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
    RTS(VK_THREAD_IDLE_KHR);
    RTS(VK_THREAD_DONE_KHR);
    RTS(VK_OPERATION_DEFERRED_KHR);
    RTS(VK_OPERATION_NOT_DEFERRED_KHR);
    RTS(VK_PIPELINE_COMPILE_REQUIRED_EXT);
    RTS(VK_RESULT_MAX_ENUM);
  default: return "Invalid vkResult";
  }

  #undef ETS
}

#define SKL_PRINT(category, message, ...)  \
{                                          \
  printf(":-skl-: %-15s :--: ", category);         \
  printf(message, __VA_ARGS__);            \
  printf("\n");                            \
}

#define SKL_PRINT_SIMPLE(message, ...)            \
{                                                 \
  SKL_PRINT(__FUNCTION__, message, __VA_ARGS__);  \
}

#define SKL_PRINT_SLIM(message, ...)  \
{                                     \
  printf(message, __VA_ARGS__);       \
  printf("\n");                       \
}

#define SKL_LOG(category, message, ...)                \
{                                                      \
  SKL_PRINT(category, message, __VA_ARGS__);           \
  SKL_PRINT_SLIM("\t%s :--: %d", __FILE__, __LINE__);  \
}

#define SKL_PRINT_ERROR(message, ...)           \
{                                               \
  SKL_LOG(__FUNCTION__, message, __VA_ARGS__);  \
}

#define SKL_ASSERT_VK(vkFunction, errorMessage, ...)    \
{                                                       \
  VkResult vkAssertResult = vkFunction;                 \
  if (vkAssertResult != VK_SUCCESS)                     \
  {                                                     \
    SKL_PRINT(SKL_ERROR_VK, errorMessage, __VA_ARGS__); \
    throw VulkanResultToString(vkAssertResult);         \
  }                                                     \
}

#endif // !SKELETON_CORE_DEBUG_TOOLS_H

