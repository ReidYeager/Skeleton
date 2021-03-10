
#pragma hdrstop
#include "pch.h"
#include <set>
#include <string>
#include <chrono>

#include "stb/stb_image.h"

#include "Renderer.h"
#include "RendererBackend.h"
#include "Skeleton/Core/Common.h"
#include "Skeleton/Core/FileSystem.h"
#include "Skeleton/Core/Vertex.h"
#include "Skeleton/Renderer/ParProgs.h"
#include "Skeleton/Core/Mesh.h"

//=================================================
// Initialize & Cleanup
//=================================================

// Initializes the renderer in its entirety
Renderer::Renderer(
	const std::vector<const char*>& _extraExtensions,
	SDL_Window* _window)
{
	backend = new SklRendererBackend(_window, _extraExtensions);
	backend->CreateInstance();
	backend->CreateDevice();
	bufferManager = new BufferManager();
	CreateCommandPools();
}

// Cleans up all vulkan objects
Renderer::~Renderer()
{
	vkDeviceWaitIdle(vulkanContext.device);

	vkDestroyDescriptorSetLayout(vulkanContext.device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(vulkanContext.device, descriptorPool, nullptr);

	CleanupRenderer();

	vkDestroyCommandPool(vulkanContext.device, graphicsPool, nullptr);

	delete(bufferManager);
	vulkanContext.Cleanup();
	vkDestroySurfaceKHR(backend->instance, backend->surface, nullptr);
	vkDestroyInstance(backend->instance, nullptr);
}

//=================================================
// RenderFrame
//=================================================

void Renderer::RenderFrame()
{
	vkWaitForFences(vulkanContext.device, 1, &flightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;
	vkAcquireNextImageKHR(vulkanContext.device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (imageIsInFlightFences[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(vulkanContext.device, 1, &imageIsInFlightFences[imageIndex], VK_TRUE, UINT64_MAX);
	}
	imageIsInFlightFences[imageIndex] = flightFences[currentFrame];

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];

	vkResetFences(vulkanContext.device, 1, &flightFences[currentFrame]);

	SKL_ASSERT_VK(
		vkQueueSubmit(vulkanContext.graphicsQueue, 1, &submitInfo, flightFences[currentFrame]),
		"Failed to submit draw command");

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;

	vkQueuePresentKHR(vulkanContext.presentQueue, &presentInfo);

	currentFrame = (currentFrame + 1) % MAX_FLIGHT_IMAGE_COUNT;
}

void Renderer::CreateDescriptorPool()
{
	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 3;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 6;

	VkDescriptorPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = 2;
	createInfo.pPoolSizes = poolSizes;
	createInfo.maxSets = 3;

	SKL_ASSERT_VK(
		vkCreateDescriptorPool(vulkanContext.device, &createInfo, nullptr, &descriptorPool),
		"Failed to create descriptor pool");
}

void Renderer::CreateDescriptorSet(
	parProg_t& _prog,
	sklRenderable_t& _renderable)
{
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
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
		if (_prog.bindings[i] == SKL_BINDING_BUFFER)
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
		if (_prog.bindings[i] == SKL_BINDING_BUFFER)
		{
			sklBuffer_t* buf = new sklBuffer_t();
			bufferManager->CreateBuffer(
				buf->buffer,
				buf->memory,
				sizeof(MVPMatrices),
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
			sklImage_t* imageA = new sklImage_t();
			CreateTextureImage(
				((i % 2 == 0) ? "res/AltImage.png" : "res/TestImage.png"),
				imageA->image,
				imageA->view,
				imageA->memory,
				imageA->sampler);
			uint32_t rendImageIdx = static_cast<uint32_t>(_renderable.images.size());
			_renderable.images.push_back(imageA);

			VkDescriptorImageInfo imageInfoA = {};
			imageInfos[imageidx].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[imageidx].sampler = _renderable.images[rendImageIdx]->sampler;
			imageInfos[imageidx].imageView = _renderable.images[rendImageIdx]->view;

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

	vkUpdateDescriptorSets(vulkanContext.device, static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, nullptr);
}

//=================================================
// CreateRenderer
//=================================================

void Renderer::CreateRenderer()
{
	CreateSwapchain();
	CreateRenderpass();

	CreateDepthImage();

	CreateFrameBuffers();

	// TODO : Move camera to Application
	cam.yaw = -90.f;
	cam.position.z = 3;
	cam.UpdateProjection(vulkanContext.renderExtent.width / float(vulkanContext.renderExtent.height));

	//CreateModelBuffers();
	CreateDescriptorPool();
	//CreateDescriptorSet(vulkanContext.parProgs[0]);
	CreateSyncObjects();
	CreateCommandBuffers();
	//RecordCommandBuffers();
}

void Renderer::CleanupRenderer()
{
	for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
	{
		vkDestroyFence(vulkanContext.device, flightFences[i], nullptr);
		vkDestroySemaphore(vulkanContext.device, imageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(vulkanContext.device, renderCompleteSemaphores[i], nullptr);
	}

	for (uint32_t i = 0; i < (uint32_t)frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(vulkanContext.device, frameBuffers[i], nullptr);
	}

	vkDestroyImage(vulkanContext.device, depthImage, nullptr);
	vkFreeMemory(vulkanContext.device, depthMemory, nullptr);
	vkDestroyImageView(vulkanContext.device, depthImageView, nullptr);

	//for (uint32_t i = 0; i < vulkanContext.parProgs.size(); i++)
	//{
	//	parProg_t& shader = vulkanContext.parProgs[i];
	//	vkDestroyPipeline(vulkanContext.device, shader.GetPipeline(), nullptr);
	//	vkDestroyPipelineLayout(vulkanContext.device, shader.pipelineLayout, nullptr);
	//}
	//vkDestroyRenderPass(vulkanContext.device, vulkanContext.renderPass, nullptr);

	for (const auto& view : swapchainImageViews)
	{
		vkDestroyImageView(vulkanContext.device, view, nullptr);
	}

	vkDestroySwapchainKHR(vulkanContext.device, swapchain, nullptr);
}

void Renderer::RecreateRenderer()
{
	vkDeviceWaitIdle(vulkanContext.device);
	CleanupRenderer();
	CreateRenderer();
}

//=================================================
// Initialization
//=================================================

void Renderer::CreateCommandPools()
{
	CreateCommandPool(graphicsPool, vulkanContext.graphicsIdx);
}

void Renderer::CreateSyncObjects()
{
	imageIsInFlightFences.resize(swapchainImages.size(), VK_NULL_HANDLE);
	flightFences.resize(MAX_FLIGHT_IMAGE_COUNT);
	imageAvailableSemaphores.resize(MAX_FLIGHT_IMAGE_COUNT);
	renderCompleteSemaphores.resize(MAX_FLIGHT_IMAGE_COUNT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (uint32_t i = 0; i < MAX_FLIGHT_IMAGE_COUNT; i++)
	{
		SKL_ASSERT_VK(
			vkCreateFence(vulkanContext.device, &fenceInfo, nullptr, &flightFences[i]),
			"Failed to create sync fence");
		SKL_ASSERT_VK(
			vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]),
			"Failed to create image semaphore");
		SKL_ASSERT_VK(
			vkCreateSemaphore(vulkanContext.device, &semaphoreInfo, nullptr, &renderCompleteSemaphores[i]),
			"Failed to create render semaphore");
	}
}

void Renderer::CreateCommandBuffers()
{
	uint32_t bufferCount = static_cast<uint32_t>(swapchainImages.size());
	commandBuffers.resize(bufferCount);

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = bufferCount;
	allocInfo.commandPool = graphicsPool;

	SKL_ASSERT_VK(
		vkAllocateCommandBuffers(vulkanContext.device, &allocInfo, commandBuffers.data()),
		"Failed to allocate command buffers");
}

//=================================================
// CreateRenderer Functions
//=================================================

void Renderer::CreateSwapchain()
{
	// Find the best surface format
	VkSurfaceFormatKHR formatInfo = vulkanContext.gpu.surfaceFormats[0];
	for (const auto& p : vulkanContext.gpu.surfaceFormats)
	{
		if (p.format == VK_FORMAT_R32G32B32A32_SFLOAT && p.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT)
		{
			formatInfo = p;
			break;
		}
	}

	// Find the best present mode
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const auto& p : vulkanContext.gpu.presentModes)
	{
		if (p == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = p;
			break;
		}
	}

	// Get the device's extent
	VkSurfaceCapabilitiesKHR& capabilities = vulkanContext.gpu.surfaceCapabilities;
	VkExtent2D extent;
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		extent = capabilities.currentExtent;
	}
	else
	{
		uint32_t width, height;
		SDL_GetWindowSize(backend->window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));
		extent.width = glm::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		extent.height = glm::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
	}

	// Choose an image count
	uint32_t imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
	{
		imageCount = capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = backend->surface;
	createInfo.clipped = VK_TRUE;
	createInfo.imageArrayLayers = 1;
	createInfo.imageFormat = formatInfo.format;
	createInfo.imageColorSpace = formatInfo.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.minImageCount = imageCount;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (vulkanContext.graphicsIdx != vulkanContext.presentIdx)
	{
		uint32_t sharedIndices[] = { vulkanContext.graphicsIdx, vulkanContext.presentIdx };
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = sharedIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	createInfo.presentMode = presentMode;

	SKL_ASSERT_VK(
		vkCreateSwapchainKHR(vulkanContext.device, &createInfo, nullptr, &swapchain),
		"Failed to create swapchain");

	swapchainFormat = formatInfo.format;
	vulkanContext.renderExtent = extent;

	vkGetSwapchainImagesKHR(vulkanContext.device, swapchain, &imageCount, nullptr);
	swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(vulkanContext.device, swapchain, &imageCount, swapchainImages.data());

	swapchainImageViews.resize(imageCount);
	for (uint32_t i = 0; i < imageCount; i++)
	{
		swapchainImageViews[i] = CreateImageView(swapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT, swapchainImages[i]);
	}
}

void Renderer::CreateRenderpass()
{
	VkAttachmentDescription colorDesc = {};
	colorDesc.format = swapchainFormat;
	colorDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	colorDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	colorDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	VkAttachmentReference colorRef;
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthDesc = {};
	depthDesc.format = FindDepthFormat();
	depthDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	depthDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthRef = {};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkAttachmentDescription attachments[] = {colorDesc, depthDesc};
	VkRenderPassCreateInfo creteInfo = {};
	creteInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	creteInfo.attachmentCount = 2;
	creteInfo.pAttachments = attachments;
	creteInfo.subpassCount = 1;
	creteInfo.pSubpasses = &subpass;
	creteInfo.dependencyCount = 1;
	creteInfo.pDependencies = &dependency;

	SKL_ASSERT_VK(
		vkCreateRenderPass(vulkanContext.device, &creteInfo, nullptr, &vulkanContext.renderPass),
		"Failed to create renderpass");
}

void Renderer::CreateDepthImage()
{
	VkFormat depthFormat = FindDepthFormat();
	CreateImage(
		vulkanContext.renderExtent.width,
		vulkanContext.renderExtent.height,
		depthFormat,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		depthImage,
		depthMemory);

	depthImageView = CreateImageView(depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImage);
}

void Renderer::CreateFrameBuffers()
{
	VkFramebufferCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = vulkanContext.renderPass;
	createInfo.layers = 1;
	createInfo.width = vulkanContext.renderExtent.width;
	createInfo.height = vulkanContext.renderExtent.height;

	uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());
	frameBuffers.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++)
	{
		VkImageView attachments[] = {swapchainImageViews[i], depthImageView};
		createInfo.attachmentCount = 2;
		createInfo.pAttachments = attachments;

		SKL_ASSERT_VK(
			vkCreateFramebuffer(vulkanContext.device, &createInfo, nullptr, &frameBuffers[i]),
			"Failed to create a framebuffer");
	}
}

void Renderer::RecordCommandBuffers()
{
	m_boundParProg = 0;
	parProg_t* parProg;

	uint32_t comandCount = static_cast<uint32_t>(commandBuffers.size());
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkClearValue clearValues[2] = {};
	clearValues[0].color = {0.20784313725f, 0.21568627451f, 0.21568627451f, 1.0f};
	clearValues[1].depthStencil = {1, 0};

	VkRenderPassBeginInfo rpBeginInfo = {};
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = vulkanContext.renderPass;
	rpBeginInfo.clearValueCount = 2;
	rpBeginInfo.pClearValues = clearValues;
	rpBeginInfo.renderArea.extent = vulkanContext.renderExtent;
	rpBeginInfo.renderArea.offset = {0, 0};

	for (uint32_t i = 0; i < comandCount; i++)
	{
		rpBeginInfo.framebuffer = frameBuffers[i];
		SKL_ASSERT_VK(
			vkBeginCommandBuffer(commandBuffers[i], &beginInfo),
			"Failed to begin a command buffer");

		vkCmdBeginRenderPass(commandBuffers[i], &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		// TODO : Only bind pipelines once
		for (uint32_t j = 0; j < vulkanContext.renderables.size(); j++)
		{
			parProg = &vulkanContext.parProgs[vulkanContext.renderables[j].parProgIndex];
			vkCmdBindPipeline(
				commandBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				parProg->GetPipeline(vulkanContext.shaders[parProg->vertIdx].module, vulkanContext.shaders[parProg->fragIdx].module));
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, parProg->pipelineLayout, 0, 1, &parProg->descriptorSet, 0, nullptr);
			VkDeviceSize offset[] = { 0 };

			mesh_t& mesh = vulkanContext.renderables[j].mesh;
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &mesh.vertexBuffer, offset);
			vkCmdBindIndexBuffer(commandBuffers[i], mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(commandBuffers[i]);

		SKL_ASSERT_VK(
			vkEndCommandBuffer(commandBuffers[i]),
			"Failed to end command buffer");
	}
}

//=================================================
// Helpers
//=================================================

VkImageView Renderer::CreateImageView(
	VkFormat _format,
	VkImageAspectFlags _aspect,
	const VkImage& _image)
{
	VkImageViewCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.subresourceRange.levelCount   = 1;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.layerCount     = 1;
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

void Renderer::CreateModelBuffers()
{
	// Create uniform buffer
	VkDeviceSize mvpSize = sizeof(MVPMatrices);
	bufferManager->CreateBuffer(
		mvpBuffer,
		mvpMemory,
		mvpSize,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void Renderer::CreateImage(
	uint32_t _width,
	uint32_t _height,
	VkFormat _format,
	VkImageTiling _tiling,
	VkImageUsageFlags _usage,
	VkMemoryPropertyFlags _memFlags,
	VkImage& _image,
	VkDeviceMemory& _memory)
{
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
		vkCreateImage(vulkanContext.device, &createInfo, nullptr, &_image),
		"Failed to create texture image");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vulkanContext.device, _image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = bufferManager->FindMemoryType(
		memRequirements.memoryTypeBits,
		_memFlags);

	SKL_ASSERT_VK(
		vkAllocateMemory(vulkanContext.device, &allocInfo, nullptr, &_memory),
		"Failed to allocate texture memory");

	vkBindImageMemory(vulkanContext.device, _image, _memory, 0);
}

void Renderer::CreateTextureImage(
	const char* _directory,
	VkImage& _image,
	VkImageView& _view,
	VkDeviceMemory& _memory,
	VkSampler& _sampler)
{
	// Image loading
	//=================================================
	int width, height;
	void* img = LoadImageFile(_directory, width, height);
	VkDeviceSize size = width * height * 4;

	// Staging buffer
	//=================================================

	VkDeviceMemory stagingMemory;
	VkBuffer stagingBuffer;

	uint32_t stagingIndex =	bufferManager->CreateBuffer(
		stagingBuffer,
		stagingMemory,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	bufferManager->FillBuffer(stagingMemory, img, size);

	DestroyImageFile(img);

	// Image creation
	//=================================================

	CreateImage(
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height),
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		_image,
		_memory);

	TransitionImageLayout(
		_image,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(
		stagingBuffer,
		_image,
		static_cast<uint32_t>(width),
		static_cast<uint32_t>(height));
	TransitionImageLayout(
		_image,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	bufferManager->RemoveAtIndex(stagingIndex);

	_view = CreateImageView(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, _image);

	_sampler = CreateSampler();
}

VkSampler Renderer::CreateSampler()
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

void Renderer::TransitionImageLayout(
	VkImage _iamge,
	VkFormat _format,
	VkImageLayout _old,
	VkImageLayout _new)
{
	VkCommandBuffer transitionCommand = BeginSingleTimeCommand(graphicsPool);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = _old;
	barrier.newLayout = _new;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = _iamge;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;

	VkPipelineStageFlagBits srcStage;
	VkPipelineStageFlagBits dstStage;


	if (_old == VK_IMAGE_LAYOUT_UNDEFINED && _new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && _new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
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

	vkCmdPipelineBarrier(transitionCommand, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	EndSingleTimeCommand(transitionCommand, graphicsPool, vulkanContext.graphicsQueue);
}

void Renderer::CopyBufferToImage(
	VkBuffer _buffer,
	VkImage _image,
	uint32_t _width,
	uint32_t _height)
{
	VkCommandBuffer command = BeginSingleTimeCommand(bufferManager->transientPool);

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;

	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageSubresource.baseArrayLayer = 0;

	region.imageOffset = {0, 0, 0};
	region.imageExtent = {_width, _height, 1};

	vkCmdCopyBufferToImage(
		command,
		_buffer,
		_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&region);

	EndSingleTimeCommand(command, bufferManager->transientPool, vulkanContext.transferQueue);
}

VkFormat Renderer::FindDepthFormat()
{
	return FindSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat Renderer::FindSupportedFormat(
	const std::vector<VkFormat>& _candidates,
	VkImageTiling _tiling,
	VkFormatFeatureFlags _features)
{
	for (VkFormat format : _candidates)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(vulkanContext.gpu.device, format, &properties);

		if (_tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & _features) == _features)
			return format;
		else if (_tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & _features) == _features)
			return format;
	}
	throw std::runtime_error("Failed to find a suitable format");
}

