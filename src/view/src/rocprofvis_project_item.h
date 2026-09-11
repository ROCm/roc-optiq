// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "json.h"
#include <filesystem>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class RocWidget;
class ProjectItemSetting;

// An ProjectItem is a single opened tab: one trace / compute analysis (or a compare
// set) plus its per-tab view configuration. A collection of Items grouped
// together is a Project (see rocprofvis_project.h).
class ProjectItem
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

    ProjectItem();
    virtual ~ProjectItem();

    /*
     * Returns the ID of the item.
     * (Internally this is the trace path)
     */
    std::string GetID() const;
    /*
     * Returns the file name of the item/trace.
     */
    std::string GetName() const;
    /*
     * Returns the RocWidget that renders the item.
     */
    std::shared_ptr<RocWidget> GetView();
    /*
     * Returns the trace type of the item.
     */
    TraceType GetTraceType() const;
    /*
     * Returns true if the item is saved to a project/item file (as opposed to a
     * plain trace).
     */
    bool IsSaved() const;
    /*
     * Returns the filelist that reconstructs this item: the single trace/compute
     * path, or the compare source paths for a compare item. Used to remember a
     * closed item so it can be reopened.
     */
    std::vector<std::string> GetFiles() const;
    /*
     * Builds and returns this item's full settings JSON (general trace/compare files
     * plus the per-view sections: track order/heights/colors, bookmarks, annotations)
     * with trace paths relative to base_dir. Used to embed the item, with its
     * settings, inside a saved project group.
     */
    jt::Json ExportSettingsJson(const std::filesystem::path& base_dir);
    /*
     * Opens this item from an in-memory settings JSON (as produced by
     * ExportSettingsJson), resolving trace paths relative to base_dir and restoring
     * the per-view settings. out_id receives the opened item's id (or the existing id
     * on Duplicate). Used to reopen items that were embedded in a project group.
     */
    OpenResult OpenFromSettingsJson(const jt::Json&              settings,
                                    const std::filesystem::path& base_dir,
                                    std::string&                 out_id);

    /*
     * Opens a project or trace file and returns Success/Duplicate/Failed.
     * @param file_path: The path of the file to open. If Duplicate is returned, this will
     * be set to the path of the duplicate which can be used to identify and open the
     * duplicate's tab.
     */
    OpenResult Open(std::string& file_path);
    /*
     * Opens two or more trace files as a single combined compare item. The traces
     * overlay on one timeline and each track is tagged with its source (A, B, ...).
     * @param item_id: Synthetic, stable id/key for the item (it has no single
     * file path on disk).
     * @param file_paths: The trace files to combine, tagged A, B, ... in order.
     */
    OpenResult OpenCompare(const std::string&              item_id,
                           const std::vector<std::string>& file_paths);
    /*
     * Overwrites the item settings to the item file without further user input.
     */
    void Save();
    /*
     * Opens file dialog and saves the item settings to a specified file.
     * @param file_path: The path of the file to save as.
     */
    void SaveAs(const std::string& file_path);
    /*
     * Clean up tasks prior to being deleted.
     */
    void Close();
    /*
     * Adds a participant to the item settings serialize/deserialize process.
     * @param setting: The settings object to include in the serialize/deserialize
     * process.
     */
    void RegisterSetting(ProjectItemSetting* setting);
    /*
     * Returns the item settings json.
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
     * Performs basic validation on the item settings for fields required to open a
     * trace.
     */
    bool JsonValidForLoad(jt::Json& json);
    /*
     * Writes the item settings into m_project_file_path;
     */
    bool SaveSetttingsJson();

    std::string                m_name;
    std::string                m_project_file_path;
    std::string                m_trace_file_path;
    // Source trace files when this is a compare item (empty otherwise). Persisted to
    // the .rpv so the compare can be reopened.
    std::vector<std::string>   m_compare_files;
    TraceType                  m_trace_type;
    std::shared_ptr<RocWidget> m_view;
    std::list<ProjectItemSetting*>    m_settings;
    jt::Json                   m_settings_json;
    // Specific open-failure message; empty falls back to the generic one.
    std::string                m_open_error_message;
};

constexpr const char* JSON_KEY_GROUP_GENERAL  = "general";
constexpr const char* JSON_KEY_GROUP_TIMELINE = "timeline";

constexpr const char* JSON_KEY_GENERAL_VERSION    = "version";
constexpr const char* JSON_KEY_GENERAL_TRACE_PATH = "trace_path";
constexpr const char* JSON_KEY_GENERAL_COMPARE_FILES = "compare_files";

constexpr const char* JSON_KEY_TIMELINE_BOOKMARK         = "bookmarks";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_KEY     = "key";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X = "view_start_ns ";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X = "view_end_ns";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Y       = "y";
constexpr const char* JSON_KEY_TIMELINE_BOOKMARK_Z       = "z";

constexpr const char* JSON_KEY_TIMELINE_TRACK                    = "tracks";
constexpr const char* JSON_KEY_TIMELINE_TRACK_ORDER              = "order";
constexpr const char* JSON_KEY_TIMELINE_SORT_MODE                = "sort_mode";
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

class ProjectItemSetting
{
public:
    ProjectItemSetting(const std::string item_id);
    virtual ~ProjectItemSetting();
    /*
     * Called by the owning item during serialization. Implementation should update its
     * item settings into m_settings_json.
     */
    virtual void ToJson() = 0;
    /*
     * Implementation should validate any fields it cares about before reading.
     */
    virtual bool Valid() const = 0;

protected:
    ProjectItem&     m_item;
    jt::Json& m_settings_json;
};

}  // namespace View
}  // namespace RocProfVis
