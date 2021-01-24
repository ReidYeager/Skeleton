
#include "pch.h"
#include "Skeleton/Core/Common.h"
#include "BufferManager.h"

skeleton::BufferManager::~BufferManager()
{
	uint32_t bufferCount = GetBufferCount();
	for (uint32_t i = 0; i < bufferCount; i++)
	{
		if (GetIndexBitMapAt(i))
		{
			vkFreeMemory(*m_device, m_memories[i], nullptr);
			vkDestroyBuffer(*m_device, m_buffers[i], nullptr);
		}
	}
}

bool skeleton::BufferManager::GetIndexBitMapAt(
	uint32_t _index)
{
	return (m_indexBitMap & (1 << _index));
}

void skeleton::BufferManager::SetIndexBitMapAt(
	uint32_t _index,
	bool _value /*= true*/)
{
	if (_value)
	{
		m_indexBitMap |= (1 << _index);
	}
	else
	{
		m_indexBitMap &= ~(1 << _index);
	}
}

const VkBuffer* skeleton::BufferManager::GetBuffer(
	uint32_t _index)
{
	if (GetIndexBitMapAt(_index))
	{
		return &m_buffers[_index];
	}
	else
	{
		return nullptr;
	}
}

const VkDeviceMemory* skeleton::BufferManager::GetMemory(
	uint32_t _index)
{
	if (GetIndexBitMapAt(_index))
	{
		return &m_memories[_index];
	}
	else
	{
		return nullptr;
	}
}

void skeleton::BufferManager::RemoveAtIndex(
	uint32_t _index)
{
	SetIndexBitMapAt(_index, false);
	vkFreeMemory(*m_device, m_memories[_index], nullptr);
	vkDestroyBuffer(*m_device, m_buffers[_index], nullptr);
}

uint32_t skeleton::BufferManager::GetFirstAvailableIndex()
{
	uint32_t arraySize = static_cast<uint32_t>(m_buffers.size());
	uint32_t i = 0;
	while (i <= arraySize && i < 64)
	{
		if (GetIndexBitMapAt(i) == 0)
		{
			return i;
		}

		i++;
	}
	SKL_LOG(SKL_ERROR, "Failed to find an available index");
	return -1;
}

uint32_t skeleton::BufferManager::CreateAndFillBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;
	uint32_t stagingBufferIndex = CreateBuffer(
		stagingBuffer,
		stagingMemory,
		_size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	FillBuffer(stagingMemory, _data, _size);

	uint32_t index = CreateBuffer(
		_buffer,
		_memory,
		_size,
		_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	CopyBuffer(stagingBuffer, _buffer, _size);

	RemoveAtIndex(stagingBufferIndex);
	return index;
}

uint32_t skeleton::BufferManager::CreateBuffer(
	VkBuffer& _buffer,
	VkDeviceMemory& _memory,
	VkDeviceSize _size,
	VkBufferUsageFlags _usage,
	VkMemoryPropertyFlags _memProperties)
{
	uint32_t index = GetFirstAvailableIndex();
	SetIndexBitMapAt(index);

	VkBufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.usage = _usage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = _size;

	VkBuffer tmpBuffer;
	SKL_ASSERT_VK(
		vkCreateBuffer(*m_device, &createInfo, nullptr, &tmpBuffer),
		"Failed to create vert buffer");

	if (index < static_cast<uint32_t>(m_buffers.size()))
	{
		m_buffers[index] = tmpBuffer;
	}
	else
	{
		m_buffers.push_back(tmpBuffer);
	}
	_buffer = m_buffers[index];

	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(*m_device, _buffer, &memReq);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memReq.memoryTypeBits,
		_memProperties);

	VkDeviceMemory tmpMemory;
	SKL_ASSERT_VK(
		vkAllocateMemory(*m_device, &allocInfo, nullptr, &tmpMemory),
		"Failed to allocate vert memory");

	if (index < static_cast<uint32_t>(m_memories.size()))
	{
		m_memories[index] = tmpMemory;
	}
	else
	{
		m_memories.push_back(tmpMemory);
	}
	_memory = m_memories[index];

	vkBindBufferMemory(*m_device, _buffer, _memory, 0);

	return index;
}

void skeleton::BufferManager::FillBuffer(
	VkDeviceMemory& _memory,
	const void* _data,
	VkDeviceSize _size)
{
	void* tmpData;
	vkMapMemory(*m_device, _memory, 0, _size, 0, &tmpData);
	memcpy(tmpData, _data, static_cast<size_t>(_size));
	vkUnmapMemory(*m_device, _memory);
}

void skeleton::BufferManager::CopyBuffer(
	VkBuffer _src,
	VkBuffer _dst,
	VkDeviceSize _size)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_device->transientPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer transferCommand;
	SKL_ASSERT_VK(
		vkAllocateCommandBuffers(*m_device, &allocInfo, &transferCommand),
		"Failed to create transient command buffer");

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(transferCommand, &beginInfo);

	VkBufferCopy region = {};
	region.size = _size;
	region.dstOffset = 0;
	region.srcOffset = 0;

	vkCmdCopyBuffer(transferCommand, _src, _dst, 1, &region);

	vkEndCommandBuffer(transferCommand);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommand;

	vkQueueSubmit(m_device->transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_device->transferQueue);

	vkFreeCommandBuffers(*m_device, m_device->transientPool, 1, &transferCommand);
}

uint32_t skeleton::BufferManager::FindMemoryType(
	uint32_t _mask,
	VkMemoryPropertyFlags _flags)
{
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(m_device->physicalDevice, &props);

	for (uint32_t i = 0; i < props.memoryTypeCount; i++)
	{
		if (_mask & (1 << i) && (props.memoryTypes[i].propertyFlags & _flags) == _flags)
		{
			return i;
		}
	}

	SKL_ASSERT_VK(
		VK_ERROR_UNKNOWN,
		"Failed to find a suitable memory type");
	return 0;
}

