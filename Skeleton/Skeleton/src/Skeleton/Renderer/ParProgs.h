
//=================================================
// PARALLEL PROGRAMS / SHADERS
//=================================================

#ifndef PARPROGS_H
#define PARPROGS_H

#include <vector>
#include "vulkan/vulkan.h"

typedef uint32_t sklFlags;
typedef sklFlags sklShaderStageFlags;
typedef sklFlags sklShaderBindingFlags;
typedef enum sklShaderStageFlagBits
{
	SKL_SHADER_VERT_STAGE = 0x1,
	SKL_SHADER_FRAG_STAGE = 0x2,
	SKL_SHADER_COMP_STAGE = 0x4
}sklShaderStageFlagBits;
typedef enum sklShaderBindingFlagBits
{
	SKL_BINDING_BUFFER,
	SKL_BINDING_SAMPLER,
	SKL_BINDING_MAX_COUNT
}sklShaderBindingFlagBits;

namespace skeleton
{
struct shader_t
{
	shader_t(const char* _name) :
		name(_name),
		stage(SKL_SHADER_VERT_STAGE),
		module(VK_NULL_HANDLE) {}

	const char* name;
	sklShaderStageFlags stage;
	VkShaderModule module;
	// buffer components
	std::vector<sklShaderBindingFlags> bindings;
};

// Parallel Program
struct parProg_t
{
	parProg_t(const char* _name) :
		name(_name),
		vertIdx(-1),
		fragIdx(-1),
		compIdx(-1),
		pipeline(VK_NULL_HANDLE) {}

	VkPipeline GetPipeline(
		VkShaderModule _vertMod,
		VkShaderModule _fragMod);

	const char* name;
	uint32_t vertIdx;
	uint32_t fragIdx;
	uint32_t compIdx;

	std::vector<sklShaderBindingFlags> bindings;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

VkPipeline CreatePipeline(
	VkShaderModule _vertModule,
	VkShaderModule _fragModule,
	VkPipelineLayout _pipeLayout);

void CreateShader(
	const char* _name,
	sklShaderStageFlags _stages);

uint32_t GetShader(const char* _name, sklShaderStageFlags _stage);

void LoadShader(uint32_t _index);
void LoadShader(shader_t& _shader);

void CreateDescriptorSetLayout(
	parProg_t& _program);

size_t PadBufferDataForShader(
	size_t _original);

} // namespace skeleton
#endif // !PARPROGS_H

