#pragma once

#include "api/Core.h"
#include "api/Mesh.h"
#include "Camera.h"

#include <QVulkanWindow>

#include <cstdint>
#include <string>
#include <vector>

namespace brep::viewer
{

class VulkanWindow;

class VulkanRenderer final : public QVulkanWindowRenderer
{
 public:
  explicit VulkanRenderer(VulkanWindow* window);

  void set_meshes(TriangleMesh triangles, EdgeMesh edges);
  void set_material(Material material);

  /// Pull shared ECS scene bake into this renderer's GPU buffers if needed.
  void sync_from_world();

  /// Selected body drawn with a solid highlight material (separate GPU mesh).
  void set_selection_mesh(TriangleMesh triangles, EdgeMesh edges,
                          Material material);
  void clear_selection_mesh();

  /// Temporary tool preview: wire edges + optional translucent solid fill.
  void set_preview_edges(EdgeMesh edges);
  void set_preview(EdgeMesh edges, TriangleMesh solid);
  void clear_preview();

  /// AccuSnap glyph drawn after the tool preview.
  void set_snap_overlay(EdgeMesh edges);
  void clear_snap_overlay();

  /// Selection outline (drawn on top of scene meshes, under tool preview).
  void set_highlight_edges(EdgeMesh edges);
  void clear_highlight();

  void initResources() override;
  void initSwapChainResources() override;
  void releaseSwapChainResources() override;
  void releaseResources() override;
  void startNextFrame() override;

 private:
  struct Ubo
  {
    float mvp[16];
    float model[16];
    float light_dir[4];
    float albedo_color[4];  // rgb + uv_scale
  };

  struct GpuBuffer
  {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
  };

  struct GpuTexture
  {
    VkImage image{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkImageView view{VK_NULL_HANDLE};
    VkSampler sampler{VK_NULL_HANDLE};
    uint32_t width{0};
    uint32_t height{0};
  };

  [[nodiscard]] GpuBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                        VkMemoryPropertyFlags props);
  void destroy_buffer(GpuBuffer& b);
  [[nodiscard]] uint32_t find_memory_type(uint32_t type_bits,
                                          VkMemoryPropertyFlags props) const;
  [[nodiscard]] VkShaderModule load_shader(const QString& file_name);
  void create_descriptors();
  void create_pipelines();
  void upload_meshes();
  void upload_selection_meshes();
  void upload_axes();
  void upload_preview();
  void upload_preview_solid();
  void upload_snap_overlay();
  void upload_highlight();
  void create_albedo_texture();
  void create_selection_albedo_texture();
  void update_albedo_descriptors();
  void bind_albedo_to_desc(VkDescriptorSet set, const GpuTexture& tex);
  void destroy_texture(GpuTexture& tex);
  void upload_colored_edges(const EdgeMesh& edges, float r, float g, float b,
                            GpuBuffer& vb, std::uint32_t& vertex_count);
  void transition_image_layout(VkImage image, VkImageLayout old_layout,
                               VkImageLayout new_layout);
  void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width,
                            uint32_t height);
  [[nodiscard]] VkCommandBuffer begin_one_shot_commands();
  void end_one_shot_commands(VkCommandBuffer cmd);

  VulkanWindow* m_window{nullptr};
  QVulkanDeviceFunctions* m_dev{nullptr};

  TriangleMesh m_triangles;
  EdgeMesh m_edges;
  TriangleMesh m_selectionTriangles;
  EdgeMesh m_selectionEdges;
  EdgeMesh m_previewEdges;
  TriangleMesh m_previewSolid;
  EdgeMesh m_snapOverlayEdges;
  EdgeMesh m_highlightEdges;
  Material m_material{};
  Material m_selectionMaterial{};
  bool m_meshesDirty{true};
  bool m_materialDirty{true};
  bool m_selectionMeshesDirty{false};
  bool m_selectionMaterialDirty{false};
  bool m_previewDirty{false};
  bool m_snapOverlayDirty{false};
  bool m_highlightDirty{false};
  std::uint64_t m_syncedSceneVersion{0};

  GpuBuffer m_triVb{};
  GpuBuffer m_triIb{};
  GpuBuffer m_lineVb{};
  GpuBuffer m_selTriVb{};
  GpuBuffer m_selTriIb{};
  GpuBuffer m_selLineVb{};
  GpuBuffer m_axisVb{};
  GpuBuffer m_previewVb{};
  GpuBuffer m_previewSolidVb{};
  GpuBuffer m_snapOverlayVb{};
  GpuBuffer m_highlightVb{};
  GpuBuffer m_ubo{};             // scene MVP + wood albedo
  GpuBuffer m_selectionUbo{};   // same MVP + orange selection albedo
  GpuBuffer m_axisUbo{};        // screen-space gizmo MVP (must be separate!)
  GpuTexture m_albedo{};
  GpuTexture m_selectionAlbedo{};

  VkDescriptorPool m_descPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descSet{VK_NULL_HANDLE};
  VkDescriptorSet m_selectionDescSet{VK_NULL_HANDLE};
  VkDescriptorSet m_axisDescSet{VK_NULL_HANDLE};

  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_triPipeline{VK_NULL_HANDLE};
  VkPipeline m_linePipeline{VK_NULL_HANDLE};
  VkPipeline m_axisPipeline{VK_NULL_HANDLE};
  VkPipeline m_previewFillPipeline{VK_NULL_HANDLE};

  VkPipelineCache m_pipelineCache{VK_NULL_HANDLE};
  std::uint32_t m_indexCount{0};
  std::uint32_t m_lineVertexCount{0};
  std::uint32_t m_selIndexCount{0};
  std::uint32_t m_selLineVertexCount{0};
  std::uint32_t m_axisVertexCount{0};
  std::uint32_t m_previewVertexCount{0};
  std::uint32_t m_previewSolidVertexCount{0};
  std::uint32_t m_snapOverlayVertexCount{0};
  std::uint32_t m_highlightVertexCount{0};
};

}  // namespace brep::viewer
