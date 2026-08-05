#include "vulkan_renderer.hpp"

#include "vulkan_window.hpp"

#include <QFile>
#include <QVulkanDeviceFunctions>

#include <cstring>
#include <stdexcept>

namespace brep::viewer {
namespace {

struct TriVertexGpu {
  float pos[3];
  float nrm[3];
};

}  // namespace

VulkanRenderer::VulkanRenderer(VulkanWindow* window) : window_(window) {}

void VulkanRenderer::set_meshes(TriangleMesh triangles, EdgeMesh edges) {
  triangles_ = std::move(triangles);
  edges_ = std::move(edges);
  meshes_dirty_ = true;
}

uint32_t VulkanRenderer::find_memory_type(uint32_t type_bits,
                                          VkMemoryPropertyFlags props) const {
  VkPhysicalDeviceMemoryProperties mem_props;
  window_->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(
      window_->physicalDevice(), &mem_props);
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
    if ((type_bits & (1u << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & props) == props) {
      return i;
    }
  }
  throw std::runtime_error("VulkanRenderer: no suitable memory type");
}

VulkanRenderer::GpuBuffer VulkanRenderer::create_buffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props) {
  const VkDevice device = window_->device();
  GpuBuffer out{};
  out.size = size;

  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = size;
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (dev_->vkCreateBuffer(device, &bi, nullptr, &out.buffer) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateBuffer failed");
  }

  VkMemoryRequirements req{};
  dev_->vkGetBufferMemoryRequirements(device, out.buffer, &req);

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
  if (dev_->vkAllocateMemory(device, &ai, nullptr, &out.memory) != VK_SUCCESS) {
    throw std::runtime_error("vkAllocateMemory failed");
  }
  dev_->vkBindBufferMemory(device, out.buffer, out.memory, 0);
  return out;
}

void VulkanRenderer::destroy_buffer(GpuBuffer& b) {
  const VkDevice device = window_->device();
  if (!dev_ || device == VK_NULL_HANDLE) {
    b = {};
    return;
  }
  if (b.buffer) dev_->vkDestroyBuffer(device, b.buffer, nullptr);
  if (b.memory) dev_->vkFreeMemory(device, b.memory, nullptr);
  b = {};
}

VkShaderModule VulkanRenderer::load_shader(const QString& file_name) {
  QFile file(file_name);
  if (!file.open(QIODevice::ReadOnly)) {
    throw std::runtime_error("failed to open shader: " + file_name.toStdString());
  }
  const QByteArray blob = file.readAll();
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = static_cast<size_t>(blob.size());
  ci.pCode = reinterpret_cast<const uint32_t*>(blob.constData());
  VkShaderModule module{VK_NULL_HANDLE};
  if (dev_->vkCreateShaderModule(window_->device(), &ci, nullptr, &module) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateShaderModule failed");
  }
  return module;
}

void VulkanRenderer::initResources() {
  dev_ = window_->vulkanInstance()->deviceFunctions(window_->device());
  const VkDevice device = window_->device();

  VkPipelineCacheCreateInfo pc{};
  pc.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  dev_->vkCreatePipelineCache(device, &pc, nullptr, &pipeline_cache_);

  ubo_ = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  create_descriptors();
  create_pipelines();
  upload_meshes();
}

void VulkanRenderer::create_descriptors() {
  const VkDevice device = window_->device();

  VkDescriptorSetLayoutBinding binding{};
  binding.binding = 0;
  binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  binding.descriptorCount = 1;
  binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo lci{};
  lci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  lci.bindingCount = 1;
  lci.pBindings = &binding;
  dev_->vkCreateDescriptorSetLayout(device, &lci, nullptr, &desc_layout_);

  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_size.descriptorCount = 1;
  VkDescriptorPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pci.maxSets = 1;
  pci.poolSizeCount = 1;
  pci.pPoolSizes = &pool_size;
  dev_->vkCreateDescriptorPool(device, &pci, nullptr, &desc_pool_);

  VkDescriptorSetAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool = desc_pool_;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts = &desc_layout_;
  dev_->vkAllocateDescriptorSets(device, &ai, &desc_set_);

  VkDescriptorBufferInfo bi{};
  bi.buffer = ubo_.buffer;
  bi.offset = 0;
  bi.range = sizeof(Ubo);
  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = desc_set_;
  write.dstBinding = 0;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &bi;
  dev_->vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

void VulkanRenderer::create_pipelines() {
  const VkDevice device = window_->device();
  const QString spv_dir = QStringLiteral(BREP_VIEWER_SPV_DIR);

  VkShaderModule vert = load_shader(spv_dir + "/mesh.vert.spv");
  VkShaderModule frag = load_shader(spv_dir + "/mesh.frag.spv");
  VkShaderModule line_vert = load_shader(spv_dir + "/line.vert.spv");
  VkShaderModule line_frag = load_shader(spv_dir + "/line.frag.spv");

  VkPipelineLayoutCreateInfo plci{};
  plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plci.setLayoutCount = 1;
  plci.pSetLayouts = &desc_layout_;
  dev_->vkCreatePipelineLayout(device, &plci, nullptr, &pipeline_layout_);

  auto make_pipeline = [&](VkShaderModule vs, VkShaderModule fs, bool lines,
                           VkPipeline* out) {
    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrs;
    if (lines) {
      binding.stride = sizeof(float) * 3;
      attrs.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
    } else {
      binding.stride = sizeof(TriVertexGpu);
      attrs.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TriVertexGpu, pos)});
      attrs.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TriVertexGpu, nrm)});
    }

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

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
    rs.cullMode = lines ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = window_->sampleCountFlagBits();

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = lines ? VK_FALSE : VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blend_att{};
    blend_att.colorWriteMask = 0xF;
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

  make_pipeline(vert, frag, false, &tri_pipeline_);
  make_pipeline(line_vert, line_frag, true, &line_pipeline_);

  dev_->vkDestroyShaderModule(device, vert, nullptr);
  dev_->vkDestroyShaderModule(device, frag, nullptr);
  dev_->vkDestroyShaderModule(device, line_vert, nullptr);
  dev_->vkDestroyShaderModule(device, line_frag, nullptr);
}

void VulkanRenderer::upload_meshes() {
  destroy_buffer(tri_vb_);
  destroy_buffer(tri_ib_);
  destroy_buffer(line_vb_);
  index_count_ = 0;
  line_vertex_count_ = 0;

  if (!triangles_.indices.empty()) {
    std::vector<TriVertexGpu> verts(triangles_.vertices.size());
    for (size_t i = 0; i < triangles_.vertices.size(); ++i) {
      verts[i].pos[0] = static_cast<float>(triangles_.vertices[i].position.x());
      verts[i].pos[1] = static_cast<float>(triangles_.vertices[i].position.y());
      verts[i].pos[2] = static_cast<float>(triangles_.vertices[i].position.z());
      verts[i].nrm[0] = static_cast<float>(triangles_.vertices[i].normal.x());
      verts[i].nrm[1] = static_cast<float>(triangles_.vertices[i].normal.y());
      verts[i].nrm[2] = static_cast<float>(triangles_.vertices[i].normal.z());
    }
    const VkDeviceSize vb_size = sizeof(TriVertexGpu) * verts.size();
    tri_vb_ = create_buffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    dev_->vkMapMemory(window_->device(), tri_vb_.memory, 0, vb_size, 0, &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(vb_size));
    dev_->vkUnmapMemory(window_->device(), tri_vb_.memory);

    const VkDeviceSize ib_size = sizeof(uint32_t) * triangles_.indices.size();
    tri_ib_ = create_buffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dev_->vkMapMemory(window_->device(), tri_ib_.memory, 0, ib_size, 0, &data);
    std::memcpy(data, triangles_.indices.data(), static_cast<size_t>(ib_size));
    dev_->vkUnmapMemory(window_->device(), tri_ib_.memory);
    index_count_ = static_cast<uint32_t>(triangles_.indices.size());
  }

  if (!edges_.positions.empty()) {
    std::vector<float> lines(edges_.positions.size() * 3);
    for (size_t i = 0; i < edges_.positions.size(); ++i) {
      lines[i * 3 + 0] = static_cast<float>(edges_.positions[i].x());
      lines[i * 3 + 1] = static_cast<float>(edges_.positions[i].y());
      lines[i * 3 + 2] = static_cast<float>(edges_.positions[i].z());
    }
    const VkDeviceSize size = sizeof(float) * lines.size();
    line_vb_ = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    dev_->vkMapMemory(window_->device(), line_vb_.memory, 0, size, 0, &data);
    std::memcpy(data, lines.data(), static_cast<size_t>(size));
    dev_->vkUnmapMemory(window_->device(), line_vb_.memory);
    line_vertex_count_ = static_cast<uint32_t>(edges_.positions.size());
  }

  meshes_dirty_ = false;
}

void VulkanRenderer::initSwapChainResources() {}

void VulkanRenderer::releaseSwapChainResources() {}

void VulkanRenderer::releaseResources() {
  const VkDevice device = window_->device();
  if (!dev_ || device == VK_NULL_HANDLE) return;

  destroy_buffer(tri_vb_);
  destroy_buffer(tri_ib_);
  destroy_buffer(line_vb_);
  destroy_buffer(ubo_);

  if (tri_pipeline_)
    dev_->vkDestroyPipeline(device, tri_pipeline_, nullptr);
  if (line_pipeline_)
    dev_->vkDestroyPipeline(device, line_pipeline_, nullptr);
  if (pipeline_layout_)
    dev_->vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
  if (pipeline_cache_)
    dev_->vkDestroyPipelineCache(device, pipeline_cache_, nullptr);
  if (desc_pool_)
    dev_->vkDestroyDescriptorPool(device, desc_pool_, nullptr);
  if (desc_layout_)
    dev_->vkDestroyDescriptorSetLayout(device, desc_layout_, nullptr);

  tri_pipeline_ = line_pipeline_ = VK_NULL_HANDLE;
  pipeline_layout_ = VK_NULL_HANDLE;
  pipeline_cache_ = VK_NULL_HANDLE;
  desc_pool_ = VK_NULL_HANDLE;
  desc_layout_ = VK_NULL_HANDLE;
  desc_set_ = VK_NULL_HANDLE;
}

void VulkanRenderer::startNextFrame() {
  if (meshes_dirty_) {
    upload_meshes();
  }

  const QSize sz = window_->swapChainImageSize();
  Ubo ubo{};
  Camera::identity(ubo.model);
  float view[16];
  float proj[16];
  window_->camera().view_matrix(view);
  Camera::perspective(45.0f, float(sz.width()) / float(std::max(1, sz.height())),
                      0.05f, 500.0f, proj);
  Camera::multiply(proj, view, ubo.mvp);
  ubo.light_dir[0] = -0.4f;
  ubo.light_dir[1] = -1.0f;
  ubo.light_dir[2] = -0.3f;
  ubo.light_dir[3] = 0.0f;

  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), ubo_.memory, 0, sizeof(Ubo), 0, &data);
  std::memcpy(data, &ubo, sizeof(Ubo));
  dev_->vkUnmapMemory(window_->device(), ubo_.memory);

  VkClearValue clears[3]{};
  clears[0].color = {{0.12f, 0.13f, 0.15f, 1.0f}};
  clears[1].depthStencil = {1.0f, 0};
  clears[2].color = {{0.12f, 0.13f, 0.15f, 1.0f}};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = window_->defaultRenderPass();
  rp.framebuffer = window_->currentFramebuffer();
  rp.renderArea.extent.width = uint32_t(sz.width());
  rp.renderArea.extent.height = uint32_t(sz.height());
  rp.clearValueCount = window_->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3u : 2u;
  rp.pClearValues = clears;

  const VkCommandBuffer cmd = window_->currentCommandBuffer();
  dev_->vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = float(sz.width());
  viewport.height = float(sz.height());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  dev_->vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent.width = uint32_t(sz.width());
  scissor.extent.height = uint32_t(sz.height());
  dev_->vkCmdSetScissor(cmd, 0, 1, &scissor);

  dev_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout_, 0, 1, &desc_set_, 0, nullptr);

  if (index_count_ > 0 && tri_pipeline_) {
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tri_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &tri_vb_.buffer, &offset);
    dev_->vkCmdBindIndexBuffer(cmd, tri_ib_.buffer, 0, VK_INDEX_TYPE_UINT32);
    dev_->vkCmdDrawIndexed(cmd, index_count_, 1, 0, 0, 0);
  }

  if (line_vertex_count_ > 0 && line_pipeline_) {
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, line_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &line_vb_.buffer, &offset);
    dev_->vkCmdDraw(cmd, line_vertex_count_, 1, 0, 0);
  }

  dev_->vkCmdEndRenderPass(cmd);
  window_->frameReady();
  window_->requestUpdate();
}

}  // namespace brep::viewer
