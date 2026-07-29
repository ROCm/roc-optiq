// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_remote_download_popup.h"

#include <cfloat>
#include <string>

#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

namespace
{
// Fixed width for the transient download popup; height auto-fits its content.
constexpr float PROGRESS_POPUP_WIDTH = 440.0f;

// Bytes shown as whole KiB in the progress label (matches the transfer's
// reporting granularity).
constexpr uint64_t BYTES_PER_KIB = 1024;
}  // namespace

void
RenderRemoteDownloadPopup(const char* popup_id, const char* id_prefix,
                          const FileStat::Snapshot& progress, const char* idle_label,
                          bool finished, bool& open)
{
    if(!open)
    {
        return;
    }

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = ImGui::GetStyle();

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    // Borderless: the cards paint their own bands edge to edge, so the window
    // carries no padding of its own but keeps the app's themed rounding.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, settings.GetDefaultStyle().WindowRounding);
    ImGui::SetNextWindowSize(ImVec2(PROGRESS_POPUP_WIDTH, 0.0f));

    if(ImGui::BeginPopupModal(popup_id, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoScrollbar))
    {
        // Tighten the gap between the stacked cards so they read as one surface.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(style.ItemSpacing.x, PANEL_CARD_STACK_SPACING_Y));

        const std::string header_id = std::string(id_prefix) + "_header";
        BeginPanelCard(header_id.c_str(), PanelCardTone::kFrame, PANEL_HEADER_PADDING, true,
                       &settings);
        {
            PanelIcon(ICON_ARROW_DOWN, Colors::kAccent, &settings);
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::BeginGroup();
            ImGui::PushFont(nullptr,
                            settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
            ImGui::TextUnformatted("Remote Download");
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::TextUnformatted("Fetching the trace over SSH.");
            ImGui::PopStyleColor();
            ImGui::EndGroup();
        }
        EndPanelCard();

        const std::string body_id = std::string(id_prefix) + "_body";
        BeginPanelCard(body_id.c_str(), PanelCardTone::kPanel, PANEL_BODY_PADDING, true,
                       &settings);
        {
            ImGui::TextWrapped("%s", progress.name.c_str());
            ImGui::Spacing();
            if(progress.size > 0)
            {
                float fraction = static_cast<float>(progress.downloaded) /
                                 static_cast<float>(progress.size);
                const std::string label =
                    std::to_string(progress.downloaded / BYTES_PER_KIB) + " / " +
                    std::to_string(progress.size / BYTES_PER_KIB) + " KiB";
                ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), label.c_str());
            }
            else
            {
                PanelFieldLabel(idle_label, false, &settings);
            }
        }
        EndPanelCard();

        if(finished)
        {
            ImGui::CloseCurrentPopup();
            open = false;
        }

        ImGui::PopStyleVar();  // ItemSpacing
        ImGui::EndPopup();
    }
    else
    {
        // The popup is no longer on the stack (dismissed, or never opened this
        // frame), so clear the flag to let the next transfer reopen it.
        open = false;
    }

    ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding
    popup_style.PopStyles();
}

}  // namespace View
}  // namespace RocProfVis
