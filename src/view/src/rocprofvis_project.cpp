#include "rocprofvis_project.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_version.h"
#ifdef COMPUTE_UI_SUPPORT
#    include "rocprofvis_compute_root.h"
#    include "rocprofvis_navigation_manager.h"
#endif
#include "widgets/rocprofvis_notification_manager.h"
#include <fstream>

namespace RocProfVis
{
namespace View
{

Project::Project()
: m_view(nullptr)
, m_trace_type(Undefined)
{}

Project::~Project() {}

std::string
Project::GetID() const
{
    return m_trace_file_path;
}

std::string
Project::GetName() const
{
    return m_name;
}

std::shared_ptr<RocWidget>
Project::GetView()
{
    return m_view;
}

Project::TraceType
Project::GetTraceType() const
{
    return m_trace_type;
}

bool
Project::IsProject() const
{
    return !m_project_file_path.empty();
}

Project::OpenResult
Project::Open(std::string& file_path)
{
    OpenResult result = Failed;
    if(std::filesystem::exists(file_path))
    {
        std::string file_ext = std::filesystem::path(file_path).extension().string();
        if(file_ext == ".rpv")
        {
            result = OpenProject(file_path);
        }
        else
        {
            result = OpenTrace(file_path);
        }
    }
    else
    {
        AppWindow::GetInstance()->ShowMessageDialog(
                "Error", "File does not exist: " + file_path);
    }
    return result;
}

void
Project::Save()
{
    if(IsProject() && SaveSetttingsJson())
    {
        NotificationManager::GetInstance().Show("Saved " + m_project_file_path + ".",
                                                NotificationLevel::Success);
    }
    else
    {
        NotificationManager::GetInstance().Show("Failed to save project.",
                                                NotificationLevel::Error);
    }
}

void
Project::SaveAs(const std::string& file_path)
{
    m_project_file_path = file_path;
    m_name              = std::filesystem::path(m_project_file_path).filename().string();
    AppWindow::GetInstance()->SetTabLabel(GetName(), GetID());
    Save();
}

void
Project::Close()
{
#ifdef COMPUTE_UI_SUPPORT
    if(m_trace_type == Compute)
    {
        NavigationManager::GetInstance()->RefreshNavigationTree();
    }
#endif
}

Project::OpenResult
Project::OpenProject(std::string& file_path)
{
    OpenResult    result = Failed;
    std::ifstream file(file_path);
    if(file.is_open())
    {
        std::string json_string;
        std::string json_line;
        while(std::getline(file, json_line))
        {
            json_string += std::move(json_line);
        }
        std::pair<jt::Json::Status, jt::Json> json_parsed = jt::Json::parse(json_string);
        if(json_parsed.first == jt::Json::success && JsonValidForLoad(json_parsed.second))
        {
            m_project_file_path = file_path;
            m_settings_json     = json_parsed.second;
            std::string trace_path =
                std::filesystem::weakly_canonical(
                    std::filesystem::path(m_project_file_path).parent_path() /
                    std::filesystem::path(m_settings_json[JSON_KEY_GROUP_GENERAL]
                                                         [JSON_KEY_GENERAL_TRACE_PATH]
                                                             .getString()))
                    .string();
            result = OpenTrace(trace_path);
            if(result == Duplicate)
            {
                file_path = trace_path;
            }
        }
        else
        {
            AppWindow::GetInstance()->ShowMessageDialog(
                "Error", "Failed to load project: " + file_path);
        }
        file.close();
    }
    return result;
}

Project::OpenResult
Project::OpenTrace(std::string& file_path)
{
    OpenResult result    = Failed;
    Project*   duplicate = AppWindow::GetInstance()->GetProject(file_path);
    if(duplicate)
    {
        file_path = duplicate->GetID();
        result    = Duplicate;
        NotificationManager::GetInstance().Show(file_path + " already open.",
                                                NotificationLevel::Warning);
    }
    else if(!m_view)
    {
        bool                       trace_result = false;
        TraceType                  trace_type   = Undefined;
        std::shared_ptr<RocWidget> view         = nullptr;
        std::string file_ext = std::filesystem::path(file_path).extension().string();
#ifdef COMPUTE_UI_SUPPORT
        if(file_ext == ".csv")
        {
            std::shared_ptr<ComputeRoot> compute_view = std::make_shared<ComputeRoot>();
            trace_result                              = compute_view->OpenTrace(file_path);
            trace_type = Compute;
            view       = compute_view;
            NavigationManager::GetInstance()->RefreshNavigationTree();
        }
        else
#endif
        {
            std::shared_ptr<TraceView> trace_view = std::make_shared<TraceView>();
            trace_result                          = trace_view->OpenFile(file_path);
            trace_type                            = System;
            view                                  = trace_view;
        }
        if(trace_result && view)
        {
            m_trace_file_path = file_path;
            m_trace_type      = trace_type;
            m_view            = view;
            m_name            = std::filesystem::path(IsProject() ? m_project_file_path
                                                                  : m_trace_file_path)
                         .filename()
                         .string();
            result = Success;
        }
    }
    return result;
}

bool
Project::JsonValidForLoad(jt::Json& json)
{
    return json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_TRACE_PATH].isString();
}

void
Project::RegisterSetting(ProjectSetting* setting)
{
    m_settings.push_back(setting);
}

jt::Json&
Project::GetSettingsJson()
{
    return m_settings_json;
}

bool
Project::SaveSetttingsJson()
{
    bool result     = false;
    m_settings_json = "";
    m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_VERSION] =
        std::to_string(ROCPROFVIS_VERSION_MAJOR) + "." +
        std::to_string(ROCPROFVIS_VERSION_MINOR) + "." +
        std::to_string(ROCPROFVIS_VERSION_PATCH);
    m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_TRACE_PATH] =
        std::filesystem::proximate(
            m_trace_file_path, std::filesystem::path(m_project_file_path).parent_path())
            .generic_string();
    for(ProjectSetting* setting : m_settings)
    {
        setting->ToJson();
    }
    if(!m_settings_json.isNull())
    {
        std::ofstream file(m_project_file_path);
        if(file.is_open())
        {
            file << m_settings_json.toStringPretty() << "\n";
            file.close();
            result = true;
        }
    }
    return result;
}

bool
Project::IsTrimSaveAllowed()
{
    // Check if save is allowed
    bool save_allowed = false;
    if(m_trace_type == System)
    {
        // Check if the active tab is a TraceView
        std::shared_ptr<TraceView> trace_view =
            std::dynamic_pointer_cast<TraceView>(m_view);
        if(trace_view)
        {
            // Check if the trace view has a selection that can be saved
            save_allowed = trace_view->IsTrimSaveAllowed();
        }
    }
    return save_allowed;
}

void
Project::TrimSave(const std::string& file_path_str)
{
    if(m_trace_type == System)
    {
        std::shared_ptr<TraceView> trace_view =
            std::dynamic_pointer_cast<TraceView>(m_view);
        if(trace_view)
        {
            trace_view->SaveSelection(file_path_str);
        }
    }
}

ProjectSetting::ProjectSetting(const std::string project_id)
: m_project(*AppWindow::GetInstance()->GetProject(project_id))
, m_settings_json(m_project.GetSettingsJson())
{
    m_project.RegisterSetting(this);
}

ProjectSetting::~ProjectSetting() {}

}  // namespace View
}  // namespace RocProfVis
