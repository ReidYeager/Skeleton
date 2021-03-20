
#include "pch.h"
#include "skeleton/renderer/renderer.h"

#include <set>
#include <string>
#include <chrono>

#include "stb/stb_image.h"

#include "skeleton/renderer/render_backend.h"
#include "skeleton/core/debug_tools.h"
#include "skeleton/core/time.h"
#include "skeleton/core/file_system.h"
#include "skeleton/core/vertex.h"
#include "skeleton/renderer/shader_program.h"
#include "skeleton/core/mesh.h"

Renderer::Renderer(const std::vector<const char*>& _extraExtensions, SDL_Window* _window)
{
  backend = new SklRenderBackend(_window, _extraExtensions);
  bufferManager = backend->bufferManager;
}

Renderer::~Renderer()
{
  delete(bufferManager);
  delete(backend);
}

void Renderer::RenderFrame()
{
  vkWaitForFences(vulkanContext.device, 1, &backend->flightFences[backend->currentFrame],
                  VK_TRUE, UINT64_MAX);

  uint32_t imageIndex;
  vkAcquireNextImageKHR(vulkanContext.device, backend->swapchain, UINT64_MAX, 
                        backend->imageAvailableSemaphores[backend->currentFrame], VK_NULL_HANDLE,
                        &imageIndex);

  if (backend->imageIsInFlightFences[imageIndex] != VK_NULL_HANDLE)
  {
    vkWaitForFences(vulkanContext.device, 1, &backend->imageIsInFlightFences[imageIndex],
                    VK_TRUE, UINT64_MAX);
  }
  backend->imageIsInFlightFences[imageIndex] = backend->flightFences[backend->currentFrame];

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = &backend->imageAvailableSemaphores[backend->currentFrame];
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &backend->commandBuffers[imageIndex];
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &backend->renderCompleteSemaphores[backend->currentFrame];

  vkResetFences(vulkanContext.device, 1, &backend->flightFences[backend->currentFrame]);

  SKL_ASSERT_VK(
      vkQueueSubmit(vulkanContext.graphicsQueue, 1, &submitInfo,
                    backend->flightFences[backend->currentFrame]),
      "Failed to submit draw command");

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &backend->renderCompleteSemaphores[backend->currentFrame];
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &backend->swapchain;
  presentInfo.pImageIndices = &imageIndex;

  vkQueuePresentKHR(vulkanContext.presentQueue, &presentInfo);

  backend->currentFrame = (backend->currentFrame + 1) % MAX_FLIGHT_IMAGE_COUNT;
}

void Renderer::CreateDescriptorSet(shaderProgram_t& _prog, sklRenderable_t& _renderable)
{
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = backend->descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &_prog.descriptorSetLayout;

  SKL_ASSERT_VK(
      vkAllocateDescriptorSets(vulkanContext.device, &allocInfo, &_prog.descriptorSet),
      "Failed to allocate descriptor set");

  uint32_t bufferidx = 0, imageidx = 0;
  std::vector<VkDescriptorBufferInfo> bufferInfos;
  std::vector<VkDescriptorImageInfo> imageInfos;
  std::vector< VkWriteDescriptorSet> writeSets(_prog.bindings.size());

  for (uint32_t i = 0; i < _prog.bindings.size(); i++)
  {
    if (_prog.bindings[i] == Skl_Binding_Buffer)
    {
      bufferidx++;
    }
    else
    {
      imageidx++;
    }
  }


  bufferInfos.resize(bufferidx);
  imageInfos.resize(imageidx);
  bufferidx = 0;
  imageidx = 0;

  for (uint32_t i = 0; i < _prog.bindings.size(); i++)
  {
    if (_prog.bindings[i] == Skl_Binding_Buffer)
    {
      sklBuffer_t* buf = new sklBuffer_t();
      bufferManager->CreateBuffer(
          buf->buffer, buf->memory, sizeof(MVPMatrices),
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      _renderable.buffers.push_back(buf);

      bufferInfos[bufferidx].buffer = buf->buffer;
      bufferInfos[bufferidx].offset = 0;
      bufferInfos[bufferidx].range = VK_WHOLE_SIZE;

      writeSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeSets[i].dstSet = _prog.descriptorSet;
      writeSets[i].dstBinding = i;
      writeSets[i].dstArrayElement = 0;
      writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writeSets[i].descriptorCount = 1;
      writeSets[i].pBufferInfo = &bufferInfos[bufferidx];
      writeSets[i].pImageInfo = nullptr;
      writeSets[i].pTexelBufferView = nullptr;

      bufferidx++;
    }
    else
    {
      uint32_t texIdx = TextureManager::CreateTexture(((i % 2 == 0) ? "res/AltImage.png" : "res/TestImage.png"), bufferManager);
      uint32_t rendImageIdx = TextureManager::textures[texIdx]->imageIndex;
      sklImage_t* imageA = ImageManager::images[rendImageIdx];
      //uint32_t rendImageIdx = static_cast<uint32_t>(_renderable.images.size());
      _renderable.images.push_back(imageA);

      VkDescriptorImageInfo imageInfoA = {};
      imageInfos[imageidx].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfos[imageidx].sampler = ImageManager::images[rendImageIdx]->sampler;
      imageInfos[imageidx].imageView = ImageManager::images[rendImageIdx]->view;

      writeSets[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeSets[i].dstSet = _prog.descriptorSet;
      writeSets[i].dstBinding = i;
      writeSets[i].dstArrayElement = 0;
      writeSets[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      writeSets[i].descriptorCount = 1;
      writeSets[i].pBufferInfo = nullptr;
      writeSets[i].pImageInfo = &imageInfos[imageidx];
      writeSets[i].pTexelBufferView = nullptr;

      imageidx++;
    }
  }

  vkUpdateDescriptorSets(vulkanContext.device, static_cast<uint32_t>(writeSets.size()),
                         writeSets.data(), 0, nullptr);
}

//=================================================
// CreateRenderer
//=================================================

void Renderer::CreateRenderer()
{
  backend->InitializeRenderComponents();
}

void Renderer::CleanupRenderer()
{
  backend->CleanupRenderComponents();
}

void Renderer::RecreateRenderer()
{
  vkDeviceWaitIdle(vulkanContext.device);
  CleanupRenderer();
  backend->CreateRenderComponents();
}

//=================================================
// Initialization
//=================================================


//=================================================
// CreateRenderer Functions
//=================================================

void Renderer::RecordCommandBuffers()
{
  shaderProgram_t* shaderProgram;

  uint32_t comandCount = static_cast<uint32_t>(backend->commandBuffers.size());
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

  VkClearValue clearValues[2] = {};
  clearValues[0].color = { 0.20784313725f, 0.21568627451f, 0.21568627451f, 1.0f };
  clearValues[1].depthStencil = { 1, 0 };

  VkRenderPassBeginInfo rpBeginInfo = {};
  rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpBeginInfo.renderPass = vulkanContext.renderPass;
  rpBeginInfo.clearValueCount = 2;
  rpBeginInfo.pClearValues = clearValues;
  rpBeginInfo.renderArea.extent = vulkanContext.renderExtent;
  rpBeginInfo.renderArea.offset = { 0, 0 };

  for (uint32_t i = 0; i < comandCount; i++)
  {
    rpBeginInfo.framebuffer = backend->frameBuffers[i];
    SKL_ASSERT_VK(
      vkBeginCommandBuffer(backend->commandBuffers[i], &beginInfo),
      "Failed to begin a command buffer");

    vkCmdBeginRenderPass(backend->commandBuffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // TODO : Only bind pipelines once
    for (uint32_t j = 0; j < vulkanContext.renderables.size(); j++)
    {
      shaderProgram =
          &vulkanContext.shaderPrograms[vulkanContext.renderables[j].shaderProgramIndex];
      vkCmdBindPipeline(
        backend->commandBuffers[i],
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shaderProgram->GetPipeline(vulkanContext.shaders[shaderProgram->vertIdx].module,
                             vulkanContext.shaders[shaderProgram->fragIdx].module));
      vkCmdBindDescriptorSets(backend->commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              shaderProgram->pipelineLayout, 0, 1, &shaderProgram->descriptorSet,
                              0, nullptr);
      VkDeviceSize offset[] = { 0 };

      mesh_t& mesh = vulkanContext.renderables[j].mesh;
      vkCmdBindVertexBuffers(backend->commandBuffers[i], 0, 1,
                             bufferManager->GetBuffer(mesh.vertexBufferIndex), offset);
      vkCmdBindIndexBuffer(backend->commandBuffers[i],
          *(bufferManager->GetBuffer(mesh.indexBufferIndex)), 0, VK_INDEX_TYPE_UINT32);
      vkCmdDrawIndexed(backend->commandBuffers[i], static_cast<uint32_t>(mesh.indices.size()),
                       1, 0, 0, 0);
    }

    vkCmdEndRenderPass(backend->commandBuffers[i]);

    SKL_ASSERT_VK(
      vkEndCommandBuffer(backend->commandBuffers[i]),
      "Failed to end command buffer");
  }
}

//=================================================
// Helpers
//=================================================

void Renderer::CreateModelBuffers()
{
  // Create uniform buffer
  VkDeviceSize mvpSize = sizeof(MVPMatrices);
  bufferManager->CreateBuffer(
      mvpBuffer, mvpMemory, mvpSize,
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

