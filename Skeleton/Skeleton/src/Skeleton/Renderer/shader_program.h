
#include <vector>

#include "vulkan/vulkan.h"

//=================================================
// PARALLEL PROGRAMS / SHADERS
//=================================================

#ifndef SKELETON_RENDERER_SHADER_PROGRAM_H
#define SKELETON_RENDERER_SHADER_PROGRAM_H 1

// TODO : Move to more general RENDERER file
typedef uint32_t sklFlags;
typedef sklFlags sklShaderStageFlags;
typedef sklFlags sklShaderBindingFlags;

typedef uint64_t sklExtendedFlags;
typedef sklExtendedFlags sklPipelineSettingFlags;

typedef enum sklShaderStageFlagBits
{
  Skl_Shader_Vert_Stage = 0x1,
  Skl_Shader_Frag_Stage = 0x2,
  Skl_Shader_Comp_Stage = 0x4
}sklShaderStageFlagBits;

typedef enum sklShaderBindingFlagBits
{
  Skl_Binding_Buffer,
  Skl_Binding_Sampler,
  Skl_Binding_Max_Count
}sklShaderBindingFlagBits;

typedef enum sklPipelineSettingFlagBits
{
  // TODO : Implement options for all pipeline settings
  // Polygon cull mode
  Skl_Cull_Mode_None  = (0 << 0),
  Skl_Cull_Mode_Front = (1 << 0),
  Skl_Cull_Mode_Back  = (2 << 0),
  Skl_Cull_Mode_Both  = (3 << 0),
  Skl_Cull_Mode_Bits  = (3 << 0),

  // Default values for each setting
  Skl_Pipeline_Default_Settings = Skl_Cull_Mode_Front
}sklPipelineSettingFlagBits;

// Stores basic shader information for use in shaderProgram_t
struct shader_t
{
  shader_t(const char* _name) :
    name(_name),
    stage(Skl_Shader_Vert_Stage),
    module(VK_NULL_HANDLE) {}

  const char* name;
  sklShaderStageFlags stage;
  VkShaderModule module;
  // buffer components
  std::vector<sklShaderBindingFlags> bindings;
};

// Program to run in parallel on the GPU
struct shaderProgram_t
{
  shaderProgram_t(const char* _name) :
      name(_name), pipelineSettingsFlags(Skl_Pipeline_Default_Settings), vertIdx(-1), fragIdx(-1),
      compIdx(-1), pipeline(VK_NULL_HANDLE), descriptorSet(VK_NULL_HANDLE),
      descriptorSetLayout(VK_NULL_HANDLE), pipelineLayout(VK_NULL_HANDLE) {}

  // Retrieves or creates a graphicsPipeline
  // based on the given shaders and the shaderProgram's pipeline settings
  VkPipeline GetPipeline(VkShaderModule _vertMod, VkShaderModule _fragMod);

  const char* name;
  uint64_t pipelineSettingsFlags;

  uint32_t vertIdx;
  uint32_t fragIdx;
  uint32_t compIdx;

  std::vector<sklShaderBindingFlags> bindings;
  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorSet descriptorSet;
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;
};

// Creates a graphicsPipeline based on the given shaders and pipelineSettingsFlags
VkPipeline CreatePipeline(VkShaderModule _vertModule, VkShaderModule _fragModule,
                          VkPipelineLayout _pipeLayout, uint64_t _pipelineSettingsBits);

// Finds or creates a shaderProgram with the given information
uint32_t GetShaderProgram(const char* _name, sklShaderStageFlags _stages,
                          uint64_t _pipelineSettings = Skl_Pipeline_Default_Settings);

// Creates a new shaderProgram and its shaders with the given information
void CreateShaderProgram(const char* _name, sklShaderStageFlags _stages,
                         uint64_t _pipelineSettings = Skl_Pipeline_Default_Settings);

// Retrieves the index of a shader from the VulkanContext
uint32_t GetShader(const char* _name, sklShaderStageFlags _stage);

// Loads a shader file from disk and extracts its bindings
void LoadShader(uint32_t _index);
void LoadShader(shader_t& _shader);

// Retrieves the shader's bindings and puts them into the shader_t
void ExtractShaderBindings(const char* _layoutDirectory, shader_t& _shader);

// Gathers bindings from the program's shaders and creates a descriptorSetLayout and pipelineLayout
void CreateDescriptorSetLayout(shaderProgram_t& _program);

// Pads a buffer to fit the GPU's memory alignment
// Returns the padded size
size_t PadBufferDataForShader(size_t _original);

#endif // !SKELETON_RENDERER_SHADER_PROGRAM_H

