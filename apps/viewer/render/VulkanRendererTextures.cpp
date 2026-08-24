#include "VulkanRenderer.h"

#include "VulkanWindow.h"

#include "api/Core.h"

#include <QImage>
#include <QVulkanDeviceFunctions>

#include <cstring>
#include <stdexcept>

namespace brep::viewer
{

void VulkanRenderer::create_albedo_texture()
{
  destroy_texture(m_albedo);

  QImage image;
  if (!m_material.AlbedoPath.empty())
  {
    image = QImage(QString::fromStdString(m_material.AlbedoPath));
  }
  if (image.isNull())
  {
    image = QImage(2, 2, QImage::Format_RGBA8888);
    const QRgb c = qRgba(int(m_material.AlbedoColor[0] * 255),
                         int(m_material.AlbedoColor[1] * 255),
                         int(m_material.AlbedoColor[2] * 255), 255);
    image.fill(c);
    BREP_WARN("albedo texture missing ('{}'); using solid color fallback",
              m_material.AlbedoPath);
  }
  else
  {
    image = image.convertToFormat(QImage::Format_RGBA8888);
    BREP_INFO("loaded albedo '{}' {}x{}", m_material.Name, image.width(),
              image.height());
  }

  const uint32_t width = uint32_t(image.width());
  const uint32_t height = uint32_t(image.height());
  const VkDeviceSize image_size = VkDeviceSize(width) * height * 4;
  m_albedo.width = width;
  m_albedo.height = height;

  GpuBuffer staging = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  m_dev->vkMapMemory(m_window->device(), staging.memory, 0, image_size, 0, &data);
  std::memcpy(data, image.constBits(), static_cast<size_t>(image_size));
  m_dev->vkUnmapMemory(m_window->device(), staging.memory);

  const VkDevice device = m_window->device();
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
  if (m_dev->vkCreateImage(device, &ii, nullptr, &m_albedo.image) != VK_SUCCESS)
  {
    destroy_buffer(staging);
    throw std::runtime_error("vkCreateImage failed");
  }

  VkMemoryRequirements req{};
  m_dev->vkGetImageMemoryRequirements(device, m_albedo.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (m_dev->vkAllocateMemory(device, &ai, nullptr, &m_albedo.memory) != VK_SUCCESS)
  {
    destroy_buffer(staging);
    throw std::runtime_error("vkAllocateMemory (image) failed");
  }
  m_dev->vkBindImageMemory(device, m_albedo.image, m_albedo.memory, 0);

  transition_image_layout(m_albedo.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(staging.buffer, m_albedo.image, width, height);
  transition_image_layout(m_albedo.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destroy_buffer(staging);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = m_albedo.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  if (m_dev->vkCreateImageView(device, &vi, nullptr, &m_albedo.view) != VK_SUCCESS)
  {
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
  if (m_dev->vkCreateSampler(device, &si, nullptr, &m_albedo.sampler) != VK_SUCCESS)
  {
    throw std::runtime_error("vkCreateSampler failed");
  }

  m_materialDirty = false;
}

void VulkanRenderer::bind_albedo_to_desc(VkDescriptorSet set,
                                         const GpuTexture& tex)
{
  if (!m_dev || set == VK_NULL_HANDLE || tex.view == VK_NULL_HANDLE ||
      tex.sampler == VK_NULL_HANDLE)
  {
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
  m_dev->vkUpdateDescriptorSets(m_window->device(), 1, &write, 0, nullptr);
}

void VulkanRenderer::update_albedo_descriptors()
{
  bind_albedo_to_desc(m_descSet, m_albedo);
  bind_albedo_to_desc(m_axisDescSet, m_albedo);
  if (m_selectionDescSet)
  {
    bind_albedo_to_desc(m_selectionDescSet, m_selectionAlbedo);
  }
}

void VulkanRenderer::create_selection_albedo_texture()
{
  destroy_texture(m_selectionAlbedo);

  QImage image(2, 2, QImage::Format_RGBA8888);
  const QRgb c = qRgba(int(m_selectionMaterial.AlbedoColor[0] * 255),
                       int(m_selectionMaterial.AlbedoColor[1] * 255),
                       int(m_selectionMaterial.AlbedoColor[2] * 255), 255);
  image.fill(c);

  const uint32_t width = 2;
  const uint32_t height = 2;
  const VkDeviceSize image_size = VkDeviceSize(width) * height * 4;
  m_selectionAlbedo.width = width;
  m_selectionAlbedo.height = height;

  GpuBuffer staging = create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  void* data = nullptr;
  m_dev->vkMapMemory(m_window->device(), staging.memory, 0, image_size, 0, &data);
  std::memcpy(data, image.constBits(), static_cast<size_t>(image_size));
  m_dev->vkUnmapMemory(m_window->device(), staging.memory);

  const VkDevice device = m_window->device();
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
  if (m_dev->vkCreateImage(device, &ii, nullptr, &m_selectionAlbedo.image) !=
      VK_SUCCESS)
  {
    destroy_buffer(staging);
    throw std::runtime_error("vkCreateImage (selection) failed");
  }

  VkMemoryRequirements req{};
  m_dev->vkGetImageMemoryRequirements(device, m_selectionAlbedo.image, &req);
  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex =
      find_memory_type(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (m_dev->vkAllocateMemory(device, &ai, nullptr, &m_selectionAlbedo.memory) !=
      VK_SUCCESS)
  {
    destroy_buffer(staging);
    throw std::runtime_error("vkAllocateMemory (selection image) failed");
  }
  m_dev->vkBindImageMemory(device, m_selectionAlbedo.image,
                          m_selectionAlbedo.memory, 0);

  transition_image_layout(m_selectionAlbedo.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copy_buffer_to_image(staging.buffer, m_selectionAlbedo.image, width, height);
  transition_image_layout(m_selectionAlbedo.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  destroy_buffer(staging);

  VkImageViewCreateInfo vi{};
  vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  vi.image = m_selectionAlbedo.image;
  vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
  vi.format = VK_FORMAT_R8G8B8A8_UNORM;
  vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  vi.subresourceRange.levelCount = 1;
  vi.subresourceRange.layerCount = 1;
  if (m_dev->vkCreateImageView(device, &vi, nullptr, &m_selectionAlbedo.view) !=
      VK_SUCCESS)
  {
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
  if (m_dev->vkCreateSampler(device, &si, nullptr, &m_selectionAlbedo.sampler) !=
      VK_SUCCESS)
  {
    throw std::runtime_error("vkCreateSampler (selection) failed");
  }

  m_selectionMaterialDirty = false;
}

}  // namespace brep::viewer
