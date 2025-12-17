#include "platform.h"
#include "context.h"
#include "renderer.h"

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)
#include "nfd-ext/src/include/nfd.h"
#endif

namespace glimmer
{
    int64_t FramesRendered()
    {
        return Config.platform->frameCount;
    }

    IODescriptor::IODescriptor()
    {
        for (auto k = 0; k <= GLIMMER_NKEY_ROLLOVER_MAX; ++k) key[k] = Key_Invalid;
        for (auto ks = 0; ks < (GLIMMER_KEY_ENUM_END - GLIMMER_KEY_ENUM_START + 1); ++ks)
            keyStatus[ks] = ButtonStatus::Default;
    }

#if defined(GLIMMER_ENABLE_NFDEXT) && !defined(__EMSCRIPTEN__)

    static int32_t ExtractPaths(std::string_view* out, int32_t outsz, const nfdpathset_t* outPaths)
    {
        nfdpathsetsize_t totalPaths = 0;
        NFD_PathSet_GetCount(outPaths, &totalPaths);
        auto pidx = 0;

        for (; pidx < std::min((int32_t)totalPaths, outsz); ++pidx)
        {
            nfdu8char_t* path = nullptr;
            NFD_PathSet_GetPathU8(outPaths, pidx, &path);
            if (pidx < outsz)
                out[pidx] = std::string_view((const char*)path);
            NFD_PathSet_FreePathU8(path);
        }

        NFD_PathSet_Free(outPaths);
        return pidx;
    }

    int32_t IPlatform::ShowFileDialog(std::string_view* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters,
        int totalFilters, const DialogProperties& props)
    {
        assert(out != nullptr && outsz >= 1);
        static Vector<nfdu8filteritem_t, int16_t, 16> filterItems{ false };

        filterItems.clear(true);
        modalDialog = true;

        for (int idx = 0; idx < totalFilters; ++idx)
        {
            nfdu8filteritem_t item{};
            item.name = filters[idx].first.data();
            item.spec = filters[idx].second.data();
            filterItems.emplace_back() = item;
        }

        nfdresult_t result = NFD_ERROR;

        if ((target & OneFile) || (target & MultipleFiles))
        {
            nfdopendialogu8args_t args = { 0 };
            args.filterList = filterItems.data();
            args.filterCount = totalFilters;
            args.defaultPath = location.data();
            GetWindowHandle(&args.parentWindow);

            if (target & MultipleFiles)
            {
                const nfdpathset_t* outPaths = nullptr;
                result = NFD_OpenDialogMultipleU8_With(&outPaths, &args);

                if (result == NFD_OKAY)
                {
                    return ExtractPaths(out, outsz, outPaths);
                }
            }
            else
            {
                nfdu8char_t* outPath = nullptr;
                result = NFD_OpenDialogU8_With(&outPath, &args);
                out[0] = std::string_view((const char*)outPath);
                NFD_FreePathU8(outPath);
                return 1;
            }
        }
        else if ((target & OneDirectory) || (target & MultipleDirectories))
        {
            nfdpickfolderu8args_t args = { 0 };
            args.defaultPath = location.data();
            GetWindowHandle(&args.parentWindow);

            if (target & OneDirectory)
            {
                nfdu8char_t* outPath = nullptr;
                result = NFD_PickFolderU8_With(&outPath, &args);
                out[0] = std::string_view((const char*)outPath);
                NFD_FreePathU8(outPath);
                return 1;
            }
            else
            {
                const nfdpathset_t* outPaths = nullptr;
                result = NFD_PickFolderMultipleU8_With(&outPaths, &args);

                if (result == NFD_OKAY)
                {
                    return ExtractPaths(out, outsz, outPaths);
                }
            }
        }
        else
        {
            LOGERROR("Invalid options...\n");
            return -1;
        }

        modalDialog = false;
        return 0;
    }

#else

    int32_t IPlatform::ShowFileDialog(std::string_view* out, int32_t outsz, int32_t target,
        std::string_view location, std::pair<std::string_view, std::string_view>* filters,
        int totalFilters, const DialogProperties& props)
    {
        return 0;
    }

#endif

    IODescriptor IPlatform::CurrentIO() const
    {
        auto& context = GetContext();
        IODescriptor result{};
        result.deltaTime = desc.deltaTime;

        if (!WidgetContextData::ActivePopUpRegion.Contains(desc.mousepos)) 
            result = desc;

        return result;
    }

    void IPlatform::SetMouseCursor(MouseCursor _cursor)
    {
        cursor = _cursor;
    }

    void IPlatform::GetWindowHandle(void* out)
    {
        out = nullptr;
    }

    bool IPlatform::EnterFrame(float width, float height)
    {
        auto color = ToRGBA(bgcolor[0], bgcolor[1], bgcolor[2], bgcolor[3]);
        if (Config.renderer->InitFrame(width, height, color, softwareCursor))
        {
#ifndef GLIMMER_CUSTOM_EVENT_BUFFER

            auto& io = ImGui::GetIO();
            auto rollover = 0;
            auto escape = false, clicked = false;

            desc.deltaTime = io.DeltaTime;
            desc.mousepos = io.MousePos;
            desc.mouseWheel = io.MouseWheel;
            desc.modifiers = io.KeyMods;
            totalTime += io.DeltaTime;

            for (auto idx = 0; idx < ImGuiMouseButton_COUNT; ++idx)
            {
                desc.mouseButtonStatus[idx] =
                    ImGui::IsMouseDown(idx) ? ButtonStatus::Pressed :
                    ImGui::IsMouseReleased(idx) ? ButtonStatus::Released :
                    ImGui::IsMouseDoubleClicked(idx) ? ButtonStatus::DoubleClicked :
                    ButtonStatus::Default;
                clicked = clicked || ImGui::IsMouseDown(idx);
            }

            for (int key = Key_Tab; key != Key_Total; ++key)
            {
                auto imkey = ImGuiKey_NamedKey_BEGIN + key;
                if (ImGui::IsKeyPressed((ImGuiKey)imkey))
                {
                    if ((ImGuiKey)imkey == ImGuiKey_CapsLock) desc.capslock = !desc.capslock;
                    else if ((ImGuiKey)imkey == ImGuiKey_Insert) desc.insert = !desc.insert;
                    else
                    {
                        if (rollover < GLIMMER_NKEY_ROLLOVER_MAX)
                            desc.key[rollover++] = (Key)key;
                        desc.keyStatus[key] = ButtonStatus::Pressed;
                        escape = imkey == ImGuiKey_Escape;
                    }
                }
                else if (ImGui::IsKeyReleased((ImGuiKey)imkey))
                    desc.keyStatus[key] = ButtonStatus::Released;
                else
                    desc.keyStatus[key] = ButtonStatus::Default;
            }

#endif

            while (rollover <= GLIMMER_NKEY_ROLLOVER_MAX)
                desc.key[rollover++] = Key_Invalid;

            InitFrameData();
            cursor = MouseCursor::Arrow;
            return true;
        }
        
        return false;
    }

    void IPlatform::ExitFrame()
    {
        ++frameCount; ++deltaFrames;
        totalDeltaTime += desc.deltaTime;
        maxFrameTime = std::max(maxFrameTime, desc.deltaTime);

        for (auto idx = 0; idx < GLIMMER_NKEY_ROLLOVER_MAX; ++idx)
            desc.key[idx] = Key_Invalid;

        ResetFrameData();

        if (totalDeltaTime > 1.f)
        {
#ifdef _DEBUG
            auto fps = ((float)deltaFrames / totalDeltaTime);
            if (fps >= 50.f)
                LOG("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            else
                LOGERROR("Total Frames: %d | Current FPS: %.0f | Max Frame Time: %.0fms\n", deltaFrames,
                    fps, (maxFrameTime * 1000.f));
            LOG("*alloc calls in last 1s: %d | Allocated: %d bytes\n", TotalMallocs, AllocatedBytes);
            TotalMallocs = 0;
            AllocatedBytes = 0;
#endif
            maxFrameTime = 0.f;
            totalDeltaTime = 0.f;
            deltaFrames = 0;
        }

        Config.renderer->FinalizeFrame((int32_t)cursor);
    }

    float IPlatform::fps() const
    {
        return (float)frameCount / totalTime;
    }

    UIConfig* IPlatform::config() const
    {
        return &Config;
    }

    bool IPlatform::hasModalDialog() const
    {
        return modalDialog;
    }
}
