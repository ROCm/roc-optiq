// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Emscripten / WebAssembly entry point for the ROCm Optiq visualizer.
//
// This file is intentionally NOT compiled into the native desktop build. It
// has its own CMakeLists.txt next to it and is configured/built with
// `emcmake cmake` + `cmake --build` (see build.ps1). Nothing in the existing
// `src/app/src/main.cpp` or root CMakeLists.txt has to change for the
// desktop binary to keep working.
//
// Milestone progression:
//   M1: bring up the toolchain. ImGui demo via GLFW (Emscripten contrib
//       port) + WebGL2 in a stand-alone HTML page.
//   M2: load the same artifacts inside a VS Code/Cursor webview panel.
//   M3: link the project's view layer (rocprofvis_view_init/render) and
//       drive it from the Emscripten main loop (this file).
//   M4: make the data model + sqlite work in the browser well enough to
//       open a trace.
//   M5: stream files from the VS Code extension into the WASM virtual FS.
//
// The view layer entry points used here are the same C-style functions
// that the desktop `src/app/src/main.cpp` calls every frame, so we are
// reusing the project's UI code unmodified.

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "rocprofvis_view_module.h"

#include <GLES3/gl3.h>
#include <GLFW/glfw3.h>

#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdio.h>
#include <string>
#include <vector>

namespace
{
GLFWwindow* g_window      = nullptr;
ImVec4      g_clear_color = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
bool        g_view_ready  = false;

void
glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EM_BOOL
on_canvas_resize(int /*event_type*/, const EmscriptenUiEvent* /*ui_event*/, void* /*user_data*/)
{
    int    width  = 0;
    int    height = 0;
    double css_w  = 0.0;
    double css_h  = 0.0;
    emscripten_get_canvas_element_size("#canvas", &width, &height);
    emscripten_get_element_css_size("#canvas", &css_w, &css_h);

    int new_w = static_cast<int>(css_w);
    int new_h = static_cast<int>(css_h);
    if (new_w <= 0 || new_h <= 0)
    {
        return EM_FALSE;
    }

    if (new_w != width || new_h != height)
    {
        emscripten_set_canvas_element_size("#canvas", new_w, new_h);
        if (g_window)
        {
            glfwSetWindowSize(g_window, new_w, new_h);
        }
    }
    return EM_TRUE;
}

void
view_notification_callback(int notification)
{
    // The desktop entry point uses these to close the window or toggle
    // fullscreen. In a webview we have no native window to control yet, so
    // log and ignore.
    fprintf(stderr, "[wasm] view notification: %d\n", notification);
}

void
frame()
{
    on_canvas_resize(0, nullptr, nullptr);

    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (g_view_ready)
    {
        rocprofvis_view_render(
            rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None);
    }
    else
    {
        ImGui::Begin("ROCm Optiq (WASM bring-up)");
        ImGui::TextUnformatted("View layer failed to initialize.");
        ImGui::End();
    }

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(g_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(g_clear_color.x * g_clear_color.w,
                 g_clear_color.y * g_clear_color.w,
                 g_clear_color.z * g_clear_color.w,
                 g_clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_window);
}
} // namespace

extern "C" EMSCRIPTEN_KEEPALIVE void
rocprofvis_wasm_open_file(const char* virtual_path)
{
    if (!virtual_path || virtual_path[0] == '\0')
    {
        fprintf(stderr, "[wasm] open_file called with an empty path\n");
        return;
    }

    if (!g_view_ready)
    {
        fprintf(stderr, "[wasm] view is not ready; cannot open %s\n", virtual_path);
        return;
    }

    fprintf(stderr, "[wasm] opening %s\n", virtual_path);
    rocprofvis_view_open_files(std::vector<std::string>{virtual_path});
}

int
main(int /*argc*/, char** /*argv*/)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        fprintf(stderr, "glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);

    g_window = glfwCreateWindow(1280, 800, "ROCm Optiq (WASM)", nullptr, nullptr);
    if (!g_window)
    {
        fprintf(stderr, "glfwCreateWindow failed\n");
        return 1;
    }

    glfwMakeContextCurrent(g_window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    ImGui::StyleColorsLight();

    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(g_window, "#canvas");
    ImGui_ImplOpenGL3_Init("#version 300 es");

    g_view_ready = rocprofvis_view_init(view_notification_callback,
                                        kRocProfVisViewFileDialog_ImGui);
    if (!g_view_ready)
    {
        fprintf(stderr, "rocprofvis_view_init failed; rendering placeholder.\n");
    }

    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_TRUE,
                                   on_canvas_resize);
    on_canvas_resize(0, nullptr, nullptr);

    emscripten_set_main_loop(frame, 0, 1);
    return 0; // unreachable when the main loop is set with simulate_infinite_loop=1
}
