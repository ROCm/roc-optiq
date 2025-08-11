// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rocprofvis_core.h"
#include "rocprofvis_imgui_backend.h"
#define GLFW_INCLUDE_NONE
#include "../resources/AMD_LOGO.h"
#include "rocprofvis_view_module.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"
#include <utility>
#include "spdlog/spdlog.h"

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
glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int
main(int, char**)
{
    int resultCode = 0;

    rocprofvis_core_enable_log();

    // Load image from memory.
    glfwSetErrorCallback(glfw_error_callback);

    if(glfwInit())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window =
            glfwCreateWindow(1280, 720, "ROCm Visualizer", nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

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

                rocprofvis_view_init();

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
                    float xscale, yscale;
                    glfwGetWindowContentScale(window, &xscale, &yscale);

                    rocprofvis_view_set_dpi(xscale);

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
                    /*
                                        ImVec2 displaySize = ImGui::GetIO().DisplaySize;

                                        ImGui::SetNextWindowPos(ImVec2(displaySize.x, 0),
                       ImGuiCond_Always, ImVec2(1.0f, 0.0f));

                                        ImGui::SetNextWindowSize(
                                            ImVec2(displaySize.x * 0.8f, displaySize.y *
                       0.8f), ImGuiCond_Always);

                                        ImGuiWindowFlags windowFlags =
                                            ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoResize;

                                        // Open ImGui window
                                        ImGui::Begin("Line Chart Window", nullptr,
                       windowFlags);
                    */
                    rocprofvis_view_render();

                    // Close ImGui window
                    //                  ImGui::End();

                    // Rendering
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

            // Free the GLFW image pixels
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
