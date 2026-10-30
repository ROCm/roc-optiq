// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compare_files_dialog.h"

#include "icons/rocprovfis_icon_defines.h"
#include "model/rocprofvis_model_types.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

#include <chrono>
#include <filesystem>
#include <list>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

constexpr const char* COMPARE_DIALOG_NAME = "Compare Traces##_compare_files";
constexpr const char* COMPARE_EXTENSION   = ".db";
constexpr const char* SLOT_HINT[]         = { "The run to compare against",
                                              "The run compared with the baseline" };
constexpr const char* DETAIL_SEPARATOR    = " \xC2\xB7 ";
constexpr ImVec2      DIALOG_SIZE         = ImVec2(820.0f, 0.0f);
constexpr ImVec2      DIALOG_MIN_SIZE     = ImVec2(560.0f, 0.0f);
constexpr ImVec2      SLOT_PADDING        = ImVec2(14.0f, 10.0f);
constexpr float       SLOT_HEIGHT_FRAMES  = 7.5f;
constexpr float       SLOT_ICON_SCALE     = 2.0f;

CompareFilesDialog::CompareFilesDialog(BrowseCallback browse_callback,
                                       OpenCallback   open_callback)
: m_browse_callback(browse_callback)
, m_open_callback(open_callback)
{}

CompareFilesDialog::SlotFile
CompareFilesDialog::Describe(const std::string& file_path)
{
    SlotFile file;
    file.path = file_path;
    if(file_path.empty())
    {
        return file;
    }

    const std::filesystem::path path(file_path);
    file.name   = path.filename().string();
    file.folder = path.parent_path().string();

    std::error_code ec;
    if(!std::filesystem::is_regular_file(path, ec))
    {
        file.problem = "The file could not be found.";
        return file;
    }
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if(!ec)
    {
        file.details = format_byte_size(static_cast<uint64_t>(size));
    }
    const std::filesystem::file_time_type write_time =
        std::filesystem::last_write_time(path, ec);
    if(!ec)
    {
        // C++17 has no clock_cast, so the file clock is mapped through "now".
        const std::chrono::system_clock::time_point system_time =
            std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                write_time - std::filesystem::file_time_type::clock::now() +
                std::chrono::system_clock::now());
        const uint64_t epoch_seconds =
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                      system_time.time_since_epoch())
                                      .count());
        file.details += (file.details.empty() ? "" : DETAIL_SEPARATOR) +
                        std::string("Modified ") + format_local_time(epoch_seconds);
    }
    if(Core::String::to_lower_copy(path.extension().string()) != COMPARE_EXTENSION)
    {
        file.problem = "Compare reads ROCm Systems Profiler databases (.db).";
    }
    else if(!rocprofvis_controller_compare_supported(file_path.c_str()))
    {
        file.problem = "Not a ROCm Systems Profiler (rocprofv3) database. Legacy rocpd "
                       "and ROCm Compute Profiler databases cannot be compared.";
    }
    return file;
}

void
CompareFilesDialog::Show(const std::string& baseline)
{
    m_slots       = { Describe(baseline), SlotFile{} };
    m_should_open = true;
    m_is_open     = true;
}

void
CompareFilesDialog::SetFilePath(FileSlot slot, const std::string& file_path)
{
    m_slots[slot == FileSlot::kFirst ? 0 : 1] = Describe(file_path);
    m_should_open                             = true;
    m_is_open                                 = true;
}

void
CompareFilesDialog::CheckPair()
{
    const std::array<std::string, 2> picks = { m_slots[0].path, m_slots[1].path };
    if(picks != m_checked_picks)
    {
        m_checked_picks = picks;
        m_same_run      = !m_slots[0].path.empty() && !m_slots[1].path.empty() &&
                     m_slots[0].problem.empty() && m_slots[1].problem.empty() &&
                     rocprofvis_controller_compare_same_run(m_slots[0].path.c_str(),
                                                            m_slots[1].path.c_str());
    }
}

void
CompareFilesDialog::AddDroppedFile(const std::string& file_path)
{
    FileSlot slot = m_slots[0].path.empty() ? FileSlot::kFirst : FileSlot::kSecond;
    SetFilePath(slot, file_path);
}

void
CompareFilesDialog::Close()
{
    m_is_open = false;
    ImGui::CloseCurrentPopup();
}

std::string
CompareFilesDialog::PairProblem() const
{
    std::string     problem;
    std::error_code ec;
    if(!m_slots[0].path.empty() && !m_slots[1].path.empty() &&
       std::filesystem::equivalent(m_slots[0].path, m_slots[1].path, ec))
    {
        problem =
            "The baseline and the target are the same file. Pick two different traces.";
    }
    else if(m_same_run)
    {
        problem = "These two files are copies of the same trace and cannot be compared. "
                  "Pick traces from two different profiling runs.";
    }
    return problem;
}

bool
CompareFilesDialog::CanCompare() const
{
    return !m_slots[0].path.empty() && !m_slots[1].path.empty() &&
           m_slots[0].problem.empty() && m_slots[1].problem.empty() &&
           PairProblem().empty();
}

void
CompareFilesDialog::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup(COMPARE_DIALOG_NAME);
        m_should_open = false;
    }
    if(!ImGui::IsPopupOpen(COMPARE_DIALOG_NAME, ImGuiPopupFlags_None))
    {
        m_is_open = false;
        return;
    }

    SettingsManager& settings = SettingsManager::GetInstance();
    PopUpStyle       popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    ImGui::SetNextWindowSize(GetResponsiveWindowSize(DIALOG_SIZE, DIALOG_MIN_SIZE));

    if(ImGui::BeginPopupModal(COMPARE_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings))
    {
        const ImGuiStyle& style = settings.GetDefaultStyle();
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped(
            "Pick a baseline and a trace to compare with it. Both open as one project: "
            "their timelines start together at zero, and the tables, details and "
            "summary show the two side by side with the change between them. You can "
            "also drop files onto the window.");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        const float  swap_width = ImGui::GetFrameHeight();
        const ImVec2 slot_size(
            (ImGui::GetContentRegionAvail().x - swap_width - 2.0f * style.ItemSpacing.x) *
                0.5f,
            ImGui::GetFrameHeightWithSpacing() * SLOT_HEIGHT_FRAMES);
        RenderSlot(FileSlot::kFirst, slot_size);
        ImGui::SameLine();
        const float slot_top = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(slot_top + 0.5f * (slot_size.y - ImGui::GetFrameHeight()));
        ImGui::BeginDisabled(m_slots[0].path.empty() && m_slots[1].path.empty());
        // No frame padding: the button is too small for the icon plus padding, and
        // ImGui then pins the icon to the padded corner instead of centering it.
        if(IconButton(ICON_ARROWS_CYCLE,
                      settings.GetFontManager().GetFont(FontType::kIcon),
                      ImVec2(swap_width, swap_width), "Swap the baseline and the target",
                      false, ImVec2(0.0f, 0.0f), settings.GetColor(Colors::kButton),
                      settings.GetColor(Colors::kButtonHovered),
                      settings.GetColor(Colors::kButtonActive)))
        {
            std::swap(m_slots[0], m_slots[1]);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetCursorPosY(slot_top);
        RenderSlot(FileSlot::kSecond, slot_size);

        CheckPair();
        const std::string pair_problem = PairProblem();
        if(!pair_problem.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextError));
            ImGui::TextWrapped("%s", pair_problem.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const char* compare_label = "Compare";
        const char* cancel_label  = "Cancel";
        const float buttons_width = ImGui::CalcTextSize(compare_label).x +
                                    ImGui::CalcTextSize(cancel_label).x +
                                    4.0f * style.FramePadding.x + style.ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
                             buttons_width);
        if(ImGui::Button(cancel_label))
        {
            Close();
        }
        ImGui::SameLine();
        const bool can_compare = CanCompare();
        ImGui::BeginDisabled(!can_compare);
        if(AccentButton(compare_label, ImVec2(0.0f, 0.0f), &settings))
        {
            m_open_callback(m_slots[0].path, m_slots[1].path);
            Close();
        }
        ImGui::EndDisabled();
        if(!can_compare && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            SetTooltipStyled("Pick two different .db traces to compare.");
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
    popup_style.PopStyles();
}

void
CompareFilesDialog::RenderSlot(FileSlot slot, const ImVec2& size)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const size_t      index    = slot == FileSlot::kFirst ? 0 : 1;
    const SlotFile&   file     = m_slots[index];

    ImGui::PushID(static_cast<int>(index));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          settings.GetColor(file.problem.empty()
                                                ? Colors::kPanelBorderSubtle
                                                : Colors::kTextError));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, PANEL_CARD_ROUNDING);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SLOT_PADDING);
    ImGui::BeginChild("##slot", size,
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    CompareSourceInfo badge;
    badge.id    = COMPARE_SOURCE_LABEL[index];
    badge.index = index;
    badge.name  = file.name;
    badge.path  = file.path;
    ImGui::AlignTextToFramePadding();
    RenderCompareSourceBadge(badge, settings);
    ImGui::SameLine();
    ImGui::TextUnformatted(COMPARE_SOURCE_ROLE[index]);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", SLOT_HINT[index]);
    ImGui::Separator();
    ImGui::Spacing();

    const float actions_y =
        ImGui::GetWindowHeight() - ImGui::GetFrameHeight() - SLOT_PADDING.y;
    if(file.path.empty())
    {
        const float  icon_size = ImGui::GetFontSize() * SLOT_ICON_SCALE;
        const char*  hint      = "Drop a .db trace here, or browse for one";
        const ImVec2 hint_size = ImGui::CalcTextSize(hint);
        ImGui::SetCursorPosX(0.5f * (ImGui::GetWindowWidth() - icon_size));
        PanelIcon(ICON_DOCUMENT, Colors::kTextDim, &settings, icon_size);
        ImGui::SetCursorPosX(0.5f * (ImGui::GetWindowWidth() - hint_size.x));
        ImGui::TextDisabled("%s", hint);

        ImGui::SetCursorPosY(actions_y);
        const float browse_width =
            ImGui::CalcTextSize("Browse...").x + 2.0f * style.FramePadding.x;
        if(ImGui::Button("Browse...", ImVec2(browse_width, 0.0f)))
        {
            m_browse_callback(slot);
        }
        ImGui::SameLine();
        RenderRecent(slot);
    }
    else
    {
        PanelIcon(ICON_DOCUMENT, Colors::kAccent, &settings);
        ImGui::SameLine();
        ElidedText(file.name.c_str(), ImGui::GetContentRegionAvail().x,
                   ImGui::GetContentRegionAvail().x);
        ImGui::PushID("folder");
        ImGui::BeginDisabled();
        ElidedText(file.folder.c_str(), ImGui::GetContentRegionAvail().x,
                   ImGui::GetContentRegionAvail().x);
        ImGui::EndDisabled();
        ImGui::PopID();
        if(!file.details.empty())
        {
            ImGui::TextDisabled("%s", file.details.c_str());
        }
        if(!file.problem.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextError));
            ImGui::TextWrapped("%s", file.problem.c_str());
            ImGui::PopStyleColor();
        }

        ImGui::SetCursorPosY(actions_y);
        if(ImGui::Button("Change..."))
        {
            m_browse_callback(slot);
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear"))
        {
            m_slots[index] = SlotFile{};
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PopID();
}

void
CompareFilesDialog::RenderRecent(FileSlot slot)
{
    const size_t                  index = slot == FileSlot::kFirst ? 0 : 1;
    const std::string&            other = m_slots[1 - index].path;
    const std::list<std::string>& recent =
        SettingsManager::GetInstance().GetInternalSettings().recent_files;

    std::vector<const std::string*> traces;
    for(const std::string& path : recent)
    {
        if(path != other &&
           Core::String::to_lower_copy(
               std::filesystem::path(path).extension().string()) == COMPARE_EXTENSION)
        {
            traces.push_back(&path);
        }
    }

    ImGui::BeginDisabled(traces.empty());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    PushComboStyles();
    if(ImGui::BeginCombo("##recent",
                         traces.empty() ? "No recent traces" : "Recent traces"))
    {
        for(size_t i = 0; i < traces.size(); i++)
        {
            ImGui::PushID(static_cast<int>(i));
            if(ImGui::Selectable(
                   std::filesystem::path(*traces[i]).filename().string().c_str()))
            {
                m_slots[index] = Describe(*traces[i]);
            }
            if(ImGui::IsItemHovered())
            {
                SetTooltipStyled("%s", traces[i]->c_str());
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    PopComboStyles();
    ImGui::EndDisabled();
}

}  // namespace View
}  // namespace RocProfVis
