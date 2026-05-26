// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compare_files_dialog.h"

#include "imgui.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>

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
}

void
CompareFilesDialog::SetFilePath(FileSlot slot, const std::string& file_path)
{
    (slot == FileSlot::kFirst ? m_first_file : m_second_file) = file_path;
    m_error_message.clear();
    m_should_open = true;
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
            "Select two trace databases to compare. An index.yaml manifest will be created next to the first file and opened.");
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
            std::string yaml_path = WriteManifest();
            if(!yaml_path.empty())
            {
                NotificationManager::GetInstance().Show("Created " + yaml_path + ".",
                                                        NotificationLevel::Success);
                m_open_callback(yaml_path);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if(ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    popup_style.PopStyles();
}

std::string
CompareFilesDialog::WriteManifest()
{
    if(m_first_file.empty() || m_second_file.empty())
    {
        m_error_message = "Select both trace files before comparing.";
        return "";
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    if(fs::equivalent(m_first_file, m_second_file, ec))
    {
        m_error_message = "Please select two different trace files to compare.";
        return "";
    }

    fs::path first_path    = fs::absolute(m_first_file);
    fs::path second_path   = fs::absolute(m_second_file);
    fs::path manifest_path = first_path.parent_path() / "index.yaml";
    fs::path manifest_dir  = manifest_path.parent_path();
    fs::path first_rel     = fs::relative(first_path, manifest_dir, ec);
    fs::path second_rel    = fs::relative(second_path, manifest_dir, ec);
    if(first_rel.empty() || second_rel.empty())
    {
        m_error_message =
            "Failed to resolve trace paths relative to:\n\n" + manifest_dir.string();
        return "";
    }

    std::string first_rel_str  = first_rel.generic_string();
    std::string second_rel_str = second_rel.generic_string();

    YAML::Emitter out;
    out << YAML::BeginMap
        << YAML::Key << "rocprofiler-sdk" << YAML::Value
        << YAML::BeginMap
        << YAML::Key << "rocpd" << YAML::Value
        << YAML::BeginMap
        << YAML::Key << "files" << YAML::Value
        << YAML::BeginSeq << first_rel_str << second_rel_str << YAML::EndSeq
        << YAML::EndMap
        << YAML::EndMap
        << YAML::Key << "optiq" << YAML::Value
        << YAML::BeginMap
        << YAML::Key << "compare" << YAML::Value << true
        << YAML::Key << "compare_files" << YAML::Value
        << YAML::BeginSeq
        << YAML::BeginMap
        << YAML::Key << "id" << YAML::Value << "A"
        << YAML::Key << "name" << YAML::Value << first_path.stem().string()
        << YAML::Key << "path" << YAML::Value << first_rel_str
        << YAML::EndMap
        << YAML::BeginMap
        << YAML::Key << "id" << YAML::Value << "B"
        << YAML::Key << "name" << YAML::Value << second_path.stem().string()
        << YAML::Key << "path" << YAML::Value << second_rel_str
        << YAML::EndMap
        << YAML::EndSeq
        << YAML::EndMap
        << YAML::EndMap;

    std::ofstream yaml(manifest_path);
    if(!yaml.is_open())
    {
        m_error_message =
            "Failed to create compare manifest:\n\n" + manifest_path.string();
        return "";
    }
    yaml << out.c_str() << "\n";
    return manifest_path.string();
}

}  // namespace View
}  // namespace RocProfVis
