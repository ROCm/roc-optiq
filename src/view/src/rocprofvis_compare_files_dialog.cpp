// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compare_files_dialog.h"

#include "imgui.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

#include <filesystem>

namespace RocProfVis
{
namespace View
{

constexpr const char* COMPARE_DIALOG_NAME = "Compare Trace Files##_compare_files";
constexpr float       COMPARE_SIDE_HEIGHT = 150.0f;

static void
RenderFileSide(const char* label, CompareFilesDialog::FileSlot slot,
               const std::string& file_path, float width,
               const CompareFilesDialog::BrowseCallback& browse)
{
    ImGui::BeginChild(label, ImVec2(width, COMPARE_SIDE_HEIGHT), true);
    SectionTitle(label, false);
    ImGui::Spacing();

    if(file_path.empty())
    {
        ImGui::TextDisabled("No file selected.");
    }
    else
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                               ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(file_path.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing() -
                         ImGui::GetStyle().WindowPadding.y);
    if(ImGui::Button("Browse...", ImVec2(-1.0f, 0.0f)))
    {
        browse(slot);
    }
    ImGui::EndChild();
}

CompareFilesDialog::CompareFilesDialog(BrowseCallback browse_callback,
                                       OpenCallback   open_callback)
: m_browse_callback(browse_callback)
, m_open_callback(open_callback)
{}

void
CompareFilesDialog::Show()
{
    m_first_file.clear();
    m_second_file.clear();
    m_error_message.clear();
    m_should_open = true;
    m_is_open     = true;
}

void
CompareFilesDialog::SetFilePath(FileSlot slot, const std::string& file_path)
{
    (slot == FileSlot::kFirst ? m_first_file : m_second_file) = file_path;
    m_error_message.clear();
    m_should_open = true;
    m_is_open     = true;
}

void
CompareFilesDialog::AddDroppedFile(const std::string& file_path)
{
    FileSlot slot = m_first_file.empty() ? FileSlot::kFirst : FileSlot::kSecond;
    SetFilePath(slot, file_path);
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
    ImGui::SetNextWindowSize(
        GetResponsiveWindowSize(ImVec2(760.0f, 0.0f), ImVec2(520.0f, 0.0f)));

    if(ImGui::BeginPopupModal(COMPARE_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextWrapped(
            "Select two trace databases to compare. They are loaded together onto a "
            "single timeline, with each track tagged by its source.");
        ImGui::Spacing();

        float side_width =
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        RenderFileSide("First Trace", FileSlot::kFirst, m_first_file, side_width,
                       m_browse_callback);
        ImGui::SameLine();
        RenderFileSide("Second Trace", FileSlot::kSecond, m_second_file, side_width,
                       m_browse_callback);

        ImGui::Spacing();
        if(!m_error_message.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextError));
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                                   ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(m_error_message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        ImGui::Separator();
        if(ImGui::Button("Compare and Open"))
        {
            if(Validate())
            {
                m_open_callback(m_first_file, m_second_file);
                m_is_open = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if(ImGui::Button("Cancel"))
        {
            m_is_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    popup_style.PopStyles();
}

bool
CompareFilesDialog::Validate()
{
    if(m_first_file.empty() || m_second_file.empty())
    {
        m_error_message = "Select both trace files before comparing.";
        return false;
    }

    std::error_code ec;
    if(std::filesystem::equivalent(m_first_file, m_second_file, ec))
    {
        m_error_message = "Please select two different trace files to compare.";
        return false;
    }

    m_error_message.clear();
    return true;
}

}  // namespace View
}  // namespace RocProfVis
