// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_glimmer_vulkan_platform.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include <GLFW/glfw3.h>
#include <cstring>

namespace RocProfVis
{
namespace View
{

GlimmerVulkanPlatform::GlimmerVulkanPlatform()
{
}

GlimmerVulkanPlatform::~GlimmerVulkanPlatform()
{
}

void
GlimmerVulkanPlatform::SetClipboardText(std::string_view input)
{
    m_clipboard_text = input;
    ImGui::SetClipboardText(m_clipboard_text.c_str());
}

std::string_view
GlimmerVulkanPlatform::GetClipboardText()
{
    const char* text = ImGui::GetClipboardText();
    if(text)
    {
        m_clipboard_text = text;
        return m_clipboard_text;
    }
    return "";
}

bool
GlimmerVulkanPlatform::CreateWindow(const glimmer::WindowParams& params)
{
    // Window is already created by the main application
    return true;
}

bool
GlimmerVulkanPlatform::PollEvents(bool (*runner)(ImVec2, glimmer::IPlatform&, void*), void* data)
{
    // Events are already polled by the main application
    return true;
}

ImTextureID
GlimmerVulkanPlatform::UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
{
    // Get Vulkan backend data
    ImGui_ImplVulkan_Data* bd = ImGui_ImplVulkan_GetBackendData();
    if(!bd)
    {
        return nullptr;
    }

    ImGui_ImplVulkan_InitInfo* v = &bd->VulkanInitInfo;
    VkResult                    err;

    // Create command pool/buffer if needed
    if(bd->TexCommandPool == VK_NULL_HANDLE)
    {
        VkCommandPoolCreateInfo info = {};
        info.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags                    = 0;
        info.queueFamilyIndex         = v->QueueFamily;
        err = vkCreateCommandPool(v->Device, &info, v->Allocator, &bd->TexCommandPool);
        if(err != VK_SUCCESS)
        {
            return nullptr;
        }
    }

    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool                 = bd->TexCommandPool;
        info.commandBufferCount          = 1;
        info.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        err = vkAllocateCommandBuffers(v->Device, &info, &command_buffer);
        if(err != VK_SUCCESS)
        {
            return nullptr;
        }
    }

    // Start command buffer
    {
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        if(err != VK_SUCCESS)
        {
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
    }

    int            width  = (int)size.x;
    int            height = (int)size.y;
    size_t upload_size    = width * height * 4 * sizeof(unsigned char);

    // Create the Image
    VkImage        image        = VK_NULL_HANDLE;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    {
        VkImageCreateInfo info = {};
        info.sType            = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType        = VK_IMAGE_TYPE_2D;
        info.format           = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent.width     = width;
        info.extent.height    = height;
        info.extent.depth     = 1;
        info.mipLevels        = 1;
        info.arrayLayers      = 1;
        info.samples          = VK_SAMPLE_COUNT_1_BIT;
        info.tiling           = VK_IMAGE_TILING_OPTIMAL;
        info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.sharingMode      = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        err                   = vkCreateImage(v->Device, &info, v->Allocator, &image);
        if(err != VK_SUCCESS)
        {
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(v->Device, image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize       = req.size;
        alloc_info.memoryTypeIndex =
            ImGui_ImplVulkan_MemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(v->Device, &alloc_info, v->Allocator, &image_memory);
        if(err != VK_SUCCESS)
        {
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
        err = vkBindImageMemory(v->Device, image, image_memory, 0);
        if(err != VK_SUCCESS)
        {
            vkFreeMemory(v->Device, image_memory, v->Allocator);
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
    }

    // Create the Image View
    VkImageView image_view = VK_NULL_HANDLE;
    {
        VkImageViewCreateInfo info = {};
        info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image                 = image;
        info.viewType              = VK_IMAGE_VIEW_TYPE_2D;
        info.format                = VK_FORMAT_R8G8B8A8_UNORM;
        info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.levelCount    = 1;
        info.subresourceRange.layerCount    = 1;
        err = vkCreateImageView(v->Device, &info, v->Allocator, &image_view);
        if(err != VK_SUCCESS)
        {
            vkFreeMemory(v->Device, image_memory, v->Allocator);
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
    }

    // Create the Descriptor Set
    ImTextureID texture_id = ImGui_ImplVulkan_AddTexture(bd->TexSampler, image_view,
                                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Create the Upload Buffer
    VkDeviceMemory upload_buffer_memory = VK_NULL_HANDLE;
    VkBuffer       upload_buffer        = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size               = upload_size;
        buffer_info.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(v->Device, &buffer_info, v->Allocator, &upload_buffer);
        if(err != VK_SUCCESS)
        {
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)texture_id);
            vkDestroyImageView(v->Device, image_view, v->Allocator);
            vkFreeMemory(v->Device, image_memory, v->Allocator);
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(v->Device, upload_buffer, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize       = req.size;
        alloc_info.memoryTypeIndex      = ImGui_ImplVulkan_MemoryType(
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(v->Device, &alloc_info, v->Allocator, &upload_buffer_memory);
        if(err != VK_SUCCESS)
        {
            vkDestroyBuffer(v->Device, upload_buffer, v->Allocator);
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)texture_id);
            vkDestroyImageView(v->Device, image_view, v->Allocator);
            vkFreeMemory(v->Device, image_memory, v->Allocator);
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
        err = vkBindBufferMemory(v->Device, upload_buffer, upload_buffer_memory, 0);
        if(err != VK_SUCCESS)
        {
            vkFreeMemory(v->Device, upload_buffer_memory, v->Allocator);
            vkDestroyBuffer(v->Device, upload_buffer, v->Allocator);
            ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)texture_id);
            vkDestroyImageView(v->Device, image_view, v->Allocator);
            vkFreeMemory(v->Device, image_memory, v->Allocator);
            vkDestroyImage(v->Device, image, v->Allocator);
            vkEndCommandBuffer(command_buffer);
            vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);
            return nullptr;
        }
    }

    // Upload to Buffer
    {
        char* map = nullptr;
        err = vkMapMemory(v->Device, upload_buffer_memory, 0, upload_size, 0, (void**)(&map));
        if(err == VK_SUCCESS)
        {
            memcpy(map, pixels, upload_size);
            VkMappedMemoryRange range[1] = {};
            range[0].sType               = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[0].memory              = upload_buffer_memory;
            range[0].size               = upload_size;
            vkFlushMappedMemoryRanges(v->Device, 1, range);
            vkUnmapMemory(v->Device, upload_buffer_memory);
        }
    }

    // Copy to Image
    {
        VkImageMemoryBarrier copy_barrier[1] = {};
        copy_barrier[0].sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier[0].dstAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier[0].oldLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier[0].newLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier[0].srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier[0].image                 = image;
        copy_barrier[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier[0].subresourceRange.levelCount    = 1;
        copy_barrier[0].subresourceRange.layerCount     = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                            copy_barrier);

        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent.width          = width;
        region.imageExtent.height         = height;
        region.imageExtent.depth          = 1;
        vkCmdCopyBufferToImage(command_buffer, upload_buffer, image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        VkImageMemoryBarrier use_barrier[1] = {};
        use_barrier[0].sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier[0].srcAccessMask         = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier[0].dstAccessMask         = VK_ACCESS_SHADER_READ_BIT;
        use_barrier[0].oldLayout             = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier[0].newLayout             = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier[0].srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        use_barrier[0].image                 = image;
        use_barrier[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier[0].subresourceRange.levelCount      = 1;
        use_barrier[0].subresourceRange.layerCount      = 1;
        vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                            use_barrier);
    }

    // End command buffer
    {
        VkSubmitInfo submit_info = {};
        submit_info.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers    = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        if(err == VK_SUCCESS)
        {
            err = vkQueueSubmit(v->Queue, 1, &submit_info, VK_NULL_HANDLE);
            if(err == VK_SUCCESS)
            {
                vkQueueWaitIdle(v->Queue);
            }
        }
    }

    // Cleanup upload buffer
    vkDestroyBuffer(v->Device, upload_buffer, v->Allocator);
    vkFreeMemory(v->Device, upload_buffer_memory, v->Allocator);
    vkFreeCommandBuffers(v->Device, bd->TexCommandPool, 1, &command_buffer);

    return texture_id;
}

}  // namespace View
}  // namespace RocProfVis

