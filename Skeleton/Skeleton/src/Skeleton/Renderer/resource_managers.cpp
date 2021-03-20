
#include "pch.h"
#include "skeleton/renderer/resource_managers.h"

#include "skeleton/core/debug_tools.h"
#include "skeleton/core/file_system.h"

//=================================================
// Buffer Manager
//=================================================

#pragma region BufferManager

VkCommandPool BufferManager::transientPool;

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

uint32_t BufferManager::CreateAndFillBuffer(const void* _data, VkDeviceSize _size,
                                            VkBufferUsageFlags _usage)
{
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingMemory;
  uint32_t stagingBufferIndex =
    CreateBuffer(stagingBuffer, stagingMemory, _size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  FillBuffer(stagingMemory, _data, _size);

  uint32_t index = CreateBuffer(_size, _usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  CopyBuffer(stagingBuffer, m_buffers[index], _size);

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

void BufferManager::CopyBufferToImage(VkBuffer _buffer, VkImage _image, uint32_t _width,
                                      uint32_t _height)
{
  VkCommandBuffer command = vulkanContext.BeginSingleTimeCommand(BufferManager::transientPool);

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.layerCount = 1;
  region.imageSubresource.baseArrayLayer = 0;

  region.imageOffset = { 0, 0, 0 };
  region.imageExtent = { _width, _height, 1 };

  vkCmdCopyBufferToImage(command, _buffer, _image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                         &region);

  vulkanContext.EndSingleTimeCommand(command, BufferManager::transientPool,
                                     vulkanContext.transferQueue);
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
  return static_cast<uint32_t>(images.size() - 1);
}

void ImageManager::TransitionImageLayout(VkImage _image, VkFormat _format,
                                         VkImageLayout _oldLayout, VkImageLayout _newLayout)
{
  VkCommandBuffer transitionCommand =
      vulkanContext.BeginSingleTimeCommand(vulkanContext.graphicsCommandPool);

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = _oldLayout;
  barrier.newLayout = _newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = _image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;

  VkPipelineStageFlagBits srcStage;
  VkPipelineStageFlagBits dstStage;


  if (_oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
    && _newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
  {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (_oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    && _newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
  {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else
  {
    SKL_LOG(SKL_ERROR, "Not a handled image transition");
  }

  vkCmdPipelineBarrier(transitionCommand, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1,
    &barrier);

  vulkanContext.EndSingleTimeCommand(transitionCommand, vulkanContext.graphicsCommandPool,
    vulkanContext.graphicsQueue);
}

VkImageView ImageManager::CreateImageView(const VkFormat _format, VkImageAspectFlags _aspect,
                                          const VkImage& _image)
{
  VkImageViewCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  createInfo.subresourceRange.levelCount = 1;
  createInfo.subresourceRange.baseMipLevel = 0;
  createInfo.subresourceRange.layerCount = 1;
  createInfo.subresourceRange.baseArrayLayer = 0;
  createInfo.subresourceRange.aspectMask = _aspect;
  createInfo.image = _image;
  createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  createInfo.format = _format;

  VkImageView tmpView;
  SKL_ASSERT_VK(
      vkCreateImageView(vulkanContext.device, &createInfo, nullptr, &tmpView),
      "Failed to create image view");

  return tmpView;
}

VkSampler ImageManager::CreateSampler()
{
  VkSamplerCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  createInfo.magFilter = VK_FILTER_LINEAR;
  createInfo.minFilter = VK_FILTER_LINEAR;
  createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  createInfo.unnormalizedCoordinates = VK_FALSE;
  createInfo.compareEnable = VK_FALSE;
  createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  createInfo.mipLodBias = 0.0f;
  createInfo.minLod = 0.0f;
  createInfo.maxLod = 0.0f;

  // TODO : Make this conditional based on the physicalDevice's capabilities
  // (Includes physical device selection)
  createInfo.anisotropyEnable = VK_TRUE;
  createInfo.maxAnisotropy = vulkanContext.gpu.properties.limits.maxSamplerAnisotropy;

  VkSampler tmpSampler;
  SKL_ASSERT_VK(
      vkCreateSampler(vulkanContext.device, &createInfo, nullptr, &tmpSampler),
      "Failed to create texture sampler");

  return tmpSampler;
}

//=================================================
// Texture Manager
//=================================================

std::vector<sklTexture_t*> TextureManager::textures;

uint32_t TextureManager::CreateTexture(const char* _directory, BufferManager* _bufferManager)
{
  for (uint32_t i = 0; i < textures.size(); i++)
  {
    if (std::strcmp(textures[i]->directory, _directory) == 0)
    {
      return i;
    }
  }

  SKL_PRINT_SIMPLE("Creating a texture from %s", _directory);
  sklTexture_t* tex = new sklTexture_t(_directory);

  // Image loading
  //=================================================
  int width, height;
  void* imageFile = LoadImageFile(_directory, width, height);
  VkDeviceSize size = width * height * 4;

  // Staging buffer
  //=================================================
  VkDeviceMemory stagingMemory;
  VkBuffer stagingBuffer;

  uint32_t stagingIndex = _bufferManager->CreateBuffer(
    stagingBuffer, stagingMemory, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  _bufferManager->FillBuffer(stagingMemory, imageFile, size);

  DestroyImageFile(imageFile);

  // Image creation
  //=================================================
  uint32_t imageIdx = ImageManager::CreateImage(
    static_cast<uint32_t>(width), static_cast<uint32_t>(height), VK_FORMAT_R8G8B8A8_UNORM,
    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  sklImage_t* img = ImageManager::images[imageIdx];

  ImageManager::TransitionImageLayout(img->image, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  BufferManager::CopyBufferToImage(stagingBuffer, img->image, static_cast<uint32_t>(width),
                                   static_cast<uint32_t>(height));
  ImageManager::TransitionImageLayout(img->image, VK_FORMAT_R8G8B8A8_UNORM,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  _bufferManager->RemoveAtIndex(stagingIndex);

  img->view = ImageManager::CreateImageView(VK_FORMAT_R8G8B8A8_UNORM,
                                            VK_IMAGE_ASPECT_COLOR_BIT, img->image);

  img->sampler = ImageManager::CreateSampler();

  tex->imageIndex = imageIdx;
  textures.push_back(tex);
  return static_cast<uint32_t>(textures.size() - 1);
}
