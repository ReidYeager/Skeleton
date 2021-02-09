
#include "pch.h"
#include "Skeleton/Renderer/ParProgs.h"
#include "Skeleton/Renderer/RenderBackend.h"
#include "Skeleton/Core/FileSystem.h"

#include <string>

VkPipeline skeleton::parProg_t::GetPipeline()
{
	return VK_NULL_HANDLE;
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
	std::string dir = "res\\Shaders\\";
	dir.append(_shader.name);

	switch (_shader.stage)
	{
	case SKL_SHADER_VERT_STAGE: dir.append(".vspv"); break;
	case SKL_SHADER_FRAG_STAGE: dir.append(".fspv"); break;
	case SKL_SHADER_COMP_STAGE: dir.append(".cspv"); break;
	}

	std::vector<char> shaderSource = skeleton::tools::LoadFile(dir.c_str());

	VkShaderModuleCreateInfo moduleCreateInfo = {};
	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.codeSize = shaderSource.size();
	moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSource.data());

	SKL_ASSERT_VK(
		vkCreateShaderModule(vulkanContext.device, &moduleCreateInfo, nullptr, &_shader.module),
		"Failed to create shader module");
}
