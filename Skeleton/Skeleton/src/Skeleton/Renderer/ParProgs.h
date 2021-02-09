
//=================================================
// PARALLEL PROGRAMS / SHADERS
//=================================================

#ifndef PARPROGS_H
#define PARPROGS_H

//#include <string>
#include "vulkan/vulkan.h"

namespace skeleton
{
typedef uint32_t sklFlags;
typedef sklFlags sklShaderStageFlags;
typedef enum sklShaderStageFlagBits
{
	SKL_SHADER_VERT_STAGE	= 0x1,
	SKL_SHADER_FRAG_STAGE	= 0x2,
	SKL_SHADER_COMP_STAGE	= 0x4
}sklShaderStageFlagBits;

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
	// buffer/sampler/pushConstant layout
};

// Parallel Program
struct parProg_t
{
	parProg_t(const char* _name) :
		name(_name),
		vertIdx(-1),
		fragIdx(-1),
		compIdx(-1) {}

	VkPipeline GetPipeline();

	const char* name;
	uint32_t vertIdx;
	uint32_t fragIdx;
	uint32_t compIdx;
};

void CreateShader(
	const char* _name,
	sklShaderStageFlags _stages);

uint32_t GetShader(const char* _name, sklShaderStageFlags _stage);

void LoadShader(uint32_t _index);
void LoadShader(shader_t& _shader);



} // namespace skeleton
#endif // !PARPROGS_H

