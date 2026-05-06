// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "spdlog/spdlog.h"

typedef struct rocprofvis_imgui_vk_texture_t
{
    VkImage         m_image          = VK_NULL_HANDLE;
    VkDeviceMemory  m_memory         = VK_NULL_HANDLE;
    VkImageView     m_image_view     = VK_NULL_HANDLE;
    VkSampler       m_sampler        = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;
} rocprofvis_imgui_vk_texture_t;

typedef struct rocprofvis_imgui_vk_data_t
{
    ImGui_ImplVulkanH_Window m_window_data;
    VkAllocationCallbacks*   m_allocator         = nullptr;
    VkInstance               m_instance          = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physical_device   = VK_NULL_HANDLE;
    VkDevice                 m_device            = VK_NULL_HANDLE;
    uint32_t                 m_queue_family      = (uint32_t) -1;
    VkQueue                  m_queue             = VK_NULL_HANDLE;
    VkPipelineCache          m_pipeline_cache    = VK_NULL_HANDLE;
    VkDescriptorPool         m_descriptor_pool   = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT m_debug_report      = VK_NULL_HANDLE;
    int                      m_min_image_count   = 2;
    bool                     m_swapchain_rebuild = false;
    std::vector<rocprofvis_imgui_vk_texture_t*>* m_textures = nullptr;
} rocprofvis_imgui_vk_data_t;

static void
rocprofvis_imgui_backend_vk_check_result(VkResult err)
{
    if(err != 0)
    {
        spdlog::error("[vulkan] Error: VkResult = {}", static_cast<int>(err));
    }
}

static bool
rocprofvis_imgui_backend_vk_success(VkResult err)
{
    if(err != 0)
    {
        spdlog::error("[vulkan] Error: VkResult = {}", static_cast<int>(err));
    }
    return (err == 0);
}

static uint32_t
rocprofvis_imgui_backend_vk_find_memory_type(VkPhysicalDevice physical_device,
                                             uint32_t type_filter,
                                             VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for(uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if((type_filter & (1u << i)) &&
           (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

static void
rocprofvis_imgui_backend_vk_destroy_texture_resource(
    rocprofvis_imgui_vk_data_t* backend_data,
    rocprofvis_imgui_vk_texture_t* texture,
    bool remove_descriptor)
{
    if(!backend_data || !texture || backend_data->m_device == VK_NULL_HANDLE)
    {
        return;
    }

    if(remove_descriptor && texture->m_descriptor_set != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkan_RemoveTexture(texture->m_descriptor_set);
        texture->m_descriptor_set = VK_NULL_HANDLE;
    }
    if(texture->m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(backend_data->m_device, texture->m_sampler,
                         backend_data->m_allocator);
        texture->m_sampler = VK_NULL_HANDLE;
    }
    if(texture->m_image_view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(backend_data->m_device, texture->m_image_view,
                           backend_data->m_allocator);
        texture->m_image_view = VK_NULL_HANDLE;
    }
    if(texture->m_image != VK_NULL_HANDLE)
    {
        vkDestroyImage(backend_data->m_device, texture->m_image,
                       backend_data->m_allocator);
        texture->m_image = VK_NULL_HANDLE;
    }
    if(texture->m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(backend_data->m_device, texture->m_memory,
                     backend_data->m_allocator);
        texture->m_memory = VK_NULL_HANDLE;
    }
}

static void
rocprofvis_imgui_backend_vk_destroy_all_textures(rocprofvis_imgui_vk_data_t* backend_data,
                                                 bool remove_descriptors)
{
    if(!backend_data || !backend_data->m_textures)
    {
        return;
    }

    if(backend_data->m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(backend_data->m_device);
    }

    for(rocprofvis_imgui_vk_texture_t* texture : *backend_data->m_textures)
    {
        rocprofvis_imgui_backend_vk_destroy_texture_resource(
            backend_data, texture, remove_descriptors);
        delete texture;
    }
    backend_data->m_textures->clear();
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL
rocprofvis_imgui_backend_vk_debug_report(VkDebugReportFlagsEXT      flags,
                                         VkDebugReportObjectTypeEXT objectType,
                                         uint64_t object, size_t location,
                                         int32_t messageCode, const char* pLayerPrefix,
                                         const char* pMessage, void* pUserData)
{
    (void) flags;
    (void) object;
    (void) location;
    (void) messageCode;
    (void) pUserData;
    (void) pLayerPrefix;  // Unused arguments
    spdlog::error("[vulkan] Debug: report from ObjectType: {}\nMessage: {}\n\n",
            static_cast<int>(objectType), pMessage);
    return VK_FALSE;
}
#endif  // _DEBUG

static bool
rocprofvis_imgui_backend_vk_has_extension(
    const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for(const VkExtensionProperties& p : properties)
        if(strcmp(p.extensionName, extension) == 0) return true;
    return false;
}

static bool
rocprofvis_imgui_backend_vk_select_physical_device(
    rocprofvis_imgui_vk_data_t* backend_data)
{
    bool     bOk = false;
    uint32_t gpu_count;
    VkResult err =
        vkEnumeratePhysicalDevices(backend_data->m_instance, &gpu_count, nullptr);
    if(rocprofvis_imgui_backend_vk_success(err))
    {
        IM_ASSERT(gpu_count > 0);

        ImVector<VkPhysicalDevice> gpus;
        gpus.resize(gpu_count);
        err = vkEnumeratePhysicalDevices(backend_data->m_instance, &gpu_count, gpus.Data);
        if(rocprofvis_imgui_backend_vk_success(err))
        {
            // Default to the first GPU
            if(gpu_count > 0)
            {
                backend_data->m_physical_device = gpus[0];
                bOk                             = true;
            }

            // Then look for the first discrete GPU, this is the simplest way to handle
            // the common cases of integrated+discrete GPUs. Really we want to identify
            // which GPU is connected to the screen.
            for(VkPhysicalDevice& m_device : gpus)
            {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(m_device, &properties);
                if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    backend_data->m_physical_device = m_device;
                    bOk                             = true;
                    break;
                }
            }
        }
    }
    else
    {
        spdlog::error("[vulkan] Error: Couldn't enumerate GPUs.");
    }
    return bOk;
}

static bool
rocprofvis_imgui_backend_vk_setup_vulkan(rocprofvis_imgui_vk_data_t* backend_data,
                                         ImVector<const char*>       instance_extensions)
{
    bool     bResult = true;
    VkResult err;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo instance_create_info = {};
        instance_create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t                        properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count,
                                                     properties.Data);
        bResult &= rocprofvis_imgui_backend_vk_success(err);
        if(bResult)
        {
            // Enable required extensions
            if(rocprofvis_imgui_backend_vk_has_extension(
                   properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
                instance_extensions.push_back(
                    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            if(rocprofvis_imgui_backend_vk_has_extension(
                   properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            {
                instance_extensions.push_back(
                    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                instance_create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif

            // Enabling validation layers
#ifdef _DEBUG
            const char* layers[]                    = { "VK_LAYER_KHRONOS_validation" };
            instance_create_info.enabledLayerCount   = 1;
            instance_create_info.ppEnabledLayerNames = layers;
            instance_extensions.push_back("VK_EXT_debug_report");
#endif

            // Create Vulkan Instance
            instance_create_info.enabledExtensionCount   = (uint32_t) instance_extensions.Size;
            instance_create_info.ppEnabledExtensionNames = instance_extensions.Data;
            err = vkCreateInstance(&instance_create_info, backend_data->m_allocator,
                                   &backend_data->m_instance);
            bResult &= rocprofvis_imgui_backend_vk_success(err);
            if(bResult)
            {
                // Setup the debug report callback
#ifdef _DEBUG
                auto f_vkCreateDebugReportCallbackEXT =
                    (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
                        backend_data->m_instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
                VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
                debug_report_ci.sType =
                    VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                        VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
                debug_report_ci.pfnCallback = rocprofvis_imgui_backend_vk_debug_report;
                debug_report_ci.pUserData   = nullptr;
                err                         = f_vkCreateDebugReportCallbackEXT(
                    backend_data->m_instance, &debug_report_ci, backend_data->m_allocator,
                    &backend_data->m_debug_report);
                bResult &= rocprofvis_imgui_backend_vk_success(err);
#endif
                if(bResult)
                {
                    // Select Physical Device (GPU)
                    if(rocprofvis_imgui_backend_vk_select_physical_device(backend_data))
                    {
                        // Select graphics m_queue family
                        {
                            uint32_t count;
                            vkGetPhysicalDeviceQueueFamilyProperties(
                                backend_data->m_physical_device, &count, nullptr);
                            VkQueueFamilyProperties* queues =
                                (VkQueueFamilyProperties*) malloc(
                                    sizeof(VkQueueFamilyProperties) * count);
                            vkGetPhysicalDeviceQueueFamilyProperties(
                                backend_data->m_physical_device, &count, queues);
                            for(uint32_t i = 0; i < count; i++)
                            {
                                if(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                                {
                                    backend_data->m_queue_family = i;
                                    break;
                                }
                            }
                            free(queues);
                            IM_ASSERT(backend_data->m_queue_family != (uint32_t) -1);
                            bResult &= (backend_data->m_queue_family != (uint32_t) -1);
                        }

                        if(bResult)
                        {
                            // Create Logical Device (with 1 m_queue)
                            {
                                ImVector<const char*> device_extensions;
                                device_extensions.push_back("VK_KHR_swapchain");

                                // Enumerate physical m_device extension
                                uint32_t                        device_properties_count;
                                ImVector<VkExtensionProperties> device_properties;
                                vkEnumerateDeviceExtensionProperties(
                                    backend_data->m_physical_device, nullptr,
                                    &device_properties_count, nullptr);
                                device_properties.resize(device_properties_count);
                                vkEnumerateDeviceExtensionProperties(
                                    backend_data->m_physical_device, nullptr,
                                    &device_properties_count, device_properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
                                if(rocprofvis_imgui_backend_vk_has_extension(
                                       device_properties,
                                       VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                                    device_extensions.push_back(
                                        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

                                const float             queue_priority[] = { 1.0f };
                                VkDeviceQueueCreateInfo queue_info[1]    = {};
                                queue_info[0].sType =
                                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                                queue_info[0].queueFamilyIndex =
                                    backend_data->m_queue_family;
                                queue_info[0].queueCount       = 1;
                                queue_info[0].pQueuePriorities = queue_priority;
                                VkDeviceCreateInfo device_create_info = {};
                                device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                                device_create_info.queueCreateInfoCount =
                                    sizeof(queue_info) / sizeof(queue_info[0]);
                                device_create_info.pQueueCreateInfos = queue_info;
                                device_create_info.enabledExtensionCount =
                                    (uint32_t) device_extensions.Size;
                                device_create_info.ppEnabledExtensionNames =
                                    device_extensions.Data;
                                err = vkCreateDevice(
                                    backend_data->m_physical_device, &device_create_info,
                                    backend_data->m_allocator, &backend_data->m_device);
                                bResult &= rocprofvis_imgui_backend_vk_success(err);
                            }

                            if(bResult)
                            {
                                vkGetDeviceQueue(backend_data->m_device,
                                                 backend_data->m_queue_family, 0,
                                                 &backend_data->m_queue);

                                // Create Descriptor Pool
                                // ImGui 1.92+'s dynamic texture system creates many more
                                // textures, so we need a larger descriptor pool.
                                {
                                    VkDescriptorPoolSize pool_sizes[] = {
                                        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100 },
                                    };
                                    VkDescriptorPoolCreateInfo pool_info = {};
                                    pool_info.sType =
                                        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                                    pool_info.flags =
                                        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                                    pool_info.maxSets = 100;
                                    pool_info.poolSizeCount =
                                        (uint32_t) IM_ARRAYSIZE(pool_sizes);
                                    pool_info.pPoolSizes = pool_sizes;
                                    err                  = vkCreateDescriptorPool(
                                        backend_data->m_device, &pool_info,
                                        backend_data->m_allocator,
                                        &backend_data->m_descriptor_pool);
                                    bResult &= rocprofvis_imgui_backend_vk_success(err);
                                }
                            }

                            if(!bResult)
                            {
                                vkDestroyDevice(backend_data->m_device,
                                                backend_data->m_allocator);
                            }
                        }
                    }
                }

                if(!bResult)
                {
                    vkDestroyInstance(backend_data->m_instance,
                                      backend_data->m_allocator);
                }
            }
        }
    }

    return bResult;
}

static void
rocprofvis_imgui_backend_vk_release_after_failed_init(rocprofvis_imgui_backend_t* backend)
{
    if(!backend || !backend->m_private_data)
    {
        memset(backend, 0, sizeof(*backend));
        return;
    }

    rocprofvis_imgui_vk_data_t* backend_data =
        (rocprofvis_imgui_vk_data_t*) backend->m_private_data;

    rocprofvis_imgui_backend_vk_destroy_all_textures(backend_data, false);
    delete backend_data->m_textures;
    backend_data->m_textures = nullptr;

    if(backend_data->m_instance != VK_NULL_HANDLE &&
       backend_data->m_device != VK_NULL_HANDLE)
    {
        ImGui_ImplVulkanH_DestroyWindow(backend_data->m_instance, backend_data->m_device,
                                          &backend_data->m_window_data,
                                          backend_data->m_allocator);
    }
    backend_data->m_window_data = ImGui_ImplVulkanH_Window();

    if(backend_data->m_descriptor_pool != VK_NULL_HANDLE &&
       backend_data->m_device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(backend_data->m_device, backend_data->m_descriptor_pool,
                                backend_data->m_allocator);
    }
    backend_data->m_descriptor_pool = VK_NULL_HANDLE;

#ifdef _DEBUG
    if(backend_data->m_debug_report != VK_NULL_HANDLE &&
       backend_data->m_instance != VK_NULL_HANDLE)
    {
        auto f_vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
                backend_data->m_instance, "vkDestroyDebugReportCallbackEXT");
        if(f_vkDestroyDebugReportCallbackEXT)
        {
            f_vkDestroyDebugReportCallbackEXT(backend_data->m_instance,
                                              backend_data->m_debug_report,
                                              backend_data->m_allocator);
        }
    }
    backend_data->m_debug_report = VK_NULL_HANDLE;
#endif

    if(backend_data->m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(backend_data->m_device, backend_data->m_allocator);
    }
    backend_data->m_device = VK_NULL_HANDLE;

    if(backend_data->m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(backend_data->m_instance, backend_data->m_allocator);
    }
    backend_data->m_instance = VK_NULL_HANDLE;

    free(backend_data);
    memset(backend, 0, sizeof(*backend));
}

ImTextureID
rocprofvis_imgui_backend_vk_create_texture_rgba32(rocprofvis_imgui_backend_t* backend,
                                                  const unsigned char* pixels,
                                                  int32_t width,
                                                  int32_t height)
{
    if(!backend || !backend->m_private_data || !pixels || width <= 0 || height <= 0)
    {
        return ImTextureID_Invalid;
    }

    rocprofvis_imgui_vk_data_t* backend_data =
        (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
    if(backend_data->m_device == VK_NULL_HANDLE || !backend_data->m_textures)
    {
        return ImTextureID_Invalid;
    }

    VkResult err = VK_SUCCESS;
    const VkDeviceSize upload_size = static_cast<VkDeviceSize>(width) * height * 4;
    rocprofvis_imgui_vk_texture_t* texture = new rocprofvis_imgui_vk_texture_t();

    VkBuffer       upload_buffer        = VK_NULL_HANDLE;
    VkDeviceMemory upload_buffer_memory = VK_NULL_HANDLE;
    VkCommandPool  command_pool         = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer      = VK_NULL_HANDLE;

    auto cleanup_upload = [&]() {
        if(command_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(backend_data->m_device, command_pool,
                                 backend_data->m_allocator);
        }
        if(upload_buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(backend_data->m_device, upload_buffer,
                            backend_data->m_allocator);
        }
        if(upload_buffer_memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(backend_data->m_device, upload_buffer_memory,
                         backend_data->m_allocator);
        }
    };
    auto fail = [&](VkResult result) {
        rocprofvis_imgui_backend_vk_check_result(result);
        cleanup_upload();
        rocprofvis_imgui_backend_vk_destroy_texture_resource(backend_data, texture, false);
        delete texture;
        return ImTextureID_Invalid;
    };

    VkImageCreateInfo image_info = {};
    image_info.sType            = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType        = VK_IMAGE_TYPE_2D;
    image_info.format           = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent.width     = static_cast<uint32_t>(width);
    image_info.extent.height    = static_cast<uint32_t>(height);
    image_info.extent.depth     = 1;
    image_info.mipLevels        = 1;
    image_info.arrayLayers      = 1;
    image_info.samples          = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling           = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage            = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_info.sharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    image_info.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    err = vkCreateImage(backend_data->m_device, &image_info, backend_data->m_allocator,
                        &texture->m_image);
    if(err != VK_SUCCESS) return fail(err);

    VkMemoryRequirements image_requirements;
    vkGetImageMemoryRequirements(backend_data->m_device, texture->m_image,
                                 &image_requirements);
    VkMemoryAllocateInfo image_alloc_info = {};
    image_alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_alloc_info.allocationSize       = image_requirements.size;
    image_alloc_info.memoryTypeIndex = rocprofvis_imgui_backend_vk_find_memory_type(
        backend_data->m_physical_device, image_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(image_alloc_info.memoryTypeIndex == UINT32_MAX)
        return fail(VK_ERROR_FEATURE_NOT_PRESENT);
    err = vkAllocateMemory(backend_data->m_device, &image_alloc_info,
                           backend_data->m_allocator, &texture->m_memory);
    if(err != VK_SUCCESS) return fail(err);
    err = vkBindImageMemory(backend_data->m_device, texture->m_image,
                            texture->m_memory, 0);
    if(err != VK_SUCCESS) return fail(err);

    VkImageViewCreateInfo view_info = {};
    view_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image                 = texture->m_image;
    view_info.viewType              = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format                = VK_FORMAT_R8G8B8A8_UNORM;
    view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.levelCount     = 1;
    view_info.subresourceRange.layerCount     = 1;
    err = vkCreateImageView(backend_data->m_device, &view_info,
                            backend_data->m_allocator, &texture->m_image_view);
    if(err != VK_SUCCESS) return fail(err);

    VkSamplerCreateInfo sampler_info = {};
    sampler_info.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter    = VK_FILTER_LINEAR;
    sampler_info.minFilter    = VK_FILTER_LINEAR;
    sampler_info.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod       = 0.0f;
    sampler_info.maxLod       = 0.0f;
    err = vkCreateSampler(backend_data->m_device, &sampler_info,
                          backend_data->m_allocator, &texture->m_sampler);
    if(err != VK_SUCCESS) return fail(err);

    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size               = upload_size;
    buffer_info.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(backend_data->m_device, &buffer_info,
                         backend_data->m_allocator, &upload_buffer);
    if(err != VK_SUCCESS) return fail(err);

    VkMemoryRequirements buffer_requirements;
    vkGetBufferMemoryRequirements(backend_data->m_device, upload_buffer,
                                  &buffer_requirements);
    VkMemoryAllocateInfo buffer_alloc_info = {};
    buffer_alloc_info.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_alloc_info.allocationSize       = buffer_requirements.size;
    buffer_alloc_info.memoryTypeIndex = rocprofvis_imgui_backend_vk_find_memory_type(
        backend_data->m_physical_device, buffer_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(buffer_alloc_info.memoryTypeIndex == UINT32_MAX)
        return fail(VK_ERROR_FEATURE_NOT_PRESENT);
    err = vkAllocateMemory(backend_data->m_device, &buffer_alloc_info,
                           backend_data->m_allocator, &upload_buffer_memory);
    if(err != VK_SUCCESS) return fail(err);
    err = vkBindBufferMemory(backend_data->m_device, upload_buffer,
                             upload_buffer_memory, 0);
    if(err != VK_SUCCESS) return fail(err);

    void* mapped_pixels = nullptr;
    err = vkMapMemory(backend_data->m_device, upload_buffer_memory, 0, upload_size, 0,
                      &mapped_pixels);
    if(err != VK_SUCCESS) return fail(err);
    memcpy(mapped_pixels, pixels, static_cast<size_t>(upload_size));
    vkUnmapMemory(backend_data->m_device, upload_buffer_memory);

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool_info.queueFamilyIndex = backend_data->m_queue_family;
    err = vkCreateCommandPool(backend_data->m_device, &pool_info,
                              backend_data->m_allocator, &command_pool);
    if(err != VK_SUCCESS) return fail(err);

    VkCommandBufferAllocateInfo command_buffer_info = {};
    command_buffer_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.commandPool        = command_pool;
    command_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_info.commandBufferCount = 1;
    err = vkAllocateCommandBuffers(backend_data->m_device, &command_buffer_info,
                                   &command_buffer);
    if(err != VK_SUCCESS) return fail(err);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(command_buffer, &begin_info);
    if(err != VK_SUCCESS) return fail(err);

    VkImageMemoryBarrier transfer_barrier = {};
    transfer_barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    transfer_barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    transfer_barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    transfer_barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    transfer_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transfer_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    transfer_barrier.image               = texture->m_image;
    transfer_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    transfer_barrier.subresourceRange.levelCount = 1;
    transfer_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &transfer_barrier);

    VkBufferImageCopy copy_region = {};
    copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageExtent.width           = static_cast<uint32_t>(width);
    copy_region.imageExtent.height          = static_cast<uint32_t>(height);
    copy_region.imageExtent.depth           = 1;
    vkCmdCopyBufferToImage(command_buffer, upload_buffer, texture->m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    VkImageMemoryBarrier shader_barrier = {};
    shader_barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    shader_barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
    shader_barrier.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
    shader_barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    shader_barrier.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    shader_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shader_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    shader_barrier.image               = texture->m_image;
    shader_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    shader_barrier.subresourceRange.levelCount = 1;
    shader_barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &shader_barrier);

    err = vkEndCommandBuffer(command_buffer);
    if(err != VK_SUCCESS) return fail(err);

    VkSubmitInfo submit_info       = {};
    submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers    = &command_buffer;
    err = vkQueueSubmit(backend_data->m_queue, 1, &submit_info, VK_NULL_HANDLE);
    if(err != VK_SUCCESS) return fail(err);
    err = vkQueueWaitIdle(backend_data->m_queue);
    if(err != VK_SUCCESS) return fail(err);

    cleanup_upload();
    command_pool = VK_NULL_HANDLE;
    upload_buffer = VK_NULL_HANDLE;
    upload_buffer_memory = VK_NULL_HANDLE;

    texture->m_descriptor_set =
        ImGui_ImplVulkan_AddTexture(texture->m_sampler, texture->m_image_view,
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    backend_data->m_textures->push_back(texture);
    return (ImTextureID) texture->m_descriptor_set;
}

void
rocprofvis_imgui_backend_vk_destroy_texture(rocprofvis_imgui_backend_t* backend,
                                            ImTextureID texture_id)
{
    if(!backend || !backend->m_private_data || texture_id == ImTextureID_Invalid)
    {
        return;
    }

    rocprofvis_imgui_vk_data_t* backend_data =
        (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
    if(!backend_data->m_textures)
    {
        return;
    }

    VkDescriptorSet descriptor_set = (VkDescriptorSet) texture_id;
    auto& textures                 = *backend_data->m_textures;
    auto it = std::find_if(textures.begin(), textures.end(),
                           [descriptor_set](rocprofvis_imgui_vk_texture_t* texture) {
                               return texture &&
                                      texture->m_descriptor_set == descriptor_set;
                           });
    if(it == textures.end())
    {
        return;
    }

    vkDeviceWaitIdle(backend_data->m_device);
    rocprofvis_imgui_backend_vk_destroy_texture_resource(backend_data, *it, true);
    delete *it;
    textures.erase(it);
}

bool
rocprofvis_imgui_backend_vk_init(rocprofvis_imgui_backend_t* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->m_private_data && window)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;

        ImVector<const char*> extensions;
        uint32_t              extensions_count = 0;
        const char**          glfw_extensions =
            glfwGetRequiredInstanceExtensions(&extensions_count);
        for(uint32_t i = 0; i < extensions_count; i++)
        {
            extensions.push_back(glfw_extensions[i]);
        }
        if(rocprofvis_imgui_backend_vk_setup_vulkan(backend_data, extensions))
        {
            VkSurfaceKHR surface = VK_NULL_HANDLE;
            VkResult     err     = glfwCreateWindowSurface(
                    backend_data->m_instance, (GLFWwindow*) window,
                    backend_data->m_allocator, &surface);
            if(err != VK_SUCCESS)
            {
                spdlog::error("[vulkan] glfwCreateWindowSurface failed, VkResult = {}", static_cast<int>(err));
            }
            else
            {
                int width  = 0;
                int height = 0;
                glfwGetFramebufferSize((GLFWwindow*) window, &width, &height);
                ImGui_ImplVulkanH_Window* wd = &backend_data->m_window_data;
                wd->Surface                  = surface;

                VkBool32 res;
                vkGetPhysicalDeviceSurfaceSupportKHR(backend_data->m_physical_device,
                                                     backend_data->m_queue_family,
                                                     wd->Surface, &res);
                if(res == VK_TRUE)
                {
                    const VkFormat requestSurfaceImageFormat[] = {
                        VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM,
                        VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
                    const VkColorSpaceKHR requestSurfaceColorSpace =
                        VK_COLORSPACE_SRGB_NONLINEAR_KHR;
                    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
                        backend_data->m_physical_device, wd->Surface,
                        requestSurfaceImageFormat,
                        (size_t) IM_ARRAYSIZE(requestSurfaceImageFormat),
                        requestSurfaceColorSpace);

                    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
                    wd->PresentMode                  = ImGui_ImplVulkanH_SelectPresentMode(
                        backend_data->m_physical_device, wd->Surface, &present_modes[0],
                        IM_ARRAYSIZE(present_modes));

                    IM_ASSERT(backend_data->m_min_image_count >= 2);
                    ImGui_ImplVulkanH_CreateOrResizeWindow(
                        backend_data->m_instance, backend_data->m_physical_device,
                        backend_data->m_device, wd, backend_data->m_queue_family,
                        backend_data->m_allocator, width, height,
                        backend_data->m_min_image_count, 0);

                    bOk = true;
                }
                else
                {
                    spdlog::error("[vulkan] Error: no WSI support on physical device");
                }
            }
        }
    }

    if(!bOk)
    {
        rocprofvis_imgui_backend_vk_release_after_failed_init(backend);
    }

    return bOk;
}

bool
rocprofvis_imgui_backend_vk_config(rocprofvis_imgui_backend_t* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->m_private_data && window)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*) window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = backend_data->m_instance;
        init_info.PhysicalDevice            = backend_data->m_physical_device;
        init_info.Device                    = backend_data->m_device;
        init_info.QueueFamily               = backend_data->m_queue_family;
        init_info.Queue                     = backend_data->m_queue;
        init_info.PipelineCache             = backend_data->m_pipeline_cache;
        init_info.DescriptorPool            = backend_data->m_descriptor_pool;
        init_info.PipelineInfoMain.RenderPass  = backend_data->m_window_data.RenderPass;
        init_info.PipelineInfoMain.Subpass     = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.MinImageCount             = backend_data->m_min_image_count;
        init_info.ImageCount                = backend_data->m_window_data.ImageCount;
        init_info.Allocator                 = backend_data->m_allocator;
        init_info.CheckVkResultFn           = rocprofvis_imgui_backend_vk_check_result;
        ImGui_ImplVulkan_Init(&init_info);

        bOk = true;
    }

    return bOk;
}

void
rocprofvis_imgui_backend_vk_update_framebuffer(rocprofvis_imgui_backend_t* backend,
                                               int32_t fb_width, int32_t fb_height)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
        if(fb_width > 0 && fb_height > 0 &&
           (backend_data->m_swapchain_rebuild ||
            backend_data->m_window_data.Width != fb_width ||
            backend_data->m_window_data.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(backend_data->m_min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                backend_data->m_instance, backend_data->m_physical_device,
                backend_data->m_device, &backend_data->m_window_data,
                backend_data->m_queue_family, backend_data->m_allocator, fb_width,
                fb_height, backend_data->m_min_image_count, 0);
            backend_data->m_window_data.FrameIndex = 0;
            backend_data->m_swapchain_rebuild      = false;
        }
    }
}

void
rocprofvis_imgui_backend_vk_new_frame(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }
}

void
rocprofvis_imgui_backend_vk_render(rocprofvis_imgui_backend_t* backend,
                                   ImDrawData* draw_data, ImVec4* clear_color)
{
    if(backend && backend->m_private_data && draw_data && clear_color)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
        VkResult err;

        auto* wd = &backend_data->m_window_data;

        wd->ClearValue.color.float32[0] = clear_color->x * clear_color->w;
        wd->ClearValue.color.float32[1] = clear_color->y * clear_color->w;
        wd->ClearValue.color.float32[2] = clear_color->z * clear_color->w;
        wd->ClearValue.color.float32[3] = clear_color->w;

        VkSemaphore image_acquired_semaphore =
            wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore =
            wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        err = vkAcquireNextImageKHR(backend_data->m_device, wd->Swapchain, UINT64_MAX,
                                    image_acquired_semaphore, VK_NULL_HANDLE,
                                    &wd->FrameIndex);
        if(err != VK_ERROR_OUT_OF_DATE_KHR && err != VK_SUBOPTIMAL_KHR)
        {
            rocprofvis_imgui_backend_vk_check_result(err);

            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
            {
                // Deliberate infinite wait
                err = vkWaitForFences(backend_data->m_device, 1, &fd->Fence, VK_TRUE,
                                      UINT64_MAX);
                rocprofvis_imgui_backend_vk_check_result(err);

                err = vkResetFences(backend_data->m_device, 1, &fd->Fence);
                rocprofvis_imgui_backend_vk_check_result(err);
            }
            {
                err = vkResetCommandPool(backend_data->m_device, fd->CommandPool, 0);
                rocprofvis_imgui_backend_vk_check_result(err);
                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
                rocprofvis_imgui_backend_vk_check_result(err);
            }
            {
                VkRenderPassBeginInfo info    = {};
                info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass               = wd->RenderPass;
                info.framebuffer              = fd->Framebuffer;
                info.renderArea.extent.width  = wd->Width;
                info.renderArea.extent.height = wd->Height;
                info.clearValueCount          = 1;
                info.pClearValues             = &wd->ClearValue;
                vkCmdBeginRenderPass(fd->CommandBuffer, &info,
                                     VK_SUBPASS_CONTENTS_INLINE);
            }

            // Record dear imgui primitives into command buffer
            ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

            // Submit command buffer
            vkCmdEndRenderPass(fd->CommandBuffer);
            {
                VkPipelineStageFlags wait_stage =
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo info         = {};
                info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.waitSemaphoreCount   = 1;
                info.pWaitSemaphores      = &image_acquired_semaphore;
                info.pWaitDstStageMask    = &wait_stage;
                info.commandBufferCount   = 1;
                info.pCommandBuffers      = &fd->CommandBuffer;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores    = &render_complete_semaphore;

                err = vkEndCommandBuffer(fd->CommandBuffer);
                rocprofvis_imgui_backend_vk_check_result(err);
                err = vkQueueSubmit(backend_data->m_queue, 1, &info, fd->Fence);
                rocprofvis_imgui_backend_vk_check_result(err);
            }
        }
        else
        {
            backend_data->m_swapchain_rebuild = true;
        }
    }
}

void
rocprofvis_imgui_backend_vk_present(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
        auto* wd = &backend_data->m_window_data;
        if(!backend_data->m_swapchain_rebuild)
        {
            VkSemaphore render_complete_semaphore =
                wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
            VkPresentInfoKHR info   = {};
            info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores    = &render_complete_semaphore;
            info.swapchainCount     = 1;
            info.pSwapchains        = &wd->Swapchain;
            info.pImageIndices      = &wd->FrameIndex;
            VkResult err            = vkQueuePresentKHR(backend_data->m_queue, &info);
            if(err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            {
                backend_data->m_swapchain_rebuild = true;
            }
            else
            {
                // This will be an unrecoverable error
                rocprofvis_imgui_backend_vk_check_result(err);

                // Now we can use the next set of semaphores
                wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
            }
        }
    }
}

void
rocprofvis_imgui_backend_vk_shutdown(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
        VkResult err = vkDeviceWaitIdle(backend_data->m_device);
        rocprofvis_imgui_backend_vk_check_result(err);
        rocprofvis_imgui_backend_vk_destroy_all_textures(backend_data, true);
        ImGui_ImplVulkan_Shutdown();
    }
}

void
rocprofvis_imgui_backend_vk_destroy(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) backend->m_private_data;
        ImGui_ImplVulkanH_DestroyWindow(backend_data->m_instance, backend_data->m_device,
                                        &backend_data->m_window_data,
                                        backend_data->m_allocator);

        vkDestroyDescriptorPool(backend_data->m_device, backend_data->m_descriptor_pool,
                                backend_data->m_allocator);

        delete backend_data->m_textures;
        backend_data->m_textures = nullptr;

#ifdef _DEBUG
        // Remove the debug report callback
        auto f_vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
                backend_data->m_instance, "vkDestroyDebugReportCallbackEXT");
        f_vkDestroyDebugReportCallbackEXT(backend_data->m_instance,
                                          backend_data->m_debug_report,
                                          backend_data->m_allocator);
#endif  // _DEBUG

        vkDestroyDevice(backend_data->m_device, backend_data->m_allocator);
        vkDestroyInstance(backend_data->m_instance, backend_data->m_allocator);

        free(backend_data);
        memset(backend, 0, sizeof(rocprofvis_imgui_backend_t));
    }
}

bool
rocprofvis_imgui_backend_setup_vulkan(rocprofvis_imgui_backend_t* backend,
                                      GLFWwindow* window)
{
    (void) window;
    bool bOk = false;

    if(backend)
    {
        rocprofvis_imgui_vk_data_t* backend_data =
            (rocprofvis_imgui_vk_data_t*) calloc(1,
                                                 sizeof(rocprofvis_imgui_vk_data_t));
        if(backend_data)
        {
            backend_data->m_window_data       = ImGui_ImplVulkanH_Window();
            backend_data->m_allocator         = nullptr;
            backend_data->m_instance          = VK_NULL_HANDLE;
            backend_data->m_physical_device   = VK_NULL_HANDLE;
            backend_data->m_device            = VK_NULL_HANDLE;
            backend_data->m_queue_family      = (uint32_t) -1;
            backend_data->m_queue             = VK_NULL_HANDLE;
            backend_data->m_pipeline_cache    = VK_NULL_HANDLE;
            backend_data->m_descriptor_pool   = VK_NULL_HANDLE;
            backend_data->m_debug_report      = VK_NULL_HANDLE;
            backend_data->m_min_image_count   = 2;
            backend_data->m_swapchain_rebuild = false;
            backend_data->m_textures =
                new std::vector<rocprofvis_imgui_vk_texture_t*>();
            backend->m_private_data           = backend_data;
            backend->m_init                   = &rocprofvis_imgui_backend_vk_init;
            backend->m_config                 = &rocprofvis_imgui_backend_vk_config;
            backend->m_update_framebuffer =
                &rocprofvis_imgui_backend_vk_update_framebuffer;
            backend->m_new_frame = &rocprofvis_imgui_backend_vk_new_frame;
            backend->m_render    = &rocprofvis_imgui_backend_vk_render;
            backend->m_present   = &rocprofvis_imgui_backend_vk_present;
            backend->m_shutdown  = &rocprofvis_imgui_backend_vk_shutdown;
            backend->m_destroy   = &rocprofvis_imgui_backend_vk_destroy;
            backend->m_create_texture_rgba32 =
                &rocprofvis_imgui_backend_vk_create_texture_rgba32;
            backend->m_destroy_texture = &rocprofvis_imgui_backend_vk_destroy_texture;
            bOk                  = true;

            spdlog::info("[rpv] Using Vulkan backend");
        }
        else
        {
            spdlog::error("[rpv] Error: Couldn't allocate Vulkan ImGui backend");
        }
    }

    return bOk;
}
