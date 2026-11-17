// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compare_panes.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui_internal.h"
#include "model/rocprofvis_model_types.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

// The track pills' padding, so in a track header the source chip is the size of
// the node pill under it.
constexpr float BADGE_PADDING_X  = 8.0f;
constexpr float BADGE_PADDING_Y  = 2.0f;
constexpr float BADGE_FILL_ALPHA = 0.35f;

// Changes smaller than this share of the first value read as no change.
constexpr double NO_CHANGE_RATIO       = 0.0005;
constexpr size_t RELATIVE_CHANGE_CHARS = 32;

bool
IsCompareTrace(const TraceDataModel& model)
{
    return model.GetCompareSource(COMPARE_SOURCE_A) != nullptr &&
           model.GetCompareSource(COMPARE_SOURCE_B) != nullptr;
}

ImU32
CompareSourceColor(size_t source_index, SettingsManager& settings)
{
    return settings.GetColor(source_index == COMPARE_SOURCE_A
                                 ? Colors::kComparisonBase
                                 : Colors::kComparisonTarget);
}

float
CompareSourceBadgeWidth(const CompareSourceInfo& source)
{
    if(source.id.empty())
    {
        return 0.0f;
    }
    FontManager& fonts = SettingsManager::GetInstance().GetFontManager();
    ImGui::PushFont(fonts.GetFont(FontType::kDefault),
                    fonts.GetFontSize(FontSize::kSmall));
    const float width =
        std::max(ImGui::GetTextLineHeight() + 2.0f * BADGE_PADDING_Y,
                 ImGui::CalcTextSize(source.id.c_str()).x + 2.0f * BADGE_PADDING_X);
    ImGui::PopFont();
    return width;
}

float
CompareSourceBadgeWidth(const TrackInfo* track_info)
{
    return track_info ? CompareSourceBadgeWidth(track_info->compare_source) : 0.0f;
}

void
RenderCompareSourceBadge(const CompareSourceInfo& source, SettingsManager& settings)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if(source.id.empty() || window->SkipItems)
    {
        return;
    }

    // Laid out the way ImGui lays out text, so the chip centers on the text of
    // its line, framed or not.
    const float  width = CompareSourceBadgeWidth(source);
    const ImVec2 pos(window->DC.CursorPos.x,
                     window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const ImRect bb(pos, ImVec2(pos.x + width, pos.y + ImGui::GetTextLineHeight()));
    ImGui::ItemSize(bb.GetSize(), 0.0f);
    if(!ImGui::ItemAdd(bb, 0))
    {
        return;
    }

    FontManager& fonts = settings.GetFontManager();
    ImGui::PushFont(fonts.GetFont(FontType::kDefault),
                    fonts.GetFontSize(FontSize::kSmall));
    const float  height    = ImGui::GetTextLineHeight() + 2.0f * BADGE_PADDING_Y;
    const ImVec2 text_size = ImGui::CalcTextSize(source.id.c_str());
    const ImVec2 chip_min(bb.Min.x, bb.GetCenter().y - 0.5f * height);
    const ImVec2 chip_max(bb.Max.x, chip_min.y + height);
    const ImU32  color = CompareSourceColor(source.index, settings);
    window->DrawList->AddRectFilled(chip_min, chip_max,
                                    ApplyAlpha(color, BADGE_FILL_ALPHA), 0.5f * height);
    window->DrawList->AddRect(chip_min, chip_max, color, 0.5f * height);
    window->DrawList->AddText(
        ImVec2(chip_min.x + 0.5f * (width - text_size.x), chip_min.y + BADGE_PADDING_Y),
        settings.GetColor(Colors::kTextMain), source.id.c_str());
    ImGui::PopFont();

    if(ImGui::IsItemHovered())
    {
        std::string tooltip = "Source " + source.id;
        if(!source.name.empty())
        {
            tooltip += ": " + source.name;
        }
        if(!source.path.empty())
        {
            tooltip += "\n" + source.path;
        }
        SetTooltipStyled("%s", tooltip.c_str());
    }
}

void
RenderCompareSourceBadge(const TrackInfo* track_info, SettingsManager& settings)
{
    if(track_info)
    {
        RenderCompareSourceBadge(track_info->compare_source, settings);
    }
}

std::string
FormatRelativeChange(double a, double b)
{
    std::string text;
    if(a != 0.0)
    {
        char buffer[RELATIVE_CHANGE_CHARS];
        std::snprintf(buffer, sizeof(buffer), "%+.1f%%", (b - a) / a * 100.0);
        text = buffer;
    }
    return text;
}

void
RenderCompareChange(double a, double b, SettingsManager& settings,
                    const std::string& difference)
{
    if(std::abs(b - a) <= std::abs(a) * NO_CHANGE_RATIO)
    {
        ImGui::TextDisabled("No change");
        return;
    }

    const bool   greater = b > a;
    const ImVec4 color   = ImGui::ColorConvertU32ToFloat4(settings.GetColor(
        greater ? Colors::kComparisonGreater : Colors::kComparisonLesser));
    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kIcon), 0.0f);
    ImGui::TextColored(color, "%s", greater ? ICON_ARROW_UP : ICON_ARROW_DOWN);
    ImGui::PopFont();
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

    std::string text = a == 0.0 ? std::string("New") : FormatRelativeChange(a, b);
    if(!difference.empty())
    {
        text += "  " + difference;
    }
    ImGui::TextColored(color, "%s", text.c_str());
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

std::shared_ptr<VSplitContainer>
MakeCompareStack(std::shared_ptr<RocWidget> pane_a, std::shared_ptr<RocWidget> pane_b)
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    const ImVec2      inset(style.ItemSpacing.x * PANE_INSET_FACTOR,
                            style.ItemSpacing.y * PANE_INSET_FACTOR);

    LayoutItem::Ptr item_a = LayoutItem::CreateFromWidget(pane_a);
    LayoutItem::Ptr item_b = LayoutItem::CreateFromWidget(pane_b);
    for(const LayoutItem::Ptr& item : { item_a, item_b })
    {
        item->m_child_flags    = ImGuiChildFlags_None;
        item->m_window_padding = inset;
    }

    std::shared_ptr<VSplitContainer> stack =
        std::make_shared<VSplitContainer>(item_a, item_b);
    stack->SetSplit(COMPARE_EVEN_SPLIT);
    return stack;
}

std::string
CountLabel(uint64_t count, const char* singular, const char* plural)
{
    return std::to_string(count) + " " + (count == 1 ? singular : plural);
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

    ImGui::AlignTextToFramePadding();
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
