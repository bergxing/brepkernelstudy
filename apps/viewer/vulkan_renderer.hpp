#pragma once

#include "brep/material.hpp"
#include "brep/mesh.hpp"
#include "camera.hpp"

#include <QVulkanWindow>

#include <cstdint>
#include <string>
#include <vector>

namespace brep::viewer {

class VulkanWindow;

class VulkanRenderer final : public QVulkanWindowRenderer {
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

  /// Selection outline (drawn on top of scene meshes, under tool preview).
  void set_highlight_edges(EdgeMesh edges);
  void clear_highlight();

  void initResources() override;
  void initSwapChainResources() override;
  void releaseSwapChainResources() override;
  void releaseResources() override;
  void startNextFrame() override;

 private:
  struct Ubo {
    float mvp[16];
    float model[16];
    float light_dir[4];
    float albedo_color[4];  // rgb + uv_scale
  };

  struct GpuBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VkDeviceMemory memory{VK_NULL_HANDLE};
    VkDeviceSize size{0};
  };

  struct GpuTexture {
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

  VulkanWindow* window_{nullptr};
  QVulkanDeviceFunctions* dev_{nullptr};

  TriangleMesh triangles_;
  EdgeMesh edges_;
  TriangleMesh selection_triangles_;
  EdgeMesh selection_edges_;
  EdgeMesh preview_edges_;
  TriangleMesh preview_solid_;
  EdgeMesh highlight_edges_;
  Material material_{};
  Material selection_material_{};
  bool meshes_dirty_{true};
  bool material_dirty_{true};
  bool selection_meshes_dirty_{false};
  bool selection_material_dirty_{false};
  bool preview_dirty_{false};
  bool highlight_dirty_{false};
  std::uint64_t synced_scene_version_{0};

  GpuBuffer tri_vb_{};
  GpuBuffer tri_ib_{};
  GpuBuffer line_vb_{};
  GpuBuffer sel_tri_vb_{};
  GpuBuffer sel_tri_ib_{};
  GpuBuffer sel_line_vb_{};
  GpuBuffer axis_vb_{};
  GpuBuffer preview_vb_{};
  GpuBuffer preview_solid_vb_{};
  GpuBuffer highlight_vb_{};
  GpuBuffer ubo_{};             // scene MVP + wood albedo
  GpuBuffer selection_ubo_{};   // same MVP + orange selection albedo
  GpuBuffer axis_ubo_{};        // screen-space gizmo MVP (must be separate!)
  GpuTexture albedo_{};
  GpuTexture selection_albedo_{};

  VkDescriptorPool desc_pool_{VK_NULL_HANDLE};
  VkDescriptorSetLayout desc_layout_{VK_NULL_HANDLE};
  VkDescriptorSet desc_set_{VK_NULL_HANDLE};
  VkDescriptorSet selection_desc_set_{VK_NULL_HANDLE};
  VkDescriptorSet axis_desc_set_{VK_NULL_HANDLE};

  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline tri_pipeline_{VK_NULL_HANDLE};
  VkPipeline line_pipeline_{VK_NULL_HANDLE};
  VkPipeline axis_pipeline_{VK_NULL_HANDLE};
  VkPipeline preview_fill_pipeline_{VK_NULL_HANDLE};

  VkPipelineCache pipeline_cache_{VK_NULL_HANDLE};
  std::uint32_t index_count_{0};
  std::uint32_t line_vertex_count_{0};
  std::uint32_t sel_index_count_{0};
  std::uint32_t sel_line_vertex_count_{0};
  std::uint32_t axis_vertex_count_{0};
  std::uint32_t preview_vertex_count_{0};
  std::uint32_t preview_solid_vertex_count_{0};
  std::uint32_t highlight_vertex_count_{0};
};

}  // namespace brep::viewer
