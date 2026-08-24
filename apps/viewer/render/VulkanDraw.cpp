#include "VulkanRenderer.h"

#include "ecs/Systems.h"
#include "VulkanWindow.h"

#include "api/Core.h"

#include <QVulkanDeviceFunctions>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace brep::viewer
{

void VulkanRenderer::initResources()
{
  try {
    m_dev = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    if (!m_dev)
    {
      throw std::runtime_error("QVulkanDeviceFunctions is null");
    }
    const VkDevice device = m_window->device();
    if (device == VK_NULL_HANDLE)
    {
      throw std::runtime_error("VkDevice is null");
    }

    VkPipelineCacheCreateInfo pc{};
    pc.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (m_dev->vkCreatePipelineCache(device, &pc, nullptr, &m_pipelineCache) !=
        VK_SUCCESS)
    {
      throw std::runtime_error("vkCreatePipelineCache failed");
    }

    m_ubo = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_selectionUbo = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_axisUbo = create_buffer(sizeof(Ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_selectionMaterial.Name = "selection";
    m_selectionMaterial.AlbedoColor[0] = 1.0f;
    m_selectionMaterial.AlbedoColor[1] = 0.45f;
    m_selectionMaterial.AlbedoColor[2] = 0.08f;

    create_albedo_texture();
    create_selection_albedo_texture();
    create_descriptors();
    create_pipelines();
    upload_axes();
    upload_meshes();
    BREP_INFO("VulkanRenderer::initResources OK (material='{}')", m_material.Name);
  } catch (const std::exception& ex)
  {
    BREP_ERROR("VulkanRenderer::initResources failed: {}", ex.what());
  }
}

void VulkanRenderer::initSwapChainResources()
{
}

void VulkanRenderer::releaseSwapChainResources()
{
}

void VulkanRenderer::releaseResources()
{
  BREP_INFO("VulkanRenderer::releaseResources begin");
  const VkDevice device = m_window->device();
  if (!m_dev || device == VK_NULL_HANDLE)
  {
    BREP_WARN("VulkanRenderer::releaseResources skipped (no device)");
    return;
  }

  destroy_buffer(m_triVb);
  destroy_buffer(m_triIb);
  destroy_buffer(m_lineVb);
  destroy_buffer(m_selTriVb);
  destroy_buffer(m_selTriIb);
  destroy_buffer(m_selLineVb);
  destroy_buffer(m_axisVb);
  destroy_buffer(m_previewVb);
  destroy_buffer(m_previewSolidVb);
  destroy_buffer(m_snapOverlayVb);
  destroy_buffer(m_highlightVb);
  destroy_buffer(m_ubo);
  destroy_buffer(m_selectionUbo);
  destroy_buffer(m_axisUbo);
  destroy_texture(m_albedo);
  destroy_texture(m_selectionAlbedo);
  m_previewVertexCount = 0;
  m_previewSolidVertexCount = 0;
  m_snapOverlayVertexCount = 0;
  m_highlightVertexCount = 0;
  m_selIndexCount = 0;
  m_selLineVertexCount = 0;
  m_selectionDescSet = VK_NULL_HANDLE;

  if (m_triPipeline)
    m_dev->vkDestroyPipeline(device, m_triPipeline, nullptr);
  if (m_linePipeline)
    m_dev->vkDestroyPipeline(device, m_linePipeline, nullptr);
  if (m_axisPipeline)
    m_dev->vkDestroyPipeline(device, m_axisPipeline, nullptr);
  if (m_previewFillPipeline)
    m_dev->vkDestroyPipeline(device, m_previewFillPipeline, nullptr);
  if (m_pipelineLayout)
    m_dev->vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
  if (m_pipelineCache)
    m_dev->vkDestroyPipelineCache(device, m_pipelineCache, nullptr);
  if (m_descPool)
    m_dev->vkDestroyDescriptorPool(device, m_descPool, nullptr);
  if (m_descLayout)
    m_dev->vkDestroyDescriptorSetLayout(device, m_descLayout, nullptr);

  m_triPipeline = m_linePipeline = m_axisPipeline = m_previewFillPipeline =
      VK_NULL_HANDLE;
  m_pipelineLayout = VK_NULL_HANDLE;
  m_pipelineCache = VK_NULL_HANDLE;
  m_descPool = VK_NULL_HANDLE;
  m_descLayout = VK_NULL_HANDLE;
  m_descSet = m_selectionDescSet = m_axisDescSet = VK_NULL_HANDLE;
  m_axisVertexCount = 0;
  BREP_INFO("VulkanRenderer::releaseResources end");
}

void VulkanRenderer::sync_from_world()
{
  if (!m_window || !m_window->world()) return;
  ecs::render_sync(m_window->world()->registry(), *this, m_syncedSceneVersion);
}

void VulkanRenderer::startNextFrame()
{
  // Window teardown clears world; do not keep requesting frames.
  if (!m_window || !m_window->world())
  {
    if (m_window) m_window->frameReady();
    return;
  }

  // Keep ECS → GPU sync current (mesh/material dirtied by systems).
  sync_from_world();

  if (!m_dev || !m_pipelineLayout ||
      (!m_triPipeline && !m_linePipeline && !m_axisPipeline))
  {
    m_window->frameReady();
    m_window->requestUpdate();
    return;
  }

  if (m_meshesDirty)
  {
    try {
      upload_meshes();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("upload_meshes failed: {}", ex.what());
      m_meshesDirty = false;
    }
  }

  // Blank scenes initialize with a solid fallback albedo; when the first body
  // (or a selection highlight) sets a new Material, rebuild the GPU texture.
  if (m_materialDirty)
  {
    try {
      create_albedo_texture();
      update_albedo_descriptors();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("create_albedo_texture failed: {}", ex.what());
      m_materialDirty = false;
    }
  }

  if (m_selectionMeshesDirty)
  {
    try {
      upload_selection_meshes();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("upload_selection_meshes failed: {}", ex.what());
      m_selectionMeshesDirty = false;
    }
  }

  if (m_selectionMaterialDirty)
  {
    try {
      create_selection_albedo_texture();
      if (m_selectionDescSet)
      {
        bind_albedo_to_desc(m_selectionDescSet, m_selectionAlbedo);
      }
    } catch (const std::exception& ex)
    {
      BREP_ERROR("create_selection_albedo_texture failed: {}", ex.what());
      m_selectionMaterialDirty = false;
    }
  }

  if (m_previewDirty)
  {
    try {
      upload_preview();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("upload_preview failed: {}", ex.what());
      m_previewDirty = false;
    }
  }

  if (m_snapOverlayDirty)
  {
    try {
      upload_snap_overlay();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("upload_snap_overlay failed: {}", ex.what());
      m_snapOverlayDirty = false;
    }
  }

  if (m_highlightDirty)
  {
    try {
      upload_highlight();
    } catch (const std::exception& ex)
    {
      BREP_ERROR("upload_highlight failed: {}", ex.what());
      m_highlightDirty = false;
    }
  }

  const QSize sz = m_window->swapChainImageSize();
  const Camera& cam = m_window->camera();
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
  if (cam.ortho)
  {
    const float half_h = std::max(0.05f, cam.ortho_half_h);
    const float half_w = half_h * aspect;
    Camera::ortho_matrix(half_w, half_h, znear, zfar, proj);
  }
  else
  {
    Camera::perspective(cam.fov_deg, aspect, znear, zfar, proj);
  }
  Camera::multiply(proj, view, ubo.mvp);
  ubo.light_dir[0] = -0.4f;
  ubo.light_dir[1] = -1.0f;
  ubo.light_dir[2] = -0.3f;
  ubo.light_dir[3] = 0.0f;
  ubo.albedo_color[0] = m_material.AlbedoColor[0];
  ubo.albedo_color[1] = m_material.AlbedoColor[1];
  ubo.albedo_color[2] = m_material.AlbedoColor[2];
  ubo.albedo_color[3] = m_material.UvScale;

  void* data = nullptr;
  m_dev->vkMapMemory(m_window->device(), m_ubo.memory, 0, sizeof(Ubo), 0, &data);
  std::memcpy(data, &ubo, sizeof(Ubo));
  m_dev->vkUnmapMemory(m_window->device(), m_ubo.memory);

  // Selection uses the same MVP but its own albedo UBO + orange texture.
  Ubo sel_ubo = ubo;
  sel_ubo.albedo_color[0] = m_selectionMaterial.AlbedoColor[0];
  sel_ubo.albedo_color[1] = m_selectionMaterial.AlbedoColor[1];
  sel_ubo.albedo_color[2] = m_selectionMaterial.AlbedoColor[2];
  sel_ubo.albedo_color[3] = m_selectionMaterial.UvScale;
  if (m_selectionUbo.memory)
  {
    void* sel_data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_selectionUbo.memory, 0, sizeof(Ubo),
                      0, &sel_data);
    std::memcpy(sel_data, &sel_ubo, sizeof(Ubo));
    m_dev->vkUnmapMemory(m_window->device(), m_selectionUbo.memory);
  }

  VkClearValue clears[3]{};
  clears[0].color = {{0.12f, 0.13f, 0.15f, 1.0f}};
  clears[1].depthStencil = {1.0f, 0};
  clears[2].color = {{0.12f, 0.13f, 0.15f, 1.0f}};

  VkRenderPassBeginInfo rp{};
  rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rp.renderPass = m_window->defaultRenderPass();
  rp.framebuffer = m_window->currentFramebuffer();
  rp.renderArea.extent.width = uint32_t(sz.width());
  rp.renderArea.extent.height = uint32_t(sz.height());
  rp.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3u : 2u;
  rp.pClearValues = clears;

  const VkCommandBuffer cmd = m_window->currentCommandBuffer();
  m_dev->vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

  VkViewport viewport{};
  viewport.width = float(sz.width());
  viewport.height = float(sz.height());
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  m_dev->vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.extent.width = uint32_t(sz.width());
  scissor.extent.height = uint32_t(sz.height());
  m_dev->vkCmdSetScissor(cmd, 0, 1, &scissor);

  m_dev->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipelineLayout, 0, 1, &m_descSet, 0, nullptr);

  if (m_indexCount > 0 && m_triPipeline)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_triPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_triVb.buffer, &offset);
    m_dev->vkCmdBindIndexBuffer(cmd, m_triIb.buffer, 0, VK_INDEX_TYPE_UINT32);
    m_dev->vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
  }

  if (m_lineVertexCount > 0 && m_linePipeline)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_linePipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_lineVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_lineVertexCount, 1, 0, 0);
  }

  // Selected body — solid orange (separate descriptor set + UBO).
  if (m_selectionDescSet &&
      (m_selIndexCount > 0 || m_selLineVertexCount > 0))
  {
    m_dev->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_pipelineLayout, 0, 1, &m_selectionDescSet,
                                  0, nullptr);
    if (m_selIndexCount > 0 && m_triPipeline && m_selTriVb.buffer &&
        m_selTriIb.buffer)
    {
      m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_triPipeline);
      VkDeviceSize offset = 0;
      m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_selTriVb.buffer, &offset);
      m_dev->vkCmdBindIndexBuffer(cmd, m_selTriIb.buffer, 0,
                                 VK_INDEX_TYPE_UINT32);
      m_dev->vkCmdDrawIndexed(cmd, m_selIndexCount, 1, 0, 0, 0);
    }
    if (m_selLineVertexCount > 0 && m_linePipeline && m_selLineVb.buffer)
    {
      m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              m_linePipeline);
      VkDeviceSize offset = 0;
      m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_selLineVb.buffer, &offset);
      m_dev->vkCmdDraw(cmd, m_selLineVertexCount, 1, 0, 0);
    }
    // Restore scene descriptor set for subsequent overlay draws.
    m_dev->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_pipelineLayout, 0, 1, &m_descSet, 0,
                                  nullptr);
  }

  // Selection outline (orange), then tool preview fill + wire (yellow).
  if (m_highlightVertexCount > 0 && m_axisPipeline && m_highlightVb.buffer)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_axisPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_highlightVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_highlightVertexCount, 1, 0, 0);
  }
  if (m_previewSolidVertexCount > 0 && m_previewFillPipeline &&
      m_previewSolidVb.buffer)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_previewFillPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_previewSolidVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_previewSolidVertexCount, 1, 0, 0);
  }
  if (m_previewVertexCount > 0 && m_axisPipeline && m_previewVb.buffer)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_axisPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_previewVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_previewVertexCount, 1, 0, 0);
  }
  if (m_snapOverlayVertexCount > 0 && m_axisPipeline &&
      m_snapOverlayVb.buffer)
  {
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_axisPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_snapOverlayVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_snapOverlayVertexCount, 1, 0, 0);
  }

  // Screen-space orientation triad (bottom-left). Uses a separate UBO so it
  // cannot overwrite the scene MVP that the mesh/edge draws will read.
  if (m_axisVertexCount > 0 && m_axisPipeline && m_axisDescSet)
  {
    Ubo axis_ubo{};
    Camera::identity(axis_ubo.model);
    float orient_view[16];
    float gizmo_proj[16];
    cam.orientation_view_matrix(orient_view);
    Camera::ortho_matrix(1.35f, 1.35f, 0.1f, 10.0f, gizmo_proj);
    Camera::multiply(gizmo_proj, orient_view, axis_ubo.mvp);

    void* axis_data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_axisUbo.memory, 0, sizeof(Ubo), 0,
                      &axis_data);
    std::memcpy(axis_data, &axis_ubo, sizeof(Ubo));
    m_dev->vkUnmapMemory(m_window->device(), m_axisUbo.memory);

    constexpr float gizmo = 112.0f;
    constexpr float margin = 14.0f;
    VkViewport gizmo_vp{};
    gizmo_vp.x = margin;
    gizmo_vp.y = float(sz.height()) - gizmo - margin;
    gizmo_vp.width = gizmo;
    gizmo_vp.height = gizmo;
    gizmo_vp.minDepth = 0.0f;
    gizmo_vp.maxDepth = 1.0f;
    m_dev->vkCmdSetViewport(cmd, 0, 1, &gizmo_vp);

    VkRect2D gizmo_sc{};
    gizmo_sc.offset.x = int32_t(margin);
    gizmo_sc.offset.y = int32_t(float(sz.height()) - gizmo - margin);
    gizmo_sc.extent.width = uint32_t(gizmo);
    gizmo_sc.extent.height = uint32_t(gizmo);
    m_dev->vkCmdSetScissor(cmd, 0, 1, &gizmo_sc);

    m_dev->vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  m_pipelineLayout, 0, 1, &m_axisDescSet, 0,
                                  nullptr);
    m_dev->vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_axisPipeline);
    VkDeviceSize offset = 0;
    m_dev->vkCmdBindVertexBuffers(cmd, 0, 1, &m_axisVb.buffer, &offset);
    m_dev->vkCmdDraw(cmd, m_axisVertexCount, 1, 0, 0);
  }

  m_dev->vkCmdEndRenderPass(cmd);
  m_window->frameReady();
  m_window->requestUpdate();
}

}  // namespace brep::viewer
