// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include <stdint.h>

struct GLFWwindow;
struct ImVec4;
struct ImDrawData;

typedef struct rpvImGuiBackend rpvImGuiBackend;

typedef bool (*rpvImGuiBackendInitFunc)(rpvImGuiBackend* backend, void* window);
typedef bool (*rpvImGuiBackendConfigFunc)(rpvImGuiBackend* backend, void* window);
typedef void (*rpvImGuiBackendUpdateFrameBufferFunc)(rpvImGuiBackend* backend, int32_t fb_width, int32_t fb_height);
typedef void (*rpvImGuiBackendNewFrame)(rpvImGuiBackend* backend);
typedef void (*rpvImGuiBackendRender)(rpvImGuiBackend* backend, ImDrawData* draw_data, ImVec4* clear_color);
typedef void (*rpvImGuiBackendPresent)(rpvImGuiBackend* backend);
typedef void (*rpvImGuiBackendShutdown)(rpvImGuiBackend* backend);
typedef void (*rpvImGuiBackendDestroy)(rpvImGuiBackend* backend);

typedef struct rpvImGuiBackend
{
    void* private_data;
    rpvImGuiBackendInitFunc init;
    rpvImGuiBackendConfigFunc config;
    rpvImGuiBackendUpdateFrameBufferFunc update_framebuffer;
    rpvImGuiBackendNewFrame new_frame;
    rpvImGuiBackendRender render;
    rpvImGuiBackendPresent present;
    rpvImGuiBackendShutdown shutdown;
    rpvImGuiBackendDestroy destroy;
} rpvImGuiBackend;

bool rpv_imgui_backend_setup(rpvImGuiBackend* backend, GLFWwindow* window);
