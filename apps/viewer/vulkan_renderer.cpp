#include "vulkan_renderer.hpp"

#include "ecs/systems.hpp"
#include "ecs/world.hpp"
#include "vulkan_window.hpp"

#include "brep/log.hpp"

#include <QFile>
#include <QImage>
#include <QVulkanDeviceFunctions>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace brep::viewer {
namespace {

struct TriVertexGpu {
  float pos[3];
  float nrm[3];
  float uv[2];
};

}  // namespace

VulkanRenderer::VulkanRenderer(VulkanWindow* window) : window_(window) {}

void VulkanRenderer::set_meshes(TriangleMesh triangles, EdgeMesh edges) {
  triangles_ = std::move(triangles);
  edges_ = std::move(edges);
  meshes_dirty_ = true;
}

void VulkanRenderer::set_material(Material material) {
  material_ = std::move(material);
  material_dirty_ = true;
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
  bi.size = std::max<VkDeviceSize>(size, 1);
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

void VulkanRenderer::destroy_texture(GpuTexture& tex) {
  const VkDevice device = window_->device();
  if (!dev_ || device == VK_NULL_HANDLE) {
    tex = {};
    return;
  }
  if (tex.sampler) dev_->vkDestroySampler(device, tex.sampler, nullptr);
  if (tex.view) dev_->vkDestroyImageView(device, tex.view, nullptr);
  if (tex.image) dev_->vkDestroyImage(device, tex.image, nullptr);
  if (tex.memory) dev_->vkFreeMemory(device, tex.memory, nullptr);
  tex = {};
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

VkCommandBuffer VulkanRenderer::begin_one_shot_commands() {
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = window_->graphicsCommandPool();
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cmd{VK_NULL_HANDLE};
  dev_->vkAllocateCommandBuffers(window_->device(), &ai, &cmd);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  dev_->vkBeginCommandBuffer(cmd, &bi);
  return cmd;
}

void VulkanRenderer::end_one_shot_commands(VkCommandBuffer cmd) {
  dev_->vkEndCommandBuffer(cmd);
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  dev_->vkQueueSubmit(window_->graphicsQueue(), 1, &si, VK_NULL_HANDLE);
  dev_->vkQueueWaitIdle(window_->graphicsQueue());
  dev_->vkFreeCommandBuffers(window_->device(), window_->graphicsCommandPool(), 1,
                             &cmd);
}

void VulkanRenderer::transition_image_layout(VkImage image, VkImageLayout old_layout,
                                             VkImageLayout new_layout) {
  VkCommandBuffer cmd = begin_one_shot_commands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    throw std::runtime_error("unsupported image layout transition");
  }

  dev_->vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
                             1, &barrier);
  end_one_shot_commands(cmd);
}

void VulkanRenderer::copy_buffer_to_image(VkBuffer buffer, VkImage image,
                                          uint32_t width, uint32_t height) {
  VkCommandBuffer cmd = begin_one_shot_commands();
  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {width, height, 1};
  dev_->vkCmdCopyBufferToImage(cmd, buffer, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  end_one_shot_commands(cmd);
}

void VulkanRenderer::create_albedo_texture() {
  destroy_texture(albedo_);

  QImage image;
  if (!material_.albedo_path.empty()) {
    image = QImage(QString::fromStdString(material_.albedo_path));
  }
  if (image.isNull()) {
    image = QImage(2, 2, QImage::Format_RGBA8888);
    const QRgb c = qRgba(int(material_.albedo_color[0] * 255),
                         int(material_.albedo_color[1] * 255),
                         int(material_.albedo_color[2] * 255), 255);
    image.fill(c);
    BREP_WARN("albedo texture missing ('{}'); using solid color fallback",
              material_.albedo_path);
  } else {
    image = image.convertToFormat(QImage::Format_RGBA8888);
    BREP_INFO("loaded albedo '{}' {}x{}", material_.name, image.width(),
              image.height());
  }

  const uint32_t width = uint32_t(image.width());
  const uint32_t height = uint32_t(image.height());
  const VkDeviceSize image_size = VkDeviceSize(width) * height * 4;
  albedo_.width = width;
  albedo_.height = height;

  GpuBuffer staging = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), staging.memory, 0, image_size, 0, &data);
  std::memcpy(data, image.constBits(), static_cast<size_t>(image_size));
  dev_->vkUnmapMemory(window_->device(), staging.memory);

  const VkDevice device = window_->device();
  VkImageCreateInfo ii{};
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.extent = {width, height, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.format = VK_FORMAT_R8G8B8A8_UNORM;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (dev_->vkCreateImage(device, &ii, nullptr, &albedo_.image) != VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkCreateImage failed");
  }

  VkMemoryRequirements req{};
  dev_->vkGetImageMemoryRequirements(device, albedo_.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (dev_->vkAllocateMemory(device, &ai, nullptr, &albedo_.memory) != VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkAllocateMemory (image) failed");
  }
  dev_->vkBindImageMemory(device, albedo_.image, albedo_.memory, 0);

  transition_image_layout(albedo_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(staging.buffer, albedo_.image, width, height);
  transition_image_layout(albedo_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destroy_buffer(staging);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = albedo_.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  if (dev_->vkCreateImageView(device, &vi, nullptr, &albedo_.view) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateImageView failed");
  }

  VkSamplerCreateInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  si.magFilter = VK_FILTER_LINEAR;
  si.minFilter = VK_FILTER_LINEAR;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.maxAnisotropy = 1.0f;
  si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  si.unnormalizedCoordinates = VK_FALSE;
  si.compareEnable = VK_FALSE;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  if (dev_->vkCreateSampler(device, &si, nullptr, &albedo_.sampler) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateSampler failed");
  }

  material_dirty_ = false;
}

void VulkanRenderer::initResources() {
  try {
    dev_ = window_->vulkanInstance()->deviceFunctions(window_->device());
    if (!dev_) {
      throw std::runtime_error("QVulkanDeviceFunctions is null");
    }
    const VkDevice device = window_->device();
    if (device == VK_NULL_HANDLE) {
      throw std::runtime_error("VkDevice is null");
    }

    VkPipelineCacheCreateInfo pc{};
    pc.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (dev_->vkCreatePipelineCache(device, &pc, nullptr, &pipeline_cache_) !=
        VK_SUCCESS) {
      throw std::runtime_error("vkCreatePipelineCache failed");
    }

    ubo_ = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    create_albedo_texture();
    create_descriptors();
    create_pipelines();
    upload_meshes();
    BREP_INFO("VulkanRenderer::initResources OK (material='{}')", material_.name);
  } catch (const std::exception& ex) {
    BREP_ERROR("VulkanRenderer::initResources failed: {}", ex.what());
  }
}

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

  VkDescriptorPoolSize pool_sizes[2]{};
  pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  pool_sizes[0].descriptorCount = 1;
  pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  pool_sizes[1].descriptorCount = 1;

  VkDescriptorPoolCreateInfo pci{};
  pci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pci.maxSets = 1;
  pci.poolSizeCount = 2;
  pci.pPoolSizes = pool_sizes;
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

  VkDescriptorImageInfo ii{};
  ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  ii.imageView = albedo_.view;
  ii.sampler = albedo_.sampler;

  VkWriteDescriptorSet writes[2]{};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = desc_set_;
  writes[0].dstBinding = 0;
  writes[0].descriptorCount = 1;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].pBufferInfo = &bi;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = desc_set_;
  writes[1].dstBinding = 1;
  writes[1].descriptorCount = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].pImageInfo = &ii;

  dev_->vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
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
      attrs.push_back({2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TriVertexGpu, uv)});
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
      verts[i].uv[0] = static_cast<float>(triangles_.vertices[i].uv.u());
      verts[i].uv[1] = static_cast<float>(triangles_.vertices[i].uv.v());
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
  destroy_texture(albedo_);

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
  // Keep ECS → GPU sync current (mesh/material dirtied by systems).
  if (window_ && window_->world()) {
    ecs::render_sync(window_->world()->registry(), *this);
  }

  if (!dev_ || !pipeline_layout_ || (!tri_pipeline_ && !line_pipeline_)) {
    window_->frameReady();
    window_->requestUpdate();
    return;
  }

  if (meshes_dirty_) {
    try {
      upload_meshes();
    } catch (const std::exception& ex) {
      BREP_ERROR("upload_meshes failed: {}", ex.what());
      meshes_dirty_ = false;
    }
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
  ubo.albedo_color[0] = material_.albedo_color[0];
  ubo.albedo_color[1] = material_.albedo_color[1];
  ubo.albedo_color[2] = material_.albedo_color[2];
  ubo.albedo_color[3] = material_.uv_scale;

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
