// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_script_editor.h"

#include <cfloat>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

#include "imgui.h"

#include "rocprofvis_appwindow.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_events.h"
#include "rocprofvis_notification_manager.h"
#include "rocprofvis_project.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "widgets/rocprofvis_gui_helpers.h"

namespace RocProfVis
{
namespace View
{

constexpr float SCRIPT_EDITOR_DEFAULT_WIDTH  = 760.0f;
constexpr float SCRIPT_EDITOR_DEFAULT_HEIGHT = 520.0f;
constexpr float SCRIPT_EDITOR_MIN_WIDTH      = 420.0f;
constexpr float SCRIPT_EDITOR_MIN_HEIGHT     = 280.0f;
constexpr float SCRIPT_EDITOR_OUTPUT_HEIGHT  = 160.0f;
constexpr float SCRIPT_EDITOR_SOURCE_MIN_HEIGHT = 80.0f;

// Even-spacing sample from SCRIPTING.md so the first Run does something
// visible against a loaded system trace.
char const* const kDefaultScript =
    "track = None\n"
    "for t in optiq.selection.tracks:\n"
    "    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:\n"
    "        track = t\n"
    "        break\n"
    "if track is None:\n"
    "    optiq.result.text('No event track in the selection')\n"
    "else:\n"
    "    events = track.events(start=optiq.selection.start, end=optiq.selection.end)\n"
    "    if len(events) < 2:\n"
    "        optiq.result.text('Need at least two events on ' + track.name)\n"
    "    else:\n"
    "        gaps = [events[i].start - events[i - 1].end for i in range(1, len(events))]\n"
    "        mean = sum(gaps) / len(gaps)\n"
    "        max_dev = max(abs(g - mean) for g in gaps)\n"
    "        even = max_dev <= (abs(mean) * 0.1)\n"
    "        optiq.result.text(track.name)\n"
    "        optiq.result.text('events=' + str(len(events)))\n"
    "        optiq.result.text('mean_gap=' + str(mean))\n"
    "        optiq.result.text('max_dev=' + str(max_dev))\n"
    "        optiq.result.text('even' if even else 'uneven')\n";

ScriptEditor* ScriptEditor::s_instance = nullptr;

ScriptEditor*
ScriptEditor::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new ScriptEditor();
    }
    return s_instance;
}

void
ScriptEditor::DestroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

ScriptEditor::ScriptEditor()
: m_visible(false)
, m_running(false)
, m_progress_percent(0)
, m_source(kDefaultScript)
, m_status("Idle")
, m_complete_token(EventManager::InvalidSubscriptionToken)
, m_progress_token(EventManager::InvalidSubscriptionToken)
{
    m_widget_name = GenUniqueName("ScriptEditor");

    m_complete_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kScriptExecuteComplete),
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<ScriptExecuteCompleteEvent> event =
                std::dynamic_pointer_cast<ScriptExecuteCompleteEvent>(e);
            if(!event)
            {
                return;
            }
            m_running            = false;
            m_progress_percent   = 0;
            m_running_project_id.clear();
            m_output = event->GetText();
            if(!event->Succeeded())
            {
                if(!m_output.empty())
                {
                    m_output += '\n';
                }
                m_output += event->GetError();
                m_status = "Failed";
            }
            else
            {
                m_status = "Done";
            }
        });

    m_progress_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        [this](std::shared_ptr<RocEvent> e) {
            std::shared_ptr<RequestProgressUpdateEvent> event =
                std::dynamic_pointer_cast<RequestProgressUpdateEvent>(e);
            if(!event || event->GetRequestType() != RequestType::kExecuteScript)
            {
                return;
            }
            m_progress_percent = event->GetProgressPercent();
            if(!event->GetMessage().empty())
            {
                m_status = event->GetMessage();
            }
        });
}

ScriptEditor::~ScriptEditor()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kScriptExecuteComplete), m_complete_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate), m_progress_token);
}

void
ScriptEditor::ToggleVisible()
{
    m_visible = !m_visible;
}

bool*
ScriptEditor::VisiblePtr()
{
    return &m_visible;
}

void
ScriptEditor::RenderToolbarButton()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kBgFrame)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonHovered)));
    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kButtonActive)));
    ImGui::PushStyleColor(ImGuiCol_Text,
                          ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextMain)));
    if(ImGui::Button("Script"))
    {
        GetInstance()->ToggleVisible();
    }
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Open the Python script editor");
    }
}

void
ScriptEditor::Render()
{
    if(!m_visible)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(SCRIPT_EDITOR_DEFAULT_WIDTH, SCRIPT_EDITOR_DEFAULT_HEIGHT),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(SCRIPT_EDITOR_MIN_WIDTH, SCRIPT_EDITOR_MIN_HEIGHT), ImVec2(FLT_MAX, FLT_MAX));
    if(ImGui::Begin("Script Editor", &m_visible))
    {
        RenderToolbar();
        ImGui::Separator();
        RenderSource();
        ImGui::Separator();
        RenderOutput();
    }
    ImGui::End();
}

void
ScriptEditor::RenderToolbar()
{
    bool can_run = CanRun();
    if(!can_run)
    {
        ImGui::BeginDisabled();
    }
    if(ImGui::Button("Run"))
    {
        Run();
    }
    if(!can_run)
    {
        ImGui::EndDisabled();
    }
    if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        if(m_running)
        {
            SetTooltipStyled("A script is already running");
        }
        else
        {
            SetTooltipStyled("Run against the open trace (open a loaded trace first)");
        }
    }

    ImGui::SameLine();
    if(!m_running)
    {
        ImGui::BeginDisabled();
    }
    if(ImGui::Button("Cancel"))
    {
        Cancel();
    }
    if(!m_running)
    {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if(ImGui::Button("Load"))
    {
        LoadFromFile();
    }
    ImGui::SameLine();
    if(ImGui::Button("Save"))
    {
        SaveToFile();
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(m_status.c_str());
    if(m_running)
    {
        ImGui::SameLine();
        ImGui::Text("(%u%%)", static_cast<unsigned>(m_progress_percent));
    }
    if(!m_file_path.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_file_path.c_str());
    }
}

void
ScriptEditor::RenderSource()
{
    // InputTextMultiline treats size.x == 0 as the default item width (~65% of
    // the window). Negative x means remaining content width, matching Output.
    ImVec2 size(-FLT_MIN, ImGui::GetContentRegionAvail().y - SCRIPT_EDITOR_OUTPUT_HEIGHT);
    if(size.y < SCRIPT_EDITOR_SOURCE_MIN_HEIGHT)
    {
        size.y = SCRIPT_EDITOR_SOURCE_MIN_HEIGHT;
    }
    InputTextMultilineString("##script_source", m_source, size,
                             ImGuiInputTextFlags_AllowTabInput);
}

void
ScriptEditor::RenderOutput()
{
    ImGui::TextUnformatted("Output");
    ImGui::BeginChild("##script_output", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(m_output.empty() ? "(no output)" : m_output.c_str());
    ImGui::EndChild();
}

DataProvider*
ScriptEditor::CurrentDataProvider() const
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app || !app->GetCurrentProject())
    {
        return nullptr;
    }
    return DataProviderForProject(app->GetCurrentProject()->GetID());
}

DataProvider*
ScriptEditor::DataProviderForProject(const std::string& project_id) const
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app)
    {
        return nullptr;
    }
    Project* project = app->GetProject(project_id);
    if(!project)
    {
        return nullptr;
    }
    RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
    if(!root_view)
    {
        return nullptr;
    }
    return root_view->GetDataProvider();
}

bool
ScriptEditor::CanRun() const
{
    if(m_running)
    {
        return false;
    }
    DataProvider* provider = CurrentDataProvider();
    return provider && provider->GetState() == ProviderState::kReady;
}

void
ScriptEditor::Run()
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app || !app->GetCurrentProject())
    {
        m_status = "Open a trace first";
        NotificationManager::GetInstance().Show("Open a trace before running a script",
                                                NotificationLevel::Warning);
        return;
    }
    Project*      project  = app->GetCurrentProject();
    DataProvider* provider = CurrentDataProvider();
    if(!provider || provider->GetState() != ProviderState::kReady)
    {
        m_status = "Trace still loading";
        return;
    }

    std::vector<uint64_t> track_ids;
    double                start_ts   = 0.0;
    double                end_ts     = 0.0;
    bool                  have_range = false;
    TraceView* trace_view = dynamic_cast<TraceView*>(project->GetView().get());
    if(trace_view && trace_view->GetTimelineSelection())
    {
        TimelineSelection* selection = trace_view->GetTimelineSelection().get();
        selection->GetSelectedTracks(track_ids);
        have_range = selection->GetSelectedTimeRange(start_ts, end_ts);
    }
    if(!have_range)
    {
        start_ts = provider->DataModel().GetTimeline().GetStartTime();
        end_ts   = provider->DataModel().GetTimeline().GetEndTime();
    }

    m_output.clear();
    m_progress_percent = 0;
    if(!provider->ExecuteScript(m_source, track_ids, start_ts, end_ts))
    {
        m_status = "Could not start script";
        NotificationManager::GetInstance().Show("Could not start the script",
                                                NotificationLevel::Error);
        return;
    }
    m_running            = true;
    m_running_project_id = project->GetID();
    m_status             = "Running";
}

void
ScriptEditor::Cancel()
{
    DataProvider* provider = DataProviderForProject(m_running_project_id);
    if(!provider)
    {
        return;
    }
    provider->CancelScript();
    m_status = "Cancelling";
}

void
ScriptEditor::LoadFromFile()
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app)
    {
        return;
    }
    FileFilter py_filter;
    py_filter.m_name       = "Python";
    py_filter.m_extensions = { "py" };
    app->ShowOpenFileDialog("Load Script", { py_filter }, m_file_path,
                            [this](std::string path) { ReadFile(path); });
}

void
ScriptEditor::SaveToFile()
{
    AppWindow* app = AppWindow::GetInstance();
    if(!app)
    {
        return;
    }
    FileFilter py_filter;
    py_filter.m_name       = "Python";
    py_filter.m_extensions = { "py" };
    app->ShowSaveFileDialog("Save Script", { py_filter }, m_file_path,
                            [this](std::string path) { WriteFile(path); });
}

void
ScriptEditor::ReadFile(const std::string& path)
{
    if(path.empty())
    {
        return;
    }
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        NotificationManager::GetInstance().Show("Could not read script file",
                                                NotificationLevel::Error);
        return;
    }
    std::string text((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    m_source    = std::move(text);
    m_file_path = path;
    m_status    = "Loaded";
}

void
ScriptEditor::WriteFile(const std::string& path)
{
    if(path.empty())
    {
        return;
    }
    std::ofstream out(path, std::ios::binary);
    if(!out)
    {
        NotificationManager::GetInstance().Show("Could not write script file",
                                                NotificationLevel::Error);
        return;
    }
    out.write(m_source.data(), static_cast<std::streamsize>(m_source.size()));
    m_file_path = path;
    m_status    = "Saved";
}

}  // namespace View
}  // namespace RocProfVis
