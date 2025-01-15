// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rpv_imgui_backend.h"
#include "rpv_trace.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <stdlib.h>

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**)
{
    int resultCode = 0;
 
    glfwSetErrorCallback(glfw_error_callback);
    
    if(glfwInit())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(1280, 720, "rocprof-visualizer", nullptr, nullptr);
        rpvImGuiBackend backend;
        if(window && rpv_imgui_backend_setup(&backend, window))
        {
            glfwShowWindow(window);

            if (backend.init(&backend, window))
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

                ImGui::StyleColorsLight();

                rpv_trace_setup();

                backend.config(&backend, window);

                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

                while(!glfwWindowShouldClose(window))
                {
                    glfwPollEvents();

                    // Handle changes in the frame buffer size
                    int fb_width, fb_height;
                    glfwGetFramebufferSize(window, &fb_width, &fb_height);
                    backend.update_framebuffer(&backend, fb_width, fb_height);

                    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
                    {
                        ImGui_ImplGlfw_Sleep(10);
                        continue;
                    }

                    backend.new_frame(&backend);
                    ImGui::NewFrame();

                    rpv_trace_draw();

                    // Rendering
                    ImGui::Render();
                    ImDrawData* draw_data = ImGui::GetDrawData();
                    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
                    if(!is_minimized)
                    {
                        backend.render(&backend, draw_data, &clear_color);
                        backend.present(&backend);
                    }
                }

                backend.shutdown(&backend);

                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();

                backend.destroy(&backend);
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
