// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_imgui_backend.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

// Forward declarations for backend-specific setup functions
bool rocprofvis_imgui_backend_setup_vulkan(rocprofvis_imgui_backend_t* backend,
                                           GLFWwindow* window);
bool rocprofvis_imgui_backend_setup_opengl(rocprofvis_imgui_backend_t* backend,
                                           GLFWwindow* window);

bool
rocprofvis_imgui_backend_setup(rocprofvis_imgui_backend_t* backend, GLFWwindow* window)
{
    bool bOk = false;

    if(backend)
    {
        // Try Vulkan first if supported
        if(glfwVulkanSupported())
        {
            fprintf(stderr, "[rpv] Vulkan is supported, attempting Vulkan backend...\n");
            bOk = rocprofvis_imgui_backend_setup_vulkan(backend, window);

            // If Vulkan setup failed, fall back to OpenGL
            if(!bOk)
            {
                fprintf(stderr,
                        "[rpv] Vulkan backend initialization failed, falling back to "
                        "OpenGL...\n");
                bOk = rocprofvis_imgui_backend_setup_opengl(backend, window);
            }
        }
        else
        {
            // Vulkan not supported, use OpenGL directly
            fprintf(stderr,
                    "[rpv] Vulkan not supported by GLFW, using OpenGL backend...\n");
            bOk = rocprofvis_imgui_backend_setup_opengl(backend, window);
        }

        if(!bOk)
        {
            fprintf(stderr, "[rpv] Error: Failed to initialize graphics backend\n");
        }
    }

    return bOk;
}

bool
rocprofvis_imgui_backend_setup_with_fallback(rocprofvis_imgui_backend_t* backend,
                                             GLFWwindow** window,
                                             int width, int height,
                                             const char* title)
{
    bool bOk = false;

    if(backend && window)
    {
        // Try Vulkan first if supported
        if(glfwVulkanSupported())
        {
            fprintf(stderr, "[rpv] Vulkan is supported, attempting Vulkan backend...\n");
            bOk = rocprofvis_imgui_backend_setup_vulkan(backend, *window);

            // If Vulkan setup failed, we need to recreate the window for OpenGL
            if(!bOk)
            {
                fprintf(stderr,
                        "[rpv] Vulkan backend initialization failed, recreating window for "
                        "OpenGL fallback...\n");

                // Destroy the Vulkan-configured window
                glfwDestroyWindow(*window);

                // Reset window hints and create OpenGL-compatible window
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

                if(*window)
                {
                    bOk = rocprofvis_imgui_backend_setup_opengl(backend, *window);
                }
                else
                {
                    fprintf(stderr, "[rpv] Error: Failed to recreate window for OpenGL\n");
                }
            }
        }
        else
        {
            // Vulkan not supported, recreate window for OpenGL
            fprintf(stderr,
                    "[rpv] Vulkan not supported by GLFW, recreating window for OpenGL...\n");

            glfwDestroyWindow(*window);

            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
#if defined(GLFW_SCALE_TO_MONITOR)
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
#if defined(__APPLE__)
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
            *window = glfwCreateWindow(width, height, title, nullptr, nullptr);

            if(*window)
            {
                bOk = rocprofvis_imgui_backend_setup_opengl(backend, *window);
            }
            else
            {
                fprintf(stderr, "[rpv] Error: Failed to create window for OpenGL\n");
            }
        }

        if(!bOk)
        {
            fprintf(stderr, "[rpv] Error: Failed to initialize graphics backend\n");
        }
    }

    return bOk;
}
