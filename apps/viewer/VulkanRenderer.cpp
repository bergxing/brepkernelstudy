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

void VulkanRenderer::set_material(Material material)
{
  m_material = std::move(material);
  m_materialDirty = true;
}

void VulkanRenderer::set_selection_mesh(TriangleMesh triangles, EdgeMesh edges,
                                        Material material)
{
  m_selectionTriangles = std::move(triangles);
  m_selectionEdges = std::move(edges);
  m_selectionMaterial = std::move(material);
  m_selectionMeshesDirty = true;
  m_selectionMaterialDirty = true;
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

}  // namespace brep::viewer
