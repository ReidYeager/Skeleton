
#include "pch.h"
#include "skeleton/renderer/shader_program.h"

#include <string>
#include <inttypes.h>

#include "skeleton/renderer/render_backend.h"
#include "skeleton/core/file_system.h"
#include "skeleton/core/vertex.h"

VkPipeline shaderProgram_t::GetPipeline(VkShaderModule _vertMod, VkShaderModule _fragMod)
{
  if (pipeline != VK_NULL_HANDLE)
  {
    return pipeline;
  }

  pipeline = CreatePipeline(_vertMod, _fragMod, pipelineLayout, pipelineSettingsFlags);
  return pipeline;
}

uint32_t GetShaderProgram(const char* _name, sklShaderStageFlags _stages,
                          uint64_t _pipelineSettings /*= Skl_Pipeline_Default_Settings*/)
{
  for (uint32_t i = 0; i < vulkanContext.shaderPrograms.size(); i++)
  {
    shaderProgram_t& prog = vulkanContext.shaderPrograms[i];
    if (prog.pipelineSettingsFlags == _pipelineSettings && std::strcmp(_name, prog.name) == 0)
    {
      return i;
    }
  }

  uint32_t index = static_cast<uint32_t>(vulkanContext.shaderPrograms.size());
  SKL_PRINT_SIMPLE("Creating new program for \"%s\"(%"PRIu64") at %u",
                   _name, _pipelineSettings, index);
  CreateShaderProgram(_name, _stages, _pipelineSettings);
  return index;
}

void CreateShaderProgram(const char* _name, sklShaderStageFlags _stages,
                         uint64_t _pipelineSettings /*= Skl_Pipeline_Default_Settings*/)
{
  uint32_t vertIdx = -1;
  if (_stages & Skl_Shader_Vert_Stage)
  {
    vertIdx = GetShader(_name, Skl_Shader_Vert_Stage);
  }

  uint32_t fragIdx = -1;
  if (_stages & Skl_Shader_Frag_Stage)
  {
    fragIdx = GetShader(_name, Skl_Shader_Frag_Stage);
  }

  shaderProgram_t prog(_name);
  prog.pipelineSettingsFlags = _pipelineSettings;
  prog.vertIdx = vertIdx;
  prog.fragIdx = fragIdx;
  CreateDescriptorSetLayout(prog);
  vulkanContext.shaderPrograms.push_back(prog);
}

VkPipeline CreatePipeline(VkShaderModule _vertModule, VkShaderModule _fragModule,
                          VkPipelineLayout _pipeLayout, uint64_t _pipelineSettings)
{
  // Initialize all VkCreateInfo structs

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
  viewportStateInfo.sType =VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateInfo.scissorCount = 1;
  viewportStateInfo.pScissors = &scissor;
  viewportStateInfo.viewportCount = 1;
  viewportStateInfo.pViewports = &viewport;

  // Vert Input State
  //=================================================
  const auto vertexInputBindingDesc = vertex_t::GetBindingDescription();
  const auto vertexInputAttribDescs = vertex_t::GetAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputStateInfo = {};
  vertexInputStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputStateInfo.vertexAttributeDescriptionCount = 
      static_cast<uint32_t>(vertexInputAttribDescs.size());
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
  rasterStateInfo.rasterizerDiscardEnable = VK_TRUE;
  rasterStateInfo.lineWidth = 1.f;
  rasterStateInfo.depthBiasEnable = VK_FALSE;
  rasterStateInfo.depthClampEnable = VK_FALSE;
  rasterStateInfo.rasterizerDiscardEnable = VK_FALSE;

  switch (_pipelineSettings & Skl_Cull_Mode_Bits)
  {
  case Skl_Cull_Mode_None:  rasterStateInfo.cullMode = VK_CULL_MODE_NONE;           break;
  case Skl_Cull_Mode_Back:  rasterStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;       break;
  case Skl_Cull_Mode_Front: rasterStateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;      break;
  case Skl_Cull_Mode_Both:  rasterStateInfo.cullMode = VK_CULL_MODE_FRONT_AND_BACK; break;
  }

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
  blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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
  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
  vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = _vertModule;
  vertShaderStageInfo.pName = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
  fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = _fragModule;
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

  createInfo.layout = _pipeLayout;
  createInfo.renderPass = vulkanContext.renderPass;

  VkPipeline tmpPipeline = VK_NULL_HANDLE;
  SKL_ASSERT_VK(
      vkCreateGraphicsPipelines(vulkanContext.device, nullptr, 1,
                                &createInfo, nullptr, &tmpPipeline),
      "Failed to create graphics pipeline");

  vkDestroyShaderModule(vulkanContext.device, _vertModule, nullptr);
  vkDestroyShaderModule(vulkanContext.device, _fragModule, nullptr);

  return tmpPipeline;
}

uint32_t GetShader(const char* _name, sklShaderStageFlags _stage)
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

void LoadShader(uint32_t _index)
{
  if (vulkanContext.shaders[_index].module == VK_NULL_HANDLE)
    LoadShader(vulkanContext.shaders[_index]);
}

void LoadShader(shader_t& _shader)
{
  // Construct shader directories from its name
  std::string shaderDirectory = "res\\Shaders\\";
  shaderDirectory.append(_shader.name);
  std::string layoutDirectory(shaderDirectory);

  switch (_shader.stage)
  {
  case Skl_Shader_Vert_Stage:
    shaderDirectory.append(".vspv");
    layoutDirectory.append(".vlayout");
    break;
  case Skl_Shader_Frag_Stage:
    shaderDirectory.append(".fspv");
    layoutDirectory.append(".flayout");
    break;
  case Skl_Shader_Comp_Stage:
    shaderDirectory.append(".cspv");
    layoutDirectory.append(".flayout");
    break;
  }

  // Load and create shader module
  std::vector<char> shaderSource = LoadFile(shaderDirectory.c_str());
  VkShaderModuleCreateInfo moduleCreateInfo = {};
  moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  moduleCreateInfo.codeSize = shaderSource.size();
  moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSource.data());

  SKL_ASSERT_VK(
    vkCreateShaderModule(vulkanContext.device, &moduleCreateInfo, nullptr, &_shader.module),
    "Failed to create shader module");

  ExtractShaderBindings(layoutDirectory.c_str(), _shader);
}

// TODO : Make this function a lot more robust
void ExtractShaderBindings(const char* _layoutDirectory, shader_t& _shader)
{
  std::vector<char> layoutSource = LoadFile(_layoutDirectory);
  std::string layout(layoutSource.data());
  uint32_t i = 0;
  while (layout[i] == 'b' || layout[i] == 's')
  {
    switch (layout[i])
    {
      // buffer
    case 'b':
      _shader.bindings.push_back(Skl_Binding_Buffer);
      break;
      // sampler
    case 's':
      _shader.bindings.push_back(Skl_Binding_Sampler);
      break;
      // push constant
      //case 'p':
      //	break;
    }
    i++;
  }
}

void CreateDescriptorSetLayout(shaderProgram_t& _program)
{
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  VkDescriptorSetLayoutBinding binding = {};
  binding.descriptorCount = 1;
  binding.pImmutableSamplers = nullptr;

  uint32_t bindingIndex = 0; // Tracks what binding is being set across shader types

  // Create binding information for each shader type
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  if (_program.vertIdx != -1)
  {
    shader_t& shader = vulkanContext.shaders[_program.vertIdx];

    for (uint32_t i = 0; i < shader.bindings.size(); i++)
    {
      switch (shader.bindings[i])
      {
      case Skl_Binding_Buffer:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
      case Skl_Binding_Sampler:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
      }
      binding.binding = bindingIndex++;
      bindings.push_back(binding);
      _program.bindings.push_back(shader.bindings[i]);
    }
  }

  binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
  if (_program.fragIdx != -1)
  {
    shader_t& shader = vulkanContext.shaders[_program.fragIdx];

    for (uint32_t i = 0; i < shader.bindings.size(); i++)
    {
      switch (shader.bindings[i])
      {
      case Skl_Binding_Buffer:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
      case Skl_Binding_Sampler:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
      }
      binding.binding = bindingIndex++;
      bindings.push_back(binding);
      _program.bindings.push_back(shader.bindings[i]);
    }
  }

  binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  if (_program.compIdx != -1)
  {
    shader_t& shader = vulkanContext.shaders[_program.compIdx];

    for (uint32_t i = 0; i < shader.bindings.size(); i++)
    {
      switch (shader.bindings[i])
      {
      case Skl_Binding_Buffer:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
      case Skl_Binding_Sampler:
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
      }
      binding.binding = bindingIndex++;
      bindings.push_back(binding);
      _program.bindings.push_back(shader.bindings[i]);
    }
  }

  VkDescriptorSetLayoutCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  createInfo.pBindings = bindings.data();
  SKL_PRINT("ShaderProgram", "%s has %u bindings", _program.name,
            static_cast<uint32_t>(bindings.size()));

  SKL_ASSERT_VK(vkCreateDescriptorSetLayout(vulkanContext.device, &createInfo,
                                            nullptr, &_program.descriptorSetLayout),
                "Failed to create descriptor set layout");

  // Pipeline Layout
  //=================================================
  VkPipelineLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &_program.descriptorSetLayout;
  layoutInfo.pushConstantRangeCount = 0;
  layoutInfo.pPushConstantRanges = nullptr;

  SKL_ASSERT_VK(
    vkCreatePipelineLayout(vulkanContext.device, &layoutInfo, nullptr, &_program.pipelineLayout),
    "Failed to create pipeline layout");
}

size_t PadBufferDataForShader(size_t _original)
{
  size_t alignment = vulkanContext.gpu.properties.limits.minUniformBufferOffsetAlignment;
  size_t alignedSize = _original;
  if (alignment > 0)
  {
    // Rounds alignedSize up to the nearest multiple of alignment
    alignedSize = (alignedSize + alignment - 1) & ~(alignment - 1);
  }
  return alignedSize;
}

