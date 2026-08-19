// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "json.h"
#include <list>
#include <memory>
#include <string>
#include <vector>

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
        Compute,
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
     * Returns the source trace files backing this project. For a single trace this is a
     * one-element list; for a combined/compare project it is all combined files in order
     * (A, B, ...). Empty if nothing has been loaded.
     */
    std::vector<std::string> GetSourceFiles() const;
    /*
     * Records that a trace file was merged into this (already-open) view via the incremental
     * add path, so GetSourceFiles()/Remove reflect it. Updates the tab name. Does not itself
     * load anything.
     */
    void AddSourceFile(const std::string& path);
    /*
     * Removes a trace file that was merged into this view (the in-place Remove path). Updates
     * GetSourceFiles()/the tab name. Keeps the project id stable. Does not unload anything.
     */
    void RemoveSourceFile(const std::string& path);
    /*
     * Returns true if the project is saved as a project (as opposed to a trace).
     */
    bool IsProject() const;
    /*
     * Returns true if this is a compare project (files overlaid with A/B/... tagging), as
     * opposed to a single trace or a merged/combined view. Add/Remove Trace do not apply to
     * compare projects (they would drop the compare tagging), so callers gate on this.
     */
    bool IsCompare() const;

    /*
     * Opens a project or trace file and returns Success/Duplicate/Failed.
     * @param file_path: The path of the file to open. If Duplicate is returned, this will
     * be set to the path of the duplicate which can be used to identify and open the
     * duplicate's tab.
     */
    OpenResult Open(std::string& file_path);
    /*
     * Opens two or more trace files as a single combined compare project. The traces
     * overlay on one timeline and each track is tagged with its source (A, B, ...).
     * @param project_id: Synthetic, stable id/key for the project (it has no single
     * file path on disk).
     * @param file_paths: The trace files to combine, tagged A, B, ... in order.
     */
    OpenResult OpenCompare(const std::string&              project_id,
                           const std::vector<std::string>& file_paths);
    /*
     * Opens one or more trace files merged into a single unified view, like a yaml
     * manifest (parts of the same program's run). Unlike OpenCompare, the sources are not
     * tagged A/B/...; they simply merge into one timeline/topology via the multinode
     * engine.
     * @param project_id: Synthetic, stable id/key for the merged project.
     * @param file_paths: The trace files to merge.
     */
    OpenResult OpenCombined(const std::string&              project_id,
                            const std::vector<std::string>& file_paths);
    /*
     * Serializes the current live view/track settings into the in-memory settings json
     * WITHOUT writing to disk. Call before a graph-view rebuild (Add/Remove Trace) so the
     * recreated tracks restore the user's state instead of falling back to defaults.
     */
    void SerializeSettings();
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
     * Removes a participant from the settings registry. Called from ~ProjectSetting so
     * settings destroyed on a graph-view rebuild do not leave dangling pointers.
     * @param setting: The settings object to remove.
     */
    void UnregisterSetting(ProjectSetting* setting);
    /*
     * Returns the project settings json.
     */
    jt::Json& GetSettingsJson();

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
     * Builds the tab/display name for a merged view from its source files: the first file's
     * stem, then "A + B" for two files or "A +N more" for more. Shared by OpenCombined and
     * AddSourceFile so both paths label the same view identically.
     */
    static std::string MakeCombinedName(const std::vector<std::string>& files);
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
    // Source trace files when this is a compare project (empty otherwise). Persisted to
    // the .rpv so the compare can be reopened.
    std::vector<std::string>   m_compare_files;
    // Source trace files when this is a merged/combined project (empty otherwise).
    // Persisted to the .rpv so the merged view can be reopened.
    std::vector<std::string>   m_combined_files;
    TraceType                  m_trace_type;
    std::shared_ptr<RocWidget> m_view;
    std::list<ProjectSetting*> m_settings;
    jt::Json                   m_settings_json;
    // Specific open-failure message; empty falls back to the generic one.
    std::string                m_open_error_message;
};

constexpr const char* JSON_KEY_GROUP_GENERAL  = "general";
constexpr const char* JSON_KEY_GROUP_TIMELINE = "timeline";

constexpr const char* JSON_KEY_GENERAL_VERSION    = "version";
constexpr const char* JSON_KEY_GENERAL_TRACE_PATH = "trace_path";
constexpr const char* JSON_KEY_GENERAL_COMPARE_FILES = "compare_files";
constexpr const char* JSON_KEY_GENERAL_COMBINED_FILES = "combined_files";

constexpr const char* JSON_KEY_TIMELINE_BOOKMARK         = "bookmarks";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_KEY     = "key";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X = "view_start_ns ";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X = "view_end_ns";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Y       = "y";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Z       = "z";

constexpr const char* JSON_KEY_TIMELINE_TRACK                    = "tracks";
constexpr const char* JSON_KEY_TIMELINE_TRACK_ORDER              = "order";
constexpr const char* JSON_KEY_TIMELINE_TRACK_DISPLAY            = "display";
constexpr const char* JSON_KEY_TIMELINE_TRACK_HEIGHT             = "height";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COMPACT_MODE       = "compact_mode";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR              = "color";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MIN    = "color_min";
constexpr const char* JSON_KEY_TIMELINE_TRACK_COLOR_RANGE_MAX    = "color_max";
constexpr const char* JSON_KEY_TIMELINE_TRACK_BOX_PLOT           = "box_plot";
constexpr const char* JSON_KEY_TIMELINE_TRACK_STRIPES            = "box_plot_stripes";
constexpr const char* JSON_KEY_TIMELINE_TRACK_MIN                = "min";
constexpr const char* JSON_KEY_TIMELINE_TRACK_MAX                = "max";
constexpr const char* JSON_KEY_TIMELINE_TRACK_MEAN               = "mean";
constexpr const char* JSON_KEY_TIMELINE_TRACK_STANDARD_DEVIATION = "standard_deviation";
constexpr const char* JSON_KEY_TIMELINE_TRACK_QUEUE_UTILIZATION  = "queue_utilization";

constexpr const char* JSON_KEY_ANNOTATIONS                 = "annotations";
constexpr const char* JSON_KEY_ANNOTATION_TIME_NS          = "time_ns";
constexpr const char* JSON_KEY_ANNOTATION_Y_OFFSET         = "y_offset";
constexpr const char* JSON_KEY_ANNOTATION_SIZE_X           = "size_x";
constexpr const char* JSON_KEY_ANNOTATION_SIZE_Y           = "size_y";
constexpr const char* JSON_KEY_ANNOTATION_TEXT             = "text";
constexpr const char* JSON_KEY_ANNOTATION_TITLE            = "title";
constexpr const char* JSON_KEY_ANNOTATION_ID               = "id";
constexpr const char* JSON_KEY_ANNOTATION_TRACK_ID         = "track_id";
constexpr const char* JSON_KEY_TIMELINE_ANNOTATION_V_MIN_X = "view_start_ns";
constexpr const char* JSON_KEY_TIMELINE_ANNOTATION_V_MAX_X = "view_end_ns";
constexpr const char* JSON_KEY_ANNOTATION_IS_MINIMIZED     = "is_minimized";
constexpr const char* JSON_KEY_ANNOTATION_IS_LOCKED        = "is_locked";

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
