// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_appwindow.h"
#include "imgui.h"
#include "implot.h"
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
#    include "nfd.h"
#endif
#include "ImGuiFileDialog.h"

#include "rocprofvis_appmonitor.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_events.h"
#include "rocprofvis_project_item.h"
#include "rocprofvis_project.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_render_scheduler.h"
#include "rocprofvis_settings_panel.h"
#include "rocprofvis_version.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_log_viewer.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_notification_manager.h"
// TEMPORARY (profiler launch): remove guard when the profiler feature graduates.
#ifdef ROCPROFVIS_ENABLE_PROFILER
#include "rocprofvis_profiler_launcher_dialog.h"
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "remote/rocprofvis_ssh_auth_modal.h"
#include "remote/rocprofvis_ssh_session.h"
#endif
#include "welcome/rocprofvis_welcome_page.h"
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>

namespace RocProfVis
{
namespace View
{

constexpr ImVec2      FILE_DIALOG_SIZE       = ImVec2(480.0f, 360.0f);
constexpr const char* FILE_DIALOG_NAME       = "ChooseFileDlgKey";
constexpr const char* TAB_CONTAINER_SRC_NAME = "MainTabContainer";
constexpr const char* ABOUT_DIALOG_NAME      = "About##_dialog";
constexpr const char* APP_SHUTDOWN_NOTIFICATION_ID = "provider_cleanup_app_shutdown";
constexpr const char* SHUTDOWN_DIALOG_NAME = "Closing Traces##_shutdown";

#ifdef ROCPROFVIS_PERFETTO_ENABLED
const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml", "json", "proto", "pftrace"};
#else
const std::vector<std::string> TRACE_EXTENSIONS   = { "db", "rpd", "yaml"};
#endif
const std::vector<std::string> PROJECT_EXTENSIONS = { "rpv" };
#ifdef ROCPROFVIS_PERFETTO_ENABLED
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv", "json", "proto", "pftrace" };
#else
const std::vector<std::string> ALL_EXTENSIONS     = { "db", "rpd", "yaml", "rpv" };
#endif
const std::vector<std::string> COMPARE_EXTENSIONS = { "db" };

// A .rpv project file can hold either a single saved item or a whole tab group
// (name, color, member filelists). Group saves reuse the .rpv extension; the
// loader disambiguates by inspecting the JSON (a group has a top-level "items").
const std::vector<std::string> PROJECT_GROUP_EXTENSIONS = { "rpv" };
constexpr const char*          PROJECT_GROUP_EXTENSION  = ".rpv";

// Session snapshot file, in the app config dir. Not a .rpv, to keep it out of the
// user-facing project space.
constexpr const char* SESSION_FILE_NAME = "last_session.json";

constexpr const char* CLEANUP_MESSAGE = "Waiting for requests to finish cleanup...";
constexpr const char* CLOSING_MESSAGE = "Closing...";

// Upper bound on how long the shutdown exit gate waits for AppMonitor
// operations to drain before exiting anyway. AppMonitor's destructor then runs
// a final bounded, cancelling drain as the backstop so no worker is left
// holding freed resources.
constexpr auto MONITOR_SHUTDOWN_GRACE_PERIOD = std::chrono::seconds(5);

// For testing DataProvider
void
RenderProviderTest(DataProvider& provider);

AppWindow* AppWindow::s_instance = nullptr;

AppWindow*
AppWindow::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new AppWindow();
    }

    return s_instance;
}

void
AppWindow::DestroyInstance()
{
    if(s_instance)
    {
        delete s_instance;
        s_instance = nullptr;
    }
}

AppWindow::AppWindow()
: m_main_view(nullptr)
, m_settings_panel(nullptr)
, m_tab_container(nullptr)
, m_default_padding(0.0f, 0.0f)
, m_default_spacing(0.0f, 0.0f)
, m_open_about_dialog(false)
, m_tabclosed_event_token(EventManager::InvalidSubscriptionToken)
, m_tabselected_event_token(EventManager::InvalidSubscriptionToken)
, m_font_changed_token(EventManager::InvalidSubscriptionToken)
#ifdef ROCPROFVIS_DEVELOPER_MODE
, m_show_debug_window(false)
, m_show_provider_test_widow(false)
, m_show_metrics(false)
#endif
, m_confirmation_dialog(std::make_unique<ConfirmationDialog>(
      SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit))
, m_message_dialog(std::make_unique<MessageDialog>())
, m_compare_files_dialog(std::make_unique<CompareFilesDialog>(
      [this](CompareFilesDialog::FileSlot slot) { HandleCompareFileBrowse(slot); },
      [this](const std::string& first, const std::string& second) {
          OpenCompare(first, second);
      }))
, m_tool_bar_index(0)
, m_is_fullscreen(false)
, m_file_dialog_preference(kRocProfVisViewFileDialog_Auto)
, m_use_native_file_dialog(false)
, m_init_file_dialog(false)
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
, m_is_native_file_dialog_open(false)
#endif
, m_disable_app_interaction(false)
, m_shutdown_requested(false)
, m_exit_notification_sent(false)
, m_restore_fullscreen_later(false)
, m_next_provider_cleanup_id(0)
, m_status_show_busy_indicator(false)
{}

AppWindow::~AppWindow()
{
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabClosed),
                                             m_tabclosed_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kFontSizeChanged),
                                             m_font_changed_token);

    for(auto& job : m_provider_cleanup_jobs)
    {
        if(job.future.valid())
        {
            job.future.get();
        }
    }
    m_provider_cleanup_jobs.clear();
    m_items.clear();
    // Destroy owners of monitored sessions (e.g. the profiler dialog and the
    // remote-trace orchestrator) before tearing down the monitor so they
    // unregister cleanly instead of lazily re-creating the singleton during
    // their own destruction.
#ifdef ROCPROFVIS_ENABLE_PROFILER
    m_profiler_launcher_dialog.reset();
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_ssh_test_dialog.reset();
#endif
    AppMonitor::DestroyInstance();

    LogViewer::DestroyInstance();
}

bool
AppWindow::Init()
{
    std::string config_path = get_application_config_path(true);

    std::filesystem::path ini_path     = std::filesystem::path(config_path) / "imgui.ini";
    ImGuiIO&              io           = ImGui::GetIO();
    static std::string    ini_path_str = ini_path.string();
    io.IniFilename                     = ini_path_str.c_str();

    ImPlot::CreateContext();

    SettingsManager& settings = SettingsManager::GetInstance();
    bool             result   = settings.Init();
    if(result)
    {
        m_settings_panel = std::make_unique<SettingsPanel>(settings);
    }
    else
    {
        spdlog::warn("Failed to initialize SettingsManager");
    }

    m_welcome_page = std::make_unique<WelcomePage>(
        [this]() { HandleOpenFile(); },
        [this](const std::string& file_path) { OpenFile(file_path); });

    constexpr float initial_status_bar_height = 30.0f;
    LayoutItem status_bar_item(-1, initial_status_bar_height);
    status_bar_item.m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderStatusBar(); });
    LayoutItem main_area_item(-1, -initial_status_bar_height);
    LayoutItem tool_bar_item(-1, 0);
    tool_bar_item.m_child_flags = ImGuiChildFlags_AutoResizeY;

    m_tab_container = std::make_shared<TabContainer>();
    m_tab_container->SetEventSourceName(TAB_CONTAINER_SRC_NAME);
    m_tab_container->EnableSendCloseEvent(true);
    m_tab_container->EnableSendChangeEvent(true);
    m_tab_container->SetTabContextMenuCallback(
        [this](const std::string& item_id) { RenderTabGroupContextMenu(item_id); });
    m_tab_container->SetChipContextMenuCallback(
        [this](const std::string& group_id) { RenderProjectChipContextMenu(group_id); });
    m_tab_container->SetTabsReorderedCallback([this]() { SyncProjectOrderToTabs(); });

    main_area_item.m_item = std::make_shared<RocCustomWidget>([this]() {
        if(m_shutdown_requested)
        {
            RenderShutdownState();
        }
        else if(m_tab_container && !m_tab_container->GetTabs().empty())
        {
            m_tab_container->Render();
        }
        else
        {
            m_welcome_page->Render();
        }
    });

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(tool_bar_item);
    m_tool_bar_index = static_cast<int>(layout_items.size() - 1);
    layout_items.push_back(main_area_item);
    layout_items.push_back(status_bar_item);
    m_main_view = std::make_shared<VFixedContainer>(layout_items);

    m_default_padding = ImGui::GetStyle().WindowPadding;
    m_default_spacing = ImGui::GetStyle().ItemSpacing;

    auto new_tab_closed_handler = [this](std::shared_ptr<RocEvent> e) {
        HandleTabClosed(e);
    };
    m_tabclosed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabClosed), new_tab_closed_handler);

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        HandleTabSelectionChanged(e);
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    auto font_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        (void) e;
        HandleFontChanged();
    };

    m_font_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kFontSizeChanged), font_changed_handler);

    ConfigureFileDialogBackend();
    HandleFontChanged();
    return result;
}

void
AppWindow::ConfigureFileDialogBackend()
{
    bool want_native = false;
    switch(m_file_dialog_preference)
    {
        case kRocProfVisViewFileDialog_ImGui:
            want_native = false;
            break;
        case kRocProfVisViewFileDialog_Native:
            want_native = true;
            break;
        case kRocProfVisViewFileDialog_Auto:
        default:
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
            want_native = !is_remote_display_session();
#else
            want_native = false;
#endif
            break;
    }

#ifndef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(want_native)
    {
        spdlog::warn("--file-dialog=native requested but native dialog was "
                     "not compiled in; using ImGui.");
        want_native = false;
    }
#else
    if(want_native)
    {
        nfdresult_t nfd_result = NFD_Init();
        if(nfd_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::warn("NFD_Init failed ({}); falling back to in-process "
                         "ImGui file dialog.",
                         err ? err : "unknown");
            NFD_ClearError();
            want_native = false;
        }
        else
        {
            NFD_Quit();
        }
    }
#endif

    m_use_native_file_dialog.store(want_native);
    spdlog::info("File dialog backend: {}",
                 want_native ? "system file dialog" : "in-process ImGuiFileDialog");
}

void
AppWindow::SetFileDialogPreference(rocprofvis_view_file_dialog_preference_t pref)
{
    m_file_dialog_preference = pref;
}

void
AppWindow::SetNotificationCallback(std::function<void(int)> callback)
{
    m_notification_callback = std::move(callback);
}

const std::string&
AppWindow::GetMainTabSourceName() const
{
    return m_tab_container->GetEventSourceName();
}

void
AppWindow::SetTabLabel(const std::string& label, const std::string& id)
{
    m_tab_container->SetTabLabel(label, id);
}

void
AppWindow::ShowCloseConfirm()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    if(m_tab_container->GetTabs().size() == 0 ||
       SettingsManager::GetInstance().GetUserSettings().dont_ask_before_exit)
    {
        BeginAppShutdown();
        return;
    }

    // Only show the dialog if there are open tabs
    ShowConfirmationDialog(
        "Confirm Close",
        "Are you sure you want to close the application? Any "
        "unsaved data will be lost.",
        [this]() { BeginAppShutdown(); });
}

void
AppWindow::SetFullscreenState(bool is_fullscreen)
{
    m_is_fullscreen = is_fullscreen;
}

bool
AppWindow::GetFullscreenState() const
{
    return m_is_fullscreen;
}

void
AppWindow::ShowConfirmationDialog(const std::string& title, const std::string& message,
                                  std::function<void()> on_confirm_callback) const
{
    m_confirmation_dialog->Show(title, message, on_confirm_callback);
}

void
AppWindow::ShowMessageDialog(const std::string& title, const std::string& message) const
{
    m_message_dialog->Show(title, message);
}

void
AppWindow::ShowSaveFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, true);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, true, callback);
}

void
AppWindow::ShowOpenFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog(file_filters, initial_path, callback, false);
        return;
    }
#endif
    ShowImGuiFileDialog(title, file_filters, initial_path, false, callback);
}

void
AppWindow::ShowPathPickerDialog(const std::string& title, const std::string& initial_path,
                                std::function<void(std::string)> callback)
{
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    if(m_use_native_file_dialog.load())
    {
        (void)title;
        ShowNativeFileDialog({}, initial_path, callback, false, true);
        return;
    }
#endif
    ShowImGuiFileDialog(title, {}, initial_path, false, callback, true);
}

ProjectItem*
AppWindow::GetItem(const std::string& id)
{
    ProjectItem* project = nullptr;
    if(m_items.count(id) > 0)
    {
        project = m_items[id].get();
    }
    return project;
}

ProjectItem*
AppWindow::GetCurrentItem()
{
    ProjectItem*       project    = nullptr;
    const TabItem* active_tab = m_tab_container->GetActiveTab();
    if(active_tab)
    {
        project = GetItem(active_tab->m_id);
    }
    return project;
}

Project*
AppWindow::GetProjectById(const std::string& project_id)
{
    Project* result = nullptr;
    for(std::unique_ptr<Project>& p : m_projects)
    {
        if(p->GetID() == project_id)
        {
            result = p.get();
            break;
        }
    }
    return result;
}

Project*
AppWindow::GetProjectForItem(const std::string& item_id)
{
    Project* result = nullptr;
    for(std::unique_ptr<Project>& p : m_projects)
    {
        if(p->ContainsItem(item_id))
        {
            result = p.get();
            break;
        }
    }
    return result;
}

Project*
AppWindow::CreateProject()
{
    // Reuse the lowest free "Project N" number so repeatedly creating/deleting groups
    // does not make the number climb forever. Color is derived from the number so a
    // given "Project N" always gets the same palette color.
    int  number = 1;
    bool taken  = true;
    while(taken)
    {
        std::string candidate = "Project " + std::to_string(number);
        taken                 = false;
        for(std::unique_ptr<Project>& p : m_projects)
        {
            if(p->GetName() == candidate)
            {
                taken = true;
                break;
            }
        }
        if(taken)
        {
            number++;
        }
    }
    const std::vector<ImU32>& palette    = SettingsManager::GetInstance().GetColorWheel();
    ImU32                     color      = palette[(number - 1) % palette.size()];
    std::string               name       = "Project " + std::to_string(number);
    std::string               project_id = "project://" + std::to_string(number) + "-" +
                             std::to_string(m_project_counter++);
    m_projects.push_back(std::make_unique<Project>(project_id, name, color));
    return m_projects.back().get();
}

Project*
AppWindow::CreateProjectNamed(const std::string& name, ImU32 color)
{
    std::string project_id = "project://named-" + std::to_string(m_project_counter++);
    m_projects.push_back(std::make_unique<Project>(project_id, name, color));
    return m_projects.back().get();
}

bool
AppWindow::IsProjectGroupFile(const std::string& file_path)
{
    std::ifstream file(file_path);
    if(!file.is_open())
    {
        return false;
    }
    std::string json_string;
    std::string line;
    while(std::getline(file, line))
    {
        json_string += line;
    }
    file.close();
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(json_string);
    if(parsed.first != jt::Json::success)
    {
        return false;
    }
    // A project (tab group) file has a top-level "items" array; a single-item .rpv
    // does not (its content lives under "general"/"timeline").
    return parsed.second["items"].isArray();
}

void
AppWindow::HandleSaveProjectGroup(const std::string& project_id)
{
    if(!GetProjectById(project_id))
    {
        return;
    }
    FileFilter project_filter;
    project_filter.m_name       = "Project";
    project_filter.m_extensions = PROJECT_GROUP_EXTENSIONS;
    std::vector<FileFilter> filters;
    filters.push_back(project_filter);
    ShowSaveFileDialog(
        "Save Project", filters, "",
        [this, project_id](std::string file_path) { SaveProjectGroup(project_id, file_path); });
}

void
AppWindow::SaveProjectGroup(const std::string& project_id, const std::string& save_path)
{
    Project* project = GetProjectById(project_id);
    if(!project)
    {
        return;
    }

    // Ensure the file carries the project-group extension so reopening it is routed
    // back to the group loader (a dialog may not append the filter extension).
    std::string file_path = save_path;
    if(std::filesystem::path(file_path).extension().string() != PROJECT_GROUP_EXTENSION)
    {
        file_path += PROJECT_GROUP_EXTENSION;
    }

    jt::Json root;
    root            = "";
    root["version"] = "1.0";
    root["name"]    = project->GetName();
    char color_buf[16];
    std::snprintf(color_buf, sizeof(color_buf), "%08X",
                  static_cast<unsigned int>(project->GetColor()));
    root["color"] = std::string(color_buf);

    // Persist each open member with its full per-view settings (track heights/order,
    // bookmarks, annotations, ...) so a reopened project restores exactly as saved.
    // Remembered closed items are stored separately (filelist only) so they stay in
    // the project's reopen list without being reopened as tabs.
    std::filesystem::path dir        = std::filesystem::path(file_path).parent_path();
    size_t                open_index = 0;
    for(const std::string& member_id : project->GetItemIds())
    {
        ProjectItem* member = GetItem(member_id);
        if(!member)
        {
            continue;
        }
        root["items"][open_index]["settings"] = member->ExportSettingsJson(dir);
        open_index++;
    }
    size_t closed_index = 0;
    for(const Project::ClosedItem& closed : project->GetClosedItems())
    {
        root["closed"][closed_index]["name"] = closed.name;
        for(size_t j = 0; j < closed.files.size(); j++)
        {
            root["closed"][closed_index]["files"][j] =
                std::filesystem::proximate(closed.files[j], dir).generic_string();
        }
        closed_index++;
    }

    std::ofstream file(file_path);
    if(file.is_open())
    {
        file << root.toStringPretty() << "\n";
        file.close();
        // Remember the path so "Save" can re-save the whole project without a dialog.
        project->SetFilePath(file_path);
        SettingsManager::GetInstance().AddRecentFile(file_path);
        NotificationManager::GetInstance().Show("Saved project \"" + project->GetName() + "\".",
                                                NotificationLevel::Success);
    }
    else
    {
        NotificationManager::GetInstance().Show("Failed to save project.",
                                                NotificationLevel::Error);
    }
}

std::string
AppWindow::OpenItemFromSettings(const jt::Json&              settings,
                                const std::filesystem::path& base_dir)
{
    std::unique_ptr<ProjectItem> new_item = std::make_unique<ProjectItem>();
    std::string                  out_id;
    ProjectItem::OpenResult open_result   = new_item->OpenFromSettingsJson(settings, base_dir, out_id);
    if(open_result == ProjectItem::OpenResult::Success)
    {
        TabItem tab{ new_item->GetName(), new_item->GetID(), new_item->GetView(), true };
        m_tab_container->AddTab(std::move(tab));
        std::string opened_id = new_item->GetID();
        m_items[opened_id]    = std::move(new_item);
        return opened_id;
    }
    if(open_result == ProjectItem::OpenResult::Duplicate)
    {
        return out_id;
    }
    return std::string();
}

void
AppWindow::OpenProjectGroupFile(const std::string& file_path)
{
    std::ifstream file(file_path);
    if(!file.is_open())
    {
        ShowMessageDialog("Error", "Could not open project file:\n\n" + file_path);
        return;
    }
    std::string json_string;
    std::string line;
    while(std::getline(file, line))
    {
        json_string += line;
    }
    file.close();

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(json_string);
    if(parsed.first != jt::Json::success)
    {
        ShowMessageDialog("Error", "The project file is invalid or corrupted:\n\n" + file_path);
        return;
    }
    jt::Json& root = parsed.second;

    // A .rpv is always a project. New files carry an "items" array (a real group);
    // old single-item files do not, and become a one-tab project named after the file.
    bool                      is_group_format = root["items"].isArray();
    const std::vector<ImU32>& palette         = SettingsManager::GetInstance().GetColorWheel();
    std::string               name;
    ImU32                     color = palette[0];
    if(is_group_format)
    {
        name = root["name"].isString() ? root["name"].getString() : "Project";
        if(root["color"].isString())
        {
            color = static_cast<ImU32>(std::stoul(root["color"].getString(), nullptr, 16));
        }
    }
    else
    {
        name = std::filesystem::path(file_path).stem().string();
    }
    Project*    project    = CreateProjectNamed(name, color);
    std::string project_id = project->GetID();
    project->SetFilePath(file_path);

    std::filesystem::path dir = std::filesystem::path(file_path).parent_path();
    if(is_group_format)
    {
        for(jt::Json& item : root["items"].getArray())
        {
            std::string opened_id;
            if(!item["settings"].isNull())
            {
                // New format: the item carries its full per-view settings.
                opened_id = OpenItemFromSettings(item["settings"], dir);
            }
            else if(item["files"].isArray())
            {
                // Legacy format (filelist only, no per-view settings).
                std::vector<std::string> files;
                for(jt::Json& entry : item["files"].getArray())
                {
                    if(entry.isString())
                    {
                        files.push_back(
                            std::filesystem::weakly_canonical(dir / entry.getString())
                                .string());
                    }
                }
                if(files.empty())
                {
                    continue;
                }
                std::unordered_set<std::string> before;
                for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
                {
                    before.insert(kv.first);
                }
                if(files.size() >= 2)
                {
                    OpenCompare(files[0], files[1]);
                }
                else
                {
                    OpenFile(files[0]);
                }
                for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
                {
                    if(before.find(kv.first) == before.end())
                    {
                        opened_id = kv.first;
                    }
                }
            }
            if(!opened_id.empty())
            {
                Project* group = GetProjectById(project_id);
                if(group)
                {
                    group->AddItem(opened_id);
                }
            }
        }
    }

    // Restore remembered closed items (kept in the project's reopen list, not opened
    // as tabs).
    if(root["closed"].isArray())
    {
        for(jt::Json& closed_json : root["closed"].getArray())
        {
            Project::ClosedItem closed;
            closed.name = closed_json["name"].isString() ? closed_json["name"].getString()
                                                         : std::string();
            if(closed_json["files"].isArray())
            {
                for(jt::Json& entry : closed_json["files"].getArray())
                {
                    if(entry.isString())
                    {
                        closed.files.push_back(
                            std::filesystem::weakly_canonical(dir / entry.getString())
                                .string());
                    }
                }
            }
            if(!closed.files.empty())
            {
                Project* group = GetProjectById(project_id);
                if(group)
                {
                    group->AddClosedItem(closed);
                }
            }
        }
    }

    if(!is_group_format)
    {
        // Old single-item .rpv: the whole file is one item's settings; open it as the
        // project's single tab (settings restored).
        std::string opened_id = OpenItemFromSettings(root, dir);
        if(!opened_id.empty())
        {
            Project* group = GetProjectById(project_id);
            if(group)
            {
                group->AddItem(opened_id);
            }
        }
    }

    // If nothing actually opened (e.g. all traces were already open), drop the empty
    // group so it does not linger as a phantom in the "Add to project" menus.
    Project* group = GetProjectById(project_id);
    if(group && group->GetItemIds().empty() && group->GetClosedItems().empty())
    {
        for(size_t idx = 0; idx < m_projects.size(); idx++)
        {
            if(m_projects[idx]->GetID() == project_id)
            {
                m_projects.erase(m_projects.begin() + idx);
                break;
            }
        }
    }

    SettingsManager::GetInstance().AddRecentFile(file_path);
    RefreshTabGroups();
}

void
AppWindow::SaveSession()
{
    // Paths are stored relative to the config dir so the snapshot is relocatable.
    std::filesystem::path config_dir   = get_application_config_path(true);
    std::filesystem::path session_path = config_dir / SESSION_FILE_NAME;

    jt::Json root;
    root            = "";
    root["version"] = "1.0";

    size_t project_index = 0;
    for(const std::unique_ptr<Project>& project : m_projects)
    {
        char color_buf[16];
        std::snprintf(color_buf, sizeof(color_buf), "%08X",
                      static_cast<unsigned int>(project->GetColor()));
        root["projects"][project_index]["name"]  = project->GetName();
        root["projects"][project_index]["color"] = std::string(color_buf);
        size_t item_index                        = 0;
        for(const std::string& member_id : project->GetItemIds())
        {
            ProjectItem* member = GetItem(member_id);
            if(!member)
            {
                continue;
            }
            root["projects"][project_index]["items"][item_index]["settings"] =
                member->ExportSettingsJson(config_dir);
            item_index++;
        }
        size_t closed_index = 0;
        for(const Project::ClosedItem& closed : project->GetClosedItems())
        {
            root["projects"][project_index]["closed"][closed_index]["name"] = closed.name;
            for(size_t j = 0; j < closed.files.size(); j++)
            {
                root["projects"][project_index]["closed"][closed_index]["files"][j] =
                    std::filesystem::proximate(closed.files[j], config_dir).generic_string();
            }
            closed_index++;
        }
        project_index++;
    }

    // Ungrouped tabs, kept in strip order.
    size_t                            ungrouped_index = 0;
    const std::vector<const TabItem*> tabs            = m_tab_container->GetTabs();
    for(const TabItem* tab : tabs)
    {
        if(GetProjectForItem(tab->m_id))
        {
            continue;  // grouped items are saved under their project
        }
        ProjectItem* item = GetItem(tab->m_id);
        if(!item)
        {
            continue;
        }
        root["ungrouped"][ungrouped_index]["settings"] = item->ExportSettingsJson(config_dir);
        ungrouped_index++;
    }

    std::error_code ec;
    if(project_index == 0 && ungrouped_index == 0)
    {
        // Nothing open: drop any stale session.
        std::filesystem::remove(session_path, ec);
        return;
    }

    std::ofstream file(session_path);
    if(file.is_open())
    {
        file << root.toStringPretty() << "\n";
        file.close();
    }
}

void
AppWindow::RestoreSession()
{
    // Missing traces are skipped silently so a moved/deleted file never blocks startup.
    std::filesystem::path config_dir   = get_application_config_path(true);
    std::filesystem::path session_path = config_dir / SESSION_FILE_NAME;
    if(!std::filesystem::exists(session_path))
    {
        return;
    }
    std::ifstream file(session_path);
    if(!file.is_open())
    {
        return;
    }
    std::string json_string;
    std::string line;
    while(std::getline(file, line))
    {
        json_string += line;
    }
    file.close();

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(json_string);
    if(parsed.first != jt::Json::success)
    {
        return;
    }
    jt::Json&                 root    = parsed.second;
    const std::vector<ImU32>& palette = SettingsManager::GetInstance().GetColorWheel();

    if(root["projects"].isArray())
    {
        for(jt::Json& project_json : root["projects"].getArray())
        {
            std::string name =
                project_json["name"].isString() ? project_json["name"].getString() : "Project";
            ImU32 color = palette.empty() ? 0 : palette[0];
            if(project_json["color"].isString())
            {
                color = static_cast<ImU32>(std::stoul(project_json["color"].getString(), nullptr, 16));
            }
            Project*    project    = CreateProjectNamed(name, color);
            std::string project_id = project->GetID();

            if(project_json["items"].isArray())
            {
                for(jt::Json& item_json : project_json["items"].getArray())
                {
                    if(item_json["settings"].isNull())
                    {
                        continue;
                    }
                    std::string opened_id = OpenItemFromSettings(item_json["settings"], config_dir);
                    if(!opened_id.empty())
                    {
                        Project* group = GetProjectById(project_id);
                        if(group)
                        {
                            group->AddItem(opened_id);
                        }
                    }
                }
            }
            if(project_json["closed"].isArray())
            {
                for(jt::Json& closed_json : project_json["closed"].getArray())
                {
                    Project::ClosedItem closed;
                    closed.name = closed_json["name"].isString() ? closed_json["name"].getString()
                                                                 : std::string();
                    if(closed_json["files"].isArray())
                    {
                        for(jt::Json& entry : closed_json["files"].getArray())
                        {
                            if(entry.isString())
                            {
                                closed.files.push_back(
                                    std::filesystem::weakly_canonical(config_dir / entry.getString())
                                        .string());
                            }
                        }
                    }
                    if(!closed.files.empty())
                    {
                        Project* group = GetProjectById(project_id);
                        if(group)
                        {
                            group->AddClosedItem(closed);
                        }
                    }
                }
            }

            // Drop a project that restored nothing.
            Project* group = GetProjectById(project_id);
            if(group && group->GetItemIds().empty() && group->GetClosedItems().empty())
            {
                for(size_t idx = 0; idx < m_projects.size(); idx++)
                {
                    if(m_projects[idx]->GetID() == project_id)
                    {
                        m_projects.erase(m_projects.begin() + idx);
                        break;
                    }
                }
            }
        }
    }

    if(root["ungrouped"].isArray())
    {
        for(jt::Json& item_json : root["ungrouped"].getArray())
        {
            if(!item_json["settings"].isNull())
            {
                OpenItemFromSettings(item_json["settings"], config_dir);
            }
        }
    }

    RefreshTabGroups();
}

void
AppWindow::AssignItemToProject(const std::string& item_id, const std::string& project_id)
{
    // No-op if it already belongs to the target project.
    Project* current = GetProjectForItem(item_id);
    if(current && current->GetID() == project_id)
    {
        return;
    }
    RemoveItemFromProjectMembership(item_id);
    Project* target = GetProjectById(project_id);
    if(target)
    {
        target->AddItem(item_id);
    }
    RefreshTabGroups();
}

void
AppWindow::RemoveItemFromProjectMembership(const std::string& item_id)
{
    for(size_t i = 0; i < m_projects.size(); i++)
    {
        if(m_projects[i]->RemoveItem(item_id))
        {
            if(m_projects[i]->Empty())
            {
                m_projects.erase(m_projects.begin() + i);
            }
            break;
        }
    }
}

void
AppWindow::UngroupProject(const std::string& project_id)
{
    for(size_t i = 0; i < m_projects.size(); i++)
    {
        if(m_projects[i]->GetID() == project_id)
        {
            m_projects.erase(m_projects.begin() + i);
            break;
        }
    }
    RefreshTabGroups();
}

void
AppWindow::CloseProjectTabs(const std::string& project_id)
{
    Project* project = GetProjectById(project_id);
    if(!project)
    {
        return;
    }
    // Copy ids: RemoveTab -> kTabClosed -> HandleTabClosed mutates the member list.
    std::vector<std::string> ids = project->GetItemIds();
    for(const std::string& id : ids)
    {
        m_tab_container->RemoveTab(id);
    }
    RefreshTabGroups();
}

void
AppWindow::ReopenClosedItem(const std::string& project_id, size_t closed_index)
{
    Project* project = GetProjectById(project_id);
    if(!project || closed_index >= project->GetClosedItems().size())
    {
        return;
    }
    Project::ClosedItem closed = project->GetClosedItems()[closed_index];
    project->RemoveClosedItemAt(closed_index);
    if(closed.files.empty())
    {
        return;
    }

    // Discover the id(s) produced by the open by diffing the open-item map.
    std::unordered_set<std::string> before;
    for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
    {
        before.insert(kv.first);
    }
    if(closed.files.size() >= 2)
    {
        OpenCompare(closed.files[0], closed.files[1]);
    }
    else
    {
        OpenFile(closed.files[0]);
    }
    // Re-fetch the project pointer (OpenFile can mutate m_projects indirectly).
    project = GetProjectById(project_id);
    if(!project)
    {
        return;
    }
    for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
    {
        if(before.find(kv.first) == before.end())
        {
            project->AddItem(kv.first);
        }
    }
    RefreshTabGroups();
}

void
AppWindow::RefreshTabGroups()
{
    if(!m_tab_container)
    {
        return;
    }
    const std::vector<const TabItem*> tabs = m_tab_container->GetTabs();
    for(const TabItem* tab : tabs)
    {
        Project* project = GetProjectForItem(tab->m_id);
        if(project)
        {
            m_tab_container->SetTabGroup(tab->m_id, project->GetColor(), project->GetID(),
                                         project->GetName());
        }
        else
        {
            m_tab_container->SetTabGroup(tab->m_id, 0, std::string(), std::string());
        }
    }
    ReorderTabsForGroups();
}

void
AppWindow::SyncProjectOrderToTabs()
{
    const std::vector<const TabItem*> tabs = m_tab_container->GetTabs();
    for(std::unique_ptr<Project>& project : m_projects)
    {
        std::vector<std::string> ordered;
        for(const TabItem* tab : tabs)
        {
            if(project->ContainsItem(tab->m_id))
            {
                ordered.push_back(tab->m_id);
            }
        }
        project->SetItemOrder(ordered);
    }
}

void
AppWindow::ReorderTabsForGroups()
{
    const std::vector<const TabItem*> tabs = m_tab_container->GetTabs();
    std::vector<std::string>          order;
    order.reserve(tabs.size());
    std::unordered_set<std::string> emitted_projects;
    for(const TabItem* tab : tabs)
    {
        Project* project = GetProjectForItem(tab->m_id);
        if(project)
        {
            if(emitted_projects.find(project->GetID()) == emitted_projects.end())
            {
                for(const std::string& member_id : project->GetItemIds())
                {
                    order.push_back(member_id);
                }
                emitted_projects.insert(project->GetID());
            }
        }
        else
        {
            order.push_back(tab->m_id);
        }
    }
    m_tab_container->ReorderTabs(order);
}

void
AppWindow::OpenFiles(const std::vector<std::string>& file_paths)
{
    if(file_paths.size() < 2)
    {
        for(const std::string& path : file_paths)
        {
            OpenFile(path);
        }
        return;
    }

    // Opening several files together auto-groups them into a new project.
    std::unordered_set<std::string> before;
    for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
    {
        before.insert(kv.first);
    }
    for(const std::string& path : file_paths)
    {
        OpenFile(path);
    }
    std::vector<std::string> new_ids;
    for(const std::pair<const std::string, std::unique_ptr<ProjectItem>>& kv : m_items)
    {
        if(before.find(kv.first) == before.end())
        {
            new_ids.push_back(kv.first);
        }
    }
    if(new_ids.size() >= 2)
    {
        Project* project = CreateProject();
        for(const std::string& id : new_ids)
        {
            project->AddItem(id);
        }
        RefreshTabGroups();
    }
}

void
AppWindow::RenderTabGroupContextMenu(const std::string& item_id)
{
    Project* current = GetProjectForItem(item_id);
    if(current)
    {
        // Inline rename of the item's current project.
        char   name_buf[128] = { 0 };
        size_t copied        = current->GetName().copy(name_buf, sizeof(name_buf) - 1);
        name_buf[copied]     = '\0';
        ImGui::SetNextItemWidth(160.0f);
        if(ImGui::InputText("###ctx_rename", name_buf, sizeof(name_buf)))
        {
            current->SetName(std::string(name_buf));
            // Update the group's tab labels immediately (no reorder, so this is safe
            // to do during the tab render).
            for(const std::string& member_id : current->GetItemIds())
            {
                m_tab_container->SetTabGroup(member_id, current->GetColor(),
                                             current->GetID(), current->GetName());
            }
        }
        ImGui::Separator();

        // Pull another open tab into this group (Chrome-style "add to group"),
        // reachable from the group chip or any member tab.
        std::vector<std::pair<std::string, std::string>> addable;
        for(const TabItem* other : m_tab_container->GetTabs())
        {
            Project* owner = GetProjectForItem(other->m_id);
            if(!owner || owner->GetID() != current->GetID())
            {
                ProjectItem* member = GetItem(other->m_id);
                addable.emplace_back(other->m_id, member ? member->GetName() : other->m_id);
            }
        }
        if(!addable.empty() && ImGui::BeginMenu("Add tab to group"))
        {
            std::string group_id = current->GetID();
            for(const std::pair<std::string, std::string>& entry : addable)
            {
                if(ImGui::MenuItem((entry.second + "###addtab_" + entry.first).c_str()))
                {
                    std::string add_id       = entry.first;
                    m_pending_project_action = [this, add_id, group_id]() {
                        AssignItemToProject(add_id, group_id);
                    };
                }
            }
            ImGui::EndMenu();
        }
    }

    // "Add to new project" only makes sense when it would actually change grouping:
    // disable it when the item is already the sole member of a group (it would just
    // create a fresh, renumbered group for the same tab).
    bool can_new_project = !(current && current->GetItemIds().size() == 1);
    if(ImGui::MenuItem("Add to new project", nullptr, false, can_new_project))
    {
        m_pending_project_action = [this, item_id]() {
            Project* project = CreateProject();
            AssignItemToProject(item_id, project->GetID());
        };
    }

    // Add-to / move-to existing projects. Only list real (open) groups; a project
    // with no open tabs (only remembered closed files) is not a valid target and
    // must not appear here.
    bool has_other = false;
    for(std::unique_ptr<Project>& p : m_projects)
    {
        if((!current || p->GetID() != current->GetID()) && !p->GetItemIds().empty())
        {
            has_other = true;
            break;
        }
    }
    if(has_other && ImGui::BeginMenu(current ? "Move to project" : "Add to project"))
    {
        for(std::unique_ptr<Project>& p : m_projects)
        {
            if((current && p->GetID() == current->GetID()) || p->GetItemIds().empty())
            {
                continue;
            }
            if(ImGui::MenuItem((p->GetName() + "###mv_" + p->GetID()).c_str()))
            {
                std::string target_id = p->GetID();
                m_pending_project_action = [this, item_id, target_id]() {
                    AssignItemToProject(item_id, target_id);
                };
            }
        }
        ImGui::EndMenu();
    }

    if(current)
    {
        std::string project_id = current->GetID();
        ImGui::Separator();
        if(ImGui::MenuItem("Remove from project"))
        {
            m_pending_project_action = [this, item_id]() {
                RemoveItemFromProjectMembership(item_id);
                RefreshTabGroups();
            };
        }
        if(ImGui::MenuItem("Ungroup"))
        {
            m_pending_project_action = [this, project_id]() { UngroupProject(project_id); };
        }
    }
}

void
AppWindow::RenderProjectMenuBody(Project* project)
{
    if(!project)
    {
        return;
    }
    const std::string project_id = project->GetID();

    // Inline rename. Updates the group's tab labels directly (no reorder), so this is
    // safe whether invoked from the File menu or from the tab-strip chip context menu
    // (which runs during the tab strip's own render).
    char   name_buf[128] = { 0 };
    size_t copied        = project->GetName().copy(name_buf, sizeof(name_buf) - 1);
    name_buf[copied]     = '\0';
    ImGui::SetNextItemWidth(180.0f);
    if(ImGui::InputText(("###pm_rename_" + project_id).c_str(), name_buf, sizeof(name_buf)))
    {
        project->SetName(std::string(name_buf));
        for(const std::string& member_id : project->GetItemIds())
        {
            m_tab_container->SetTabGroup(member_id, project->GetColor(), project_id,
                                         project->GetName());
        }
    }

    // Color swatch grid.
    if(ImGui::BeginMenu(("Color###pm_color_" + project_id).c_str()))
    {
        const std::vector<ImU32>& palette = SettingsManager::GetInstance().GetColorWheel();
        for(size_t c = 0; c < palette.size(); c++)
        {
            ImVec4      swatch    = ImGui::ColorConvertU32ToFloat4(palette[c]);
            std::string swatch_id = "###pm_col_" + std::to_string(c) + project_id;
            if(ImGui::ColorButton(swatch_id.c_str(), swatch,
                                  ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha,
                                  ImVec2(22.0f, 22.0f)))
            {
                project->SetColor(palette[c]);
                for(const std::string& member_id : project->GetItemIds())
                {
                    m_tab_container->SetTabGroup(member_id, project->GetColor(), project_id,
                                                 project->GetName());
                }
            }
            if((c % 3) != 2)
            {
                ImGui::SameLine();
            }
        }
        ImGui::EndMenu();
    }

    // Pull another open tab into this group.
    std::vector<std::pair<std::string, std::string>> addable;
    for(const TabItem* tab : m_tab_container->GetTabs())
    {
        Project* owner = GetProjectForItem(tab->m_id);
        if(!owner || owner->GetID() != project_id)
        {
            ProjectItem* member = GetItem(tab->m_id);
            addable.emplace_back(tab->m_id, member ? member->GetName() : tab->m_id);
        }
    }
    if(!addable.empty() && ImGui::BeginMenu(("Add tab to group###pm_add_" + project_id).c_str()))
    {
        for(const std::pair<std::string, std::string>& entry : addable)
        {
            if(ImGui::MenuItem((entry.second + "###pm_addtab_" + entry.first).c_str()))
            {
                std::string add_id       = entry.first;
                m_pending_project_action = [this, add_id, project_id]() {
                    AssignItemToProject(add_id, project_id);
                };
            }
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();

    // Open members (click to focus the tab).
    const std::vector<std::string>& members = project->GetItemIds();
    if(!members.empty())
    {
        ImGui::TextDisabled("Open");
        for(const std::string& member_id : members)
        {
            ProjectItem*       member = GetItem(member_id);
            std::string label =
                (member ? member->GetName() : member_id) + "###pm_open_" + member_id;
            if(ImGui::MenuItem(label.c_str()))
            {
                m_tab_container->SetActiveTab(member_id);
            }
        }
    }

    // Closed members (click to reopen).
    const std::vector<Project::ClosedItem>& closed = project->GetClosedItems();
    if(!closed.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled("Closed");
        for(size_t ci = 0; ci < closed.size(); ci++)
        {
            std::string label = closed[ci].name + "  (reopen)###pm_closed_" +
                                std::to_string(ci) + project_id;
            if(ImGui::MenuItem(label.c_str()))
            {
                size_t index             = ci;
                m_pending_project_action = [this, project_id, index]() {
                    ReopenClosedItem(project_id, index);
                };
            }
        }
    }

    ImGui::Separator();
    if(ImGui::MenuItem(("Save Project...###pm_save_" + project_id).c_str()))
    {
        m_pending_project_action = [this, project_id]() { HandleSaveProjectGroup(project_id); };
    }
    if(ImGui::MenuItem(("Ungroup###pm_ungroup_" + project_id).c_str()))
    {
        m_pending_project_action = [this, project_id]() { UngroupProject(project_id); };
    }
    if(!members.empty() &&
       ImGui::MenuItem(("Close all tabs###pm_close_" + project_id).c_str()))
    {
        m_pending_project_action = [this, project_id]() { CloseProjectTabs(project_id); };
    }
}

void
AppWindow::RenderProjectChipContextMenu(const std::string& group_id)
{
    Project* project = GetProjectById(group_id);
    if(project)
    {
        RenderProjectMenuBody(project);
    }
}

void
AppWindow::RenderProjectsMenu()
{
    if(!ImGui::BeginMenu("Projects", !m_projects.empty()))
    {
        return;
    }
    for(std::unique_ptr<Project>& proj : m_projects)
    {
        const std::string project_id = proj->GetID();
        ImGui::PushStyleColor(ImGuiCol_Text, proj->GetColor());
        bool project_open = ImGui::BeginMenu((proj->GetName() + "###proj_" + project_id).c_str());
        ImGui::PopStyleColor();
        if(!project_open)
        {
            continue;
        }
        RenderProjectMenuBody(proj.get());
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}

void
AppWindow::BeginAppShutdown()
{
    if(m_shutdown_requested)
    {
        RequestExitIfProviderCleanupsComplete();
        return;
    }

    m_shutdown_requested      = true;
    m_shutdown_start          = std::chrono::steady_clock::now();
    m_disable_app_interaction = true;

    // Snapshot the session while items/views are still alive.
    SaveSession();

    NotificationManager::GetInstance().ShowPersistent(
        APP_SHUTDOWN_NOTIFICATION_ID,
        "Closing traces... " + std::to_string(m_provider_cleanup_jobs.size()) +
            " cleanup job(s) remaining",
        NotificationLevel::Info);

    for(auto& item : m_items)
    {
        if(item.second)
        {
            item.second->Close();
            DetachItemProviderCleanup(*item.second,
                                         ProviderCleanupReason::kAppShutdown);
        }
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    StartProviderCleanup(m_test_data_provider.DetachCleanupWork(),
                         "developer data provider",
                         ProviderCleanupReason::kAppShutdown);
#endif

    m_items.clear();
    if(m_main_view)
    {
        m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
    }

    // Release the profiler dialog and remote-trace orchestrator now so their
    // sessions transfer any in-flight work to the AppMonitor (non-blocking).
    // Subsequent Update() frames drain the monitor; the exit gate waits until it
    // is empty.
#ifdef ROCPROFVIS_ENABLE_PROFILER
    m_profiler_launcher_dialog.reset();
#endif
#ifdef ROCPROFVIS_ENABLE_REMOTE
    m_ssh_test_dialog.reset();
#endif

    if(!m_provider_cleanup_jobs.empty())
    {
        NotificationManager::GetInstance().ShowPersistent(
            APP_SHUTDOWN_NOTIFICATION_ID, "Closing traces...",
            NotificationLevel::Info);
    }

    RequestExitIfProviderCleanupsComplete();
}

void
AppWindow::DetachItemProviderCleanup(ProjectItem& project, ProviderCleanupReason reason)
{
    std::shared_ptr<RootView> root_view =
        std::dynamic_pointer_cast<RootView>(project.GetView());
    if(!root_view)
    {
        return;
    }

    std::optional<DataProviderCleanupWork> cleanup_work =
        root_view->DetachProviderCleanup();
    if(cleanup_work)
    {
        StartProviderCleanup(std::move(*cleanup_work), project.GetName(), reason);
    }
}

void
AppWindow::StartProviderCleanup(DataProviderCleanupWork cleanup_work,
                                const std::string&    label,
                                ProviderCleanupReason reason)
{
    if(cleanup_work.requests.empty() && !cleanup_work.controller)
    {
        return;
    }

    const std::string cleanup_label =
        label.empty() ? cleanup_work.trace_file_path : label;
    ProviderCleanupJob job;
    job.label           = cleanup_label;
    job.reason          = reason;
    job.notification_id = "provider_cleanup_" +
                          std::to_string(++m_next_provider_cleanup_id);

    const std::string message =
        "Closing trace: " + cleanup_label + ", canceling " +
        std::to_string(cleanup_work.requests.size()) + " request(s)";
    NotificationManager::GetInstance().ShowPersistent(job.notification_id, message,
                                                      NotificationLevel::Info);

    job.future = std::async(
        std::launch::async,
        [cleanup_work = std::move(cleanup_work)]() mutable {
            return DataProvider::CleanupDetachedResources(std::move(cleanup_work));
        });
    m_provider_cleanup_jobs.push_back(std::move(job));
}

void
AppWindow::UpdateProviderCleanups()
{
    for(auto it = m_provider_cleanup_jobs.begin(); it != m_provider_cleanup_jobs.end();)
    {
        if(it->future.valid() &&
           it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            DataProviderCleanupResult result = it->future.get();
            spdlog::info("Provider cleanup completed for {} ({} request(s))",
                         result.trace_file_path.empty() ? it->label
                                                        : result.trace_file_path,
                         result.request_count);
            NotificationManager::GetInstance().Hide(it->notification_id);
            it = m_provider_cleanup_jobs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if(m_shutdown_requested)
    {
        if(m_provider_cleanup_jobs.empty())
        {
            NotificationManager::GetInstance().Hide(APP_SHUTDOWN_NOTIFICATION_ID);
        }
        RequestExitIfProviderCleanupsComplete();
    }
}

void
AppWindow::RequestExitIfProviderCleanupsComplete()
{
    if(!m_shutdown_requested || m_exit_notification_sent ||
       !m_provider_cleanup_jobs.empty())
    {
        return;
    }

    if(AppMonitor::GetInstance()->HasPendingOperations())
    {
        // The monitor drains non-blocking each shutdown frame. Bound the wait so
        // a stuck / never-resolving future cannot pin the app on the shutdown
        // screen forever. AppMonitor's destructor runs a final bounded,
        // cancelling drain (kShutdownDrainTimeoutSeconds) as the backstop.
        if(std::chrono::steady_clock::now() - m_shutdown_start <
           MONITOR_SHUTDOWN_GRACE_PERIOD)
        {
            return;
        }
        spdlog::warn("AppWindow: {} monitored operation(s) still pending after shutdown "
                     "grace period; exiting anyway",
                     AppMonitor::GetInstance()->GetActiveOperationCount());
    }

    m_exit_notification_sent = true;
    m_disable_app_interaction = false;
    if(m_notification_callback)
    {
        m_notification_callback(
            rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App);
    }
}

void
AppWindow::Update()
{
    RenderScheduler::GetInstance().BeginFrame();

#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    UpdateNativeFileDialog();
#endif
    UpdateProviderCleanups();
    if(m_shutdown_requested)
    {
        // Keep draining cancelling/in-flight monitored operations so their
        // resources are freed (non-blocking) before the app exits.
        AppMonitor::GetInstance()->Update();
        return;
    }

    HotkeyManager::GetInstance().ProcessInput();
    SettingsManager::GetInstance().GetFontManager().Update();
    // Poll long-running operations (profiler sessions, SSH) and queue any
    // status-change events before they are dispatched below this frame.
    AppMonitor::GetInstance()->Update();
    EventManager::GetInstance()->DispatchEvents();
    LogViewer::GetInstance()->Poll();
    DebugWindow::GetInstance()->ClearTransient();
    m_tab_container->Update();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    if (m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog->Update();
    }
#endif
#ifdef ROCPROFVIS_DEVELOPER_MODE
    m_test_data_provider.Update();
#endif
    UpdateStatusBar();
}

bool
AppWindow::WantsContinuousRender()
{
    // Animations and render-driven work push a frame request from their own
    // Update()/Render() via RenderScheduler, so no per-feature branch is needed
    // here.
    if(RenderScheduler::GetInstance().WantsRender())
    {
        return true;
    }

    if(!m_provider_cleanup_jobs.empty() || m_disable_app_interaction ||
       m_shutdown_requested || EventManager::GetInstance()->HasPendingEvents())
    {
        return true;
    }

#ifdef ROCPROFVIS_DEVELOPER_MODE
    if(m_test_data_provider.GetState() == ProviderState::kLoading ||
       m_test_data_provider.GetPendingRequestCount() > 0)
    {
        return true;
    }
#endif

    // Polled across every tab (not just the active one) so background loads keep
    // progressing. kLoading spans the whole load even when the pending count
    // briefly hits zero between stages, so we never freeze mid-load.
    bool wants_render = false;
    for(const auto& [id, project] : m_items)
    {
        RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
        if(root_view)
        {
            DataProvider* data_provider = root_view->GetDataProvider();
            if(data_provider &&
               (data_provider->GetState() == ProviderState::kLoading ||
                data_provider->GetPendingRequestCount() > 0))
            {
                wants_render = true;
                break;
            }
        }
    }
    return wants_render;
}

void
AppWindow::Render()
{
    Update();

#ifdef ROCPROFVIS_ENABLE_INTERNAL_BANNER
    DrawInternalBuildBanner("Evaluation Build");
#endif
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
#else
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

    ImGui::Begin("Main Window", nullptr,
                 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoDocking);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14, m_default_spacing.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    if(ImGui::BeginMenuBar())
    {
        ProjectItem* project = GetCurrentItem();
        RenderFileMenu(project);
        RenderEditMenu(project);
        RenderViewMenu(project);
        RenderHelpMenu();
#ifdef ROCPROFVIS_DEVELOPER_MODE
        RenderDeveloperMenu();
#endif
        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(3);  // ItemSpacing, WindowPadding, FramePadding

    if(m_main_view)
    {
        m_main_view->Render();
    }

    // Apply any queued project-group mutation now that all tab/menu rendering for the
    // frame is done (reordering tabs / editing m_projects mid-render is unsafe).
    if(m_pending_project_action)
    {
        std::function<void()> action = std::move(m_pending_project_action);
        m_pending_project_action     = nullptr;
        action();
        // The action reorders/relabels tabs; keep drawing a few more frames so the
        // lazy render loop actually paints the new layout (otherwise the change is
        // invisible until the next input event).
        RenderScheduler::GetInstance().RequestRenderForSeconds(0.2);
    }

    if(m_open_about_dialog)
    {
        ImGui::OpenPopup(ABOUT_DIALOG_NAME);
        m_open_about_dialog = false;  // Reset the flag after opening the dialog
    }
    RenderAboutDialog();  // Popup dialogs need to be rendered as part of the main window
#ifdef ROCPROFVIS_ENABLE_REMOTE
    if(m_ssh_test_dialog)
    {
        m_ssh_test_dialog->Render();
    }
#endif
    m_confirmation_dialog->Render();
    m_message_dialog->Render();
    m_compare_files_dialog->Render();
    m_settings_panel->Render();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    if (m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog->Render();
    }
#endif

    ImGui::End();
    // Pop ImGuiStyleVar_ItemSpacing, ImGuiStyleVar_WindowPadding,
    // ImGuiStyleVar_WindowRounding
    ImGui::PopStyleVar(3);

    RenderFileDialog();

    LogViewer::GetInstance()->Render();
#ifdef ROCPROFVIS_DEVELOPER_MODE
    RenderDebugOuput();
#endif

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Centralized SSH auth prompts: draw the blocking prompt / host-key modal
    // for every live session (including connections owned privately by widgets
    // such as the remote file browser), so no session can wedge its worker
    // waiting on a prompt that no dialog happens to render.
    RenderSshAuthModals();
#endif

    // render notifications last
    NotificationManager::GetInstance().Render();

    RenderDisableScreen();
}

void
AppWindow::RenderShutdownState()
{
    ImGui::OpenPopup(SHUTDOWN_DIALOG_NAME);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
               viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_Always);

    PopUpStyle ps;
    ps.PushPopupStyles();
    ps.CenterPopup();
    if(ImGui::BeginPopupModal(SHUTDOWN_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoCollapse))
    {
        const size_t cleanup_jobs = m_provider_cleanup_jobs.size();
        const size_t monitor_ops  = AppMonitor::GetInstance()->GetActiveOperationCount();

        if(cleanup_jobs == 0 && monitor_ops == 0)
        {
            CenterNextTextItem(CLOSING_MESSAGE);
            ImGui::TextUnformatted(CLOSING_MESSAGE);
        }
        else
        {
            CenterNextTextItem(CLEANUP_MESSAGE);
            ImGui::TextUnformatted(CLEANUP_MESSAGE);
            ImGui::Spacing();
            // Draw indicator dots to show that the app is still responsive
            RenderLoadingIndicator(SettingsManager::GetInstance().GetColor(Colors::kTextMain),
                                   nullptr, kCenterHorizontal);
            ImGui::Spacing();
            if(cleanup_jobs > 0)
            {
                const std::string remaining_message =
                    "Cleanup jobs remaining: " + std::to_string(cleanup_jobs);
                CenterNextTextItem(remaining_message.c_str());
                ImGui::TextUnformatted(remaining_message.c_str());
            }
            if(monitor_ops > 0)
            {
                const std::string ops_message =
                    "Background operations remaining: " + std::to_string(monitor_ops);
                CenterNextTextItem(ops_message.c_str());
                ImGui::TextUnformatted(ops_message.c_str());
            }
        }
        ImGui::EndPopup();
    }
    ps.PopStyles();
}

void
AppWindow::RenderFileDialog()
{
    if(!ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME))
    {
        return;  // No file dialog is opened, nothing to render
    }

    // Set Itemspacing to values from original default ImGui style
    // custom values to break the 3rd party file dialog implementation
    // especially the cell padding
    auto defaultStyle = SettingsManager::GetInstance().GetDefaultIMGUIStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding);

    if(m_init_file_dialog)
    {
        // Basically ImGuiCond_Appearing, except overwrite confirmation is a popup
        // ontop of dialog which triggers ImGuiCond_Appearing, thus flag cannot be used.
        ImGui::SetNextWindowPos(
            ImVec2(m_default_spacing.x, m_default_spacing.y + ImGui::GetFrameHeight()));
        ImGui::SetNextWindowSize(FILE_DIALOG_SIZE);
        m_init_file_dialog = false;
    }

    if(ImGuiFileDialog::Instance()->Display(FILE_DIALOG_NAME))
    {
        if(ImGuiFileDialog::Instance()->IsOk())
        {
            // Directory mode reports its result via GetCurrentPath(); GetFilePathName()
            // is empty in that case.
            const std::string result = m_imgui_file_dialog_folder_mode
                                            ? ImGuiFileDialog::Instance()->GetCurrentPath()
                                            : ImGuiFileDialog::Instance()->GetFilePathName();
            m_file_dialog_callback(std::filesystem::path(result).string());
        }
        ImGuiFileDialog::Instance()->Close();
    }
    ImGui::PopStyleVar(3);
}

void
AppWindow::OpenFile(std::string file_path)
{
    // While the Compare dialog is up, dropped/opened files fill its slots rather than
    // opening standalone trace tabs behind the modal.
    if(m_compare_files_dialog->IsOpen())
    {
        m_compare_files_dialog->AddDroppedFile(file_path);
        return;
    }

    // Every .rpv (and legacy .rpvproj) is a project: route it to the project loader,
    // which handles both the new group format and old single-item files (opening the
    // latter as a project with one tab).
    std::string extension = std::filesystem::path(file_path).extension().string();
    if(extension == ".rpv" || extension == ".rpvproj")
    {
        OpenProjectGroupFile(file_path);
        return;
    }

    spdlog::info("Opening file: {}", file_path);

    std::unique_ptr<ProjectItem> project = std::make_unique<ProjectItem>();
    switch(project->Open(file_path))
    {
        case ProjectItem::OpenResult::Success:
        {
            TabItem tab =
                TabItem{ project->GetName(), project->GetID(), project->GetView(), true };
            m_tab_container->AddTab(std::move(tab));
            m_tab_container->SetActiveTab(project->GetID());            
            m_items[project->GetID()] = std::move(project);
            SettingsManager::GetInstance().AddRecentFile(file_path);
            break;
        }
        case ProjectItem::OpenResult::Duplicate:
        {
            // trace already open, tell the user which tab and switch to it
            ProjectItem* existing = GetItem(file_path);
            ShowMessageDialog("Trace Already Open",
                              "This trace is already open in \"" +
                                  (existing ? existing->GetName() : file_path) +
                                  "\".\n\nSwitched to the existing tab.");
            m_tab_container->SetActiveTab(file_path);
            break;
        }
        default:
        {
            SettingsManager::GetInstance().RemoveRecentFile(file_path);
            break;
        }
    }
}

std::string
AppWindow::MakeCompareId(const std::vector<std::string>& files)
{
    std::string id = "compare://";
    for(size_t i = 0; i < files.size(); i++)
    {
        if(i > 0)
        {
            id += "|";
        }
        id += files[i];
    }
    return id;
}

void
AppWindow::OpenCompare(const std::string& first_file, const std::string& second_file)
{
    spdlog::info("Opening compare: {} vs {}", first_file, second_file);

    // Synthetic, deterministic project id so the compare tab has a stable identity
    // without a file on disk (the two traces are loaded directly by the controller).
    const std::string compare_id = MakeCompareId({ first_file, second_file });
    if(GetItem(compare_id))
    {
        m_tab_container->SetActiveTab(compare_id);
        return;
    }

    std::unique_ptr<ProjectItem> project = std::make_unique<ProjectItem>();
    if(project->OpenCompare(compare_id, { first_file, second_file }) ==
       ProjectItem::OpenResult::Success)
    {
        TabItem tab =
            TabItem{ project->GetName(), project->GetID(), project->GetView(), true };
        m_tab_container->AddTab(std::move(tab));
        m_items[project->GetID()] = std::move(project);
    }
}

void
AppWindow::RenderDisableScreen()
{
    if(m_shutdown_requested)
    {
        return;
    }

    if(m_disable_app_interaction)
    {
        ImGui::OpenPopup("GhostModal");
    }

    // Use a modal popup to disable interaction with the rest of the UI
    if(ImGui::IsPopupOpen("GhostModal"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        if(ImGui::BeginPopupModal("GhostModal", NULL,
                                  ImGuiWindowFlags_NoDecoration |
                                      ImGuiWindowFlags_NoBackground))
        {
            ImGui::SetWindowSize(ImVec2(1, 1));  // As small as possible
            if(!m_disable_app_interaction)
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);
    }
}

void
AppWindow::RenderFileMenu(ProjectItem* project)
{
    bool is_open_file_dialog_open = ImGuiFileDialog::Instance()->IsOpened(FILE_DIALOG_NAME);
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    is_open_file_dialog_open = is_open_file_dialog_open || m_is_native_file_dialog_open.load();
#endif

    if(ImGui::BeginMenu("File"))
    {
        if(ImGui::MenuItem("Open", nullptr, false, !is_open_file_dialog_open))
        {
            HandleOpenFile();
        }
#ifdef ROCPROFVIS_DEVELOPER_MODE
        if(ImGui::MenuItem("Compare", nullptr, false, !is_open_file_dialog_open))
        {
            HandleCompareFiles();
        }
#endif
        // A single Save / Save As that operates on the active tab's whole project when
        // it is grouped, or on just that item when it is not. "Save" re-saves to the
        // remembered path; "Save As" always prompts.
        Project* current_group = project ? GetProjectForItem(project->GetID()) : nullptr;
        bool     can_save =
            !is_open_file_dialog_open &&
            (current_group ? current_group->IsSaved() : (project && project->IsSaved()));
        if(ImGui::MenuItem("Save", nullptr, false, can_save))
        {
            if(current_group)
            {
                SaveProjectGroup(current_group->GetID(), current_group->GetFilePath());
            }
            else if(project)
            {
                project->Save();
            }
        }
        if(ImGui::MenuItem("Save As...", nullptr, false,
                           project != nullptr && !is_open_file_dialog_open))
        {
            HandleSaveAsFile();
        }

        RenderProjectsMenu();
        
#ifdef ROCPROFVIS_ENABLE_PROFILER
        // TEMPORARY (profiler launch): remove guard when the feature graduates.
        if(ImGui::MenuItem("Launch Profiler..."))
        {
            ShowProfilerLauncher();
        }
#endif
        ImGui::Separator();
        {
            TraceView* trace_view = nullptr;
            bool       has_trace  = false;
            bool       cleanup_pending = false;
            if(project && project->GetTraceType() == ProjectItem::System)
            {
                trace_view = dynamic_cast<TraceView*>(project->GetView().get());
                has_trace  = (trace_view != nullptr);
                if(has_trace)
                {
                    cleanup_pending = trace_view->IsCleanupPending();
                }
            }

            std::string project_id = has_trace ? project->GetID() : "";
            auto start_cleanup = [this, trace_view, project_id](bool rebuild) {
                trace_view->CleanupDatabase(rebuild, [this, project_id]() {
                    m_tab_container->RemoveTab(project_id);
                });
            };

            bool submenu_enabled = has_trace && !cleanup_pending;
            if(ImGui::BeginMenu("Database", submenu_enabled))
            {
#ifdef ROCPROFVIS_DEVELOPER_MODE
                if(ImGui::MenuItem("Fast Cleanup"))
                {
                    start_cleanup(false);
                }
#endif
                if(ImGui::MenuItem("Full Cleanup"))
                {
                    ShowConfirmationDialog(
                        "Full Database Cleanup",
                        "This will remove all Optiq metadata present in the database, "
                        "including service tables, indexes, and rebuild "
                        "(VACUUM) the database file. This may take a while.\n\n"
                        "Continue?",
                        [start_cleanup]() { start_cleanup(true); });
                }
                ImGui::EndMenu();
            }
        }
        
        ImGui::Separator();
        const std::list<std::string>& recent_files =
            SettingsManager::GetInstance().GetInternalSettings().recent_files;
        if(ImGui::BeginMenu("Recent Files", !recent_files.empty()))
        {
            for(std::string file : recent_files)
            {
                if(ImGui::MenuItem(file.c_str(), nullptr))
                {
                    OpenFile(file);
                    break;
                }
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Clear Recent Files"))
            {
                SettingsManager::GetInstance().ClearRecentFiles();
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit"))
        {
            ShowCloseConfirm();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderEditMenu(ProjectItem* project)
{
    if(ImGui::BeginMenu("Edit"))
    {
        if(project)
        {
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(project->GetView());
            if(root_view)
            {
                root_view->RenderEditMenuOptions();
            }
        }
        if(ImGui::MenuItem("Preferences"))
        {
            m_settings_panel->Show();
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderViewMenu(ProjectItem* project)
{
    (void) project;

    if(ImGui::BeginMenu("View"))
    {
        AppWindowSettings& settings =
            SettingsManager::GetInstance().GetAppWindowSettings();
        if(ImGui::MenuItem("Show Tool Bar", nullptr, &settings.show_toolbar))
        {
            LayoutItem* tool_bar_item = m_main_view->GetMutableAt(m_tool_bar_index);
            if(tool_bar_item)
            {
                tool_bar_item->m_visible = settings.show_toolbar;
            }
        }
#ifndef __APPLE__
        if(ImGui::MenuItem("Fullscreen", "F11", m_is_fullscreen))
        {
            if(m_notification_callback)
            {
                m_notification_callback(
                    rocprofvis_view_notification_t::
                        kRocProfVisViewNotification_Toggle_Fullscreen);
            }
        }
#endif
        ImGui::SeparatorText("System Profiler Panels");
        if(ImGui::MenuItem("Show Advanced Details Panel", nullptr,
                           &settings.show_details_panel))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetAnalysisViewVisibility(
                        settings.show_details_panel);
            }
        }
        if(ImGui::MenuItem("Show System Topology Panel", nullptr, &settings.show_sidebar))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetSidebarViewVisibility(settings.show_sidebar);
            }
        }
        if(ImGui::MenuItem("Show Timeline Overview", nullptr, &settings.show_histogram))
        {
            for(const auto& tab : m_tab_container->GetTabs())
            {
                auto trace_view_tab =
                    std::dynamic_pointer_cast<RocProfVis::View::TraceView>(tab->m_widget);
                if(trace_view_tab)
                    trace_view_tab->SetHistogramVisibility(settings.show_histogram);
            }
        }
        ImGui::MenuItem("Show Summary", nullptr, &settings.show_summary);

        ImGui::Separator();
        ImGui::MenuItem("Show Log Viewer", nullptr,
                        LogViewer::GetInstance()->VisiblePtr());
        ImGui::EndMenu();
    }
}

void
AppWindow::RenderHelpMenu()
{
    if(ImGui::BeginMenu("Help"))
    {
        if(ImGui::MenuItem("About"))
        {
            m_open_about_dialog = true;
        }
        ImGui::EndMenu();
    }
}

void
AppWindow::HandleOpenFile()
{
    std::vector<FileFilter> file_filters;

    FileFilter all_filter;
    all_filter.m_name       = "All Supported";
    all_filter.m_extensions = ALL_EXTENSIONS;

    FileFilter trace_filter;
    trace_filter.m_name       = "Traces";
    trace_filter.m_extensions = TRACE_EXTENSIONS;

    FileFilter project_filter;
    project_filter.m_name       = "Projects";
    project_filter.m_extensions = PROJECT_EXTENSIONS;

    file_filters.push_back(all_filter);
    file_filters.push_back(trace_filter);
    file_filters.push_back(project_filter);

    ShowOpenFileDialog(
        "Choose File", file_filters, "",
        [this](std::string file_path) -> void { this->OpenFile(file_path); });
}

void
AppWindow::HandleCompareFiles()
{
    m_compare_files_dialog->Show();
}

void
AppWindow::HandleCompareFileBrowse(CompareFilesDialog::FileSlot slot)
{
    std::vector<FileFilter> file_filters;

    FileFilter trace_filter;
    trace_filter.m_name       = "Trace Files";
    trace_filter.m_extensions = COMPARE_EXTENSIONS;
    file_filters.push_back(trace_filter);

    ShowOpenFileDialog(
        "Choose Trace", file_filters, "",
        [this, slot](std::string file_path) -> void {
            m_compare_files_dialog->SetFilePath(slot, file_path);
        });
}

void
AppWindow::HandleSaveAsFile()
{    
    ProjectItem* item = GetCurrentItem();
    if(!item)
    {
        return;
    }

    // Single "Save As": if the active tab belongs to a project, save the whole
    // project (reusing the project save path); otherwise save just this item.
    Project* group = GetProjectForItem(item->GetID());
    if(group)
    {
        HandleSaveProjectGroup(group->GetID());
        return;
    }

    FileFilter filter;
    filter.m_name       = "Projects";
    filter.m_extensions = { "rpv" };
    std::vector<FileFilter> filters;
    filters.push_back(filter);
    std::string item_id = item->GetID();
    ShowSaveFileDialog("Save As", filters, "", [this, item_id](std::string file_path) {
        ProjectItem* current = GetItem(item_id);
        if(current)
        {
            current->SaveAs(file_path);
        }
    });
}

void
AppWindow::HandleTabClosed(std::shared_ptr<RocEvent> e)
{
    auto tab_closed_event = std::dynamic_pointer_cast<TabEvent>(e);
    auto project_it =
        tab_closed_event ? m_items.find(tab_closed_event->GetTabId())
                         : m_items.end();
    if(tab_closed_event && project_it != m_items.end())
    {
        auto active_item = GetCurrentItem();
        if(!active_item)
        {
            spdlog::debug("No active project found after tab closed");
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;
        }
        else
        {
            spdlog::debug("Active project found after tab closed: {}",
                          active_item->GetName());
            std::shared_ptr<RootView> root_view =
                std::dynamic_pointer_cast<RootView>(active_item->GetView());
            if(root_view)
            {
                m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                    root_view->GetToolbar();
            }
        }
        spdlog::debug("Tab closed: {}", tab_closed_event->GetTabId());

        // If this tab belonged to a project group, remember it as a closed item so it
        // can be reopened from File > Projects, then drop its open membership.
        const std::string& closed_id = tab_closed_event->GetTabId();
        Project*            owning    = GetProjectForItem(closed_id);
        if(owning)
        {
            Project::ClosedItem closed;
            closed.name  = project_it->second->GetName();
            closed.files = project_it->second->GetFiles();
            owning->AddClosedItem(closed);
            owning->RemoveItem(closed_id);
        }

        project_it->second->Close();
        DetachItemProviderCleanup(*project_it->second,
                                     ProviderCleanupReason::kTabClose);
        m_items.erase(project_it);

        // Refresh coloring/order after the tab is gone.
        RefreshTabGroups();
    }
}

void
AppWindow::HandleTabSelectionChanged(std::shared_ptr<RocEvent> e)
{
    auto tab_selected_event = std::dynamic_pointer_cast<TabEvent>(e);
    if(tab_selected_event)
    {
        // Only handle the event if the tab source is the main tab source
        if(tab_selected_event->GetSourceId() == GetMainTabSourceName())
        {
            m_main_view->GetMutableAt(m_tool_bar_index)->m_item = nullptr;

            auto id = tab_selected_event->GetTabId();
            spdlog::debug("Tab selected: {}", id);
            auto project = GetItem(id);
            if(!project)
            {
                spdlog::warn("Project not found for tab: {}", id);
                return;
            }
            else
            {
                std::shared_ptr<RootView> root_view =
                    std::dynamic_pointer_cast<RootView>(project->GetView());
                if(root_view)
                {
                    m_main_view->GetMutableAt(m_tool_bar_index)->m_item =
                        root_view->GetToolbar();
                }
            }
        }
    }
}

void
AppWindow::HandleFontChanged()
{
    // Update status bar height based on new font size
    int count = static_cast<int>(m_main_view->ItemCount());

    // status bar (assume as the last item)
    auto status_bar_item = m_main_view->GetMutableAt(count - 1);
    if(!status_bar_item)
    {
        return;
    }

    // Size the slot to one framed text line plus the child border so the
    // status bar content does not overflow into a scrollbar.
    const ImGuiStyle& default_style = SettingsManager::GetInstance().GetDefaultStyle();
    const float       content_height =
        ImGui::GetFontSize() + (default_style.FramePadding.y * 2.0f);
    const float border_height = (status_bar_item->m_window_padding.y * 2.0f) +
                                (ImGui::GetStyle().ChildBorderSize * 2.0f);
    status_bar_item->m_height = content_height + border_height;

    // adjust main view's size to account for new status bar height
    auto main_view_item = m_main_view->GetMutableAt(count - 2);
    if(!main_view_item)
    {
        return;
    }
    main_view_item->m_height = -status_bar_item->m_height;
}

void
AppWindow::RenderAboutDialog()
{
    static constexpr const char* NAME_LABEL = "ROCm (TM) Optiq";
    static constexpr const char* COPYRIGHT_LABEL =
        "Copyright (C) 2026 Advanced Micro Devices, Inc. All rights reserved.";
    static constexpr const char* DOC_LABEL = "ROCm (TM) Optiq Documentation";
    static constexpr const char* DOC_URL =
        "https://rocm.docs.amd.com/projects/roc-optiq/en/latest/";
    static const std::string VERSION_LABEL = []() {
        std::stringstream ss;
        ss << "Version " << ROCPROFVIS_VERSION_MAJOR << "." << ROCPROFVIS_VERSION_MINOR
           << "." << ROCPROFVIS_VERSION_PATCH;
        return ss.str();
    }();

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(
        GetResponsiveWindowSize(ImVec2(580.0f, 0.0f), ImVec2(360.0f, 0.0f)));

    if(ImGui::BeginPopupModal(ABOUT_DIALOG_NAME, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoMove))
    {
        ImGui::PushFont(NULL, SettingsManager::GetInstance().GetFontManager().GetFontSize(
                                  FontSize::kLarge));

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(NAME_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(NAME_LABEL);
        ImGui::PopFont();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(VERSION_LABEL.c_str()).x) *
            0.5f);
        ImGui::TextUnformatted(VERSION_LABEL.c_str());

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(COPYRIGHT_LABEL).x) * 0.5f);
        ImGui::TextUnformatted(COPYRIGHT_LABEL);

        ImGui::Spacing();

        ImGui::SetCursorPosX(
            (ImGui::GetWindowSize().x - ImGui::CalcTextSize(DOC_LABEL).x) * 0.5f);
        ImGui::TextLink(DOC_LABEL);
        if(ImGui::IsItemClicked())
        {
            open_url(DOC_URL);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width =
            ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SetCursorPosX(ImGui::GetWindowSize().x - button_width -
                             ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    popup_style.PopStyles();

 }

#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
void
AppWindow::UpdateNativeFileDialog()
{
    if(m_is_native_file_dialog_open)
    {
        if(m_file_dialog_future.valid() &&
           m_file_dialog_future.wait_for(std::chrono::seconds(0)) ==
               std::future_status::ready)
        {
            m_disable_app_interaction = false;
            std::string file_path     = m_file_dialog_future.get();
            if(!file_path.empty() && m_file_dialog_callback)
            {
                m_file_dialog_callback(file_path);
            }
            m_is_native_file_dialog_open = false;
            m_file_dialog_callback       = nullptr;

            if(m_restore_fullscreen_later)
            {
                // toggle fullscreen on if it should be restored after dialog closes
                if(!m_is_fullscreen && m_notification_callback)
                {
                    m_notification_callback(
                        rocprofvis_view_notification_t::
                            kRocProfVisViewNotification_Toggle_Fullscreen);
                }
                m_restore_fullscreen_later = false;
            }
        }
    }
}

void
AppWindow::ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                                const std::string&               initial_path,
                                std::function<void(std::string)> callback,
                                bool                             save_dialog,
                                bool                             path_picker)
{
    if(m_is_native_file_dialog_open)
    {
        return;
    }
    m_is_native_file_dialog_open = true;
    m_file_dialog_callback       = callback;
    m_disable_app_interaction    = true;

    if(m_is_fullscreen)
    {
        // toggle fullscreen off before opening native file dialog
        if(m_notification_callback)
        {
            m_restore_fullscreen_later = true;
            m_notification_callback(rocprofvis_view_notification_t::
                                        kRocProfVisViewNotification_Toggle_Fullscreen);
        }
    }

    auto dialog_task = [=]() -> std::string {
        nfdresult_t init_result = NFD_Init();
        if(init_result != NFD_OKAY)
        {
            const char* err = NFD_GetError();
            spdlog::error("NFD_Init failed at dialog open: {}",
                          err ? err : "unknown");
            NFD_ClearError();
            m_use_native_file_dialog.store(false);
            return std::string();
        }
        nfdu8char_t* outPath = nullptr;

        nfdu8filteritem_t*       filters = nullptr;
        std::vector<std::string> extension_stings;
        if(!file_filters.empty())
        {
            filters = new nfdu8filteritem_t[file_filters.size()];
            for(size_t i = 0; i < file_filters.size(); ++i)
            {
                std::string extensions_str;
                for(size_t j = 0; j < file_filters[i].m_extensions.size(); ++j)
                {
                    extensions_str += file_filters[i].m_extensions[j];
                    if(j < file_filters[i].m_extensions.size() - 1)
                    {
                        extensions_str += ",";
                    }
                }
                extension_stings.push_back(std::move(extensions_str));
            }
            for(size_t i = 0; i < file_filters.size(); ++i)
            {
                filters[i] = { file_filters[i].m_name.c_str(), extension_stings[i].c_str() };
            }
        }

        nfdresult_t result;
        if(path_picker)
        {
            nfdpickfolderu8args_t args = {};
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_PickFolderU8_With(&outPath, &args);
        }
        else if(save_dialog)
        {
            nfdsavedialogu8args_t args = {};
            args.filterList            = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_SaveDialogU8_With(&outPath, &args);
        }
        else
        {
            nfdopendialogu8args_t args = {};
            args.filterList  = filters;
            args.filterCount = static_cast<nfdfiltersize_t>(file_filters.size());
            if(!initial_path.empty())
            {
                args.defaultPath = initial_path.c_str();
            }
            result = NFD_OpenDialogU8_With(&outPath, &args);
        }
        if(filters != nullptr)
        {
            delete[] filters;
        }
        std::string file_path;
        if(result == NFD_OKAY)
        {
            file_path = outPath;
            if(outPath)
            {
                // Save dialog only: append default extension when the name has none (e.g. Linux save).
                // Open dialog must not do this — extensionless executables would get ".*/.exe" appended
                // from the filter list (e.g. "transpose" + "."" + ".*" -> "transpose..*").
                std::filesystem::path p(file_path);
                if(save_dialog && !path_picker && !file_filters.empty() && !p.has_extension())
                {
                    file_path += "." + file_filters[0].m_extensions[0];
                }

                NFD_FreePathU8(outPath);
            }
        }
        else
        {
            spdlog::error("Error opening dialog: {}", NFD_GetError());
            if(outPath)
            {
                NFD_FreePathU8(outPath);
            }
            NFD_ClearError();
        }
        NFD_Quit();
        return file_path;
    };

#if defined(__APPLE__)
    // NSOpenPanel / NSSavePanel are AppKit objects and must be driven from the
    // main thread. Run synchronously here and hand the result to the existing
    // future-polling path via a ready promise.
    std::promise<std::string> dialog_promise;
    dialog_promise.set_value(dialog_task());
    m_file_dialog_future = dialog_promise.get_future();
#else
    m_file_dialog_future = std::async(std::launch::async, std::move(dialog_task));
#endif
}

#endif

void
AppWindow::ShowImGuiFileDialog(const std::string& title, const std::vector<FileFilter>& file_filters,
                          const std::string& initial_path, const bool& confirm_overwrite,
                          std::function<void(std::string)> callback, bool folder_mode)
{
    m_file_dialog_callback          = callback;
    m_init_file_dialog              = true;
    m_imgui_file_dialog_folder_mode = folder_mode;

    std::stringstream filter_stream;
    for(const auto& filter : file_filters)
    {
        std::stringstream extensions;
        for(size_t i = 0; i < filter.m_extensions.size(); ++i)
        {
            extensions << "." << filter.m_extensions[i];
            if(i < filter.m_extensions.size() - 1)
            {
                extensions << ",";
            }
        }

        filter_stream << filter.m_name << " (" << extensions.str() << "){"
                      << extensions.str() << "}";
        if(&filter != &file_filters.back())
        {
            filter_stream << ",";
        }
    }

    // An empty filter list leaves ImGuiFileDialog with no dLGFilters, which then hides
    // every regular file (directory-only mode). The regex form matches any file name,
    // including extensionless executables (Linux/macOS).
    std::string filter_string = filter_stream.str();
    if(filter_string.empty())
    {
        filter_string = "All files{((.*))}";
    }

    IGFD::FileDialogConfig config;
    config.path  = initial_path;
    config.flags = confirm_overwrite
                       ? ImGuiFileDialogFlags_Default
                       : ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_HideColumnType;
    // A nullptr filter switches ImGuiFileDialog into directory-selection mode.
    const char* filters = folder_mode ? nullptr : filter_string.c_str();
    ImGuiFileDialog::Instance()->OpenDialog(FILE_DIALOG_NAME, title, filters, config);
}

#if defined(ROCPROFVIS_DEVELOPER_MODE) && defined(ROCPROFVIS_ENABLE_REMOTE)

void
AppWindow::HandleTestRemoteSSH()
{
    if(!m_ssh_test_dialog)
    {
        m_ssh_test_dialog = std::make_unique<SshTestDialog>(this);
    }
    m_ssh_test_dialog->Show();
}

#endif // ROCPROFVIS_DEVELOPER_MODE && ROCPROFVIS_ENABLE_REMOTE
void
AppWindow::UpdateStatusBar()
{
    // Update status message every N frames to avoid rebuilding the string each
    // frame while background work is in flight.
    constexpr int STATUS_BAR_UPDATE_FRAME_STEP = 4;
    if(ImGui::GetFrameCount() % STATUS_BAR_UPDATE_FRAME_STEP == 0)
    {
        // Get number of pending requests from data provider
        size_t pending_requests = 0;
        for(const auto& [id, project] : m_items)
        {
            auto root_view = dynamic_cast<RootView*>(project->GetView().get());
            if(root_view)
            {
                auto data_provider = root_view->GetDataProvider();
                if(data_provider)
                {
                    pending_requests += data_provider->GetPendingRequestCount();
                }
            }
        }
        // also check if there are any cleanup jobs pending
        size_t clean_up_jobs = m_provider_cleanup_jobs.size();
        // background operations tracked by the monitor (SSH, profiler, etc.)
        AppMonitor* monitor     = AppMonitor::GetInstance();
        size_t      monitor_ops = monitor->GetActiveOperationCount();

        // Live remote/SSH sessions (connections), including idle ones between
        // operations. Only meaningful when remote support is built.
        size_t remote_sessions = 0;
#ifdef ROCPROFVIS_ENABLE_REMOTE
        remote_sessions = SshSession::ActiveSessionCount();
#endif

        // In-flight work drives the busy spinner; an idle-but-connected SSH
        // session is surfaced without the spinner so the user knows a connection
        // is open without implying activity.
        bool has_active_work = (pending_requests > 0 || clean_up_jobs > 0 || monitor_ops > 0);

        std::vector<std::string> segments;
        if(pending_requests > 0)
        {
            segments.push_back("Working: " + std::to_string(pending_requests) +
                               " pending request(s)");
        }
        if(clean_up_jobs > 0)
        {
            segments.push_back("Cleaning up: " + std::to_string(clean_up_jobs) +
                               " pending job(s)");
        }
        if(monitor_ops > 0)
        {
            // Break the generic count down by domain (SSH / profiler) so the
            // user can tell what is keeping the app busy. The domain grouping
            // lives here (the caller), not in the generic AppMonitor.
            size_t remote_ops =
                monitor->GetActiveOperationCount(MonitorOperationType::SshConnection) +
                monitor->GetActiveOperationCount(MonitorOperationType::SshAuthentication) +
                monitor->GetActiveOperationCount(MonitorOperationType::FileTransfer) +
                monitor->GetActiveOperationCount(MonitorOperationType::DirectoryListing);
            size_t profiler_ops =
                monitor->GetActiveOperationCount(MonitorOperationType::ProfilerSession);
            std::string detail;
            if(remote_ops > 0)
            {
                detail = std::to_string(remote_ops) + " SSH";
            }
            if(profiler_ops > 0)
            {
                detail += (detail.empty() ? "" : ", ") +
                          std::to_string(profiler_ops) + " profiler";
            }
            segments.push_back("Background: " + std::to_string(monitor_ops) +
                               " operation(s)" +
                               (detail.empty() ? "" : " (" + detail + ")"));
        }
        if(remote_sessions > 0)
        {
            segments.push_back("SSH: " + std::to_string(remote_sessions) + " session(s)");
        }

        if(!segments.empty())
        {
            m_status_message.clear();
            for(size_t i = 0; i < segments.size(); ++i)
            {
                m_status_message += (i > 0 ? " | " : "") + segments[i];
            }
            m_status_show_busy_indicator = has_active_work;
        }
        else
        {
            m_status_message             = "Ready";
            m_status_show_busy_indicator = false;
        }
    }
}

void
AppWindow::RenderStatusBar()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, style.ItemSpacing);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);

    ImGui::AlignTextToFramePadding();
    ImGui::Dummy(ImVec2(0.f, ImGui::GetFrameHeight()));
    ImGui::SameLine();
    if(m_status_show_busy_indicator)
    {
        float radius = ImGui::GetTextLineHeight() * 0.25f;
        RenderLoadingIndicator(settings.GetColor(Colors::kTextDim), nullptr,
                               kCenterVertical, radius);
        ImGui::SameLine(0.f, style.ItemSpacing.x);
    }
    ImGui::TextUnformatted(m_status_message.c_str());
    ImGui::PopStyleVar(2);
}

#ifdef ROCPROFVIS_DEVELOPER_MODE
void
AppWindow::RenderDeveloperMenu()
{
    if(ImGui::BeginMenu("Developer Options"))
    {
        // Toggele ImGui's built-in metrics window
        if(ImGui::MenuItem("Show Metrics", nullptr, m_show_metrics))
        {
            m_show_metrics = !m_show_metrics;
        }
        // Toggle debug output window
        if(ImGui::MenuItem("Show Debug Output Window", nullptr, m_show_debug_window))
        {
            m_show_debug_window = !m_show_debug_window;
            if(m_show_debug_window)
            {
                ImGui::SetWindowFocus("Debug Window");
            }
        }
        // Open a file to test the DataProvider
        if(ImGui::MenuItem("Test Provider", nullptr))
        {
            std::vector<FileFilter> file_filters;
            FileFilter trace_filter;
            trace_filter.m_name       = "Traces";
            trace_filter.m_extensions = { "db", "rpd" };
            file_filters.push_back(trace_filter);
            ShowOpenFileDialog("Choose File", file_filters, "",
                               [this](std::string file_path) -> void {
                                   std::string config_path = get_application_config_path(true);
                                    rocprofvis_controller_t*   controller = rocprofvis_controller_alloc(file_path.c_str(), config_path.c_str());
                                   if(controller)
                                   {
                                       this->m_test_data_provider.FetchTrace(controller, file_path);
                                       spdlog::info("Opening file: {}", file_path);
                                       m_show_provider_test_widow = true;
                                   }
                                   else
                                   {
                                       rocprofvis_controller_free(controller);
                                   }
                               });
        }
#ifdef ROCPROFVIS_ENABLE_REMOTE
        if(ImGui::MenuItem("Open Remote...", nullptr, false))
        {
            HandleTestRemoteSSH();
        }
#endif        
        ImGui::EndMenu();
    }
}

void
RenderProviderTest(DataProvider& provider)
{
    ImGui::Begin("Data Provider Test Window", nullptr, ImGuiWindowFlags_None);

    static char    track_index_buffer[64]     = "0";
    static char    end_track_index_buffer[64] = "1";  // for setting table track range
    static uint8_t group_id_counter           = 0;

    // Callback function to filter non-numeric characters
    auto NumericFilter = [](ImGuiInputTextCallbackData* data) -> int {
        if(data->EventChar < '0' || data->EventChar > '9')
        {
            // Allow backspace
            if(data->EventChar != '\b')
            {
                return 1;  // Block non-numeric characters
            }
        }
        return 0;  // Allow numeric characters
    };

    ImGui::InputText("Track index", track_index_buffer, IM_ARRAYSIZE(track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    int index = std::atoi(track_index_buffer);

    ImGui::Separator();
    ImGui::Text("Table Parameters");
    ImGui::InputText("End Track index", end_track_index_buffer,
                     IM_ARRAYSIZE(end_track_index_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);

    static char row_start_buffer[64] = "-1";
    ImGui::InputText("Start Row", row_start_buffer, IM_ARRAYSIZE(row_start_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t start_row = std::atoi(row_start_buffer);

    static char row_count_buffer[64] = "-1";
    ImGui::InputText("Row Count", row_count_buffer, IM_ARRAYSIZE(row_count_buffer),
                     ImGuiInputTextFlags_CallbackCharFilter, NumericFilter);
    uint64_t row_count = std::atoi(row_count_buffer);

    TimelineModel& timeline = provider.DataModel().GetTimeline();
    if(ImGui::Button("Fetch Single Track Event Table"))
    {
        provider.FetchSingleTrackEventTable(index, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", "", "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Fetch Multi Track Event Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackEventTable(vect, timeline.GetStartTime(),
                                           timeline.GetEndTime(), "", "", "", start_row,
                                           row_count);
    }
    if(ImGui::Button("Print Event Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kEventTable);
    }

    if(ImGui::Button("Fetch Single Track Sample Table"))
    {
        provider.FetchSingleTrackSampleTable(index, timeline.GetStartTime(),
                                             timeline.GetEndTime(), "", start_row,
                                             row_count);
    }
    if(ImGui::Button("Fetch Multi Track Sample Table"))
    {
        int                   end_index = std::atoi(end_track_index_buffer);
        std::vector<uint64_t> vect;
        for(int i = index; i < end_index; ++i)
        {
            vect.push_back(i);
        }
        provider.FetchMultiTrackSampleTable(vect, timeline.GetStartTime(),
                                            timeline.GetEndTime(), "", start_row,
                                            row_count);
    }
    if(ImGui::Button("Print Sample Table"))
    {
        provider.DataModel().GetTables().DumpTable(TableType::kSampleTable);
    }

    ImGui::Separator();

    if(ImGui::Button("Fetch Track"))
    {
        provider.FetchTrack(index, timeline.GetStartTime(), timeline.GetEndTime(), 1000,
                            group_id_counter++);
    }

    if(ImGui::Button("Fetch Whole Track"))
    {
        provider.FetchWholeTrack(index, timeline.GetStartTime(), timeline.GetEndTime(),
                                 1000, group_id_counter++);
    }
    if(ImGui::Button("Delete Track"))
    {
        timeline.FreeTrackData(index);
    }
    if(ImGui::Button("Print Track"))
    {
        timeline.DumpTrack(index);
    }
    if(ImGui::Button("Print Track List"))
    {
        timeline.DumpMetaData();
    }

    ImGui::End();
}

void
AppWindow::RenderDebugOuput()
{
    if(m_show_metrics)
    {
        ImGui::ShowMetricsWindow(&m_show_metrics);
    }

    if(m_show_debug_window)
    {
        DebugWindow::GetInstance()->Render();
    }

    if(m_show_provider_test_widow)
    {
        RenderProviderTest(m_test_data_provider);
    }
}
#endif  // ROCPROFVIS_DEVELOPER_MODE

#ifdef ROCPROFVIS_ENABLE_PROFILER
// TEMPORARY (profiler launch): remove guard when the feature graduates.
void
AppWindow::ShowProfilerLauncher()
{
    // Create dialog if it doesn't exist (lazy initialization)
    // Dialog owns its own DataProvider - not tied to any specific trace
    if (!m_profiler_launcher_dialog)
    {
        m_profiler_launcher_dialog = std::make_unique<ProfilerLauncherDialog>(this);
    }

    m_profiler_launcher_dialog->Show();
}
#endif  // ROCPROFVIS_ENABLE_PROFILER

}  // namespace View
}  // namespace RocProfVis

