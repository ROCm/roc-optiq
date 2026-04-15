// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_presets.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "widgets/rocprofvis_widget.h"
#include <fstream>

namespace RocProfVis
{
namespace View
{

const constexpr int   PRESET_JSON_VERSION        = 1;
const constexpr char* PRESET_JSON_FILE_NAME      = "presets.json";
const constexpr char* PRESET_JSON_KEY_VERSION    = "version";
const constexpr char* PRESET_JSON_KEY_PRESETS    = "presets";
const constexpr char* PRESET_JSON_KEY_COMPONENTS = "components";
constexpr std::array<const char*, PresetManager::ComponentType::NumComponentTypes>
    PRESET_JSON_COMPONENT_TYPES = { "compute_comparison", "compute_tables",
                                    "compute_pivot" };

PresetManager&
PresetManager::GetInstance()
{
    static PresetManager instance;
    return instance;
}

void
PresetManager::RegisterComponent(const std::string& project_id,
                                 PresetComponent&   component)
{
    m_components[project_id][component.Type()] = &component;
}

void
PresetManager::UnregisterComponent(const std::string& project_id,
                                   PresetComponent&   component)
{
    if(m_components.count(project_id) > 0)
    {
        if(m_components.at(project_id).size() > 1)
        {
            m_components.at(project_id).erase(component.Type());
        }
        else
        {
            m_components.erase(project_id);
        }
    }
}

void
PresetManager::UnregisterComponents(const std::string& project_id)
{
    m_components.erase(project_id);
}

void
PresetManager::ResetComponents(const std::string& project_id)
{
    if(m_components.count(project_id) > 0)
    {
        for(std::pair<const ComponentType, PresetComponent*>& component :
            m_components.at(project_id))
        {
            component.second->Reset();
        }
    }
}

void
PresetManager::ListPresets(std::vector<std::string>& output)
{
    output.clear();
    if(m_presets_json.isObject() && m_presets_json.contains(PRESET_JSON_KEY_PRESETS) &&
       m_presets_json[PRESET_JSON_KEY_PRESETS].isObject())
    {
        const std::map<std::string, jt::Json>& presets =
            m_presets_json[PRESET_JSON_KEY_PRESETS].getObject();
        output.reserve(presets.size());
        for(const std::pair<const std::string, jt::Json>& preset : presets)
        {
            output.emplace_back(preset.first);
        }
    }
}

PresetManager::Result
PresetManager::SavePreset(const std::string& project_id, const std::string& preset_name,
                          bool overwrite)
{
    Result result = Error;
    if(project_id.empty() || preset_name.empty() || m_components.count(project_id) == 0)
    {
        result = ErrorInvalidArgument;
    }
    else if(overwrite != (m_presets_json.isObject() &&
                          m_presets_json.contains(PRESET_JSON_KEY_PRESETS) &&
                          m_presets_json[PRESET_JSON_KEY_PRESETS].isObject() &&
                          m_presets_json[PRESET_JSON_KEY_PRESETS].contains(preset_name)))
    {
        result = ErrorOverwrite;
    }
    else
    {
        jt::Json preset           = nullptr;
        bool     component_result = true;
        jt::Json component_data   = nullptr;
        for(std::pair<const ComponentType, PresetComponent*>& component :
            m_components.at(project_id))
        {
            component_result &= component.second->ToJson(component_data);
            if(!component_result)
            {
                result = Error;
                break;
            }
            else if(!component_data.isNull())
            {
                preset[PRESET_JSON_KEY_COMPONENTS]
                      [PRESET_JSON_COMPONENT_TYPES[component.second->Type()]] =
                          std::move(component_data);
            }
            component_data = nullptr;
        }
        if(component_result)
        {
            if(preset.isNull())
            {
                result = ErrorPresetEmpty;
            }
            else
            {
                DeletePreset(preset_name);
                m_presets_json[PRESET_JSON_KEY_VERSION] = PRESET_JSON_VERSION;
                m_presets_json[PRESET_JSON_KEY_PRESETS][preset_name] = std::move(preset);
                if(WritePresetFile())
                {
                    result = Success;
                }
                else
                {
                    result = Error;
                }
            }
        }
    }
    return result;
}

PresetManager::Result
PresetManager::LoadPreset(const std::string& project_id, const std::string& preset_name)
{
    Result result = Error;
    if(project_id.empty() || preset_name.empty() || m_components.count(project_id) == 0)
    {
        result = ErrorInvalidArgument;
    }
    else if(m_presets_json.isObject() &&
            m_presets_json.contains(PRESET_JSON_KEY_PRESETS) &&
            m_presets_json[PRESET_JSON_KEY_PRESETS].isObject() &&
            m_presets_json[PRESET_JSON_KEY_PRESETS].contains(preset_name))
    {
        jt::Json& preset           = m_presets_json[PRESET_JSON_KEY_PRESETS][preset_name];
        bool      component_result = true;
        if(preset.isObject() && preset.contains(PRESET_JSON_KEY_COMPONENTS) &&
           preset[PRESET_JSON_KEY_COMPONENTS].isObject())
        {
            jt::Json& components = preset[PRESET_JSON_KEY_COMPONENTS];
            for(std::pair<const ComponentType, PresetComponent*>& component :
                m_components.at(project_id))
            {
                if(components.contains(
                       PRESET_JSON_COMPONENT_TYPES[component.second->Type()]))
                {
                    component_result &= component.second->FromJson(
                        components
                            [PRESET_JSON_COMPONENT_TYPES[component.second->Type()]]);
                    if(!component_result)
                    {
                        break;
                    }
                }
            }
        }
        if(component_result)
        {
            result = Success;
        }
    }
    return result;
}

PresetManager::Result
PresetManager::DeletePreset(const std::string& preset_name)
{
    Result result = Error;
    if(preset_name.empty())
    {
        result = ErrorInvalidArgument;
    }
    else if(m_presets_json.isObject() &&
            m_presets_json.contains(PRESET_JSON_KEY_PRESETS) &&
            m_presets_json[PRESET_JSON_KEY_PRESETS].isObject() &&
            m_presets_json[PRESET_JSON_KEY_PRESETS].contains(preset_name))
    {
        jt::Json& presets = m_presets_json[PRESET_JSON_KEY_PRESETS];
        if(presets.getObject().size() > 1)
        {
            presets.getObject().erase(preset_name);
        }
        else
        {
            m_presets_json.getObject().erase(PRESET_JSON_KEY_PRESETS);
        }
        if(WritePresetFile())
        {
            result = Success;
        }
    }
    return result;
}

PresetManager::PresetManager()
: m_presets_json_path(std::filesystem::path(get_application_config_path(true)) /
                      PRESET_JSON_FILE_NAME)
{
    if(std::filesystem::exists(m_presets_json_path))
    {
        std::ifstream file(m_presets_json_path);
        if(file.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
            file.close();
            std::pair<jt::Json::Status, jt::Json> read = jt::Json::parse(content);
            if(read.first == jt::Json::success)
            {
                m_presets_json = read.second;
            }
        }
    }
}

PresetComponent::PresetComponent(PresetManager::ComponentType type,
                                 const std::string&           project_id)
: m_type(type)
, m_project_id(project_id)
{
    PresetManager::GetInstance().RegisterComponent(m_project_id, *this);
}

PresetComponent::~PresetComponent()
{
    PresetManager::GetInstance().UnregisterComponent(m_project_id, *this);
}

bool
PresetManager::WritePresetFile()
{
    std::ofstream file(m_presets_json_path);
    bool          result = file.is_open();
    if(result)
    {
        file << m_presets_json.toStringPretty() << "\n";
        file.close();
    }
    return result;
}

PresetManager::ComponentType
PresetComponent::Type() const
{
    return m_type;
}

PresetBrowser::PresetBrowser()
: m_should_open(false)
, m_presets_list_changed(false)
, m_pos_x(0.0f)
, m_pos_y(0.0f)
, m_text_input("\0")
, m_presets(PresetManager::GetInstance())
, m_notifications(NotificationManager::GetInstance())
, m_settings(SettingsManager::GetInstance())
{}

void
PresetBrowser::Update()
{
    if(m_presets_list_changed)
    {
        m_presets.ListPresets(m_presets_list);
        m_presets_list_changed = false;
    }
}

void
PresetBrowser::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup("preset_browser", ImGuiPopupFlags_NoReopen);
        m_should_open = false;
    }
    if(ImGui::IsPopupOpen("preset_browser"))
    {
        const ImGuiStyle& style = m_settings.GetDefaultStyle();
        ImFont* icons = m_settings.GetFontManager().GetIconFont(FontType::kDefault);
        ImGui::SetNextWindowSize(ImGui::GetWindowSize() * 0.2f);
        ImGui::SetNextWindowPos(
            ImVec2(m_pos_x - ImGui::GetWindowWidth() * 0.2f, m_pos_y));
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        if(ImGui::BeginPopup("preset_browser"))
        {
            ImGui::PushFont(icons);
            float manage_width =
                ImGui::CalcTextSize(ICON_OPEN).x + ImGui::CalcTextSize(ICON_ARCHIVE).x +
                ImGui::CalcTextSize(ICON_TRASH_CAN).x + 2.0f * style.ItemSpacing.x;
            float edit_width = ImGui::CalcTextSize(ICON_ADD_NOTE).x +
                               ImGui::CalcTextSize(ICON_ARROWS_CYCLE).x +
                               4.0f * style.FramePadding.x + style.ItemSpacing.x;
            ImGui::PopFont();
            ImGui::BeginChild("list",
                              ImGui::GetContentRegionAvail() -
                                  ImVec2(0.0f, ImGui::GetFrameHeightWithSpacing()));
            if(m_presets_list.empty())
            {
                ImGui::SetCursorPosY(
                    (ImGui::GetContentRegionAvail().y - ImGui::GetFontSize()) * 0.5f);
                CenterNextTextItem("No presets found.");
                ImGui::TextDisabled("No presets found.");
            }
            else
            {
                if(ImGui::BeginTable(
                       "list", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoPadInnerX,
                       ImGui::GetContentRegionAvail()))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Preset Name",
                                            ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Manage", ImGuiTableColumnFlags_WidthFixed,
                                            manage_width);
                    ImGui::TableHeadersRow();
                    for(size_t i = 0; i < m_presets_list.size(); i++)
                    {
                        ImGui::PushID(static_cast<int>(i));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ElidedText(m_presets_list[i].c_str(),
                                   ImGui::GetContentRegionAvail().x,
                                   ImGui::GetContentRegionAvail().x);
                        ImGui::TableNextColumn();
                        CenterNextItem(manage_width);
                        if(IconButton(ICON_OPEN, icons, ImVec2(0.0f, 0.0f),
                                      "Recall Preset"))
                        {
                            PresetManager::Result result = PresetManager::Error;
                            const Project*        project =
                                AppWindow::GetInstance()->GetCurrentProject();
                            if(project)
                            {
                                result = m_presets.LoadPreset(project->GetID(),
                                                              m_presets_list[i]);
                            }
                            if(result == PresetManager::Success)
                            {
                                m_notifications.Show("Preset recalled: " +
                                                         m_presets_list[i],
                                                     NotificationLevel::Success);
                            }
                            else
                            {
                                m_notifications.Show("Failed to recall preset: " +
                                                         m_presets_list[i],
                                                     NotificationLevel::Error);
                            }
                        }
                        ImGui::SameLine();
                        if(IconButton(ICON_ARCHIVE, icons, ImVec2(0.0f, 0.0f),
                                      "Overwrite Preset"))
                        {
                            PresetManager::Result result = PresetManager::Error;
                            const Project*        project =
                                AppWindow::GetInstance()->GetCurrentProject();
                            if(project)
                            {
                                result = m_presets.SavePreset(project->GetID(),
                                                              m_presets_list[i], true);
                            }
                            switch(result)
                            {
                                case PresetManager::Success:
                                {
                                    m_presets_list_changed = true;
                                    m_notifications.Show("Preset updated: " +
                                                             m_presets_list[i],
                                                         NotificationLevel::Success);
                                    break;
                                }
                                case PresetManager::ErrorPresetEmpty:
                                {
                                    m_notifications.Show(
                                        "No customizations to update preset:  " +
                                            m_presets_list[i],
                                        NotificationLevel::Warning);
                                    break;
                                }
                                default:
                                {
                                    m_notifications.Show("Failed to update preset: " +
                                                             m_presets_list[i],
                                                         NotificationLevel::Error);
                                }
                                break;
                            }
                        }
                        ImGui::SameLine();
                        if(IconButton(ICON_TRASH_CAN, icons, ImVec2(0.0f, 0.0f),
                                      "Delete Preset"))
                        {
                            PresetManager::Result result =
                                m_presets.DeletePreset(m_presets_list[i]);
                            if(result == PresetManager::Success)
                            {
                                m_presets_list_changed = true;
                                m_notifications.Show("Preset deleted: " +
                                                         m_presets_list[i],
                                                     NotificationLevel::Success);
                            }
                            else
                            {
                                m_notifications.Show("Failed to delete preset: " +
                                                         m_presets_list[i],
                                                     NotificationLevel::Error);
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();
            std::pair<bool, bool> input = InputTextWithClear(
                "create", "New Preset Name", m_text_input,
                static_cast<size_t>(IM_ARRAYSIZE(m_text_input)), icons,
                m_settings.GetColor(Colors::kBgMain), style,
                ImGui::GetContentRegionAvail().x - edit_width - style.ItemSpacing.x);
            if(input.second)
            {
                m_text_input[0] = '\0';
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(strlen(m_text_input) == 0);
            if(IconButton(
                   ICON_ADD_NOTE, icons, ImVec2(0.0f, 0.0f),
                   "Create preset of current customizations", false, style.FramePadding,
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)),
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)),
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive))))
            {
                PresetManager::Result result = PresetManager::Error;
                const Project* project = AppWindow::GetInstance()->GetCurrentProject();
                if(project)
                {
                    result = m_presets.SavePreset(project->GetID(), m_text_input, false);
                    m_presets_list_changed = (result == PresetManager::Success);
                }
                switch(result)
                {
                    case PresetManager::Success:
                    {
                        m_notifications.Show("Preset created: " +
                                                 std::string(m_text_input),
                                             NotificationLevel::Success);
                        m_text_input[0] = '\0';
                        break;
                    }
                    case PresetManager::ErrorOverwrite:
                    {
                        m_notifications.Show("Preset already exists: " +
                                                 std::string(m_text_input),
                                             NotificationLevel::Warning);
                        break;
                    }
                    case PresetManager::ErrorPresetEmpty:
                    {
                        m_notifications.Show("No customizations to create preset: " +
                                                 std::string(m_text_input),
                                             NotificationLevel::Warning);
                        break;
                    }
                    default:
                    {
                        m_notifications.Show("Failed to create preset: " +
                                                 std::string(m_text_input),
                                             NotificationLevel::Error);
                    }
                    break;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if(IconButton(
                   ICON_ARROWS_CYCLE, icons, ImVec2(0.0f, 0.0f), "Reset customizations",
                   false, style.FramePadding,
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_Button)),
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered)),
                   ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive))))
            {
                bool           result  = false;
                const Project* project = AppWindow::GetInstance()->GetCurrentProject();
                if(project)
                {
                    m_presets.ResetComponents(project->GetID());
                    result = true;
                }
                if(result)
                {
                    m_notifications.Show("Customizations reset",
                                         NotificationLevel::Success);
                }
                else
                {
                    m_notifications.Show("Failed to reset customizations",
                                         NotificationLevel::Error);
                }
            }
            ImGui::EndPopup();
        }
    }
}

void
PresetBrowser::Show()
{
    m_should_open          = true;
    m_presets_list_changed = true;
}

void
PresetBrowser::SetPosition(float pos_x, float pos_y)
{
    m_pos_x = pos_x;
    m_pos_y = pos_y;
}

}  // namespace View
}  // namespace RocProfVis
