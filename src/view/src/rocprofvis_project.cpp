// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_project.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_presets.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "compute/rocprofvis_compute_view.h"
#include "widgets/rocprofvis_notification_manager.h"
#include <algorithm>
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

Project::~Project()
{
    // Destroy the view (which owns all ProjectSetting instances) while m_settings is still a
    // live list, so their destructors' UnregisterSetting() calls stay valid. Member
    // destruction order would otherwise free m_settings before m_view (a use-after-free).
    m_view.reset();
    m_settings.clear();
}

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

std::vector<std::string>
Project::GetSourceFiles() const
{
    if(!m_combined_files.empty())
    {
        return m_combined_files;
    }
    if(!m_compare_files.empty())
    {
        return m_compare_files;
    }
    if(!m_trace_file_path.empty())
    {
        return { m_trace_file_path };
    }
    return {};
}

std::string
Project::MakeCombinedName(const std::vector<std::string>& files)
{
    if(files.empty())
    {
        return {};
    }
    std::string first = std::filesystem::path(files.front()).stem().string();
    if(files.size() == 1)
    {
        return first;
    }
    if(files.size() == 2)
    {
        return first + " + " + std::filesystem::path(files[1]).stem().string();
    }
    return first + " +" + std::to_string(files.size() - 1) + " more";
}

void
Project::AddSourceFile(const std::string& path)
{
    if(m_combined_files.empty())
    {
        // Was a single-file (or compare) view; seed the combined list with its current
        // sources so subsequent add/remove operate on the full merged set.
        m_combined_files = GetSourceFiles();
    }
    if(std::find(m_combined_files.begin(), m_combined_files.end(), path) ==
       m_combined_files.end())
    {
        m_combined_files.push_back(path);
    }

    if(!m_combined_files.empty())
    {
        m_name = MakeCombinedName(m_combined_files);
        AppWindow::GetInstance()->SetTabLabel(m_name, GetID());
    }
}

void
Project::RemoveSourceFile(const std::string& path)
{
    if(m_combined_files.empty())
    {
        // Seed from the current sources so the remove operates on the full merged set.
        m_combined_files = GetSourceFiles();
    }
    m_combined_files.erase(
        std::remove(m_combined_files.begin(), m_combined_files.end(), path),
        m_combined_files.end());
    // Down to one file: relabel as that single trace (the id/key stays stable, and the
    // Remove menu auto-disables at <2 sources, so it behaves like a single-trace view).
    m_name = MakeCombinedName(GetSourceFiles());
    AppWindow::GetInstance()->SetTabLabel(m_name, GetID());
}

bool
Project::IsProject() const
{
    return !m_project_file_path.empty();
}

bool
Project::IsCompare() const
{
    return !m_compare_files.empty();
}

Project::OpenResult
Project::Open(std::string& file_path)
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
                std::vector<std::string> files;
                for(jt::Json& entry : general[JSON_KEY_GENERAL_COMPARE_FILES].getArray())
                {
                    files.push_back(
                        std::filesystem::weakly_canonical(
                            project_dir / std::filesystem::path(entry.getString()))
                            .string());
                }
                std::string compare_id = AppWindow::MakeCompareId(files);
                if(AppWindow::GetInstance()->GetProject(compare_id))
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
            else if(general[JSON_KEY_GENERAL_COMBINED_FILES].isArray())
            {
                std::vector<std::string> files;
                for(jt::Json& entry : general[JSON_KEY_GENERAL_COMBINED_FILES].getArray())
                {
                    files.push_back(
                        std::filesystem::weakly_canonical(
                            project_dir / std::filesystem::path(entry.getString()))
                            .string());
                }
                std::string combined_id = AppWindow::MakeCombinedId(files);
                if(AppWindow::GetInstance()->GetProject(combined_id))
                {
                    file_path = combined_id;
                    result    = Duplicate;
                    NotificationManager::GetInstance().Show(
                        "This merged view is already open.", NotificationLevel::Warning);
                }
                else
                {
                    result = OpenCombined(combined_id, files);
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
    if(!duplicate)
    {
        // Also treat the file as open when it is a source of a merged view under another id.
        duplicate = AppWindow::GetInstance()->FindProjectContainingSource(file_path);
    }
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
                // Compare wants counterpart tracks interleaved (A next to B, ...).
                provider->DataModel().SetReorderCounterparts(true);
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

Project::OpenResult
Project::OpenCombined(const std::string&              project_id,
                      const std::vector<std::string>& file_paths)
{
    OpenResult result = Failed;
    if(file_paths.empty() || m_view)
    {
        return result;
    }

    for(const std::string& path : file_paths)
    {
        if(!std::filesystem::exists(path))
        {
            AppWindow::GetInstance()->ShowMessageDialog("Error",
                                                        "File does not exist: " + path);
            spdlog::error("Failed to open trace file: {}, file does not exist", path);
            return result;
        }
    }

    std::vector<const char*> file_ptrs;
    file_ptrs.reserve(file_paths.size());
    for(const std::string& path : file_paths)
    {
        file_ptrs.push_back(path.c_str());
    }

    // The combine goes through the same multinode engine a .yaml manifest uses. Unlike
    // compare, sources are NOT interleaved (they merge into one topology/timeline). Per-track
    // source-file disambiguation is handled uniformly in the view from each track's source
    // path (kRPVControllerTrackSourceFilePath), so it also covers in-place add/remove - no
    // A/B tagging is needed here.
    rocprofvis_controller_t* controller =
        rocprofvis_controller_alloc_compare(file_ptrs.data(), file_ptrs.size());
    if(controller)
    {
        std::shared_ptr<TraceView> trace_view = std::make_shared<TraceView>();
        if(trace_view->LoadTrace(controller, project_id))
        {
            m_trace_file_path = project_id;
            m_combined_files  = file_paths;
            m_trace_type      = System;
            m_view            = trace_view;

            m_name = MakeCombinedName(file_paths);
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
            "Error", "The selected traces could not be opened together.");
    }
    return result;
}

bool
Project::JsonValidForLoad(jt::Json& json)
{
    jt::Json& general = json[JSON_KEY_GROUP_GENERAL];
    return general[JSON_KEY_GENERAL_TRACE_PATH].isString() ||
           general[JSON_KEY_GENERAL_COMPARE_FILES].isArray() ||
           general[JSON_KEY_GENERAL_COMBINED_FILES].isArray();
}

void
Project::RegisterSetting(ProjectSetting* setting)
{
    m_settings.push_back(setting);
}

void
Project::UnregisterSetting(ProjectSetting* setting)
{
    m_settings.remove(setting);
}

void
Project::SerializeSettings()
{
    // Snapshot every setting's live state into the in-memory json (like SaveSetttingsJson,
    // but no file write) so a graph-view rebuild can restore the user's track settings.
    for(ProjectSetting* setting : m_settings)
    {
        setting->ToJson();
    }
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
    if(!m_compare_files.empty())
    {
        // Compare project: persist the source files (relative to the .rpv) so it can be
        // reopened as a combined trace without a separate manifest on disk.
        jt::Json& compare_files =
            m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_COMPARE_FILES];
        for(size_t i = 0; i < m_compare_files.size(); i++)
        {
            compare_files[i] =
                std::filesystem::proximate(m_compare_files[i], project_dir).generic_string();
        }
    }
    else if(!m_combined_files.empty())
    {
        // Merged/combined project: persist the source files (relative to the .rpv) so the
        // merged view can be reopened without a separate manifest on disk.
        jt::Json& combined_files =
            m_settings_json[JSON_KEY_GROUP_GENERAL][JSON_KEY_GENERAL_COMBINED_FILES];
        for(size_t i = 0; i < m_combined_files.size(); i++)
        {
            combined_files[i] =
                std::filesystem::proximate(m_combined_files[i], project_dir).generic_string();
        }
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

ProjectSetting::~ProjectSetting()
{
    // Unregister so a rebuilt graph view does not leave a dangling pointer in the registry.
    m_project.UnregisterSetting(this);
}

}  // namespace View
}  // namespace RocProfVis
