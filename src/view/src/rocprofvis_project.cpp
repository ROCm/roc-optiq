// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_project.h"
#include "protocol/rocprofvis_data_channel.h"
#include "protocol/rocprofvis_protocol_uri.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_presets.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "compute/rocprofvis_compute_view.h"
#include "widgets/rocprofvis_notification_manager.h"
#ifdef ROCPROFVIS_ENABLE_CLIENT_SERVER
#include "clientserver/rocprofvis_server_open.h"
#endif
#include <fstream>

constexpr const char* PROJECT_VERSION = "1.0";

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
    m_open_error_message.clear();
    if(is_server_uri(file_path))
    {
#ifdef ROCPROFVIS_ENABLE_CLIENT_SERVER
        file_path = canonical_server_uri(file_path);
#endif
        result = OpenRemoteTrace(file_path, { file_path });
        if(result == Failed)
        {
            AppWindow::GetInstance()->ShowMessageDialog(
                "Error", m_open_error_message.empty()
                             ? "The trace could not be opened:\n\n" + file_path
                             : m_open_error_message);
            spdlog::error("Failed to open {}: {}", file_path, m_open_error_message);
        }
    }
    else if(std::filesystem::exists(file_path))
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

        if(result == Failed)
        {
            // Use the specific failure message if one was set, else a generic one.
            AppWindow::GetInstance()->ShowMessageDialog(
                "Error",
                m_open_error_message.empty()
                    ? "The file could not be opened:\n\n" + file_path +
                          "\n\nPlease make sure the file is a valid trace or project file."
                    : m_open_error_message);
            spdlog::error("Failed to open file: {}", file_path);
        }
    }
    else
    {
        AppWindow::GetInstance()->ShowMessageDialog(
            "Recent File Not Found",
            "This recent file could not be found and was removed from the list:\n\n" +
                file_path);
        spdlog::error("Failed to open file: {}, file does not exist", file_path);
    }
    return result;
}

void
Project::Save()
{
    if(IsProject() && SaveSetttingsJson())
    {
        SettingsManager::GetInstance().AddRecentFile(m_project_file_path);
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
    PresetManager::GetInstance().UnregisterComponents(m_trace_file_path);
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
            std::filesystem::path project_dir =
                std::filesystem::path(m_project_file_path).parent_path();
            jt::Json& general = m_settings_json[JSON_KEY_GROUP_GENERAL];

            if(general[JSON_KEY_GENERAL_COMPARE_FILES].isArray())
            {
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
                std::vector<std::string> files;
                size_t                   server_files = 0;
                for(jt::Json& entry : general[JSON_KEY_GENERAL_COMPARE_FILES].getArray())
                {
                    if(is_server_uri(entry.getString()))
                    {
                        files.push_back(entry.getString());
                        server_files++;
                    }
                    else
                    {
                        files.push_back(
                            std::filesystem::weakly_canonical(
                                project_dir / std::filesystem::path(entry.getString()))
                                .string());
                    }
                }
                std::string compare_id = AppWindow::MakeCompareId(files);
                if(AppWindow::GetInstance()->GetProject(compare_id))
                {
                    file_path = compare_id;
                    result    = Duplicate;
                    NotificationManager::GetInstance().Show(
                        "This comparison is already open.", NotificationLevel::Warning);
                }
                else if(server_files == files.size())
                {
                    result = OpenRemoteTrace(compare_id, files);
                }
                else if(server_files > 0)
                {
                    m_open_error_message = "This comparison mixes traces on a server with "
                                           "local traces, which cannot be compared.";
                }
                else
                {
                    result = OpenCompare(compare_id, files);
                }
#else
                m_open_error_message =
                    "This project is a systems-trace comparison, which is not "
                    "available in this build.\n\nRebuild with "
                    "ROCPROFVIS_ENABLE_TRACE_COMPARE=ON to open it.";
#endif
            }
            else if(is_server_uri(general[JSON_KEY_GENERAL_TRACE_PATH].getString()))
            {
                std::string trace_uri = general[JSON_KEY_GENERAL_TRACE_PATH].getString();
                result                = OpenRemoteTrace(trace_uri, { trace_uri });
                if(result == Duplicate)
                {
                    file_path = trace_uri;
                }
            }
            else
            {
                std::string trace_path =
                    std::filesystem::weakly_canonical(
                        project_dir / std::filesystem::path(
                                          general[JSON_KEY_GENERAL_TRACE_PATH].getString()))
                        .string();
                if(std::filesystem::exists(trace_path))
                {
                    result = OpenTrace(trace_path);
                    if(result == Duplicate)
                    {
                        file_path = trace_path;
                    }
                }
                else
                {
                    // Referenced trace is gone: name it and don't open it, which would
                    // create an empty database at its original path.
                    m_open_error_message =
                        "The trace file referenced by this project could not be "
                        "found:\n\n" +
                        trace_path +
                        "\n\nIt may have been moved or deleted. Restore it and try "
                        "again.";
                    spdlog::error("Failed to open project {}: referenced trace file "
                                  "does not exist: {}",
                                  file_path, trace_path);
                }
            }
        }
        else
        {
            m_open_error_message = "Failed to load project:\n\n" + file_path +
                                   "\n\nThe project file is invalid or corrupted.";
        }
        file.close();
    }
    return result;
}

Project::OpenResult
Project::OpenTrace(std::string& file_path)
{
    OpenResult open_result = Failed;
    // canonicalize so a .db and the path stored in a .rpv resolve to the same trace
    file_path = std::filesystem::weakly_canonical(file_path).string();
    // trace already open, return duplicate so we switch tabs instead of loading it twice
    Project* duplicate = AppWindow::GetInstance()->GetProject(file_path);
    if(duplicate)
    {
        file_path   = duplicate->GetID();
        open_result = Duplicate;
    }
    else if(!m_view)
    {
        bool                       trace_result = false;
        TraceType                  trace_type   = Undefined;
        std::shared_ptr<RocWidget> view         = nullptr;
        std::string config_path = get_application_config_path(true);
        rocprofvis_controller_t*   controller =
            rocprofvis_controller_alloc(file_path.c_str(), config_path.c_str());
#ifdef ROCPROFVIS_THIN_CLIENT
        if(!controller)
        {
            m_open_error_message =
                "This build of roc-optiq opens traces only through a roc-optiq-server:\n\n" +
                file_path +
                "\n\nUpload it with File > Open from Server, or open it on the server as "
                "optiq://host:port/path/to/trace.";
        }
#endif
        if(controller)
        {
            rocprofvis_controller_object_type_t type =
                kRPVControllerObjectTypeControllerSystem;
            rocprofvis_result_t controller_result =
                rocprofvis_controller_get_object_type(controller, &type);
            if(controller_result == kRocProfVisResultSuccess)
            {
                if(type == kRPVControllerObjectTypeControllerSystem)
                {
                    std::shared_ptr<TraceView> trace_view = std::make_shared<TraceView>();
                    trace_result = trace_view->LoadTrace(controller, file_path);
                    trace_type   = System;
                    view         = trace_view;
                }
                else if(type == kRPVControllerObjectTypeControllerCompute)
                {
                    std::shared_ptr<ComputeView> compute_view =
                        std::make_shared<ComputeView>();
                    trace_result = compute_view->LoadTrace(controller, file_path);
                    trace_type   = Compute;
                    view         = compute_view;
                }
            }
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
            open_result = Success;
        }
        else
        {
            rocprofvis_controller_free(controller);
        }
    }
    return open_result;
}

Project::OpenResult
Project::OpenRemoteTrace(const std::string& project_id, const std::vector<std::string>& uris)
{
    OpenResult open_result = Failed;
    if(AppWindow::GetInstance()->GetProject(project_id))
    {
        open_result = Duplicate;
    }
    else
    {
#ifdef ROCPROFVIS_ENABLE_CLIENT_SERVER
        ServerOpenResult opened = open_on_server(uris);
        open_result = OpenServerTrace(project_id, uris, opened.channel, opened.is_compute,
                                      opened.error);
#else
        m_open_error_message = "This trace is on a roc-optiq-server:\n\n" + uris.front() +
                               "\n\nThis build cannot connect to servers. Rebuild with "
                               "ROCPROFVIS_ENABLE_CLIENT_SERVER=ON to open it.";
#endif
    }
    return open_result;
}

#ifdef ROCPROFVIS_ENABLE_CLIENT_SERVER
Project::OpenResult
Project::OpenServerTrace(const std::string& project_id, const std::vector<std::string>& uris,
                         std::shared_ptr<IDataChannel> channel, bool is_compute,
                         const std::string& error)
{
    OpenResult                 open_result  = Failed;
    bool                       trace_result = false;
    const bool                 is_compare   = uris.size() > 1;
    std::shared_ptr<RocWidget> view;
    if(m_view)
    {
        spdlog::error("Project already has a trace, cannot open {}", project_id);
    }
    else if(!channel)
    {
        m_open_error_message = error;
    }
#ifndef ROCPROFVIS_ENABLE_TRACE_COMPARE
    else if(is_compare)
    {
        m_open_error_message = "Comparing traces is not available in this build.";
    }
#endif
    else if(is_compute)
    {
        std::shared_ptr<ComputeView> compute_view = std::make_shared<ComputeView>();
        trace_result = compute_view->LoadRemoteTrace(channel, project_id);
        view         = compute_view;
    }
    else
    {
        std::shared_ptr<TraceView> trace_view = std::make_shared<TraceView>();
        trace_result = trace_view->LoadRemoteTrace(channel, project_id);
        view         = trace_view;
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
        if(trace_result && is_compare)
        {
            std::vector<std::string> paths;
            for(const std::string& uri : uris)
            {
                paths.push_back(server_uri_path(uri));
            }
            std::vector<CompareSourceInfo> sources = make_compare_sources(paths);
            trace_view->GetDataProvider()->DataModel().SetCompareSources(sources);
            m_compare_files = uris;
            m_name = "Compare: " + sources[0].name + " vs " + sources[1].name;
        }
#endif
    }

    if(trace_result)
    {
        m_trace_file_path = project_id;
        m_trace_type      = is_compute ? Compute : System;
        m_view            = view;
        if(IsProject())
        {
            m_name = std::filesystem::path(m_project_file_path).filename().string();
        }
        else if(!is_compare)
        {
            m_name = server_trace_label(project_id);
        }
        open_result = Success;
    }
    else if(channel)
    {
        channel->Close();
    }
    return open_result;
}
#endif  // ROCPROFVIS_ENABLE_CLIENT_SERVER

const std::string&
Project::GetOpenErrorMessage() const
{
    return m_open_error_message;
}

#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
Project::OpenResult
Project::OpenCompare(const std::string&              project_id,
                     const std::vector<std::string>& file_paths)
{
    OpenResult result = Failed;
    if(file_paths.size() < 2 || m_view)
    {
        return result;
    }

    for(const std::string& path : file_paths)
    {
        if(!std::filesystem::exists(path))
        {
            AppWindow::GetInstance()->ShowMessageDialog("Error",
                                                        "File does not exist: " + path);
            spdlog::error("Failed to open compare file: {}, file does not exist", path);
            return result;
        }
    }

    std::vector<const char*> file_ptrs;
    file_ptrs.reserve(file_paths.size());
    for(const std::string& path : file_paths)
    {
        file_ptrs.push_back(path.c_str());
    }

    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc_compare(file_ptrs.data(), file_ptrs.size());
    if(controller)
    {
        std::shared_ptr<TraceView> trace_view = std::make_shared<TraceView>();
        if(trace_view->LoadTrace(controller, project_id))
        {
            std::vector<CompareSourceInfo> sources = make_compare_sources(file_paths);
            if(DataProvider* provider = trace_view->GetDataProvider())
            {
                provider->DataModel().SetCompareSources(sources);
            }

            m_trace_file_path = project_id;
            m_compare_files   = file_paths;
            m_trace_type      = System;
            m_view            = trace_view;
            m_name = "Compare: " + sources[0].name + " vs " + sources[1].name;
            result = Success;
        }
        else
        {
            rocprofvis_controller_free(controller);
        }
    }

    if(result == Failed)
    {
        AppWindow::GetInstance()->ShowMessageDialog(
            "Error", "The selected traces could not be opened for comparison.");
    }
    return result;
}
#endif  // ROCPROFVIS_ENABLE_TRACE_COMPARE

bool
Project::JsonValidForLoad(jt::Json& json)
{
    jt::Json& general = json[JSON_KEY_GROUP_GENERAL];
    return general[JSON_KEY_GENERAL_TRACE_PATH].isString() ||
           general[JSON_KEY_GENERAL_COMPARE_FILES].isArray();
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
    bool result                                                       = false;
    m_settings_json                                                   = "";
    m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_VERSION] = PROJECT_VERSION;
    std::filesystem::path project_dir =
        std::filesystem::path(m_project_file_path).parent_path();
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
    if(!m_compare_files.empty())
    {
        // Compare project: persist the source files (relative to the .rpv) so it can be
        // reopened as a combined trace without a separate manifest on disk.
        jt::Json& compare_files =
            m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_COMPARE_FILES];
        for(size_t i = 0; i < m_compare_files.size(); i++)
        {
            compare_files[i] =
                is_server_uri(m_compare_files[i])
                    ? m_compare_files[i]
                    : std::filesystem::proximate(m_compare_files[i], project_dir)
                          .generic_string();
        }
    }
    else
#endif
    if(is_server_uri(m_trace_file_path))
    {
        m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_TRACE_PATH] =
            m_trace_file_path;
    }
    else
    {
        m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_TRACE_PATH] =
            std::filesystem::proximate(m_trace_file_path, project_dir).generic_string();
    }
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

ProjectSetting::ProjectSetting(const std::string project_id)
: m_project(*AppWindow::GetInstance()->GetProject(project_id))
, m_settings_json(m_project.GetSettingsJson())
{
    m_project.RegisterSetting(this);
}

ProjectSetting::~ProjectSetting() {}

}  // namespace View
}  // namespace RocProfVis
