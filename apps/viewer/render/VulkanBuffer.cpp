#include "VulkanRenderer.h"

#include "VulkanWindow.h"

#include <QFile>
#include <QVulkanDeviceFunctions>

#include <cstring>
#include <stdexcept>

namespace brep::viewer
{

uint32_t VulkanRenderer::find_memory_type(uint32_t type_bits,
                                          VkMemoryPropertyFlags props) const
{
  VkPhysicalDeviceMemoryProperties mem_props;
  m_window->vulkanInstance()->functions()->vkGetPhysicalDeviceMemoryProperties(
      m_window->physicalDevice(), &mem_props);
  for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i)
  {
    if ((type_bits & (1u << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & props) == props)
    {
      return i;
    }
  }
  throw std::runtime_error("VulkanRenderer: no suitable memory type");
}

VulkanRenderer::GpuBuffer VulkanRenderer::create_buffer(
    VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props)
{
  const VkDevice device = m_window->device();
  GpuBuffer out{};
  out.size = size;

  VkBufferCreateInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bi.size = std::max<VkDeviceSize>(size, 1);
  bi.usage = usage;
  bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (m_dev->vkCreateBuffer(device, &bi, nullptr, &out.buffer) != VK_SUCCESS)
  {
    throw std::runtime_error("vkCreateBuffer failed");
  }

  VkMemoryRequirements req{};
  m_dev->vkGetBufferMemoryRequirements(device, out.buffer, &req);

  VkMemoryAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.allocationSize = req.size;
  ai.memoryTypeIndex = find_memory_type(req.memoryTypeBits, props);
  if (m_dev->vkAllocateMemory(device, &ai, nullptr, &out.memory) != VK_SUCCESS)
  {
    throw std::runtime_error("vkAllocateMemory failed");
  }
  m_dev->vkBindBufferMemory(device, out.buffer, out.memory, 0);
  return out;
}

void VulkanRenderer::destroy_buffer(GpuBuffer& b)
{
  const VkDevice device = m_window->device();
  if (!m_dev || device == VK_NULL_HANDLE)
  {
    b = {};
    return;
  }
  if (b.buffer) m_dev->vkDestroyBuffer(device, b.buffer, nullptr);
  if (b.memory) m_dev->vkFreeMemory(device, b.memory, nullptr);
  b = {};
}

void VulkanRenderer::destroy_texture(GpuTexture& tex)
{
  const VkDevice device = m_window->device();
  if (!m_dev || device == VK_NULL_HANDLE)
  {
    tex = {};
    return;
  }
  if (tex.sampler) m_dev->vkDestroySampler(device, tex.sampler, nullptr);
  if (tex.view) m_dev->vkDestroyImageView(device, tex.view, nullptr);
  if (tex.image) m_dev->vkDestroyImage(device, tex.image, nullptr);
  if (tex.memory) m_dev->vkFreeMemory(device, tex.memory, nullptr);
  tex = {};
}

VkShaderModule VulkanRenderer::load_shader(const QString& file_name)
{
  QFile file(file_name);
  if (!file.open(QIODevice::ReadOnly))
  {
    throw std::runtime_error("failed to open shader: " + file_name.toStdString());
  }
  const QByteArray blob = file.readAll();
  VkShaderModuleCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = static_cast<size_t>(blob.size());
  ci.pCode = reinterpret_cast<const uint32_t*>(blob.constData());
  VkShaderModule module{VK_NULL_HANDLE};
  if (m_dev->vkCreateShaderModule(m_window->device(), &ci, nullptr, &module) !=
      VK_SUCCESS)
  {
    throw std::runtime_error("vkCreateShaderModule failed");
  }
  return module;
}

VkCommandBuffer VulkanRenderer::begin_one_shot_commands()
{
  VkCommandBufferAllocateInfo ai{};
  ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool = m_window->graphicsCommandPool();
  ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VkCommandBuffer cmd{VK_NULL_HANDLE};
  m_dev->vkAllocateCommandBuffers(m_window->device(), &ai, &cmd);

  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  m_dev->vkBeginCommandBuffer(cmd, &bi);
  return cmd;
}

void VulkanRenderer::end_one_shot_commands(VkCommandBuffer cmd)
{
  m_dev->vkEndCommandBuffer(cmd);
  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &cmd;
  m_dev->vkQueueSubmit(m_window->graphicsQueue(), 1, &si, VK_NULL_HANDLE);
  m_dev->vkQueueWaitIdle(m_window->graphicsQueue());
  m_dev->vkFreeCommandBuffers(m_window->device(), m_window->graphicsCommandPool(), 1,
                             &cmd);
}

void VulkanRenderer::transition_image_layout(VkImage image, VkImageLayout old_layout,
                                             VkImageLayout new_layout)
{
  VkCommandBuffer cmd = begin_one_shot_commands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
  {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
  {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else
  {
    throw std::runtime_error("unsupported image layout transition");
  }

  m_dev->vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr,
                             1, &barrier);
  end_one_shot_commands(cmd);
}

void VulkanRenderer::copy_buffer_to_image(VkBuffer buffer, VkImage image,
                                          uint32_t width, uint32_t height)
{
  VkCommandBuffer cmd = begin_one_shot_commands();
  VkBufferImageCopy region{};
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {width, height, 1};
  m_dev->vkCmdCopyBufferToImage(cmd, buffer, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  end_one_shot_commands(cmd);
}

}  // namespace brep::viewer
