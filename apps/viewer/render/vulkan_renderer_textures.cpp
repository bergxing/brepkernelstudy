#include "vulkan_renderer.hpp"

#include "vulkan_window.hpp"

#include "brep/log.hpp"

#include <QImage>
#include <QVulkanDeviceFunctions>

#include <cstring>
#include <stdexcept>

namespace brep::viewer {

void VulkanRenderer::create_albedo_texture() {
  destroy_texture(albedo_);

  QImage image;
  if (!material_.albedo_path.empty()) {
    image = QImage(QString::fromStdString(material_.albedo_path));
  }
  if (image.isNull()) {
    image = QImage(2, 2, QImage::Format_RGBA8888);
    const QRgb c = qRgba(int(material_.albedo_color[0] * 255),
                         int(material_.albedo_color[1] * 255),
                         int(material_.albedo_color[2] * 255), 255);
    image.fill(c);
    BREP_WARN("albedo texture missing ('{}'); using solid color fallback",
              material_.albedo_path);
  } else {
    image = image.convertToFormat(QImage::Format_RGBA8888);
    BREP_INFO("loaded albedo '{}' {}x{}", material_.name, image.width(),
              image.height());
  }

  const uint32_t width = uint32_t(image.width());
  const uint32_t height = uint32_t(image.height());
  const VkDeviceSize image_size = VkDeviceSize(width) * height * 4;
  albedo_.width = width;
  albedo_.height = height;

  GpuBuffer staging = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), staging.memory, 0, image_size, 0, &data);
  std::memcpy(data, image.constBits(), static_cast<size_t>(image_size));
  dev_->vkUnmapMemory(window_->device(), staging.memory);

  const VkDevice device = window_->device();
  VkImageCreateInfo ii{};
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.extent = {width, height, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.format = VK_FORMAT_R8G8B8A8_UNORM;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (dev_->vkCreateImage(device, &ii, nullptr, &albedo_.image) != VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkCreateImage failed");
  }

  VkMemoryRequirements req{};
  dev_->vkGetImageMemoryRequirements(device, albedo_.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (dev_->vkAllocateMemory(device, &ai, nullptr, &albedo_.memory) != VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkAllocateMemory (image) failed");
  }
  dev_->vkBindImageMemory(device, albedo_.image, albedo_.memory, 0);

  transition_image_layout(albedo_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(staging.buffer, albedo_.image, width, height);
  transition_image_layout(albedo_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destroy_buffer(staging);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = albedo_.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  if (dev_->vkCreateImageView(device, &vi, nullptr, &albedo_.view) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateImageView failed");
  }

  VkSamplerCreateInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  si.magFilter = VK_FILTER_LINEAR;
  si.minFilter = VK_FILTER_LINEAR;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.maxAnisotropy = 1.0f;
  si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  si.unnormalizedCoordinates = VK_FALSE;
  si.compareEnable = VK_FALSE;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  if (dev_->vkCreateSampler(device, &si, nullptr, &albedo_.sampler) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateSampler failed");
  }

  material_dirty_ = false;
}

void VulkanRenderer::bind_albedo_to_desc(VkDescriptorSet set,
                                         const GpuTexture& tex) {
  if (!dev_ || set == VK_NULL_HANDLE || tex.view == VK_NULL_HANDLE ||
      tex.sampler == VK_NULL_HANDLE) {
    return;
  }
  VkDescriptorImageInfo ii{};
  ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  ii.imageView = tex.view;
  ii.sampler = tex.sampler;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = 1;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  write.pImageInfo = &ii;
  dev_->vkUpdateDescriptorSets(window_->device(), 1, &write, 0, nullptr);
}

void VulkanRenderer::update_albedo_descriptors() {
  bind_albedo_to_desc(desc_set_, albedo_);
  bind_albedo_to_desc(axis_desc_set_, albedo_);
  if (selection_desc_set_) {
    bind_albedo_to_desc(selection_desc_set_, selection_albedo_);
  }
}

void VulkanRenderer::create_selection_albedo_texture() {
  destroy_texture(selection_albedo_);

  QImage image(2, 2, QImage::Format_RGBA8888);
  const QRgb c = qRgba(int(selection_material_.albedo_color[0] * 255),
                       int(selection_material_.albedo_color[1] * 255),
                       int(selection_material_.albedo_color[2] * 255), 255);
  image.fill(c);

  const uint32_t width = 2;
  const uint32_t height = 2;
  const VkDeviceSize image_size = VkDeviceSize(width) * height * 4;
  selection_albedo_.width = width;
  selection_albedo_.height = height;

  GpuBuffer staging = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  dev_->vkMapMemory(window_->device(), staging.memory, 0, image_size, 0, &data);
  std::memcpy(data, image.constBits(), static_cast<size_t>(image_size));
  dev_->vkUnmapMemory(window_->device(), staging.memory);

  const VkDevice device = window_->device();
  VkImageCreateInfo ii{};
  ii.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ii.imageType = VK_IMAGE_TYPE_2D;
  ii.extent = {width, height, 1};
  ii.mipLevels = 1;
  ii.arrayLayers = 1;
  ii.format = VK_FORMAT_R8G8B8A8_UNORM;
  ii.tiling = VK_IMAGE_TILING_OPTIMAL;
  ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ii.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  ii.samples = VK_SAMPLE_COUNT_1_BIT;
  ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (dev_->vkCreateImage(device, &ii, nullptr, &selection_albedo_.image) !=
      VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkCreateImage (selection) failed");
  }

  VkMemoryRequirements req{};
  dev_->vkGetImageMemoryRequirements(device, selection_albedo_.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (dev_->vkAllocateMemory(device, &ai, nullptr, &selection_albedo_.memory) !=
      VK_SUCCESS) {
    destroy_buffer(staging);
    throw std::runtime_error("vkAllocateMemory (selection image) failed");
  }
  dev_->vkBindImageMemory(device, selection_albedo_.image,
                          selection_albedo_.memory, 0);

  transition_image_layout(selection_albedo_.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(staging.buffer, selection_albedo_.image, width, height);
  transition_image_layout(selection_albedo_.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destroy_buffer(staging);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = selection_albedo_.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  if (dev_->vkCreateImageView(device, &vi, nullptr, &selection_albedo_.view) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateImageView (selection) failed");
  }

  VkSamplerCreateInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  si.magFilter = VK_FILTER_LINEAR;
  si.minFilter = VK_FILTER_LINEAR;
  si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  si.maxAnisotropy = 1.0f;
  si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  si.unnormalizedCoordinates = VK_FALSE;
  si.compareEnable = VK_FALSE;
  si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  if (dev_->vkCreateSampler(device, &si, nullptr, &selection_albedo_.sampler) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateSampler (selection) failed");
  }

  selection_material_dirty_ = false;
}

}  // namespace brep::viewer
