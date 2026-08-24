#include "VulkanRenderer.h"

#include "render/VulkanRendererDetail.h"
#include "VulkanWindow.h"

#include <QVulkanDeviceFunctions>

#include <cstring>
#include <vector>

namespace brep::viewer
{

void VulkanRenderer::upload_axes()
{
  destroy_buffer(m_axisVb);
  m_axisVertexCount = 0;

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
  m_axisVb = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  m_dev->vkMapMemory(m_window->device(), m_axisVb.memory, 0, size, 0, &data);
  std::memcpy(data, axes, static_cast<size_t>(size));
  m_dev->vkUnmapMemory(m_window->device(), m_axisVb.memory);
  m_axisVertexCount = 6;
}

void VulkanRenderer::upload_meshes()
{
  destroy_buffer(m_triVb);
  destroy_buffer(m_triIb);
  destroy_buffer(m_lineVb);
  m_indexCount = 0;
  m_lineVertexCount = 0;

  if (!m_triangles.Indices.empty())
  {
    std::vector<TriVertexGpu> verts(m_triangles.Vertices.size());
    for (size_t i = 0; i < m_triangles.Vertices.size(); ++i)
    {
      verts[i].pos[0] = static_cast<float>(m_triangles.Vertices[i].Position.x());
      verts[i].pos[1] = static_cast<float>(m_triangles.Vertices[i].Position.y());
      verts[i].pos[2] = static_cast<float>(m_triangles.Vertices[i].Position.z());
      verts[i].nrm[0] = static_cast<float>(m_triangles.Vertices[i].Normal.x());
      verts[i].nrm[1] = static_cast<float>(m_triangles.Vertices[i].Normal.y());
      verts[i].nrm[2] = static_cast<float>(m_triangles.Vertices[i].Normal.z());
      verts[i].uv[0] = static_cast<float>(m_triangles.Vertices[i].Uv.u());
      verts[i].uv[1] = static_cast<float>(m_triangles.Vertices[i].Uv.v());
    }
    const VkDeviceSize vb_size = sizeof(TriVertexGpu) * verts.size();
    m_triVb = create_buffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_triVb.memory, 0, vb_size, 0, &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(vb_size));
    m_dev->vkUnmapMemory(m_window->device(), m_triVb.memory);

    const VkDeviceSize ib_size = sizeof(uint32_t) * m_triangles.Indices.size();
    m_triIb = create_buffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_dev->vkMapMemory(m_window->device(), m_triIb.memory, 0, ib_size, 0, &data);
    std::memcpy(data, m_triangles.Indices.data(), static_cast<size_t>(ib_size));
    m_dev->vkUnmapMemory(m_window->device(), m_triIb.memory);
    m_indexCount = static_cast<uint32_t>(m_triangles.Indices.size());
  }

  if (!m_edges.Positions.empty())
  {
    std::vector<float> lines(m_edges.Positions.size() * 3);
    for (size_t i = 0; i < m_edges.Positions.size(); ++i)
    {
      lines[i * 3 + 0] = static_cast<float>(m_edges.Positions[i].x());
      lines[i * 3 + 1] = static_cast<float>(m_edges.Positions[i].y());
      lines[i * 3 + 2] = static_cast<float>(m_edges.Positions[i].z());
    }
    const VkDeviceSize size = sizeof(float) * lines.size();
    m_lineVb = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_lineVb.memory, 0, size, 0, &data);
    std::memcpy(data, lines.data(), static_cast<size_t>(size));
    m_dev->vkUnmapMemory(m_window->device(), m_lineVb.memory);
    m_lineVertexCount = static_cast<uint32_t>(m_edges.Positions.size());
  }

  m_meshesDirty = false;
}

void VulkanRenderer::upload_selection_meshes()
{
  destroy_buffer(m_selTriVb);
  destroy_buffer(m_selTriIb);
  destroy_buffer(m_selLineVb);
  m_selIndexCount = 0;
  m_selLineVertexCount = 0;

  if (!m_selectionTriangles.Indices.empty())
  {
    std::vector<TriVertexGpu> verts(m_selectionTriangles.Vertices.size());
    for (size_t i = 0; i < m_selectionTriangles.Vertices.size(); ++i)
    {
      verts[i].pos[0] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Position.x());
      verts[i].pos[1] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Position.y());
      verts[i].pos[2] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Position.z());
      verts[i].nrm[0] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Normal.x());
      verts[i].nrm[1] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Normal.y());
      verts[i].nrm[2] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Normal.z());
      verts[i].uv[0] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Uv.u());
      verts[i].uv[1] =
          static_cast<float>(m_selectionTriangles.Vertices[i].Uv.v());
    }
    const VkDeviceSize vb_size = sizeof(TriVertexGpu) * verts.size();
    m_selTriVb = create_buffer(vb_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_selTriVb.memory, 0, vb_size, 0,
                      &data);
    std::memcpy(data, verts.data(), static_cast<size_t>(vb_size));
    m_dev->vkUnmapMemory(m_window->device(), m_selTriVb.memory);

    const VkDeviceSize ib_size =
        sizeof(uint32_t) * m_selectionTriangles.Indices.size();
    m_selTriIb = create_buffer(ib_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    m_dev->vkMapMemory(m_window->device(), m_selTriIb.memory, 0, ib_size, 0,
                      &data);
    std::memcpy(data, m_selectionTriangles.Indices.data(),
                static_cast<size_t>(ib_size));
    m_dev->vkUnmapMemory(m_window->device(), m_selTriIb.memory);
    m_selIndexCount =
        static_cast<uint32_t>(m_selectionTriangles.Indices.size());
  }

  if (!m_selectionEdges.Positions.empty())
  {
    std::vector<float> lines(m_selectionEdges.Positions.size() * 3);
    for (size_t i = 0; i < m_selectionEdges.Positions.size(); ++i)
    {
      lines[i * 3 + 0] =
          static_cast<float>(m_selectionEdges.Positions[i].x());
      lines[i * 3 + 1] =
          static_cast<float>(m_selectionEdges.Positions[i].y());
      lines[i * 3 + 2] =
          static_cast<float>(m_selectionEdges.Positions[i].z());
    }
    const VkDeviceSize size = sizeof(float) * lines.size();
    m_selLineVb = create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    void* data = nullptr;
    m_dev->vkMapMemory(m_window->device(), m_selLineVb.memory, 0, size, 0,
                      &data);
    std::memcpy(data, lines.data(), static_cast<size_t>(size));
    m_dev->vkUnmapMemory(m_window->device(), m_selLineVb.memory);
    m_selLineVertexCount =
        static_cast<uint32_t>(m_selectionEdges.Positions.size());
  }

  m_selectionMeshesDirty = false;
}

void VulkanRenderer::upload_colored_edges(const EdgeMesh& edges, float r,
                                          float g, float b, GpuBuffer& vb,
                                          std::uint32_t& vertex_count)
                                          {
  destroy_buffer(vb);
  vertex_count = 0;
  if (edges.Positions.empty()) return;

  std::vector<AxisVertexGpu> verts(edges.Positions.size());
  for (size_t i = 0; i < edges.Positions.size(); ++i)
  {
    verts[i].pos[0] = static_cast<float>(edges.Positions[i].x());
    verts[i].pos[1] = static_cast<float>(edges.Positions[i].y());
    verts[i].pos[2] = static_cast<float>(edges.Positions[i].z());
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
  m_dev->vkMapMemory(m_window->device(), vb.memory, 0, size, 0, &data);
  std::memcpy(data, verts.data(), static_cast<size_t>(size));
  m_dev->vkUnmapMemory(m_window->device(), vb.memory);
  vertex_count = static_cast<uint32_t>(verts.size());
}

void VulkanRenderer::upload_preview_solid()
{
  destroy_buffer(m_previewSolidVb);
  m_previewSolidVertexCount = 0;
  if (m_previewSolid.Indices.empty() || m_previewSolid.Vertices.empty()) return;

  std::vector<AxisVertexGpu> verts;
  verts.reserve(m_previewSolid.Indices.size());
  constexpr float kR = 1.0f;
  constexpr float kG = 0.92f;
  constexpr float kB = 0.15f;
  constexpr float kA = 0.28f;
  for (std::uint32_t idx : m_previewSolid.Indices)
  {
    if (idx >= m_previewSolid.Vertices.size()) continue;
    const auto& v = m_previewSolid.Vertices[idx];
    AxisVertexGpu g{};
    g.pos[0] = static_cast<float>(v.Position.x());
    g.pos[1] = static_cast<float>(v.Position.y());
    g.pos[2] = static_cast<float>(v.Position.z());
    g.color[0] = kR;
    g.color[1] = kG;
    g.color[2] = kB;
    g.color[3] = kA;
    verts.push_back(g);
  }
  if (verts.empty()) return;

  const VkDeviceSize size = sizeof(AxisVertexGpu) * verts.size();
  m_previewSolidVb =
      create_buffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  m_dev->vkMapMemory(m_window->device(), m_previewSolidVb.memory, 0, size, 0,
                    &data);
  std::memcpy(data, verts.data(), static_cast<size_t>(size));
  m_dev->vkUnmapMemory(m_window->device(), m_previewSolidVb.memory);
  m_previewSolidVertexCount = static_cast<uint32_t>(verts.size());
}

void VulkanRenderer::upload_preview()
{
  upload_colored_edges(m_previewEdges, 1.0f, 0.92f, 0.15f, m_previewVb,
                       m_previewVertexCount);
  upload_preview_solid();
  m_previewDirty = false;
}

void VulkanRenderer::upload_snap_overlay()
{
  upload_colored_edges(m_snapOverlayEdges, 0.2f, 1.0f, 0.85f,
                       m_snapOverlayVb, m_snapOverlayVertexCount);
  m_snapOverlayDirty = false;
}

void VulkanRenderer::upload_highlight()
{
  // Orange selection outline.
  upload_colored_edges(m_highlightEdges, 1.0f, 0.55f, 0.1f, m_highlightVb,
                       m_highlightVertexCount);
  m_highlightDirty = false;
}

}  // namespace brep::viewer
