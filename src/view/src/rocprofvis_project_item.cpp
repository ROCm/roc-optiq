// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_project_item.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_presets.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "compute/rocprofvis_compute_view.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <fstream>

constexpr const char* PROJECT_VERSION = "1.0";

namespace RocProfVis
{
namespace View
{

ProjectItem::ProjectItem()
: m_view(nullptr)
, m_trace_type(Undefined)
{}

ProjectItem::~ProjectItem() {}

std::string
ProjectItem::GetID() const
{
    return m_trace_file_path;
}

std::string
ProjectItem::GetName() const
{
    return m_name;
}

std::shared_ptr<RocWidget>
ProjectItem::GetView()
{
    return m_view;
}

ProjectItem::TraceType
ProjectItem::GetTraceType() const
{
    return m_trace_type;
}

bool
ProjectItem::IsSaved() const
{
    return !m_project_file_path.empty();
}

std::vector<std::string>
ProjectItem::GetFiles() const
{
    if(!m_compare_files.empty())
    {
        return m_compare_files;
    }
    return std::vector<std::string>{ m_trace_file_path };
}

ProjectItem::OpenResult
ProjectItem::Open(std::string& file_path)
{
    OpenResult result = Failed;
    m_open_error_message.clear();
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
ProjectItem::Save()
{
    if(IsSaved() && SaveSetttingsJson())
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
ProjectItem::SaveAs(const std::string& file_path)
{
    m_project_file_path = file_path;
    m_name              = std::filesystem::path(m_project_file_path).filename().string();
    AppWindow::GetInstance()->SetTabLabel(GetName(), GetID());
    Save();
}

void
ProjectItem::Close()
{
    PresetManager::GetInstance().UnregisterComponents(m_trace_file_path);
}

ProjectItem::OpenResult
ProjectItem::OpenProject(std::string& file_path)
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
                std::vector<std::string> files;
                for(jt::Json& entry : general[JSON_KEY_GENERAL_COMPARE_FILES].getArray())
                {
                    files.push_back(
                        std::filesystem::weakly_canonical(
                            project_dir / std::filesystem::path(entry.getString()))
                            .string());
                }
                std::string compare_id = AppWindow::MakeCompareId(files);
                if(AppWindow::GetInstance()->GetItem(compare_id))
                {
                    file_path = compare_id;
                    result    = Duplicate;
                    NotificationManager::GetInstance().Show(
                        "This comparison is already open.", NotificationLevel::Warning);
                }
                else
                {
                    result = OpenCompare(compare_id, files);
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

ProjectItem::OpenResult
ProjectItem::OpenTrace(std::string& file_path)
{
    OpenResult open_result = Failed;
    // canonicalize so a .db and the path stored in a .rpv resolve to the same trace
    file_path = std::filesystem::weakly_canonical(file_path).string();
    // trace already open, return duplicate so we switch tabs instead of loading it twice
    ProjectItem* duplicate = AppWindow::GetInstance()->GetItem(file_path);
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
            m_name            = std::filesystem::path(IsSaved() ? m_project_file_path
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

ProjectItem::OpenResult
ProjectItem::OpenCompare(const std::string&              project_id,
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
            // Tag each source A, B, ... in selection order so the timeline and sidebar
            // badges can resolve a track's instance index back to its file.
            std::vector<CompareSourceInfo> sources;
            sources.reserve(file_paths.size());
            for(size_t i = 0; i < file_paths.size(); i++)
            {
                CompareSourceInfo info;
                info.id   = std::string(1, static_cast<char>('A' + i));
                info.name = std::filesystem::path(file_paths[i]).stem().string();
                info.path = file_paths[i];
                sources.push_back(std::move(info));
            }
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

bool
ProjectItem::JsonValidForLoad(jt::Json& json)
{
    jt::Json& general = json[JSON_KEY_GROUP_GENERAL];
    return general[JSON_KEY_GENERAL_TRACE_PATH].isString() ||
           general[JSON_KEY_GENERAL_COMPARE_FILES].isArray();
}

void
ProjectItem::RegisterSetting(ProjectItemSetting* setting)
{
    m_settings.push_back(setting);
}

jt::Json&
ProjectItem::GetSettingsJson()
{
    return m_settings_json;
}

jt::Json
ProjectItem::ExportSettingsJson(const std::filesystem::path& base_dir)
{
    m_settings_json                                                  = "";
    m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_VERSION] = PROJECT_VERSION;
    if(!m_compare_files.empty())
    {
        // Compare item: persist the source files (relative to base_dir) so it can be
        // reopened as a combined trace without a separate manifest on disk.
        jt::Json& compare_files =
            m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_COMPARE_FILES];
        for(size_t i = 0; i < m_compare_files.size(); i++)
        {
            compare_files[i] =
                std::filesystem::proximate(m_compare_files[i], base_dir).generic_string();
        }
    }
    else
    {
        m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_TRACE_PATH] =
            std::filesystem::proximate(m_trace_file_path, base_dir).generic_string();
    }
    // Each registered per-view setting writes its current state (track heights/order,
    // bookmarks, annotations, ...) into m_settings_json.
    for(ProjectItemSetting* setting : m_settings)
    {
        setting->ToJson();
    }
    return m_settings_json;
}

ProjectItem::OpenResult
ProjectItem::OpenFromSettingsJson(const jt::Json& settings, const std::filesystem::path& base_dir,
                          std::string& out_id)
{
    OpenResult result = Failed;
    m_open_error_message.clear();
    // Inject the settings before the view is built so the per-view settings objects
    // restore from it (same restore path as opening a single-item .rpv).
    m_settings_json = settings;
    if(!JsonValidForLoad(m_settings_json))
    {
        return Failed;
    }
    jt::Json& general = m_settings_json[JSON_KEY_GROUP_GENERAL];
    if(general[JSON_KEY_GENERAL_COMPARE_FILES].isArray())
    {
        std::vector<std::string> files;
        for(jt::Json& entry : general[JSON_KEY_GENERAL_COMPARE_FILES].getArray())
        {
            files.push_back(std::filesystem::weakly_canonical(
                                base_dir / std::filesystem::path(entry.getString()))
                                .string());
        }
        std::string compare_id = AppWindow::MakeCompareId(files);
        if(AppWindow::GetInstance()->GetItem(compare_id))
        {
            out_id = compare_id;
            result = Duplicate;
        }
        else
        {
            result = OpenCompare(compare_id, files);
            if(result == Success)
            {
                out_id = GetID();
            }
        }
    }
    else
    {
        std::string trace_path =
            std::filesystem::weakly_canonical(
                base_dir /
                std::filesystem::path(general[JSON_KEY_GENERAL_TRACE_PATH].getString()))
                .string();
        if(std::filesystem::exists(trace_path))
        {
            std::string resolved = trace_path;
            result               = OpenTrace(resolved);
            out_id               = resolved;
        }
    }
    return result;
}

bool
ProjectItem::SaveSetttingsJson()
{
    bool     result = false;
    jt::Json json =
        ExportSettingsJson(std::filesystem::path(m_project_file_path).parent_path());
    if(!json.isNull())
    {
        std::ofstream file(m_project_file_path);
        if(file.is_open())
        {
            file << json.toStringPretty() << "\n";
            file.close();
            result = true;
        }
    }
    return result;
}

ProjectItemSetting::ProjectItemSetting(const std::string project_id)
: m_item(*AppWindow::GetInstance()->GetItem(project_id))
, m_settings_json(m_item.GetSettingsJson())
{
    m_item.RegisterSetting(this);
}

ProjectItemSetting::~ProjectItemSetting() {}

}  // namespace View
}  // namespace RocProfVis
