// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compare_panes.h"
#include "model/rocprofvis_model_types.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_track_item.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"

#include <algorithm>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Half an item gap is inset around each pane, so the two cards end up sitting a
// full gap apart.
constexpr float PANE_INSET_FACTOR = 0.5f;
constexpr float PANE_MIN_WIDTH    = 280.0f;

// Panes render with ItemSpacing zeroed, so the gap after the badge is taken
// from the default style instead.
constexpr float BADGE_GAP_FACTOR = 2.0f;
// Width of the tooltip shown when a source name has to be elided.
constexpr float NAME_TOOLTIP_GLYPHS = 24.0f;

bool
IsCompareTrace(const TraceDataModel& model)
{
    return model.GetCompareSource(COMPARE_SOURCE_A) != nullptr &&
           model.GetCompareSource(COMPARE_SOURCE_B) != nullptr;
}

std::shared_ptr<HSplitContainer>
MakeCompareSplit(std::shared_ptr<RocWidget> pane_a, std::shared_ptr<RocWidget> pane_b)
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    const ImVec2      inset(style.ItemSpacing.x * PANE_INSET_FACTOR,
                            style.ItemSpacing.y * PANE_INSET_FACTOR);

    // The items are plain gutters: the card inside each pane draws the border and
    // keeps its own padding, so the pane only insets it.
    LayoutItem::Ptr item_a = LayoutItem::CreateFromWidget(pane_a);
    LayoutItem::Ptr item_b = LayoutItem::CreateFromWidget(pane_b);
    for(const LayoutItem::Ptr& item : { item_a, item_b })
    {
        item->m_child_flags    = ImGuiChildFlags_None;
        item->m_window_padding = inset;
    }

    std::shared_ptr<HSplitContainer> split =
        std::make_shared<HSplitContainer>(item_a, item_b);
    split->SetSplit(COMPARE_EVEN_SPLIT);
    split->SetMinLeftWidth(PANE_MIN_WIDTH);
    split->SetMinRightWidth(PANE_MIN_WIDTH);
    return split;
}

void
BeginCompareCard(const char* id, SettingsManager& settings, const ImVec2& size,
                 ImGuiWindowFlags window_flags)
{
    const ImGuiStyle& style = settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::BeginChild(id, size,
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      window_flags);
}

void
EndCompareCard()
{
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void
BuildCompareGroupByChoices(const std::vector<std::string>& columns_a,
                           const std::vector<std::string>& columns_b,
                           std::vector<std::string>&       names_out,
                           std::vector<std::string>&       labels_out)
{
    names_out.clear();
    labels_out.clear();
    names_out.reserve(columns_a.size() + columns_b.size());
    labels_out.reserve(columns_a.size() + columns_b.size());

    for(size_t i = 0; i < columns_a.size(); i++)
    {
        names_out.push_back(columns_a[i]);
    }
    for(size_t i = 0; i < columns_b.size(); i++)
    {
        if(std::find(names_out.begin(), names_out.end(), columns_b[i]) == names_out.end())
        {
            names_out.push_back(columns_b[i]);
        }
    }

    const std::string tag_a =
        std::string(" (") + COMPARE_SOURCE_LABEL[COMPARE_SOURCE_A] + ")";
    const std::string tag_b =
        std::string(" (") + COMPARE_SOURCE_LABEL[COMPARE_SOURCE_B] + ")";
    for(size_t i = 0; i < names_out.size(); i++)
    {
        const bool in_a =
            std::find(columns_a.begin(), columns_a.end(), names_out[i]) != columns_a.end();
        const bool in_b =
            std::find(columns_b.begin(), columns_b.end(), names_out[i]) != columns_b.end();
        if(in_a && in_b)
        {
            labels_out.push_back(names_out[i]);
        }
        else if(in_a)
        {
            labels_out.push_back(names_out[i] + tag_a);
        }
        else
        {
            labels_out.push_back(names_out[i] + tag_b);
        }
    }
}

void
RenderCompareCardTitle(const CompareSourceInfo& source, SettingsManager& settings,
                       const std::string& summary)
{
    const ImGuiStyle& style = settings.GetDefaultStyle();

    RenderCompareSourceBadge(source, settings);
    ImGui::SameLine(0.0f, style.ItemSpacing.x * BADGE_GAP_FACTOR);

    float name_width = ImGui::GetContentRegionAvail().x;
    if(!summary.empty())
    {
        name_width -= ImGui::CalcTextSize(summary.c_str()).x + style.ItemSpacing.x;
        name_width = std::max(name_width, 0.0f);
    }
    const std::string& name = source.name.empty() ? source.path : source.name;
    ElidedText(name.c_str(), name_width, ImGui::GetFontSize() * NAME_TOOLTIP_GLYPHS,
               Alignment_Left, true);

    if(!summary.empty())
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", summary.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

}  // namespace View
}  // namespace RocProfVis
