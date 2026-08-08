#include "vulkan_renderer.hpp"

namespace brep::viewer {

VulkanRenderer::VulkanRenderer(VulkanWindow* window) : window_(window) {}

void VulkanRenderer::set_meshes(TriangleMesh triangles, EdgeMesh edges) {
  const bool incoming_empty =
      triangles.indices.empty() && edges.positions.empty();
  const bool current_empty =
      triangles_.indices.empty() && edges_.positions.empty();
  if (incoming_empty && current_empty) return;

  triangles_ = std::move(triangles);
  edges_ = std::move(edges);
  meshes_dirty_ = true;
}

void VulkanRenderer::set_material(Material material) {
  material_ = std::move(material);
  material_dirty_ = true;
}

void VulkanRenderer::set_selection_mesh(TriangleMesh triangles, EdgeMesh edges,
                                        Material material) {
  selection_triangles_ = std::move(triangles);
  selection_edges_ = std::move(edges);
  selection_material_ = std::move(material);
  selection_meshes_dirty_ = true;
  selection_material_dirty_ = true;
}

void VulkanRenderer::clear_selection_mesh() {
  if (selection_triangles_.indices.empty() &&
      selection_edges_.positions.empty() && sel_index_count_ == 0 &&
      sel_line_vertex_count_ == 0) {
    return;
  }
  selection_triangles_ = {};
  selection_edges_ = {};
  selection_meshes_dirty_ = true;
}

void VulkanRenderer::set_preview_edges(EdgeMesh edges) {
  set_preview(std::move(edges), {});
}

void VulkanRenderer::set_preview(EdgeMesh edges, TriangleMesh solid) {
  preview_edges_ = std::move(edges);
  preview_solid_ = std::move(solid);
  preview_dirty_ = true;
}

void VulkanRenderer::clear_preview() {
  if (preview_edges_.positions.empty() && preview_solid_.indices.empty() &&
      preview_vertex_count_ == 0 && preview_solid_vertex_count_ == 0) {
    return;
  }
  preview_edges_ = {};
  preview_solid_ = {};
  preview_dirty_ = true;
}

void VulkanRenderer::set_highlight_edges(EdgeMesh edges) {
  highlight_edges_ = std::move(edges);
  highlight_dirty_ = true;
}

void VulkanRenderer::clear_highlight() {
  if (highlight_edges_.positions.empty() && highlight_vertex_count_ == 0) {
    return;
  }
  highlight_edges_ = {};
  highlight_dirty_ = true;
}

}  // namespace brep::viewer
