#include "vulkan_renderer.hpp"

#include "ecs/systems.hpp"
#include "vulkan_window.hpp"

#include "brep/log.hpp"

#include <QVulkanDeviceFunctions>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace brep::viewer {

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
    selection_ubo_ = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    axis_ubo_ = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    selection_material_.name = "selection";
    selection_material_.albedo_color[0] = 1.0f;
    selection_material_.albedo_color[1] = 0.45f;
    selection_material_.albedo_color[2] = 0.08f;

    create_albedo_texture();
    create_selection_albedo_texture();
    create_descriptors();
    create_pipelines();
    upload_axes();
    upload_meshes();
    BREP_INFO("VulkanRenderer::initResources OK (material='{}')", material_.name);
  } catch (const std::exception& ex) {
    BREP_ERROR("VulkanRenderer::initResources failed: {}", ex.what());
  }
}

void VulkanRenderer::initSwapChainResources() {}

void VulkanRenderer::releaseSwapChainResources() {}

void VulkanRenderer::releaseResources() {
  BREP_INFO("VulkanRenderer::releaseResources begin");
  const VkDevice device = window_->device();
  if (!dev_ || device == VK_NULL_HANDLE) {
    BREP_WARN("VulkanRenderer::releaseResources skipped (no device)");
    return;
  }

  destroy_buffer(tri_vb_);
  destroy_buffer(tri_ib_);
  destroy_buffer(line_vb_);
  destroy_buffer(sel_tri_vb_);
  destroy_buffer(sel_tri_ib_);
  destroy_buffer(sel_line_vb_);
  destroy_buffer(axis_vb_);
  destroy_buffer(preview_vb_);
  destroy_buffer(preview_solid_vb_);
  destroy_buffer(highlight_vb_);
  destroy_buffer(ubo_);
  destroy_buffer(selection_ubo_);
  destroy_buffer(axis_ubo_);
  destroy_texture(albedo_);
  destroy_texture(selection_albedo_);
  preview_vertex_count_ = 0;
  preview_solid_vertex_count_ = 0;
  highlight_vertex_count_ = 0;
  sel_index_count_ = 0;
  sel_line_vertex_count_ = 0;
  selection_desc_set_ = VK_NULL_HANDLE;

  if (tri_pipeline_)
    dev_->vkDestroyPipeline(device, tri_pipeline_, nullptr);
  if (line_pipeline_)
    dev_->vkDestroyPipeline(device, line_pipeline_, nullptr);
  if (axis_pipeline_)
    dev_->vkDestroyPipeline(device, axis_pipeline_, nullptr);
  if (preview_fill_pipeline_)
    dev_->vkDestroyPipeline(device, preview_fill_pipeline_, nullptr);
  if (pipeline_layout_)
    dev_->vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
  if (pipeline_cache_)
    dev_->vkDestroyPipelineCache(device, pipeline_cache_, nullptr);
  if (desc_pool_)
    dev_->vkDestroyDescriptorPool(device, desc_pool_, nullptr);
  if (desc_layout_)
    dev_->vkDestroyDescriptorSetLayout(device, desc_layout_, nullptr);

  tri_pipeline_ = line_pipeline_ = axis_pipeline_ = preview_fill_pipeline_ =
      VK_NULL_HANDLE;
  pipeline_layout_ = VK_NULL_HANDLE;
  pipeline_cache_ = VK_NULL_HANDLE;
  desc_pool_ = VK_NULL_HANDLE;
  desc_layout_ = VK_NULL_HANDLE;
  desc_set_ = selection_desc_set_ = axis_desc_set_ = VK_NULL_HANDLE;
  axis_vertex_count_ = 0;
  BREP_INFO("VulkanRenderer::releaseResources end");
}

void VulkanRenderer::sync_from_world() {
  if (!window_ || !window_->world()) return;
  ecs::render_sync(window_->world()->registry(), *this, synced_scene_version_);
}

void VulkanRenderer::startNextFrame() {
  // Window teardown clears world; do not keep requesting frames.
  if (!window_ || !window_->world()) {
    if (window_) window_->frameReady();
    return;
  }

  // Keep ECS → GPU sync current (mesh/material dirtied by systems).
  sync_from_world();

  if (!dev_ || !pipeline_layout_ ||
      (!tri_pipeline_ && !line_pipeline_ && !axis_pipeline_)) {
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

  // Blank scenes initialize with a solid fallback albedo; when the first body
  // (or a selection highlight) sets a new Material, rebuild the GPU texture.
  if (material_dirty_) {
    try {
      create_albedo_texture();
      update_albedo_descriptors();
    } catch (const std::exception& ex) {
      BREP_ERROR("create_albedo_texture failed: {}", ex.what());
      material_dirty_ = false;
    }
  }

  if (selection_meshes_dirty_) {
    try {
      upload_selection_meshes();
    } catch (const std::exception& ex) {
      BREP_ERROR("upload_selection_meshes failed: {}", ex.what());
      selection_meshes_dirty_ = false;
    }
  }

  if (selection_material_dirty_) {
    try {
      create_selection_albedo_texture();
      if (selection_desc_set_) {
        bind_albedo_to_desc(selection_desc_set_, selection_albedo_);
      }
    } catch (const std::exception& ex) {
      BREP_ERROR("create_selection_albedo_texture failed: {}", ex.what());
      selection_material_dirty_ = false;
    }
  }

  if (preview_dirty_) {
    try {
      upload_preview();
    } catch (const std::exception& ex) {
      BREP_ERROR("upload_preview failed: {}", ex.what());
      preview_dirty_ = false;
    }
  }

  if (highlight_dirty_) {
    try {
      upload_highlight();
    } catch (const std::exception& ex) {
      BREP_ERROR("upload_highlight failed: {}", ex.what());
      highlight_dirty_ = false;
    }
  }

  const QSize sz = window_->swapChainImageSize();
  const Camera& cam = window_->camera();
  const float aspect =
      float(sz.width()) / float(std::max(1, sz.height()));

  Ubo ubo{};
  Camera::identity(ubo.model);
  float view[16];
  float proj[16];
  cam.view_matrix(view);
  // Near plane sits well in front of the eye but far closer than the target,
  // so a pan near the model cannot carve a triangular hole through faces.
  const float znear = std::clamp(cam.distance * 0.002f, 0.01f, 0.25f);
  const float zfar = std::max(500.0f, cam.distance * 50.0f);
  if (cam.ortho) {
    const float half_h = std::max(0.05f, cam.ortho_half_h);
    const float half_w = half_h * aspect;
    Camera::ortho_matrix(half_w, half_h, znear, zfar, proj);
  } else {
    Camera::perspective(cam.fov_deg, aspect, znear, zfar, proj);
  }
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

  // Selection uses the same MVP but its own albedo UBO + orange texture.
  Ubo sel_ubo = ubo;
  sel_ubo.albedo_color[0] = selection_material_.albedo_color[0];
  sel_ubo.albedo_color[1] = selection_material_.albedo_color[1];
  sel_ubo.albedo_color[2] = selection_material_.albedo_color[2];
  sel_ubo.albedo_color[3] = selection_material_.uv_scale;
  if (selection_ubo_.memory) {
    void* sel_data = nullptr;
    dev_->vkMapMemory(window_->device(), selection_ubo_.memory, 0, sizeof(Ubo),
                      0, &sel_data);
    std::memcpy(sel_data, &sel_ubo, sizeof(Ubo));
    dev_->vkUnmapMemory(window_->device(), selection_ubo_.memory);
  }

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

  // Selected body — solid orange (separate descriptor set + UBO).
  if (selection_desc_set_ &&
      (sel_index_count_ > 0 || sel_line_vertex_count_ > 0)) {
    dev_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline_layout_, 0, 1, &selection_desc_set_,
                                  0, nullptr);
    if (sel_index_count_ > 0 && tri_pipeline_ && sel_tri_vb_.buffer &&
        sel_tri_ib_.buffer) {
      dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              tri_pipeline_);
      VkDeviceSize offset = 0;
      dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &sel_tri_vb_.buffer, &offset);
      dev_->vkCmdBindIndexBuffer(cmd, sel_tri_ib_.buffer, 0,
                                 VK_INDEX_TYPE_UINT32);
      dev_->vkCmdDrawIndexed(cmd, sel_index_count_, 1, 0, 0, 0);
    }
    if (sel_line_vertex_count_ > 0 && line_pipeline_ && sel_line_vb_.buffer) {
      dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              line_pipeline_);
      VkDeviceSize offset = 0;
      dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &sel_line_vb_.buffer, &offset);
      dev_->vkCmdDraw(cmd, sel_line_vertex_count_, 1, 0, 0);
    }
    // Restore scene descriptor set for subsequent overlay draws.
    dev_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline_layout_, 0, 1, &desc_set_, 0,
                                  nullptr);
  }

  // Selection outline (orange), then tool preview fill + wire (yellow).
  if (highlight_vertex_count_ > 0 && axis_pipeline_ && highlight_vb_.buffer) {
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, axis_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &highlight_vb_.buffer, &offset);
    dev_->vkCmdDraw(cmd, highlight_vertex_count_, 1, 0, 0);
  }
  if (preview_solid_vertex_count_ > 0 && preview_fill_pipeline_ &&
      preview_solid_vb_.buffer) {
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            preview_fill_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &preview_solid_vb_.buffer, &offset);
    dev_->vkCmdDraw(cmd, preview_solid_vertex_count_, 1, 0, 0);
  }
  if (preview_vertex_count_ > 0 && axis_pipeline_ && preview_vb_.buffer) {
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, axis_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &preview_vb_.buffer, &offset);
    dev_->vkCmdDraw(cmd, preview_vertex_count_, 1, 0, 0);
  }

  // Screen-space orientation triad (bottom-left). Uses a separate UBO so it
  // cannot overwrite the scene MVP that the mesh/edge draws will read.
  if (axis_vertex_count_ > 0 && axis_pipeline_ && axis_desc_set_) {
    Ubo axis_ubo{};
    Camera::identity(axis_ubo.model);
    float orient_view[16];
    float gizmo_proj[16];
    cam.orientation_view_matrix(orient_view);
    Camera::ortho_matrix(1.35f, 1.35f, 0.1f, 10.0f, gizmo_proj);
    Camera::multiply(gizmo_proj, orient_view, axis_ubo.mvp);

    void* axis_data = nullptr;
    dev_->vkMapMemory(window_->device(), axis_ubo_.memory, 0, sizeof(Ubo), 0,
                      &axis_data);
    std::memcpy(axis_data, &axis_ubo, sizeof(Ubo));
    dev_->vkUnmapMemory(window_->device(), axis_ubo_.memory);

    constexpr float gizmo = 112.0f;
    constexpr float margin = 14.0f;
    VkViewport gizmo_vp{};
    gizmo_vp.x = margin;
    gizmo_vp.y = float(sz.height()) - gizmo - margin;
    gizmo_vp.width = gizmo;
    gizmo_vp.height = gizmo;
    gizmo_vp.minDepth = 0.0f;
    gizmo_vp.maxDepth = 1.0f;
    dev_->vkCmdSetViewport(cmd, 0, 1, &gizmo_vp);

    VkRect2D gizmo_sc{};
    gizmo_sc.offset.x = int32_t(margin);
    gizmo_sc.offset.y = int32_t(float(sz.height()) - gizmo - margin);
    gizmo_sc.extent.width = uint32_t(gizmo);
    gizmo_sc.extent.height = uint32_t(gizmo);
    dev_->vkCmdSetScissor(cmd, 0, 1, &gizmo_sc);

    dev_->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  pipeline_layout_, 0, 1, &axis_desc_set_, 0,
                                  nullptr);
    dev_->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, axis_pipeline_);
    VkDeviceSize offset = 0;
    dev_->vkCmdBindVertexBuffers(cmd, 0, 1, &axis_vb_.buffer, &offset);
    dev_->vkCmdDraw(cmd, axis_vertex_count_, 1, 0, 0);
  }

  dev_->vkCmdEndRenderPass(cmd);
  window_->frameReady();
  window_->requestUpdate();
}

}  // namespace brep::viewer
