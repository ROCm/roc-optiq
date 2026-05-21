// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_imgui_backend.h"

// Include windows.h before GLFW to prevent APIENTRY redefinition warnings
#if defined(_WIN32)
#    include <windows.h>
#endif

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// OpenGL loader - use system headers on macOS, ImGui's loader on Windows/Linux
#if defined(__APPLE__)
#    include <OpenGL/gl.h>
#else
// Windows/Linux: Use ImGui's built-in OpenGL loader
// Do NOT include system GL headers (gl/GL.h) - causes conflicts with the loader
#    include "imgui_impl_opengl3_loader.h"
#endif

#include "spdlog/spdlog.h"

typedef struct rocprofvis_imgui_gl_data_t
{
    GLFWwindow* m_window       = nullptr;
    const char* m_glsl_version = nullptr;
} rocprofvis_imgui_gl_data_t;

bool
rocprofvis_imgui_backend_gl_init(rocprofvis_imgui_backend_t* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->m_private_data && window)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) backend->m_private_data;

        backend_data->m_window = (GLFWwindow*) window;

        // Make the OpenGL context current
        glfwMakeContextCurrent(backend_data->m_window);
        glfwSwapInterval(1);  // Enable vsync

        bOk = true;
    }

    return bOk;
}

bool
rocprofvis_imgui_backend_gl_config(rocprofvis_imgui_backend_t* backend, void* window)
{
    bool bOk = false;

    if(backend && backend->m_private_data && window)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) backend->m_private_data;

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL((GLFWwindow*) window, true);
        ImGui_ImplOpenGL3_Init(backend_data->m_glsl_version);

        bOk = true;
    }

    return bOk;
}

void
rocprofvis_imgui_backend_gl_update_framebuffer(rocprofvis_imgui_backend_t* backend,
                                               int32_t fb_width, int32_t fb_height)
{
    // OpenGL handles framebuffer updates automatically via glViewport
    // which is set by ImGui_ImplOpenGL3_RenderDrawData
    (void) backend;
    (void) fb_width;
    (void) fb_height;
}

void
rocprofvis_imgui_backend_gl_new_frame(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }
}

void
rocprofvis_imgui_backend_gl_render(rocprofvis_imgui_backend_t* backend,
                                   ImDrawData* draw_data, ImVec4* clear_color)
{
    if(backend && backend->m_private_data && draw_data && clear_color)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) backend->m_private_data;

        int display_w, display_h;
        glfwGetFramebufferSize(backend_data->m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color->x * clear_color->w, clear_color->y * clear_color->w,
                     clear_color->z * clear_color->w, clear_color->w);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render ImGui draw data
        ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    }
}

void
rocprofvis_imgui_backend_gl_present(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) backend->m_private_data;

        glfwSwapBuffers(backend_data->m_window);
    }
}

void
rocprofvis_imgui_backend_gl_shutdown(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        ImGui_ImplOpenGL3_Shutdown();
    }
}

void
rocprofvis_imgui_backend_gl_destroy(rocprofvis_imgui_backend_t* backend)
{
    if(backend && backend->m_private_data)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) backend->m_private_data;

        free(backend_data);
        memset(backend, 0, sizeof(rocprofvis_imgui_backend_t));
    }
}

bool
rocprofvis_imgui_backend_setup_opengl(rocprofvis_imgui_backend_t* backend,
                                      GLFWwindow*                 window)
{
    (void) window;
    bool bOk = false;

    if(backend)
    {
        rocprofvis_imgui_gl_data_t* backend_data =
            (rocprofvis_imgui_gl_data_t*) calloc(1, sizeof(rocprofvis_imgui_gl_data_t));
        if(backend_data)
        {
            // Decide GL+GLSL versions
#if defined(__APPLE__)
            // GL 3.2 + GLSL 150
            backend_data->m_glsl_version = "#version 150";
#elif defined(_WIN32)
            // GL 3.0 + GLSL 130
            backend_data->m_glsl_version = "#version 130";
#else
            // GL 3.0 + GLSL 130
            backend_data->m_glsl_version = "#version 130";
#endif

            backend->m_private_data = backend_data;
            backend->m_init         = &rocprofvis_imgui_backend_gl_init;
            backend->m_config       = &rocprofvis_imgui_backend_gl_config;
            backend->m_update_framebuffer =
                &rocprofvis_imgui_backend_gl_update_framebuffer;
            backend->m_new_frame = &rocprofvis_imgui_backend_gl_new_frame;
            backend->m_render    = &rocprofvis_imgui_backend_gl_render;
            backend->m_present   = &rocprofvis_imgui_backend_gl_present;
            backend->m_shutdown  = &rocprofvis_imgui_backend_gl_shutdown;
            backend->m_destroy   = &rocprofvis_imgui_backend_gl_destroy;
            bOk                  = true;

            spdlog::info("[rpv] Using OpenGL backend (GLSL version: {})",
                         backend_data->m_glsl_version);
        }
        else
        {
            spdlog::error("[rpv] Error: Couldn't allocate OpenGL ImGui backend");
        }
    }

    return bOk;
}
