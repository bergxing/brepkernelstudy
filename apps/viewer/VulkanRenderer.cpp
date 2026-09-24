#include "VulkanRenderer.h"

namespace brep::viewer
{

VulkanRenderer::VulkanRenderer(VulkanWindow* window) : m_window(window)
{
}

void VulkanRenderer::set_meshes(TriangleMesh triangles, EdgeMesh edges)
{
  const bool incoming_empty =
      triangles.Indices.empty() && edges.Positions.empty();
  const bool current_empty =
      m_triangles.Indices.empty() && m_edges.Positions.empty();
  if (incoming_empty && current_empty) return;

  m_triangles = std::move(triangles);
  m_edges = std::move(edges);
  m_meshesDirty = true;
}

namespace
{

[[nodiscard]] bool SameAlbedo(const Material& a, const Material& b) noexcept
{
  return a.AlbedoPath == b.AlbedoPath && a.UvScale == b.UvScale &&
         a.AlbedoColor[0] == b.AlbedoColor[0] &&
         a.AlbedoColor[1] == b.AlbedoColor[1] &&
         a.AlbedoColor[2] == b.AlbedoColor[2];
}

}  // namespace

void VulkanRenderer::set_material(Material material)
{
  if (m_albedo.image != VK_NULL_HANDLE && SameAlbedo(m_material, material))
  {
    m_material = std::move(material);
    return;
  }
  m_material = std::move(material);
  m_materialDirty = true;
}

void VulkanRenderer::set_selection_mesh(TriangleMesh triangles, EdgeMesh edges,
                                        Material material)
{
  const bool materialChanged =
      m_selectionAlbedo.image == VK_NULL_HANDLE ||
      !SameAlbedo(m_selectionMaterial, material);
  m_selectionTriangles = std::move(triangles);
  m_selectionEdges = std::move(edges);
  m_selectionMaterial = std::move(material);
  m_selectionMeshesDirty = true;
  if (materialChanged)
  {
    m_selectionMaterialDirty = true;
  }
}

void VulkanRenderer::clear_selection_mesh()
{
  if (m_selectionTriangles.Indices.empty() &&
      m_selectionEdges.Positions.empty() && m_selIndexCount == 0 &&
      m_selLineVertexCount == 0)
      {
    return;
  }
  m_selectionTriangles = {};
  m_selectionEdges = {};
  m_selectionMeshesDirty = true;
}

void VulkanRenderer::set_preview_edges(EdgeMesh edges)
{
  set_preview(std::move(edges), {});
}

void VulkanRenderer::set_preview(EdgeMesh edges, TriangleMesh solid)
{
  m_previewEdges = std::move(edges);
  m_previewSolid = std::move(solid);
  m_previewDirty = true;
}

void VulkanRenderer::clear_preview()
{
  if (m_previewEdges.Positions.empty() && m_previewSolid.Indices.empty() &&
      m_previewVertexCount == 0 && m_previewSolidVertexCount == 0)
  {
    return;
  }
  m_previewEdges = {};
  m_previewSolid = {};
  m_previewDirty = true;
}

void VulkanRenderer::set_snap_overlay(EdgeMesh edges)
{
  m_snapOverlayEdges = std::move(edges);
  m_snapOverlayDirty = true;
}

void VulkanRenderer::clear_snap_overlay()
{
  if (m_snapOverlayEdges.Positions.empty() &&
      m_snapOverlayVertexCount == 0)
  {
    return;
  }
  m_snapOverlayEdges = {};
  m_snapOverlayDirty = true;
}

void VulkanRenderer::set_highlight_edges(EdgeMesh edges)
{
  m_highlightEdges = std::move(edges);
  m_highlightDirty = true;
}

void VulkanRenderer::clear_highlight()
{
  if (m_highlightEdges.Positions.empty() && m_highlightVertexCount == 0)
  {
    return;
  }
  m_highlightEdges = {};
  m_highlightDirty = true;
}

void VulkanRenderer::set_hover_edges(EdgeMesh edges)
{
  m_hoverEdges = std::move(edges);
  m_hoverDirty = true;
}

void VulkanRenderer::clear_hover()
{
  if (m_hoverEdges.Positions.empty() && m_hoverVertexCount == 0)
  {
    return;
  }
  m_hoverEdges = {};
  m_hoverDirty = true;
}

void VulkanRenderer::set_viewport_colors(float clearR, float clearG, float clearB,
                                        float wireR, float wireG, float wireB,
                                        float hoverR, float hoverG, float hoverB,
                                        float previewR, float previewG,
                                        float previewB)
{
  m_clearColor[0] = clearR;
  m_clearColor[1] = clearG;
  m_clearColor[2] = clearB;
  m_wireColor[0] = wireR;
  m_wireColor[1] = wireG;
  m_wireColor[2] = wireB;
  m_hoverColor[0] = hoverR;
  m_hoverColor[1] = hoverG;
  m_hoverColor[2] = hoverB;
  m_previewColor[0] = previewR;
  m_previewColor[1] = previewG;
  m_previewColor[2] = previewB;
  m_meshesDirty = true;
  m_selectionMeshesDirty = true;
  m_hoverDirty = true;
  m_previewDirty = true;
}

}  // namespace brep::viewer
