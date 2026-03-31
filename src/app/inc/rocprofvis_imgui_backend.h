// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include <stdint.h>

struct GLFWwindow;
struct ImVec4;
struct ImDrawData;

typedef struct rocprofvis_imgui_backend_t rocprofvis_imgui_backend_t;

// Backend selection preference for initialization
typedef enum rocprofvis_imgui_backend_preference_t
{
    kRPVBackendAuto,         // Try Vulkan first, fallback to OpenGL on failure
    kRPVBackendForceVulkan,  // Only use Vulkan, fail if it doesn't work
    kRPVBackendForceOpenGL   // Only use OpenGL, fail if it doesn't work
} rocprofvis_imgui_backend_preference_t;

typedef bool (*rocprofvis_imgui_backend_init_t)(rocprofvis_imgui_backend_t* backend, void* window);
typedef bool (*rocprofvis_imgui_backend_config_t)(rocprofvis_imgui_backend_t* backend, void* window);
typedef void (*rocprofvis_imgui_backend_update_framebuffer_t)(rocprofvis_imgui_backend_t* backend, int32_t fb_width, int32_t fb_height);
typedef void (*rocprofvis_imgui_backend_new_frame_t)(rocprofvis_imgui_backend_t* backend);
typedef void (*rocprofvis_imgui_backend_render_t)(rocprofvis_imgui_backend_t* backend, ImDrawData* draw_data, ImVec4* clear_color);
typedef void (*rocprofvis_imgui_backend_present_t)(rocprofvis_imgui_backend_t* backend);
typedef void (*rocprofvis_imgui_backend_shutdown_t)(rocprofvis_imgui_backend_t* backend);
typedef void (*rocprofvis_imgui_backend_destroy_t)(rocprofvis_imgui_backend_t* backend);

typedef struct rocprofvis_imgui_backend_t
{
    void* m_private_data;
    rocprofvis_imgui_backend_init_t m_init;
    rocprofvis_imgui_backend_config_t m_config;
    rocprofvis_imgui_backend_update_framebuffer_t m_update_framebuffer;
    rocprofvis_imgui_backend_new_frame_t m_new_frame;
    rocprofvis_imgui_backend_render_t m_render;
    rocprofvis_imgui_backend_present_t m_present;
    rocprofvis_imgui_backend_shutdown_t m_shutdown;
    rocprofvis_imgui_backend_destroy_t m_destroy;
} rocprofvis_imgui_backend_t;

// Setup function with backend preference and window recreation support
// preference: Auto (with fallback), ForceVulkan (no fallback), or ForceOpenGL (no fallback)
// Returns false if the requested/preferred backend fails to initialize
bool rocprofvis_imgui_backend_setup_with_fallback(
    rocprofvis_imgui_backend_t* backend,
    GLFWwindow** window,
    int width, int height,
    const char* title,
    rocprofvis_imgui_backend_preference_t preference = kRPVBackendAuto);

// After setup_with_fallback(), call this instead of backend->m_init alone. When preference is
// Auto and Vulkan init fails (e.g. no ICD), tears down Vulkan and retries with OpenGL.
bool rocprofvis_imgui_backend_complete_init_with_opengl_fallback(
    rocprofvis_imgui_backend_t* backend,
    GLFWwindow** window,
    int width,
    int height,
    const char* title,
    rocprofvis_imgui_backend_preference_t preference);
