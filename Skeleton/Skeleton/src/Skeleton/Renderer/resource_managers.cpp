
#include "pch.h"
#include "skeleton/renderer/resource_managers.h"

#include "skeleton/core/debug_tools.h"
#include "skeleton/core/file_system.h"

//=================================================
// Buffer Manager
//=================================================

#pragma region BufferManager
BufferManager::BufferManager()
{
  VkCommandPoolCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.queueFamilyIndex = vulkanContext.transferIdx;
  createInfo.flags = 0;
  SKL_ASSERT_VK(
    vkCreateCommandPool(vulkanContext.device, &createInfo, nullptr, &transientPool),
    "Failed to create command pool with flags %u", 0);
}

BufferManager::~BufferManager()
{
  vkDeviceWaitIdle(vulkanContext.device);

  uint32_t bufferCount = GetBufferCount();
  for (uint32_t i = 0; i < bufferCount; i++)
  {
    if (GetIndexBitMapAt(i))
    {
      vkFreeMemory(vulkanContext.device, m_memories[i], nullptr);
      vkDestroyBuffer(vulkanContext.device, m_buffers[i], nullptr);
    }
  }

  vkDestroyCommandPool(vulkanContext.device, transientPool, nullptr);
}

bool BufferManager::GetIndexBitMapAt(uint32_t _index)
{
  return (m_indexBitMap & (1i64 << _index));
}

void BufferManager::SetIndexBitMapAt(uint32_t _index, bool _value /*= true*/)
{
  if (_value)
  {
    m_indexBitMap |= (1i64 << _index);
  }
  else
  {
    m_indexBitMap &= ~(1i64 << _index);
  }
}

const VkBuffer* BufferManager::GetBuffer(uint32_t _index)
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

const VkDeviceMemory* BufferManager::GetMemory(uint32_t _index)
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

void BufferManager::RemoveAtIndex(uint32_t _index)
{
  SetIndexBitMapAt(_index, false);
  vkFreeMemory(vulkanContext.device, m_memories[_index], nullptr);
  vkDestroyBuffer(vulkanContext.device, m_buffers[_index], nullptr);
}

uint32_t BufferManager::GetFirstAvailableIndex()
{
  uint32_t arraySize = static_cast<uint32_t>(m_buffers.size());
  uint32_t i = 0;
  while (i <= arraySize && i < INDEXBITMAPSIZE)
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

uint32_t BufferManager::CreateAndFillBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory,
  const void* _data, VkDeviceSize _size,
  VkBufferUsageFlags _usage)
{
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;
  uint32_t stagingBufferIndex =
    CreateBuffer(stagingBuffer, stagingMemory, _size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  FillBuffer(stagingMemory, _data, _size);

  uint32_t index = CreateBuffer(_buffer, _memory, _size, _usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CopyBuffer(stagingBuffer, _buffer, _size);

  RemoveAtIndex(stagingBufferIndex);
  return index;
}

uint32_t BufferManager::CreateBuffer(VkBuffer& _buffer, VkDeviceMemory& _memory,
  VkDeviceSize _size, VkBufferUsageFlags _usage,
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
    vkCreateBuffer(vulkanContext.device, &createInfo, nullptr, &tmpBuffer),
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
  vkGetBufferMemoryRequirements(vulkanContext.device, _buffer, &memReq);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, _memProperties);

  VkDeviceMemory tmpMemory;
  SKL_ASSERT_VK(
    vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &tmpMemory),
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

  vkBindBufferMemory(vulkanContext.device, _buffer, _memory, 0);

  return index;
}

uint32_t BufferManager::CreateBuffer(VkDeviceSize _size, VkBufferUsageFlags _usage,
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
    vkCreateBuffer(vulkanContext.device, &createInfo, nullptr, &tmpBuffer),
    "Failed to create vert buffer");

  if (index < static_cast<uint32_t>(m_buffers.size()))
  {
    m_buffers[index] = tmpBuffer;
  }
  else
  {
    m_buffers.push_back(tmpBuffer);
  }

  VkMemoryRequirements memReq;
  vkGetBufferMemoryRequirements(vulkanContext.device, m_buffers[index], &memReq);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, _memProperties);

  VkDeviceMemory tmpMemory;
  SKL_ASSERT_VK(
    vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &tmpMemory),
    "Failed to allocate vert memory");

  if (index < static_cast<uint32_t>(m_memories.size()))
  {
    m_memories[index] = tmpMemory;
  }
  else
  {
    m_memories.push_back(tmpMemory);
  }

  vkBindBufferMemory(vulkanContext.device, m_buffers[index], m_memories[index], 0);

  return index;
}

void BufferManager::FillBuffer(VkDeviceMemory& _memory, const void* _data, VkDeviceSize _size)
{
  void* tmpData;
  vkMapMemory(vulkanContext.device, _memory, 0, _size, 0, &tmpData);
  memcpy(tmpData, _data, static_cast<size_t>(_size));
  vkUnmapMemory(vulkanContext.device, _memory);
}

void BufferManager::CopyBuffer(VkBuffer _src, VkBuffer _dst, VkDeviceSize _size)
{
  VkCommandBuffer copyCommand = vulkanContext.BeginSingleTimeCommand(transientPool);

  VkBufferCopy region = {};
  region.size = _size;
  region.dstOffset = 0;
  region.srcOffset = 0;

  vkCmdCopyBuffer(copyCommand, _src, _dst, 1, &region);

  vulkanContext.EndSingleTimeCommand(copyCommand, transientPool, vulkanContext.transferQueue);
}

uint32_t BufferManager::FindMemoryType(uint32_t _mask, VkMemoryPropertyFlags _flags)
{
  const VkPhysicalDeviceMemoryProperties& props = vulkanContext.gpu.memProperties;

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
#pragma endregion

//=================================================
// ImageManager
//=================================================

std::vector<sklImage_t*> ImageManager::images;

void ImageManager::Cleanup()
{
  for (const auto& i : images)
  {
    vkDestroyImage(vulkanContext.device, i->image, nullptr);
    vkFreeMemory(vulkanContext.device, i->memory, nullptr);
    vkDestroyImageView(vulkanContext.device, i->view, nullptr);
    vkDestroySampler(vulkanContext.device, i->sampler, nullptr);
    free(i);
  }
}

uint32_t ImageManager::CreateImage(uint32_t _width, uint32_t _height, VkFormat _format,
                                   VkImageTiling _tiling, VkImageUsageFlags _usage,
                                   VkMemoryPropertyFlags _memFlags)
{
  sklImage_t* img = new sklImage_t();
  VkImageCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  createInfo.imageType = VK_IMAGE_TYPE_2D;
  createInfo.extent.width = _width;
  createInfo.extent.height = _height;
  createInfo.extent.depth = 1;
  createInfo.mipLevels = 1;
  createInfo.arrayLayers = 1;
  createInfo.format = _format;
  createInfo.tiling = _tiling;
  createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  createInfo.usage = _usage;
  createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  createInfo.flags = 0;

  SKL_ASSERT_VK(
    vkCreateImage(vulkanContext.device, &createInfo, nullptr, &img->image),
    "Failed to create texture image");

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(vulkanContext.device, img->image, &memRequirements);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = BufferManager::FindMemoryType(memRequirements.memoryTypeBits,
                                                            _memFlags);

  SKL_ASSERT_VK(
    vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &img->memory),
    "Failed to allocate texture memory");

  vkBindImageMemory(vulkanContext.device, img->image, img->memory, 0);
  img->format = _format;
  images.push_back(img);
  SKL_PRINT_SIMPLE("Created image %u", static_cast<uint32_t>(images.size() - 1));
  return static_cast<uint32_t>(images.size() - 1);
}

//=================================================
// Texture Manager
//=================================================

//std::vector<sklTexture_t*> TextureManager::textures;
//
//uint32_t TextureManager::CreateTexture(const char* _directory)
//{
//  sklTexture_t* tex = new sklTexture_t(_directory);
//  textures.push_back(tex);
//  return static_cast<uint32_t>(textures.size() - 1);
//}
