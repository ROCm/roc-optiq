// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_event_manager.h"
#include "rocprofvis_widget.h"

#include <cstdint>
#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TimelineSelection;

// Where a script the assistant offered has got to. A script it wrote never
// runs unattended: the editor holds it until the user reads it and decides.
enum class ScriptApproval : uint8_t
{
    // Nothing offered. Manual runs stay in this state throughout.
    kNone,
    kPending,
    kRejected,
    kRunning,
    kFinished,
    // Approved, but the run could not be started at all.
    kFailedToStart
};

/**
 * @brief The Script tab of the details panel: a Python editor over the trace
 * that owns it.
 *
 * One per trace, built and owned by AnalysisView like the other tabs, so Run
 * goes straight to its own DataProvider rather than working out which tab is
 * in front.
 */
class ScriptEditor : public RocWidget
{
public:
    ScriptEditor(DataProvider& data_provider,
                 std::shared_ptr<TimelineSelection> timeline_selection);
    ~ScriptEditor() override;

    void Render() override;

    // Offers a script the assistant wrote. Nothing runs until the user presses
    // Run. Selecting the tab is the caller's job, through OptiqActions, so that
    // every UI change the assistant makes goes through one place.
    void ProposeScript(const std::string& source);

    ScriptApproval ProposalState() const;
    void           ClearProposal();

private:
    void        Run();
    void        Cancel();
    void        Reject();
    void        LoadFromFile();
    void        SaveToFile();
    void        ReadFile(const std::string& path);
    void        WriteFile(const std::string& path);
    void        RenderHeaderCard();
    void        RenderProposalBanner();
    void        RenderSource(const ImVec2& size);
    void        RenderOutput(const ImVec2& size);
    // split_extent is the workspace along the axis being split; cross_length is
    // the handle's length across it.
    void        RenderSplitter(bool columns, float split_extent, float cross_length);
    bool        CanRun() const;
    std::string SubtitleText() const;

    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;

    bool        m_running;
    // Share of the workspace given to the Result pane, as dragged.
    float       m_result_ratio;
    uint64_t    m_progress_percent;
    std::string m_source;
    std::string m_output;
    bool        m_output_is_error;
    std::string m_status;
    std::string m_file_path;
    // Trace the running script belongs to, which is what the completion and
    // progress events carry. Every editor hears every event, so without it a
    // script finishing on another trace would drop this one out of Running and
    // wipe its output. Empty when nothing is running here.
    std::string    m_running_source_id;
    ScriptApproval m_approval;

    EventManager::SubscriptionToken m_complete_token;
    EventManager::SubscriptionToken m_progress_token;
};

}  // namespace View
}  // namespace RocProfVis
