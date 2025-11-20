// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "json.h"
#include <list>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class RocWidget;
class ProjectSetting;

class Project
{
public:
    enum OpenResult
    {
        Success,
        Duplicate,
        Failed,
    };

    enum TraceType
    {
        Undefined,
        System,
#ifdef COMPUTE_UI_SUPPORT
        Compute,
#endif
    };

    Project();
    virtual ~Project();

    /*
     * Returns the ID of the project.
     * (Internally this is the trace path)
     */
    std::string GetID() const;
    /*
     * Returns the file name of the project/trace.
     */
    std::string GetName() const;
    /*
     * Returns the RocWidget that renders the project.
     */
    std::shared_ptr<RocWidget> GetView();
    /*
     * Returns the trace type of the project.
     */
    TraceType GetTraceType() const;
    /*
     * Returns true if the project is saved as a project (as opposed to a trace).
     */
    bool IsProject() const;

    /*
     * Opens a project or trace file and returns Success/Duplicate/Failed.
     * @param file_path: The path of the file to open. If Duplicate is returned, this will
     * be set to the path of the duplicate which can be used to identify and open the
     * duplicate's tab.
     */
    OpenResult Open(std::string& file_path);
    /*
     * Overwrites the project settings to the project file without further user input.
     */
    void Save();
    /*
     * Opens file dialog and saves the project settings to a specified file.
     * @param file_path: The path of the file to save as.
     */
    void SaveAs(const std::string& file_path);
    /*
     * Clean up tasks prior to being deleted.
     */
    void Close();
    /*
     * Adds a participant to the project settings serialize/deserialize process.
     * @param setting: The settings object to include in the serialize/deserialize
     * process.
     */
    void RegisterSetting(ProjectSetting* setting);
    /*
     * Returns the project settings json.
     */
    jt::Json& GetSettingsJson();

    // System trace specific stuff...(could subclass to SystemsProject if there gets to be
    // many)
    bool IsTrimSaveAllowed();
    void TrimSave(const std::string& file_path_str);

private:
    /*
     * Opens a project + attached trace file and returns Success/Duplicate/Failed.
     * @param file_path: The path of the file to open. If Duplicate is returned, this will
     * be set to the path of the duplicate which can be used to identify and open the
     * duplicate's tab.
     */
    OpenResult OpenProject(std::string& file_path);
    /*
     * Opens a trace file and returns Success/Duplicate/Failed.
     * @param file_path: The path of the file to open. If Duplicate is returned, this will
     * be set to the path of the duplicate which can be used to identify and open the
     * duplicate's tab.
     */
    OpenResult OpenTrace(std::string& file_path);
    /*
     * Performs basic validation on the project settings for fields required to open a
     * trace.
     */
    bool JsonValidForLoad(jt::Json& json);
    /*
     * Writes the project settings into m_project_file_path;
     */
    bool SaveSetttingsJson();

    std::string                m_name;
    std::string                m_project_file_path;
    std::string                m_trace_file_path;
    TraceType                  m_trace_type;
    std::shared_ptr<RocWidget> m_view;
    std::list<ProjectSetting*> m_settings;
    jt::Json                   m_settings_json;
};

constexpr const char* JSON_KEY_GROUP_GENERAL  = "general";
constexpr const char* JSON_KEY_GROUP_TIMELINE = "timeline";

constexpr const char* JSON_KEY_GENERAL_VERSION    = "version";
constexpr const char* JSON_KEY_GENERAL_TRACE_PATH = "trace_path";

constexpr const char* JSON_KEY_TIMELINE_BOOKMARK         = "bookmarks";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_KEY     = "key";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X = "view_start_ns ";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X = "view_end_ns";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Y       = "y";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Z       = "z";

constexpr const char* JSON_KEY_TIMELINE_TRACK                 = "tracks";
constexpr const char* JSON_KEY_TIMELINE_TRACK_ORDER           = "order";
constexpr const char* JSON_KEY_TIMELINE_TRACK_DISPLAY         = "display";
constexpr const char* JSON_KEY_TIMELINE_TRACK_HEIGHT          = "height";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COMPACT_MODE    = "compact_mode";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR           = "color";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN = "color_min";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX = "color_max";
constexpr const char* JSON_KEY_TIMELINE_TRACK_BOX_PLOT        = "box_plot";

constexpr const char* JSON_KEY_ANNOTATIONS                 = "annotations";
constexpr const char* JSON_KEY_ANNOTATION_TIME_NS          = "time_ns";
constexpr const char* JSON_KEY_ANNOTATION_Y_OFFSET         = "y_offset";
constexpr const char* JSON_KEY_ANNOTATION_SIZE_X           = "size_x";
constexpr const char* JSON_KEY_ANNOTATION_SIZE_Y           = "size_y";
constexpr const char* JSON_KEY_ANNOTATION_TEXT             = "text";
constexpr const char* JSON_KEY_ANNOTATION_TITLE            = "title";
constexpr const char* JSON_KEY_ANNOTATION_ID               = "id";
constexpr const char* JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X = "view_start_ns";
constexpr const char* JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X = "view_end_ns";

class ProjectSetting
{
public:
    ProjectSetting(const std::string project_id);
    virtual ~ProjectSetting();
    /*
     * Called by the owning project during serialization. Implementation should update its
     * project settings into m_settings_json.
     */
    virtual void ToJson() = 0;
    /*
     * Implementation should validate any fields it cares about before reading.
     */
    virtual bool Valid() const = 0;

protected:
    Project&  m_project;
    jt::Json& m_settings_json;
};

}  // namespace View
}  // namespace RocProfVis
