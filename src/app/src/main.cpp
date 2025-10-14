// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rocprofvis_core.h"
#include "rocprofvis_imgui_backend.h"
#define GLFW_INCLUDE_NONE
#include "../resources/AMD_LOGO.h"
#include "rocprofvis_view_module.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"
#include <utility>

// globals shared with callbacks
static std::vector<std::string> g_dropped_file_paths;
static bool g_file_was_dropped = false;
static rocprofvis_view_render_options_t g_render_options =
    rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None;

std::pair<GLFWimage, unsigned char*>
glfw_create_icon()
{
    int            width, height, channels;
    unsigned char* pixels = stbi_load_from_memory(AMD_LOGO_png, AMD_LOGO_png_len, &width,
                                                  &height, &channels, STBI_rgb_alpha);

    GLFWimage image;
    if(!pixels)
    {
        spdlog::error("Failed to load icon image: {}", stbi_failure_reason());
        image = { 0, 0, nullptr };
        return { image, nullptr };
    }
    else
    {
        image.width  = width;
        image.height = height;
        image.pixels = pixels;
        return { image, pixels };
    }
}

static void
drop_callback(GLFWwindow* window, int count, const char* paths[])
{
    (void) window; // Unused parameter
    g_dropped_file_paths.clear();
    for(int i = 0; i < count; i++)
    {
        g_dropped_file_paths.push_back(paths[i]);
    }
    g_file_was_dropped = true;
}

static void
content_scale_callback(GLFWwindow* window, float xscale, float yscale)
{
    // Unused parameters
    (void) window;
    (void) yscale;
    rocprofvis_view_set_dpi(xscale);
}

static void
close_callback(GLFWwindow* window)
{
    g_render_options = rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_RequestExit;
    glfwSetWindowShouldClose(window, GLFW_FALSE);
}

static void
app_notification_callback(GLFWwindow* window, int notification)
{
    if(notification == static_cast<int>(rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

static void
glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int
main(int, char**)
{
    int resultCode = 0;

    std::string config_path = rocprofvis_get_application_config_path();
    std::filesystem::path log_path = std::filesystem::path(config_path) / "visualizer.log.txt";
    rocprofvis_core_enable_log(log_path.string().c_str());

    glfwSetErrorCallback(glfw_error_callback);

    if(glfwInit())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if defined(GLFW_SCALE_TO_MONITOR) // GLFW 3.3+
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif        
        GLFWwindow* window =
            glfwCreateWindow(1280, 720, "ROCm Visualizer Beta", nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

        // Drop file callback
        glfwSetDropCallback(window, drop_callback);
        // DPI scaling callback
        glfwSetWindowContentScaleCallback(window, content_scale_callback);
        // Initialize once (callback may not fire immediately on some platforms)
        {
            float xs, ys;
            glfwGetWindowContentScale(window, &xs, &ys);
            content_scale_callback(window, xs, ys);
        }
        // Window close callback
        glfwSetWindowCloseCallback(window, close_callback);

        if(window && rocprofvis_imgui_backend_setup(&backend, window))
        {
            glfwShowWindow(window);

            if(backend.m_init(&backend, window))
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

                ImGui::StyleColorsLight();

                rocprofvis_view_init([window](int notification) -> void {
                    app_notification_callback(window, notification);
                });

                backend.m_config(&backend, window);

                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

                auto [image, pixels] = glfw_create_icon();
                if(image.pixels != nullptr)
                {
                    // Set the window icon
                    GLFWimage images[1] = { image };
                    glfwSetWindowIcon(window, 1, images);

                    // Free the image pixels after setting the icon
                    stbi_image_free(pixels);
                }

                while(!glfwWindowShouldClose(window))
                {
                    // handle dropped file signal flag from callback
                    if (g_file_was_dropped)
                    {
                        rocprofvis_view_open_files(g_dropped_file_paths);
                        g_file_was_dropped = false;
                    }                    

                    glfwPollEvents();

                    // Handle changes in the frame buffer size
                    int fb_width, fb_height;
                    glfwGetFramebufferSize(window, &fb_width, &fb_height);
                    backend.m_update_framebuffer(&backend, fb_width, fb_height);

                    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
                    {
                        ImGui_ImplGlfw_Sleep(10);
                        continue;
                    }

                    backend.m_new_frame(&backend);
                    ImGui::NewFrame();

                    rocprofvis_view_render(g_render_options);
                    g_render_options = rocprofvis_view_render_options_t::
                        kRocProfVisViewRenderOption_None;

                    ImGui::Render();
                    ImDrawData* draw_data    = ImGui::GetDrawData();
                    const bool  is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                               draw_data->DisplaySize.y <= 0.0f);
                    if(!is_minimized)
                    {
                        backend.m_render(&backend, draw_data, &clear_color);
                        backend.m_present(&backend);
                    }
                }

                backend.m_shutdown(&backend);

                rocprofvis_view_destroy();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();

                backend.m_destroy(&backend);
            }

            glfwDestroyWindow(window);
        }
        else
        {
            fprintf(stderr, "GLFW: Failed to initialize window & graphics API\n");
            resultCode = 1;
        }

        glfwTerminate();
    }
    else
    {
        fprintf(stderr, "GLFW: Failed to initialize GLFW\n");
        resultCode = 1;
    }

    return resultCode;
}
