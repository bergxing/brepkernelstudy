#include "vulkan_renderer.hpp"

#include "render/vulkan_renderer_detail.hpp"
#include "vulkan_window.hpp"

#include <QVulkanDeviceFunctions>

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace brep::viewer {

void VulkanRenderer::create_descriptors() {
  const VkDevice device = window_->device();

  VkDescriptorSetLayoutBinding bindings[2]{};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags =
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo lci{};
  lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  lci.bindingCount = 2;
  lci.pBindings = bindings;
  dev_->vkCreateDescriptorSetLayout(device, &lci, nullptr, &desc_layout_);

  // Three sets: scene, selection highlight, axis/gizmo. Each needs its own UBO
  // buffer because draws share one command buffer.
  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 3;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = 3;

  VkDescriptorPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pci.maxSets = 3;
  pci.poolSizeCount = 2;
  pci.pPoolSizes = pool_sizes;
  dev_->vkCreateDescriptorPool(device, &pci, nullptr, &desc_pool_);

  VkDescriptorSetLayout layouts[3] = {desc_layout_, desc_layout_, desc_layout_};
  VkDescriptorSetAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool = desc_pool_;
  ai.descriptorSetCount = 3;
  ai.pSetLayouts = layouts;
  VkDescriptorSet sets[3]{};
  dev_->vkAllocateDescriptorSets(device, &ai, sets);
  desc_set_ = sets[0];
  selection_desc_set_ = sets[1];
  axis_desc_set_ = sets[2];

  VkDescriptorImageInfo ii{};
  ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  ii.imageView = albedo_.view;
  ii.sampler = albedo_.sampler;

  VkDescriptorImageInfo sel_ii{};
  sel_ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  sel_ii.imageView = selection_albedo_.view;
  sel_ii.sampler = selection_albedo_.sampler;

  VkDescriptorBufferInfo scene_bi{};
  scene_bi.buffer = ubo_.buffer;
  scene_bi.offset = 0;
  scene_bi.range = sizeof(Ubo);

  VkDescriptorBufferInfo sel_bi{};
  sel_bi.buffer = selection_ubo_.buffer;
  sel_bi.offset = 0;
  sel_bi.range = sizeof(Ubo);

  VkDescriptorBufferInfo axis_bi{};
  axis_bi.buffer = axis_ubo_.buffer;
  axis_bi.offset = 0;
  axis_bi.range = sizeof(Ubo);

  VkWriteDescriptorSet writes[6]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = desc_set_;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &scene_bi;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = desc_set_;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].pImageInfo = &ii;

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = selection_desc_set_;
  writes[2].dstBinding = 0;
  writes[2].descriptorCount = 1;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[2].pBufferInfo = &sel_bi;

  writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[3].dstSet = selection_desc_set_;
  writes[3].dstBinding = 1;
  writes[3].descriptorCount = 1;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[3].pImageInfo = &sel_ii;

  writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[4].dstSet = axis_desc_set_;
  writes[4].dstBinding = 0;
  writes[4].descriptorCount = 1;
  writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[4].pBufferInfo = &axis_bi;

  writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[5].dstSet = axis_desc_set_;
  writes[5].dstBinding = 1;
  writes[5].descriptorCount = 1;
  writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[5].pImageInfo = &ii;

  dev_->vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
}

void VulkanRenderer::create_pipelines() {
  const VkDevice device = window_->device();
  const QString spv_dir = QStringLiteral(BREP_VIEWER_SPV_DIR);

  VkShaderModule vert = load_shader(spv_dir + "/mesh.vert.spv");
  VkShaderModule frag = load_shader(spv_dir + "/mesh.frag.spv");
  VkShaderModule line_vert = load_shader(spv_dir + "/line.vert.spv");
  VkShaderModule line_frag = load_shader(spv_dir + "/line.frag.spv");
  VkShaderModule axis_vert = load_shader(spv_dir + "/axis.vert.spv");
  VkShaderModule axis_frag = load_shader(spv_dir + "/axis.frag.spv");

  VkPipelineLayoutCreateInfo plci{};
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plci.setLayoutCount = 1;
  plci.pSetLayouts = &desc_layout_;
  dev_->vkCreatePipelineLayout(device, &plci, nullptr, &pipeline_layout_);

  enum class PipeKind { Mesh, Line, Axis, PreviewFill };
  auto make_pipeline = [&](VkShaderModule vs, VkShaderModule fs, PipeKind kind,
                           VkPipeline* out) {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs;
    if (kind == PipeKind::Line) {
      binding.stride = sizeof(float) * 3;
      attrs.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
    } else if (kind == PipeKind::Axis || kind == PipeKind::PreviewFill) {
      binding.stride = sizeof(AxisVertexGpu);
      attrs.push_back(
          {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(AxisVertexGpu, pos)});
      attrs.push_back(
          {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(AxisVertexGpu, color)});
    } else {
      binding.stride = sizeof(TriVertexGpu);
      attrs.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TriVertexGpu, pos)});
      attrs.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TriVertexGpu, nrm)});
      attrs.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TriVertexGpu, uv)});
    }

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    const bool lines = kind == PipeKind::Line || kind == PipeKind::Axis;
    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                        : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = (kind == PipeKind::Mesh) ? VK_CULL_MODE_BACK_BIT
                                           : VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = window_->sampleCountFlagBits();

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    // World axes stay visible through the model (CAD-style gizmo feel).
    ds.depthTestEnable = kind == PipeKind::Axis ? VK_FALSE : VK_TRUE;
    ds.depthWriteEnable = kind == PipeKind::Mesh ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = 0xF;
    if (kind == PipeKind::PreviewFill) {
      blend_att.blendEnable = VK_TRUE;
      blend_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      blend_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend_att.colorBlendOp = VK_BLEND_OP_ADD;
      blend_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      blend_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      blend_att.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &blend_att;

    VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                   VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dyn_states;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = pipeline_layout_;
    gp.renderPass = window_->defaultRenderPass();

    if (dev_->vkCreateGraphicsPipelines(device, pipeline_cache_, 1, &gp, nullptr,
                                        out) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateGraphicsPipelines failed");
    }
  };

  make_pipeline(vert, frag, PipeKind::Mesh, &tri_pipeline_);
  make_pipeline(line_vert, line_frag, PipeKind::Line, &line_pipeline_);
  make_pipeline(axis_vert, axis_frag, PipeKind::Axis, &axis_pipeline_);
  make_pipeline(axis_vert, axis_frag, PipeKind::PreviewFill,
                &preview_fill_pipeline_);

  dev_->vkDestroyShaderModule(device, vert, nullptr);
  dev_->vkDestroyShaderModule(device, frag, nullptr);
  dev_->vkDestroyShaderModule(device, line_vert, nullptr);
  dev_->vkDestroyShaderModule(device, line_frag, nullptr);
  dev_->vkDestroyShaderModule(device, axis_vert, nullptr);
  dev_->vkDestroyShaderModule(device, axis_frag, nullptr);
}

}  // namespace brep::viewer
