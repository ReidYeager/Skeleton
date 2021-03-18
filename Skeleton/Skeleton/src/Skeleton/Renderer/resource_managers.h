
#include <vector>

#include "vulkan/vulkan.h"

#include "skeleton/renderer/vulkan_context.h"

#ifndef SKELETON_RDNERER_RESOURCE_MANAGERS_H
#define SKELETON_RDNERER_RESOURCE_MANAGERS_H 1

// TODO : Handle buffer memory better
// Handles the life of VkBuffers and their memory
class BufferManager
{
  //=================================================
  // Variables
  //=================================================
private:
  uint64_t m_indexBitMap = 0;
  const uint32_t INDEXBITMAPSIZE = 64;
  std::vector<VkBuffer> m_buffers;
  std::vector<VkDeviceMemory> m_memories;

public:
  VkCommandPool transientPool; // Used for single-time commands

  //=================================================
  // Functions
  //=================================================
public:
  // Initializes the transientPool
  BufferManager();
  // Destroys all buffers and frees their memory
  ~BufferManager();

  // Retrieves the status of a buffer
  bool GetIndexBitMapAt(uint32_t _index);
  // Sets the status of a buffer
  void SetIndexBitMapAt(uint32_t _index, bool _value = true);

  // Retrieves the number of active buffers
  uint32_t GetBufferCount() { return static_cast<uint32_t>(m_buffers.size()); }

  // Retrieves a specific buffer, returns nullptr if one is not found
  const VkBuffer* GetBuffer(uint32_t _index);
  // Retrieves a specific buffer's memory, returns nullptr if none is not found
  const VkDeviceMemory* GetMemory(uint32_t _index);
  // Destroys a specific buffer and frees its memory
  void RemoveAtIndex(uint32_t _index);
  // Retrieves the index of the first available buffer slot
  uint32_t GetFirstAvailableIndex();

  // Creates a new buffer and fills its memory
  uint32_t CreateAndFillBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory, const void* _data,
    VkDeviceSize size, VkBufferUsageFlags _usage);

  // Creates a new buffer and allocates its memory, returns the buffer and memory
  uint32_t CreateBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory, VkDeviceSize _size,
    VkBufferUsageFlags _usage, VkMemoryPropertyFlags _memProperties);
  // Creates a new buffer and allocates its memory
  uint32_t CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage,
    VkMemoryPropertyFlags _memProperties);

  // Copies data into deviceMemory
  void FillBuffer(VkDeviceMemory& memory, const void* _data, VkDeviceSize _size);
  // Copies the data from a source buffer into another
  void CopyBuffer(VkBuffer _src, VkBuffer _dst, VkDeviceSize _size);
  // Finds the first memory property that fits input criteria
  static uint32_t FindMemoryType(uint32_t _mask, VkMemoryPropertyFlags _flags);

}; // BufferManager

// Handles the lives of sklImage_t's
class ImageManager
{
public:
  static std::vector<sklImage_t*> images;

public:
  static void Cleanup();

  static uint32_t CreateImage(uint32_t _width, uint32_t _height, VkFormat _format,
                              VkImageTiling _tiling, VkImageUsageFlags _usage,
                              VkMemoryPropertyFlags _memFlags);
};

//class TextureManager
//{
//public:
//  static std::vector<sklTexture_t*> textures;
//
//public:
//  static uint32_t CreateTexture(const char* _directory);
//};

#endif // !SKELETON_RDNERER_RESOURCE_MANAGERS_H

