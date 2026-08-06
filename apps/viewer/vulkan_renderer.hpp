#pragma once

#include "brep/material.hpp"
#include "brep/mesh.hpp"
#include "camera.hpp"

#include <QVulkanWindow>

#include <string>
#include <vector>

namespace brep::viewer {

class VulkanWindow;

class VulkanRenderer final : public QVulkanWindowRenderer {
 public:
  explicit VulkanRenderer(VulkanWindow* window);

  void set_meshes(TriangleMesh triangles, EdgeMesh edges);
  void set_material(Material material);

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
  void upload_axes();
  void create_albedo_texture();
  void destroy_texture(GpuTexture& tex);
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
  Material material_{};
  bool meshes_dirty_{true};
  bool material_dirty_{true};

  GpuBuffer tri_vb_{};
  GpuBuffer tri_ib_{};
  GpuBuffer line_vb_{};
  GpuBuffer axis_vb_{};
  GpuBuffer ubo_{};
  GpuTexture albedo_{};

  VkDescriptorPool desc_pool_{VK_NULL_HANDLE};
  VkDescriptorSetLayout desc_layout_{VK_NULL_HANDLE};
  VkDescriptorSet desc_set_{VK_NULL_HANDLE};

  VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
  VkPipeline tri_pipeline_{VK_NULL_HANDLE};
  VkPipeline line_pipeline_{VK_NULL_HANDLE};
  VkPipeline axis_pipeline_{VK_NULL_HANDLE};

  VkPipelineCache pipeline_cache_{VK_NULL_HANDLE};
  std::uint32_t index_count_{0};
  std::uint32_t line_vertex_count_{0};
  std::uint32_t axis_vertex_count_{0};
};

}  // namespace brep::viewer
