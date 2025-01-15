// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rpv_imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct rpv_imgui_vk_data
{
    ImGui_ImplVulkanH_Window window_data;
    VkAllocationCallbacks*   allocator         = nullptr;
    VkInstance               instance          = VK_NULL_HANDLE;
    VkPhysicalDevice         physical_device   = VK_NULL_HANDLE;
    VkDevice                 device            = VK_NULL_HANDLE;
    uint32_t                 queue_family      = (uint32_t) -1;
    VkQueue                  queue             = VK_NULL_HANDLE;
    VkPipelineCache          pipeline_cache    = VK_NULL_HANDLE;
    VkDescriptorPool         descriptor_pool   = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debug_report      = VK_NULL_HANDLE;
    int                      min_image_count   = 2;
    bool                     swapchain_rebuild = false;
} rpv_imgui_vk_data;

static void
rpv_imgui_backend_vk_check_result(VkResult err)
{
    if(err != 0)
    {
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if(err < 0) abort();
    }
}

static bool
rpv_imgui_backend_vk_success(VkResult err)
{
    if(err != 0)
    {
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    }
    return (err == 0);
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL
rpv_imgui_backend_vk_debug_report(VkDebugReportFlagsEXT      flags,
                                  VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                  size_t location, int32_t messageCode,
                                  const char* pLayerPrefix, const char* pMessage,
                                  void* pUserData)
{
    (void) flags;
    (void) object;
    (void) location;
    (void) messageCode;
    (void) pUserData;
    (void) pLayerPrefix;  // Unused arguments
    fprintf(stderr, "[vulkan] Debug: report from ObjectType: %i\nMessage: %s\n\n",
            objectType, pMessage);
    return VK_FALSE;
}
#endif  // _DEBUG

static bool
rpv_imgui_backend_vk_has_extension(const ImVector<VkExtensionProperties>& properties,
                                   const char*                            extension)
{
    for(const VkExtensionProperties& p : properties)
        if(strcmp(p.extensionName, extension) == 0) return true;
    return false;
}

static bool
rpv_imgui_backend_vk_select_physical_device(rpv_imgui_vk_data* backend_data)
{
    bool     bOk = false;
    uint32_t gpu_count;
    VkResult err =
        vkEnumeratePhysicalDevices(backend_data->instance, &gpu_count, nullptr);
    if(rpv_imgui_backend_vk_success(err))
    {
        IM_ASSERT(gpu_count > 0);

        ImVector<VkPhysicalDevice> gpus;
        gpus.resize(gpu_count);
        err = vkEnumeratePhysicalDevices(backend_data->instance, &gpu_count, gpus.Data);
        if(rpv_imgui_backend_vk_success(err))
        {
            // Default to the first GPU
            if(gpu_count > 0)
            {
                backend_data->physical_device = gpus[0];
                bOk                           = true;
            }

            // Then look for the first discrete GPU, this is the simplest way to handle
            // the common cases of integrated+discrete GPUs. Really we want to identify
            // which GPU is connected to the screen.
            for(VkPhysicalDevice& device : gpus)
            {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(device, &properties);
                if(properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                {
                    backend_data->physical_device = device;
                    bOk                           = true;
                    break;
                }
            }
        }
    }
    else
    {
        fprintf(stderr, "[vulkan] Error: Couldn't enumerate GPUs.\n");
    }
    return bOk;
}

static bool
rpv_imgui_backend_vk_setup_vulkan(rpv_imgui_vk_data*    backend_data,
                                  ImVector<const char*> instance_extensions)
{
    bool     bResult = true;
    VkResult err;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType                = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t                        properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count,
                                                     properties.Data);
        bResult &= rpv_imgui_backend_vk_success(err);
        if(bResult)
        {
            // Enable required extensions
            if(rpv_imgui_backend_vk_has_extension(
                   properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
                instance_extensions.push_back(
                    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            if(rpv_imgui_backend_vk_has_extension(
                   properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            {
                instance_extensions.push_back(
                    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif

            // Enabling validation layers
#ifdef _DEBUG
            const char* layers[]            = { "VK_LAYER_KHRONOS_validation" };
            create_info.enabledLayerCount   = 1;
            create_info.ppEnabledLayerNames = layers;
            instance_extensions.push_back("VK_EXT_debug_report");
#endif

            // Create Vulkan Instance
            create_info.enabledExtensionCount   = (uint32_t) instance_extensions.Size;
            create_info.ppEnabledExtensionNames = instance_extensions.Data;
            err = vkCreateInstance(&create_info, backend_data->allocator,
                                   &backend_data->instance);
            bResult &= rpv_imgui_backend_vk_success(err);
            if(bResult)
            {
                // Setup the debug report callback
#ifdef _DEBUG
                auto f_vkCreateDebugReportCallbackEXT =
                    (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(
                        backend_data->instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
                VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
                debug_report_ci.sType =
                    VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                        VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
                debug_report_ci.pfnCallback = rpv_imgui_backend_vk_debug_report;
                debug_report_ci.pUserData   = nullptr;
                err                         = f_vkCreateDebugReportCallbackEXT(
                    backend_data->instance, &debug_report_ci, backend_data->allocator,
                    &backend_data->debug_report);
                bResult &= rpv_imgui_backend_vk_success(err);
#endif
                if(bResult)
                {
                    // Select Physical Device (GPU)
                    if(rpv_imgui_backend_vk_select_physical_device(backend_data))
                    {
                        // Select graphics queue family
                        {
                            uint32_t count;
                            vkGetPhysicalDeviceQueueFamilyProperties(
                                backend_data->physical_device, &count, nullptr);
                            VkQueueFamilyProperties* queues =
                                (VkQueueFamilyProperties*) malloc(
                                    sizeof(VkQueueFamilyProperties) * count);
                            vkGetPhysicalDeviceQueueFamilyProperties(
                                backend_data->physical_device, &count, queues);
                            for(uint32_t i = 0; i < count; i++)
                            {
                                if(queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                                {
                                    backend_data->queue_family = i;
                                    break;
                                }
                            }
                            free(queues);
                            IM_ASSERT(backend_data->queue_family != (uint32_t) -1);
                            bResult &= (backend_data->queue_family != (uint32_t) -1);
                        }

                        if(bResult)
                        {
                            // Create Logical Device (with 1 queue)
                            {
                                ImVector<const char*> device_extensions;
                                device_extensions.push_back("VK_KHR_swapchain");

                                // Enumerate physical device extension
                                uint32_t                        properties_count;
                                ImVector<VkExtensionProperties> properties;
                                vkEnumerateDeviceExtensionProperties(
                                    backend_data->physical_device, nullptr,
                                    &properties_count, nullptr);
                                properties.resize(properties_count);
                                vkEnumerateDeviceExtensionProperties(
                                    backend_data->physical_device, nullptr,
                                    &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
                                if(rpv_imgui_backend_vk_has_extension(
                                       properties,
                                       VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                                    device_extensions.push_back(
                                        VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

                                const float             queue_priority[] = { 1.0f };
                                VkDeviceQueueCreateInfo queue_info[1]    = {};
                                queue_info[0].sType =
                                    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                                queue_info[0].queueFamilyIndex =
                                    backend_data->queue_family;
                                queue_info[0].queueCount       = 1;
                                queue_info[0].pQueuePriorities = queue_priority;
                                VkDeviceCreateInfo create_info = {};
                                create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                                create_info.queueCreateInfoCount =
                                    sizeof(queue_info) / sizeof(queue_info[0]);
                                create_info.pQueueCreateInfos = queue_info;
                                create_info.enabledExtensionCount =
                                    (uint32_t) device_extensions.Size;
                                create_info.ppEnabledExtensionNames =
                                    device_extensions.Data;
                                err = vkCreateDevice(
                                    backend_data->physical_device, &create_info,
                                    backend_data->allocator, &backend_data->device);
                                bResult &= rpv_imgui_backend_vk_success(err);
                            }

                            if(bResult)
                            {
                                vkGetDeviceQueue(backend_data->device,
                                                 backend_data->queue_family, 0,
                                                 &backend_data->queue);

                                // Create Descriptor Pool
                                // The example only requires a single combined image
                                // sampler descriptor for the font image and only uses one
                                // descriptor set (for that) If you wish to load e.g.
                                // additional textures you may need to alter pools sizes.
                                {
                                    VkDescriptorPoolSize pool_sizes[] = {
                                        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
                                    };
                                    VkDescriptorPoolCreateInfo pool_info = {};
                                    pool_info.sType =
                                        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                                    pool_info.flags =
                                        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
                                    pool_info.maxSets = 1;
                                    pool_info.poolSizeCount =
                                        (uint32_t) IM_ARRAYSIZE(pool_sizes);
                                    pool_info.pPoolSizes = pool_sizes;
                                    err                  = vkCreateDescriptorPool(
                                        backend_data->device, &pool_info,
                                        backend_data->allocator,
                                        &backend_data->descriptor_pool);
                                    bResult &= rpv_imgui_backend_vk_success(err);
                                }
                            }

                            if(!bResult)
                            {
                                vkDestroyDevice(backend_data->device,
                                                backend_data->allocator);
                            }
                        }
                    }
                }

                if(!bResult)
                {
                    vkDestroyInstance(backend_data->instance, backend_data->allocator);
                }
            }
        }
    }

    return bResult;
}

bool
rpv_imgui_backend_vk_init(rpvImGuiBackend* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->private_data && window)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;

        ImVector<const char*> extensions;
        uint32_t              extensions_count = 0;
        const char**          glfw_extensions =
            glfwGetRequiredInstanceExtensions(&extensions_count);
        for(uint32_t i = 0; i < extensions_count; i++)
        {
            extensions.push_back(glfw_extensions[i]);
        }
        if(rpv_imgui_backend_vk_setup_vulkan(backend_data, extensions))
        {
            // Create Window Surface
            VkSurfaceKHR surface;
            VkResult     err =
                glfwCreateWindowSurface(backend_data->instance, (GLFWwindow*) window,
                                        backend_data->allocator, &surface);
            rpv_imgui_backend_vk_check_result(err);

            // Create Framebuffers
            int width  = 0;
            int height = 0;
            glfwGetFramebufferSize((GLFWwindow*) window, &width, &height);
            ImGui_ImplVulkanH_Window* wd = &backend_data->window_data;
            wd->Surface                  = surface;

            // Check for WSI support
            VkBool32 res;
            vkGetPhysicalDeviceSurfaceSupportKHR(backend_data->physical_device,
                                                 backend_data->queue_family, wd->Surface,
                                                 &res);
            if(res == VK_TRUE)
            {
                // Select Surface Format
                const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM,
                                                               VK_FORMAT_R8G8B8A8_UNORM,
                                                               VK_FORMAT_B8G8R8_UNORM,
                                                               VK_FORMAT_R8G8B8_UNORM };
                const VkColorSpaceKHR requestSurfaceColorSpace =
                    VK_COLORSPACE_SRGB_NONLINEAR_KHR;
                wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(
                    backend_data->physical_device, wd->Surface, requestSurfaceImageFormat,
                    (size_t) IM_ARRAYSIZE(requestSurfaceImageFormat),
                    requestSurfaceColorSpace);

                // Select Present Mode
                VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
                wd->PresentMode                  = ImGui_ImplVulkanH_SelectPresentMode(
                    backend_data->physical_device, wd->Surface, &present_modes[0],
                    IM_ARRAYSIZE(present_modes));

                // Create SwapChain, RenderPass, Framebuffer, etc.
                IM_ASSERT(backend_data->min_image_count >= 2);
                ImGui_ImplVulkanH_CreateOrResizeWindow(
                    backend_data->instance, backend_data->physical_device,
                    backend_data->device, wd, backend_data->queue_family,
                    backend_data->allocator, width, height,
                    backend_data->min_image_count);

                bOk = true;
            }
            else
            {
                fprintf(stderr, "[vulkan] Error: no WSI support on physical device\n");
            }
        }
    }

    return bOk;
}

bool
rpv_imgui_backend_vk_config(rpvImGuiBackend* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->private_data && window)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan((GLFWwindow*) window, true);
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance                  = backend_data->instance;
        init_info.PhysicalDevice            = backend_data->physical_device;
        init_info.Device                    = backend_data->device;
        init_info.QueueFamily               = backend_data->queue_family;
        init_info.Queue                     = backend_data->queue;
        init_info.PipelineCache             = backend_data->pipeline_cache;
        init_info.DescriptorPool            = backend_data->descriptor_pool;
        init_info.RenderPass                = backend_data->window_data.RenderPass;
        init_info.Subpass                   = 0;
        init_info.MinImageCount             = backend_data->min_image_count;
        init_info.ImageCount                = backend_data->window_data.ImageCount;
        init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator                 = backend_data->allocator;
        init_info.CheckVkResultFn           = rpv_imgui_backend_vk_check_result;
        ImGui_ImplVulkan_Init(&init_info);

        bOk = true;
    }

    return bOk;
}

void
rpv_imgui_backend_vk_update_framebuffer(rpvImGuiBackend* backend, int32_t fb_width,
                                        int32_t fb_height)
{
    if(backend && backend->private_data)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;
        if(fb_width > 0 && fb_height > 0 &&
           (backend_data->swapchain_rebuild ||
            backend_data->window_data.Width != fb_width ||
            backend_data->window_data.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(backend_data->min_image_count);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                backend_data->instance, backend_data->physical_device,
                backend_data->device, &backend_data->window_data,
                backend_data->queue_family, backend_data->allocator, fb_width, fb_height,
                backend_data->min_image_count);
            backend_data->window_data.FrameIndex = 0;
            backend_data->swapchain_rebuild      = false;
        }
    }
}

void
rpv_imgui_backend_vk_new_frame(rpvImGuiBackend* backend)
{
    if(backend && backend->private_data)
    {
        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }
}

void
rpv_imgui_backend_vk_render(rpvImGuiBackend* backend, ImDrawData* draw_data,
                            ImVec4* clear_color)
{
    if(backend && backend->private_data && draw_data && clear_color)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;
        VkResult           err;

        auto* wd = &backend_data->window_data;

        wd->ClearValue.color.float32[0] = clear_color->x * clear_color->w;
        wd->ClearValue.color.float32[1] = clear_color->y * clear_color->w;
        wd->ClearValue.color.float32[2] = clear_color->z * clear_color->w;
        wd->ClearValue.color.float32[3] = clear_color->w;

        VkSemaphore image_acquired_semaphore =
            wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
        VkSemaphore render_complete_semaphore =
            wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
        err = vkAcquireNextImageKHR(backend_data->device, wd->Swapchain, UINT64_MAX,
                                    image_acquired_semaphore, VK_NULL_HANDLE,
                                    &wd->FrameIndex);
        if(err != VK_ERROR_OUT_OF_DATE_KHR && err != VK_SUBOPTIMAL_KHR)
        {
            rpv_imgui_backend_vk_check_result(err);

            ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
            {
                // Deliberate infinite wait
                err = vkWaitForFences(backend_data->device, 1, &fd->Fence, VK_TRUE,
                                      UINT64_MAX);
                rpv_imgui_backend_vk_check_result(err);

                err = vkResetFences(backend_data->device, 1, &fd->Fence);
                rpv_imgui_backend_vk_check_result(err);
            }
            {
                err = vkResetCommandPool(backend_data->device, fd->CommandPool, 0);
                rpv_imgui_backend_vk_check_result(err);
                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
                rpv_imgui_backend_vk_check_result(err);
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
                rpv_imgui_backend_vk_check_result(err);
                err = vkQueueSubmit(backend_data->queue, 1, &info, fd->Fence);
                rpv_imgui_backend_vk_check_result(err);
            }
        }
        else
        {
            backend_data->swapchain_rebuild = true;
        }
    }
}

void
rpv_imgui_backend_vk_present(rpvImGuiBackend* backend)
{
    if(backend && backend->private_data)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;
        auto*              wd           = &backend_data->window_data;
        if(!backend_data->swapchain_rebuild)
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
            VkResult err            = vkQueuePresentKHR(backend_data->queue, &info);
            if(err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
            {
                backend_data->swapchain_rebuild = true;
            }
            else
            {
                // This will be an unrecoverable error
                rpv_imgui_backend_vk_check_result(err);

                // Now we can use the next set of semaphores
                wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;
            }
        }
    }
}

void
rpv_imgui_backend_vk_shutdown(rpvImGuiBackend* backend)
{
    if(backend && backend->private_data)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;
        VkResult           err          = vkDeviceWaitIdle(backend_data->device);
        rpv_imgui_backend_vk_check_result(err);
        ImGui_ImplVulkan_Shutdown();
    }
}

void
rpv_imgui_backend_vk_destroy(rpvImGuiBackend* backend)
{
    if(backend && backend->private_data)
    {
        rpv_imgui_vk_data* backend_data = (rpv_imgui_vk_data*) backend->private_data;
        ImGui_ImplVulkanH_DestroyWindow(backend_data->instance, backend_data->device,
                                        &backend_data->window_data,
                                        backend_data->allocator);

        vkDestroyDescriptorPool(backend_data->device, backend_data->descriptor_pool,
                                backend_data->allocator);

#ifdef _DEBUG
        // Remove the debug report callback
        auto f_vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(
                backend_data->instance, "vkDestroyDebugReportCallbackEXT");
        f_vkDestroyDebugReportCallbackEXT(
            backend_data->instance, backend_data->debug_report, backend_data->allocator);
#endif  // _DEBUG

        vkDestroyDevice(backend_data->device, backend_data->allocator);
        vkDestroyInstance(backend_data->instance, backend_data->allocator);

        free(backend_data);
        memset(backend, 0, sizeof(rpvImGuiBackend));
    }
}

bool
rpv_imgui_backend_setup(rpvImGuiBackend* backend, GLFWwindow* window)
{
    bool bOk = false;
    if(backend)
    {
        if(glfwVulkanSupported())
        {
            rpv_imgui_vk_data* backend_data =
                (rpv_imgui_vk_data*) calloc(1, sizeof(rpv_imgui_vk_data));
            if(backend_data)
            {
                backend_data->window_data       = ImGui_ImplVulkanH_Window();
                backend_data->allocator         = nullptr;
                backend_data->instance          = VK_NULL_HANDLE;
                backend_data->physical_device   = VK_NULL_HANDLE;
                backend_data->device            = VK_NULL_HANDLE;
                backend_data->queue_family      = (uint32_t) -1;
                backend_data->queue             = VK_NULL_HANDLE;
                backend_data->pipeline_cache    = VK_NULL_HANDLE;
                backend_data->descriptor_pool   = VK_NULL_HANDLE;
                backend_data->debug_report      = VK_NULL_HANDLE;
                backend_data->min_image_count   = 2;
                backend_data->swapchain_rebuild = false;
                backend->private_data           = backend_data;
                backend->init                   = &rpv_imgui_backend_vk_init;
                backend->config                 = &rpv_imgui_backend_vk_config;
                backend->update_framebuffer     = &rpv_imgui_backend_vk_update_framebuffer;
                backend->new_frame              = &rpv_imgui_backend_vk_new_frame;
                backend->render                 = &rpv_imgui_backend_vk_render;
                backend->present                = &rpv_imgui_backend_vk_present;
                backend->shutdown               = &rpv_imgui_backend_vk_shutdown;
                backend->destroy                = &rpv_imgui_backend_vk_destroy;
                bOk                             = true;
            }
            else
            {
                fprintf(stderr, "[rpv] Error: Couldn't allocate ImGui backend\n");
            }
        }
        else
        {
            fprintf(stderr, "[GLFW] Error: Vulkan Not Supported\n");
        }
    }
    return bOk;
}
