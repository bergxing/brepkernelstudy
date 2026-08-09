#include "vulkan_renderer.hpp"

#include "render/vulkan_renderer_detail.hpp"
#include "vulkan_window.hpp"

#include <QVulkanDeviceFunctions>

#include <cstring>
#include <vector>

namespace brep::viewer {

void VulkanRenderer::upload_axes() {
  destroy_buffer(axis_vb_);
  axis_vertex_count_ = 0;

  // Unit triad for the screen-space corner gizmo (not world-anchored).
  constexpr float L = 1.0f;
  const AxisVertexGpu axes[] = {
      {{0, 0, 0}, {1.0f, 0.15f, 0.15f, 1.0f}},
      {{L, 0, 0}, {1.0f, 0.15f, 0.15f, 1.0f}},  // X
      {{0, 0, 0}, {0.2f, 0.9f, 0.25f, 1.0f}},
      {{0, L, 0}, {0.2f, 0.9f, 0.25f, 1.0f}},  // Y
      {{0, 0, 0}, {0.25f, 0.45f, 1.0f, 1.0f}},
      {{0, 0, L}, {0.25f, 0.45f, 1.0f, 1.0f}},  // Z
  };

  const VkDeviceSize size = sizeof(axes);
  axis_vb_ = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), axis_vb_.memory, 0, size, 0, &data);
  std::memcpy(data, axes, static_cast<size_t>(size));
  dev_->vkUnmapMemory(window_->device(), axis_vb_.memory);
  axis_vertex_count_ = 6;
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

void VulkanRenderer::upload_selection_meshes() {
  destroy_buffer(sel_tri_vb_);
  destroy_buffer(sel_tri_ib_);
  destroy_buffer(sel_line_vb_);
  sel_index_count_ = 0;
  sel_line_vertex_count_ = 0;

  if (!selection_triangles_.indices.empty()) {
    std::vector<TriVertexGpu> verts(selection_triangles_.vertices.size());
    for (size_t i = 0; i < selection_triangles_.vertices.size(); ++i) {
      verts[i].pos[0] =
          static_cast<float>(selection_triangles_.vertices[i].position.x());
      verts[i].pos[1] =
          static_cast<float>(selection_triangles_.vertices[i].position.y());
      verts[i].pos[2] =
          static_cast<float>(selection_triangles_.vertices[i].position.z());
      verts[i].nrm[0] =
          static_cast<float>(selection_triangles_.vertices[i].normal.x());
      verts[i].nrm[1] =
          static_cast<float>(selection_triangles_.vertices[i].normal.y());
      verts[i].nrm[2] =
          static_cast<float>(selection_triangles_.vertices[i].normal.z());
      verts[i].uv[0] =
          static_cast<float>(selection_triangles_.vertices[i].uv.u());
      verts[i].uv[1] =
          static_cast<float>(selection_triangles_.vertices[i].uv.v());
    }
    const VkDeviceSize vb_size = sizeof(TriVertexGpu) * verts.size();
    sel_tri_vb_ = create_buffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    dev_->vkMapMemory(window_->device(), sel_tri_vb_.memory, 0, vb_size, 0,
                      &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(vb_size));
    dev_->vkUnmapMemory(window_->device(), sel_tri_vb_.memory);

    const VkDeviceSize ib_size =
        sizeof(uint32_t) * selection_triangles_.indices.size();
    sel_tri_ib_ = create_buffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    dev_->vkMapMemory(window_->device(), sel_tri_ib_.memory, 0, ib_size, 0,
                      &data);
    std::memcpy(data, selection_triangles_.indices.data(),
                static_cast<size_t>(ib_size));
    dev_->vkUnmapMemory(window_->device(), sel_tri_ib_.memory);
    sel_index_count_ =
        static_cast<uint32_t>(selection_triangles_.indices.size());
  }

  if (!selection_edges_.positions.empty()) {
    std::vector<float> lines(selection_edges_.positions.size() * 3);
    for (size_t i = 0; i < selection_edges_.positions.size(); ++i) {
      lines[i * 3 + 0] =
          static_cast<float>(selection_edges_.positions[i].x());
      lines[i * 3 + 1] =
          static_cast<float>(selection_edges_.positions[i].y());
      lines[i * 3 + 2] =
          static_cast<float>(selection_edges_.positions[i].z());
    }
    const VkDeviceSize size = sizeof(float) * lines.size();
    sel_line_vb_ = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    dev_->vkMapMemory(window_->device(), sel_line_vb_.memory, 0, size, 0,
                      &data);
    std::memcpy(data, lines.data(), static_cast<size_t>(size));
    dev_->vkUnmapMemory(window_->device(), sel_line_vb_.memory);
    sel_line_vertex_count_ =
        static_cast<uint32_t>(selection_edges_.positions.size());
  }

  selection_meshes_dirty_ = false;
}

void VulkanRenderer::upload_colored_edges(const EdgeMesh& edges, float r,
                                          float g, float b, GpuBuffer& vb,
                                          std::uint32_t& vertex_count) {
  destroy_buffer(vb);
  vertex_count = 0;
  if (edges.positions.empty()) return;

  std::vector<AxisVertexGpu> verts(edges.positions.size());
  for (size_t i = 0; i < edges.positions.size(); ++i) {
    verts[i].pos[0] = static_cast<float>(edges.positions[i].x());
    verts[i].pos[1] = static_cast<float>(edges.positions[i].y());
    verts[i].pos[2] = static_cast<float>(edges.positions[i].z());
    verts[i].color[0] = r;
    verts[i].color[1] = g;
    verts[i].color[2] = b;
    verts[i].color[3] = 1.0f;
  }
  const VkDeviceSize size = sizeof(AxisVertexGpu) * verts.size();
  vb = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), vb.memory, 0, size, 0, &data);
  std::memcpy(data, verts.data(), static_cast<size_t>(size));
  dev_->vkUnmapMemory(window_->device(), vb.memory);
  vertex_count = static_cast<uint32_t>(verts.size());
}

void VulkanRenderer::upload_preview_solid() {
  destroy_buffer(preview_solid_vb_);
  preview_solid_vertex_count_ = 0;
  if (preview_solid_.indices.empty() || preview_solid_.vertices.empty()) return;

  std::vector<AxisVertexGpu> verts;
  verts.reserve(preview_solid_.indices.size());
  constexpr float kR = 1.0f;
  constexpr float kG = 0.92f;
  constexpr float kB = 0.15f;
  constexpr float kA = 0.28f;
  for (std::uint32_t idx : preview_solid_.indices) {
    if (idx >= preview_solid_.vertices.size()) continue;
    const auto& v = preview_solid_.vertices[idx];
    AxisVertexGpu g{};
    g.pos[0] = static_cast<float>(v.position.x());
    g.pos[1] = static_cast<float>(v.position.y());
    g.pos[2] = static_cast<float>(v.position.z());
    g.color[0] = kR;
    g.color[1] = kG;
    g.color[2] = kB;
    g.color[3] = kA;
    verts.push_back(g);
  }
  if (verts.empty()) return;

  const VkDeviceSize size = sizeof(AxisVertexGpu) * verts.size();
  preview_solid_vb_ =
      create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), preview_solid_vb_.memory, 0, size, 0,
                    &data);
  std::memcpy(data, verts.data(), static_cast<size_t>(size));
  dev_->vkUnmapMemory(window_->device(), preview_solid_vb_.memory);
  preview_solid_vertex_count_ = static_cast<uint32_t>(verts.size());
}

void VulkanRenderer::upload_preview() {
  upload_colored_edges(preview_edges_, 1.0f, 0.92f, 0.15f, preview_vb_,
                       preview_vertex_count_);
  upload_preview_solid();
  preview_dirty_ = false;
}

void VulkanRenderer::upload_snap_overlay() {
  upload_colored_edges(snap_overlay_edges_, 0.2f, 1.0f, 0.85f,
                       snap_overlay_vb_, snap_overlay_vertex_count_);
  snap_overlay_dirty_ = false;
}

void VulkanRenderer::upload_highlight() {
  // Orange selection outline.
  upload_colored_edges(highlight_edges_, 1.0f, 0.55f, 0.1f, highlight_vb_,
                       highlight_vertex_count_);
  highlight_dirty_ = false;
}

}  // namespace brep::viewer
