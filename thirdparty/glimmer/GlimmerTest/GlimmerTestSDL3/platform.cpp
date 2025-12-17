#include "platform.h"
#include "context.h"
#include "renderer.h"

#include <cstring>
#include <list>

#ifndef GLIMMER_IMGUI_MAINWINDOW_NAME
#define GLIMMER_IMGUI_MAINWINDOW_NAME "main-window"
#endif

#include "libs/inc/SDL3/SDL.h"
#include "libs/inc/SDL3/SDL_gpu.h"
#include "libs/inc/imguisdl3/imgui_impl_sdl3.h"
#include "libs/inc/imguisdl3/imgui_impl_sdlgpu3.h"
#include "libs/inc/imguisdl3/imgui_impl_sdlrenderer3.h"

#ifdef GLIMMER_ENABLE_NFDEXT
#include <mutex>
#include "nfd-ext/src/include/nfd.h"
#endif

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <WinUser.h>
#undef CreateWindow

static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    desc.capslock = GetAsyncKeyState(VK_CAPITAL) < 0;
    desc.insert = false;
}
#elif __linux__
#include <ctsdio>
#include <unistd.h>

static std::string exec(const char* cmd)
{
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
        result += buffer;

    pclose(pipe);
    return result;
}

static void DetermineKeyStatus(glimmer::IODescriptor& desc)
{
    std::string xset_output = exec("xset -q | grep Caps | sed -n 's/^.*Caps Lock:\\s*\\(\\S*\\).*$/\\1/p'");
    desc.capslock = xset_output.find("on") != std::string::npos;
    desc.insert = false;
}
#endif

namespace glimmer
{
    static void RegisterKeyBindings()
    {
        KeyMappings.resize(512, { 0, 0 });
        KeyMappings[Key_0] = { '0', ')' }; KeyMappings[Key_1] = { '1', '!' }; KeyMappings[Key_2] = { '2', '@' };
        KeyMappings[Key_3] = { '3', '#' }; KeyMappings[Key_4] = { '4', '$' }; KeyMappings[Key_5] = { '5', '%' };
        KeyMappings[Key_6] = { '6', '^' }; KeyMappings[Key_7] = { '7', '&' }; KeyMappings[Key_8] = { '8', '*' };
        KeyMappings[Key_9] = { '9', '(' };

        KeyMappings[Key_A] = { 'A', 'a' }; KeyMappings[Key_B] = { 'B', 'b' }; KeyMappings[Key_C] = { 'C', 'c' };
        KeyMappings[Key_D] = { 'D', 'd' }; KeyMappings[Key_E] = { 'E', 'e' }; KeyMappings[Key_F] = { 'F', 'f' };
        KeyMappings[Key_G] = { 'G', 'g' }; KeyMappings[Key_H] = { 'H', 'h' }; KeyMappings[Key_I] = { 'I', 'i' };
        KeyMappings[Key_J] = { 'J', 'j' }; KeyMappings[Key_K] = { 'K', 'k' }; KeyMappings[Key_L] = { 'L', 'l' };
        KeyMappings[Key_M] = { 'M', 'm' }; KeyMappings[Key_N] = { 'N', 'n' }; KeyMappings[Key_O] = { 'O', 'o' };
        KeyMappings[Key_P] = { 'P', 'p' }; KeyMappings[Key_Q] = { 'Q', 'q' }; KeyMappings[Key_R] = { 'R', 'r' };
        KeyMappings[Key_S] = { 'S', 's' }; KeyMappings[Key_T] = { 'T', 't' }; KeyMappings[Key_U] = { 'U', 'u' };
        KeyMappings[Key_V] = { 'V', 'v' }; KeyMappings[Key_W] = { 'W', 'w' }; KeyMappings[Key_X] = { 'X', 'x' };
        KeyMappings[Key_Y] = { 'Y', 'y' }; KeyMappings[Key_Z] = { 'Z', 'z' };

        KeyMappings[Key_Apostrophe] = { '\'', '"' }; KeyMappings[Key_Backslash] = { '\\', '|' };
        KeyMappings[Key_Slash] = { '/', '?' }; KeyMappings[Key_Comma] = { ',', '<' };
        KeyMappings[Key_Minus] = { '-', '_' }; KeyMappings[Key_Period] = { '.', '>' };
        KeyMappings[Key_Semicolon] = { ';', ':' }; KeyMappings[Key_Equal] = { '=', '+' };
        KeyMappings[Key_LeftBracket] = { '[', '{' }; KeyMappings[Key_RightBracket] = { ']', '}' };
        KeyMappings[Key_Space] = { ' ', ' ' }; KeyMappings[Key_Tab] = { '\t', '\t' };
        KeyMappings[Key_GraveAccent] = { '`', '~' };
    }

    static std::list<SDL_GPUTextureSamplerBinding> SamplerBindings;

    struct ImGuiSDL3Platform final : public IPlatform
    {
        void SetClipboardText(std::string_view input)
        {
            SDL_SetClipboardText(input.data());
        }

        std::string_view GetClipboardText()
        {
            return SDL_GetClipboardText();
        }

        bool CreateWindow(const WindowParams& params)
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                printf("Error: SDL_Init(): %s\n", SDL_GetError());
                return false;
            }

            SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED;

            window = SDL_CreateWindow(params.title.data(), (int)params.size.x, (int)params.size.y, window_flags);
            if (window == nullptr)
            {
                printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
                return false;
            }
            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            if (params.size.x == -1.f || params.size.y == -1.f) SDL_MaximizeWindow(window);
            SDL_ShowWindow(window);

            // Create GPU Device
#ifdef _WIN32
            device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXBC, true, "direct3d12");
#elif __linux__
            device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
#elif __APPLE__
            device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_METALLIB, true, "metal");
#endif
            if (device == nullptr)
            {
                printf("Warning [Unable to create GPU device], falling back to software rendering : %s\n", SDL_GetError());
                fallback = SDL_CreateRenderer(window, "software");
                SDL_SetRenderVSync(fallback, 1);

                if (fallback == nullptr)
                {
                    printf("Error: Could not create SDL renderer: %s\n", SDL_GetError());
                    return false;
                }
            }
            else
            {
                // Claim window for GPU Device
                if (!SDL_ClaimWindowForGPUDevice(device, window))
                {
                    printf("Error: SDL_ClaimWindowForGPUDevice(): %s\n", SDL_GetError());
                    return false;
                }

                SDL_SetGPUSwapchainParameters(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
            }


            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;
            bgcolor[0] = (float)params.bgcolor[0] / 255.f;
            bgcolor[1] = (float)params.bgcolor[1] / 255.f;
            bgcolor[2] = (float)params.bgcolor[2] / 255.f;
            bgcolor[3] = (float)params.bgcolor[3] / 255.f;
            softwareCursor = params.softwareCursor;

#ifdef _DEBUG
            _CrtSetDbgFlag(_CRTDBG_DELAY_FREE_MEM_DF);
#endif //  _DEBUG

            device ? ImGui_ImplSDL3_InitForSDLGPU(window) : ImGui_ImplSDL3_InitForSDLRenderer(window, fallback);
        }

        bool PollEvents(bool (*runner)(ImVec2, IPlatform&, void*), void* data)
        {
            if (device)
            {
                ImGui_ImplSDLGPU3_InitInfo init_info = {};
                init_info.Device = device;
                init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
                init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
                ImGui_ImplSDLGPU3_Init(&init_info);
            }
            else
                ImGui_ImplSDLRenderer3_Init(fallback);

            bool done = false;
            while (!done)
            {
                int width = 0, height = 0;
                SDL_GetWindowSize(window, &width, &height);

                SDL_Event event;
                while (SDL_PollEvent(&event))
                {
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    if (event.type == SDL_EVENT_QUIT)
                        done = true;
                    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                        done = true;
                }

                if (done) break;

                // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
                if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
                {
                    SDL_Delay(10);
                    continue;
                }

                // Start the Dear ImGui frame
                device ? ImGui_ImplSDLGPU3_NewFrame() : ImGui_ImplSDLRenderer3_NewFrame();
                ImGui_ImplSDL3_NewFrame();

                if (EnterFrame(width, height))
                    done = !runner(ImVec2{ (float)width, (float)height }, *this, data);

                ExitFrame();

                if (device)
                {
                    ImDrawData* draw_data = ImGui::GetDrawData();
                    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

                    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device); // Acquire a GPU command buffer

                    SDL_GPUTexture* swapchain_texture;
                    SDL_AcquireGPUSwapchainTexture(command_buffer, window, &swapchain_texture, nullptr, nullptr); // Acquire a swapchain texture

                    if (swapchain_texture != nullptr && !is_minimized)
                    {
                        // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index buffer!
                        Imgui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

                        // Setup and start a render pass
                        SDL_GPUColorTargetInfo target_info = {};
                        target_info.texture = swapchain_texture;
                        target_info.clear_color = SDL_FColor{ bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3] };
                        target_info.load_op = SDL_GPU_LOADOP_CLEAR;
                        target_info.store_op = SDL_GPU_STOREOP_STORE;
                        target_info.mip_level = 0;
                        target_info.layer_or_depth_plane = 0;
                        target_info.cycle = false;
                        SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

                        // Render ImGui
                        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

                        SDL_EndGPURenderPass(render_pass);
                    }

                    // Submit the command buffer
                    SDL_SubmitGPUCommandBuffer(command_buffer);
                }
                else
                {
                    auto& io = ImGui::GetIO();
                    SDL_SetRenderScale(fallback, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
                    SDL_SetRenderDrawColorFloat(fallback, bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
                    SDL_RenderClear(fallback);
                    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), fallback);
                    SDL_RenderPresent(fallback);
                }
            }

            if (device)
            {
                SDL_WaitForGPUIdle(device);
                ImGui_ImplSDL3_Shutdown();
                ImGui_ImplSDLGPU3_Shutdown();
                ImGui::DestroyContext();

                SDL_ReleaseWindowFromGPUDevice(device, window);
                SDL_DestroyGPUDevice(device);
            }
            else
            {
                ImGui_ImplSDLRenderer3_Shutdown();
                ImGui_ImplSDL3_Shutdown();
                ImGui::DestroyContext();

                SDL_DestroyRenderer(fallback);
            }
            
            SDL_DestroyWindow(window);
#ifdef GLIMMER_ENABLE_NFDEXT
            NFD_Quit();
#endif
            SDL_Quit();
            Cleanup();
            return done;
        }

        ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels)
        {
            if (device)
            {
                SDL_GPUTextureCreateInfo textureInfo = {
                    .type = SDL_GPU_TEXTURETYPE_2D,
                    .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, // Adjust format as needed
                    .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                    .width = (Uint32)size.x,
                    .height = (Uint32)size.y,
                    .layer_count_or_depth = 1,
                    .num_levels = 1,
                };

                SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);
                if (texture == nullptr)
                {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create destination texture.");
                    return 0;
                }

                SDL_GPUTransferBufferCreateInfo transferInfo{
                    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                    .size = textureInfo.width * textureInfo.height * 4
                };
                SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);

                void* mapped_data = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
                memcpy(mapped_data, pixels, transferInfo.size);
                SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

                SDL_GPUCommandBuffer* cmd_buffer = SDL_AcquireGPUCommandBuffer(device);
                SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd_buffer);

                // Upload from transfer buffer to texture
                SDL_GPUTextureTransferInfo src_info = {
                    .transfer_buffer = transfer_buffer,
                    .offset = 0
                };

                SDL_GPUTextureRegion dst_region = {
                    .texture = texture,
                    .mip_level = 0,
                    .layer = 0,
                    .x = 0, .y = 0, .z = 0,
                    .w = textureInfo.width, .h = textureInfo.height, .d = 1
                };

                SDL_UploadToGPUTexture(copy_pass, &src_info, &dst_region, false);
                SDL_EndGPUCopyPass(copy_pass);
                SDL_SubmitGPUCommandBuffer(cmd_buffer);

                // Clean up transfer buffer (texture remains on GPU)
                SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

                SDL_GPUSamplerCreateInfo sampler_info = {};
                sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
                sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
                sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
                sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
                sampler_info.mip_lod_bias = 0.0f;
                sampler_info.min_lod = -1000.0f;
                sampler_info.max_lod = 1000.0f;
                sampler_info.enable_anisotropy = false;
                sampler_info.max_anisotropy = 1.0f;
                sampler_info.enable_compare = false;

                auto& binding = SamplerBindings.emplace_back();
                binding.sampler = SDL_CreateGPUSampler(device, &sampler_info);
                binding.texture = texture;

                return (ImTextureID)(intptr_t)(&binding);
            }
            else
            {
                auto texture = SDL_CreateTexture(fallback, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, (int)size.x, (int)size.y);
                SDL_UpdateTexture(texture, nullptr, pixels, 4 * (int)size.x);
                SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                return (ImTextureID)(intptr_t)(texture);
            }
        }

#ifdef GLIMMER_ENABLE_NFDEXT

        void GetWindowHandle(void* out) override
        {
            std::call_once(nfdInitialized, [] { NFD_Init(); });
			auto nativeWindow = (nfdwindowhandle_t*)out;
#if defined(__WIN32__)
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_WINDOWS;
            nativeWindow->handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);;
#elif defined(__MACOSX__)
            nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_COCOA;
            nativeWindow->handle = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, NULL);
#elif defined(__LINUX__)
            if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "x11") == 0)
            {
                nativeWindow->type = NFD_WINDOW_HANDLE_TYPE_X11;
                nativeWindow->handle = SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);;
            }
#endif
        }

#else

        int32_t ShowFileDialog(std::string_view* out, int32_t outsz, int32_t target,
            std::string_view location, std::pair<std::string_view, std::string_view>* filters,
            int totalFilters, const DialogProperties& props) override
        {
            struct PathSet
            {
                std::string_view* out = nullptr;
                int32_t outsz = 0;
                int32_t filled = 0;
                bool done = false;
            };

            static PathSet pathset{};
            static Vector<SDL_DialogFileFilter, int16_t> sdlfilters;

            pathset = PathSet{ out, outsz, 0, false };
            SDL_DialogFileCallback callback = [](void* userdata, const char* const* filelist, int) {
                auto& pathset = *(PathSet*)userdata;
                auto idx = 0;

                while (filelist[idx] != nullptr && idx < pathset.outsz)
                {
                    pathset.out[idx] = filelist[idx];
                    pathset.filled++;
                    idx++;
                }

                pathset.done = true;
            };

            if ((target & OneFile) || (target & MultipleFiles))
            {
                sdlfilters.clear(true);
                for (auto idx = 0; idx < totalFilters; ++idx)
                {
                    auto& sdlfilter = sdlfilters.emplace_back();
                    sdlfilter.name = filters[idx].first.data();
                    sdlfilter.pattern = filters[idx].second.data();
                }

                auto allowMany = (target & MultipleFiles) != 0;
                auto pset = SDL_CreateProperties();
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, sdlfilters.data());
                SDL_SetNumberProperty(pset, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, totalFilters);
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.data());
                SDL_SetBooleanProperty(pset, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);
                if (!props.title.empty())
                    SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_TITLE_STRING, props.title.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, props.confirmBtnText.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_CANCEL_STRING, props.cancelBtnText.data());

                SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFILE, callback, &pathset, pset);
				SDL_DestroyProperties(pset);
            }
            else
            {
                auto allowMany = (target & MultipleDirectories) != 0;
                auto pset = SDL_CreateProperties();
                SDL_SetPointerProperty(pset, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.data());
                SDL_SetBooleanProperty(pset, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);
                if (!props.title.empty())
                    SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_TITLE_STRING, props.title.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_ACCEPT_STRING, props.confirmBtnText.data());
                SDL_SetStringProperty(pset, SDL_PROP_FILE_DIALOG_CANCEL_STRING, props.cancelBtnText.data());

                SDL_ShowFileDialogWithProperties(SDL_FILEDIALOG_OPENFOLDER, callback, &pathset, pset);
                SDL_DestroyProperties(pset);
            }

            SDL_Event event;
            while (!pathset.done)
            {
                SDL_PollEvent(&event);
				SDL_Delay(10);
            }

            return pathset.filled;
        }

#endif

        SDL_Window* window = nullptr;
        SDL_GPUDevice* device = nullptr;
        SDL_Renderer* fallback = nullptr;
#ifdef GLIMMER_ENABLE_NFDEXT
        std::once_flag nfdInitialized;
#endif
    };

    IPlatform* GetPlatform(ImVec2 size)
    {
        static ImGuiSDL3Platform platform;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            RegisterKeyBindings();
            PushContext(-1);
        }

        return &platform;
    }
}
