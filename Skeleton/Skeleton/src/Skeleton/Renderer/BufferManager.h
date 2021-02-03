#pragma once

#include <vector>

#include "vulkan/vulkan.h"

#include "RendererBackend.h"

namespace skeleton
{
class BufferManager
{
//=================================================
// Variables
//=================================================
private:
	// TODO : Upgrade this to allow more buffers
	uint64_t m_indexBitMap = 0;
	#define INDEXBITMAPSIZE 64
	std::vector<VkBuffer> m_buffers;
	std::vector<VkDeviceMemory> m_memories;

public:
	VkCommandPool transientPool;

//=================================================
// Functions
//=================================================
public:
	BufferManager()
	{
		CreateCommandPool(transientPool, vulkanContext.transferIdx);
	}
	~BufferManager();

	bool GetIndexBitMapAt(uint32_t _index);
	void SetIndexBitMapAt(uint32_t _index, bool _value = true);

	uint32_t GetBufferCount() { return static_cast<uint32_t>(m_buffers.size()); }

	const VkBuffer* GetBuffer(uint32_t _index);
	const VkDeviceMemory* GetMemory(uint32_t _index);
	void RemoveAtIndex(uint32_t _index);
	uint32_t GetFirstAvailableIndex();

	uint32_t CreateAndFillBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		const void* _data,
		VkDeviceSize size,
		VkBufferUsageFlags _usage);

	uint32_t CreateBuffer(
		VkBuffer& _buffer,
		VkDeviceMemory& _memory,
		VkDeviceSize _size,
		VkBufferUsageFlags _usage,
		VkMemoryPropertyFlags _memProperties);

	uint32_t CreateBuffer(
		VkDeviceSize _size,
		VkBufferUsageFlags _usage,
		VkMemoryPropertyFlags _memProperties);

	void FillBuffer(
		VkDeviceMemory& memory,
		const void* _data,
		VkDeviceSize _size);

	void CopyBuffer(
		VkBuffer _src,
		VkBuffer _dst,
		VkDeviceSize _size);

	uint32_t FindMemoryType(
		uint32_t _mask,
		VkMemoryPropertyFlags _flags);

}; // BufferManager
} // namespace skeleton

