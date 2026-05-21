// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_imgui_backend.h"
#include "rocprofvis_view_module.h"
#include "spdlog/spdlog.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

// Forward declarations for backend-specific setup functions
bool
rocprofvis_imgui_backend_setup_vulkan(rocprofvis_imgui_backend_t* backend,
                                      GLFWwindow*                 window);
bool
rocprofvis_imgui_backend_setup_opengl(rocprofvis_imgui_backend_t* backend,
                                      GLFWwindow*                 window);
bool
rocprofvis_imgui_backend_vk_init(rocprofvis_imgui_backend_t* backend, void* window);

static bool
setup_opengl_window_and_backend(rocprofvis_imgui_backend_t* backend, GLFWwindow** window,
                                int width, int height, const char* title)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#if defined(GLFW_SCALE_TO_MONITOR)
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
#if defined(__APPLE__)
    // GL 3.2 + GLSL 150
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    // GL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    *window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if(!*window)
    {
        spdlog::error("[rpv] Error: Failed to create window for OpenGL backend");
        return false;
    }

    return rocprofvis_imgui_backend_setup_opengl(backend, *window);
}

bool
rocprofvis_imgui_backend_setup_with_fallback(
    rocprofvis_imgui_backend_t* backend, GLFWwindow** window, int width, int height,
    const char* title, rocprofvis_imgui_backend_preference_t preference)
{
    bool bOk = false;

    if(!backend || !window)
    {
        return false;
    }

    switch(preference)
    {
        case kRPVBackendForceOpenGL:
            // User explicitly requested OpenGL only
            spdlog::info("[rpv] Force using OpenGL backend");
            glfwDestroyWindow(*window);
            bOk = setup_opengl_window_and_backend(backend, window, width, height, title);
            if(!bOk)
            {
                spdlog::error("[rpv] Error: Failed to initialize forced OpenGL backend");
            }
            break;

        case kRPVBackendForceVulkan:
            // User explicitly requested Vulkan only
            spdlog::info("[rpv] Force using Vulkan backend");
            if(!glfwVulkanSupported())
            {
                spdlog::error("[rpv] Error: Vulkan not supported by GLFW, cannot use "
                              "forced Vulkan backend");
                bOk = false;
            }
            else
            {
                bOk = rocprofvis_imgui_backend_setup_vulkan(backend, *window);
                if(!bOk)
                {
                    spdlog::error(
                        "[rpv] Error: Failed to initialize forced Vulkan backend");
                }
            }
            break;

        case kRPVBackendAuto:
        default:
        {
            // SSH X11 forwarding can cause Vulkan drivers to crash during
            // swapchain creation rather than returning an error code.
            // Detect this and skip straight to OpenGL.
            bool skip_vulkan = false;
            if(rocprofvis_view_is_remote_display_session())
            {
                spdlog::warn("[rpv] Remote display session detected, "
                             "skipping Vulkan to avoid driver issues with X11 forwarding. "
                             "Use --backend vulkan to override.");
                skip_vulkan = true;
            }
            // Auto mode: Try Vulkan first, fallback to OpenGL on failure
            if(!skip_vulkan && glfwVulkanSupported())
            {
                spdlog::info("[rpv] Vulkan is supported, attempting Vulkan backend...");
                bOk = rocprofvis_imgui_backend_setup_vulkan(backend, *window);

                // If Vulkan setup failed, fallback to OpenGL
                if(!bOk)
                {
                    spdlog::warn("[rpv] Vulkan backend initialization failed, recreating "
                                 "window for OpenGL fallback...");
                    glfwDestroyWindow(*window);
                    bOk = setup_opengl_window_and_backend(backend, window, width, height,
                                                          title);
                }
            }
            else
            {
                if(!skip_vulkan)
                {
                    spdlog::info("[rpv] Vulkan not supported by GLFW, recreating window "
                                 "for OpenGL...");
                }
                glfwDestroyWindow(*window);
                bOk = setup_opengl_window_and_backend(backend, window, width, height,
                                                      title);
            }
            break;
        }
    }

    if(!bOk)
    {
        spdlog::error("[rpv] Error: Failed to initialize graphics backend");
    }

    return bOk;
}

bool
rocprofvis_imgui_backend_complete_init_with_opengl_fallback(
    rocprofvis_imgui_backend_t* backend, GLFWwindow** window, int width, int height,
    const char* title, rocprofvis_imgui_backend_preference_t preference)
{
    if(!backend || !window || !*window)
    {
        return false;
    }

    const bool was_vulkan = (backend->m_init == rocprofvis_imgui_backend_vk_init);

    if(backend->m_init(backend, *window))
    {
        return true;
    }

    if(preference != kRPVBackendAuto || !was_vulkan)
    {
        return false;
    }

    spdlog::warn("[rpv] Vulkan initialization failed, falling back to OpenGL...");

    glfwDestroyWindow(*window);
    *window = nullptr;

    if(!setup_opengl_window_and_backend(backend, window, width, height, title))
    {
        spdlog::error("[rpv] Error: OpenGL fallback failed to create window or backend");
        return false;
    }

    if(!backend->m_init(backend, *window))
    {
        spdlog::error("[rpv] Error: OpenGL initialization failed after Vulkan fallback");
        if(*window)
        {
            glfwDestroyWindow(*window);
            *window = nullptr;
        }
        backend->m_destroy(backend);
        return false;
    }

    return true;
}
