
#include "pch.h"
#include "Skeleton/Renderer/ParProgs.h"
#include "Skeleton/Renderer/RenderBackend.h"
#include "Skeleton/Core/FileSystem.h"
#include "Skeleton/Core/Vertex.h"

#include <string>

VkPipeline skeleton::parProg_t::GetPipeline(
	VkPipelineLayout _layout,
	VkRenderPass _renderpass)
{
	// Viewport State
	//=================================================
	VkViewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)vulkanContext.renderExtent.width;
	viewport.height = (float)vulkanContext.renderExtent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissor = {};
	scissor.extent = vulkanContext.renderExtent;
	scissor.offset = { 0, 0 };

	VkPipelineViewportStateCreateInfo viewportStateInfo = {};
	viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateInfo.scissorCount = 1;
	viewportStateInfo.pScissors = &scissor;
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.pViewports = &viewport;

	// Vert Input State
	//=================================================
	const auto vertexInputBindingDesc = skeleton::Vertex::GetBindingDescription();
	const auto vertexInputAttribDescs = skeleton::Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
	vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttribDescs.size());
	vertexInputStateInfo.pVertexAttributeDescriptions = vertexInputAttribDescs.data();
	vertexInputStateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateInfo.pVertexBindingDescriptions = &vertexInputBindingDesc;

	// Input Assembly
	//=================================================
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateInfo = {};
	inputAssemblyStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateInfo.primitiveRestartEnable = VK_FALSE;

	// Rasterizer
	//=================================================
	VkPipelineRasterizationStateCreateInfo rasterStateInfo = {};
	rasterStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	//rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterStateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterStateInfo.rasterizerDiscardEnable = VK_TRUE;
	rasterStateInfo.lineWidth = 1.f;
	rasterStateInfo.depthBiasEnable = VK_FALSE;
	rasterStateInfo.depthClampEnable = VK_FALSE;
	rasterStateInfo.rasterizerDiscardEnable = VK_FALSE;

	// Multisample State
	//=================================================
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {};
	multisampleStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateInfo.sampleShadingEnable = VK_FALSE;

	// Depth State
	//=================================================
	VkPipelineDepthStencilStateCreateInfo depthStateInfo = {};
	depthStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStateInfo.depthTestEnable = VK_TRUE;
	depthStateInfo.depthWriteEnable = VK_TRUE;
	depthStateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStateInfo.depthBoundsTestEnable = VK_FALSE;

	// Color Blend State
	//=================================================
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendStateInfo = {};
	blendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendStateInfo.logicOpEnable = VK_FALSE;
	blendStateInfo.attachmentCount = 1;
	blendStateInfo.pAttachments = &blendAttachmentState;

	// Dynamic states
	//=================================================
	VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
	dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateInfo.dynamicStateCount = 0;
	dynamicStateInfo.pDynamicStates = nullptr;

	// Shader modules
	//=================================================
	VkShaderModule vertModule = vulkanContext.shaders[vertIdx].module;
	VkShaderModule fragModule = vulkanContext.shaders[fragIdx].module;

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Pipeline
	//=================================================
	VkGraphicsPipelineCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pViewportState = &viewportStateInfo;
	createInfo.pVertexInputState = &vertexInputStateInfo;
	createInfo.pInputAssemblyState = &inputAssemblyStateInfo;
	createInfo.pRasterizationState = &rasterStateInfo;
	createInfo.pMultisampleState = &multisampleStateInfo;
	createInfo.pDepthStencilState = &depthStateInfo;
	createInfo.pColorBlendState = &blendStateInfo;
	createInfo.pDynamicState = &dynamicStateInfo;

	createInfo.stageCount = 2;
	createInfo.pStages = shaderStages;

	createInfo.layout = _layout;
	createInfo.renderPass = _renderpass;

	VkPipeline tmpPipeline = VK_NULL_HANDLE;
	SKL_ASSERT_VK(
		vkCreateGraphicsPipelines(vulkanContext.device, nullptr, 1, &createInfo, nullptr, &tmpPipeline),
		"Failed to create graphics pipeline");

	vkDestroyShaderModule(vulkanContext.device, vertModule, nullptr);
	vkDestroyShaderModule(vulkanContext.device, fragModule, nullptr);

	return tmpPipeline;
}

void skeleton::CreateShader(
	const char* _name,
	sklShaderStageFlags _stages)
{
	std::string n(_name);

	uint32_t vertIdx = -1;
	if (_stages & SKL_SHADER_VERT_STAGE)
	{
		vertIdx = skeleton::GetShader(_name, SKL_SHADER_VERT_STAGE);
	}

	uint32_t fragIdx = -1;
	if (_stages & SKL_SHADER_FRAG_STAGE)
	{
		fragIdx = skeleton::GetShader(_name, SKL_SHADER_FRAG_STAGE);
	}

	skeleton::parProg_t prog(_name);
	prog.vertIdx = vertIdx;
	prog.fragIdx = fragIdx;
	skeleton::CreateDescriptorSetLayout(prog);
	vulkanContext.parProgs.push_back(prog);
}

uint32_t skeleton::GetShader(const char* _name, sklShaderStageFlags _stage)
{
	for (uint32_t i = 0; i < vulkanContext.shaders.size(); i++)
	{
		shader_t& shader = vulkanContext.shaders[i];
		if (std::strcmp(shader.name, _name) == 0 && shader.stage == _stage)
		{
			LoadShader(i);
			return i;
		}
	}

	shader_t shader(_name);
	shader.stage = _stage;
	uint32_t index = static_cast<uint32_t>(vulkanContext.shaders.size());
	vulkanContext.shaders.push_back(shader);

	LoadShader(index);
	return index;
}

void skeleton::LoadShader(uint32_t _index)
{
	if (vulkanContext.shaders[_index].module == VK_NULL_HANDLE)
		skeleton::LoadShader(vulkanContext.shaders[_index]);
}

void skeleton::LoadShader(shader_t& _shader)
{
	std::string shaderDirectory = "res\\Shaders\\";
	shaderDirectory.append(_shader.name);
	std::string layoutDirectory(shaderDirectory);

	switch (_shader.stage)
	{
	case SKL_SHADER_VERT_STAGE:
		shaderDirectory.append(".vspv");
		layoutDirectory.append(".vlayout");
		break;
	case SKL_SHADER_FRAG_STAGE:
		shaderDirectory.append(".fspv");
		layoutDirectory.append(".flayout");
		break;
	case SKL_SHADER_COMP_STAGE:
		shaderDirectory.append(".cspv");
		layoutDirectory.append(".flayout");
		break;
	}

	// Shader
	//=================================================
	std::vector<char> shaderSource = skeleton::tools::LoadFile(shaderDirectory.c_str());
	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = shaderSource.size();
	moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSource.data());

	SKL_ASSERT_VK(
		vkCreateShaderModule(vulkanContext.device, &moduleCreateInfo, nullptr, &_shader.module),
		"Failed to create shader module");

	// Layout
	//=================================================
	std::vector<char> layoutSource = skeleton::tools::LoadFile(layoutDirectory.c_str());
	//SKL_PRINT("Layout", "\n:--:%s\n:--:%s", layoutDirectory.c_str(), layoutSource.data());

	std::string layout(layoutSource.data());
	uint32_t i = 0;
	while (layout[i] == 'b' || layout[i] == 's')
	{
		switch (layout[i])
		{
		// buffer
		case 'b':
			_shader.bindings.push_back(SKL_BINDING_BUFFER);
			break;
		// sampler
		case 's':
			_shader.bindings.push_back(SKL_BINDING_SAMPLER);
			break;
		// push constant
		//case 'p':
		//	break;
		}
		i++;
	}
}

void skeleton::CreateDescriptorSetLayout(parProg_t& _program)
{
	CreateDescriptorSetLayout(_program.descriptorSetLayout, _program.vertIdx, _program.fragIdx);
}

void skeleton::CreateDescriptorSetLayout(
	VkDescriptorSetLayout& _layout,
	uint32_t _vertIdx /*= -1*/,
	uint32_t _fragIdx /*= -1*/,
	uint32_t _compIdx /*= -1*/)
{
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	VkDescriptorSetLayoutBinding binding = {};
	binding.descriptorCount = 1;
	binding.pImmutableSamplers = nullptr;

	uint32_t bindingIndex = 0;

	binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	if (_vertIdx != -1)
	{
		skeleton::shader_t& shader = vulkanContext.shaders[_vertIdx];

		for (uint32_t i = 0; i < shader.bindings.size(); i++)
		{
			switch (shader.bindings[i])
			{
			case SKL_BINDING_BUFFER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
			case SKL_BINDING_SAMPLER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
			}
			binding.binding = bindingIndex++;
			bindings.push_back(binding);
		}
	}

	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	if (_fragIdx != -1)
	{
		skeleton::shader_t& shader = vulkanContext.shaders[_fragIdx];

		for (uint32_t i = 0; i < shader.bindings.size(); i++)
		{
			switch (shader.bindings[i])
			{
			case SKL_BINDING_BUFFER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
			case SKL_BINDING_SAMPLER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
			}
			binding.binding = bindingIndex++;
			bindings.push_back(binding);
		}
	}

	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	if (_compIdx != -1)
	{
		skeleton::shader_t& shader = vulkanContext.shaders[_compIdx];

		for (uint32_t i = 0; i < shader.bindings.size(); i++)
		{
			switch (shader.bindings[i])
			{
			case SKL_BINDING_BUFFER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
			case SKL_BINDING_SAMPLER:
				binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
			}
			binding.binding = bindingIndex++;
			bindings.push_back(binding);
		}
	}

	VkDescriptorSetLayoutCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	createInfo.pBindings = bindings.data();

	SKL_ASSERT_VK(
		vkCreateDescriptorSetLayout(vulkanContext.device, &createInfo, nullptr, &_layout),
		"Failed to create descriptor set layout");

}

