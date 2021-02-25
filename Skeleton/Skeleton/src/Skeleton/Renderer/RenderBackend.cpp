
#include "pch.h"
#include "RenderBackend.h"

SklVulkanContext_t vulkanContext;

void SklVulkanContext_t::Cleanup()
{
	SKL_PRINT("Vulkan Context", "Cleanup =================================================");
	for (uint32_t i = 0; i < parProgs.size(); i++)
	{
		vkDestroyPipeline(device, parProgs[i].pipeline, nullptr);
		vkDestroyPipelineLayout(device, parProgs[i].pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, parProgs[i].descriptorSetLayout, nullptr);
	}

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyDevice(device, nullptr);
}
