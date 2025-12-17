// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "glimmer/src/platform.h"
#include "imgui_impl_vulkan.h"

namespace RocProfVis
{
namespace View
{

// Vulkan platform adapter for glimmer
class GlimmerVulkanPlatform : public glimmer::IPlatform
{
public:
    GlimmerVulkanPlatform();
    ~GlimmerVulkanPlatform();

    // IPlatform interface
    void SetClipboardText(std::string_view input) override;
    std::string_view GetClipboardText() override;
    bool CreateWindow(const glimmer::WindowParams& params) override;
    bool PollEvents(bool (*runner)(ImVec2, glimmer::IPlatform&, void*), void* data) override;
    ImTextureID UploadTexturesToGPU(ImVec2 size, unsigned char* pixels) override;

private:
    std::string m_clipboard_text;
};

}  // namespace View
}  // namespace RocProfVis

