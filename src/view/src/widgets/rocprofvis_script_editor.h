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

// Where a script the assistant offered has got to. A script it wrote is the
// one thing it does that runs real code, so it never runs unattended: the
// editor holds it until the user reads it and decides.
enum class ScriptApproval : uint8_t
{
    // Nothing offered. Manual runs stay in this state throughout.
    kNone,
    kPending,
    kRejected,
    // Approved and handed to the interpreter.
    kRunning,
    kFinished,
    // Approved, but the run could not be started at all.
    kFailedToStart
};

/**
 * @brief The Script tab of the details panel: a Python editor over the trace
 * that owns it.
 *
 * One per trace, built and owned by AnalysisView like the other tabs, so the
 * source, the output, and any run belong to the trace the user is looking at.
 * That is also what lets Run go straight to its own DataProvider rather than
 * working out which tab is in front.
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

    // Where that offer has got to, and how the caller lets go of it once it has
    // read the answer.
    ScriptApproval ProposalState() const;
    void           ClearProposal();

private:
    void Run();
    void Cancel();
    void Reject();
    void LoadFromFile();
    void SaveToFile();
    void ReadFile(const std::string& path);
    void WriteFile(const std::string& path);
    // Title, subtitle and the action row, in the header band the rest of the
    // app's panels use.
    void RenderHeaderCard();
    // Only while a script is waiting on the user: says where it came from and
    // what pressing Run would do.
    void RenderProposalBanner();
    void RenderSource(const ImVec2& size);
    void RenderOutput(const ImVec2& size);
    bool CanRun() const;
    // What the subtitle under the title says: the loaded file, or how the
    // script got here.
    std::string SubtitleText() const;

    DataProvider&                      m_data_provider;
    std::shared_ptr<TimelineSelection> m_timeline_selection;

    bool        m_running;
    uint64_t    m_progress_percent;
    std::string m_source;
    std::string m_output;
    // Tints the output pane, so a traceback reads as a failure at a glance
    // instead of looking like results.
    bool        m_output_is_error;
    std::string m_status;
    std::string m_file_path;
    // Trace the running script belongs to, which is what the completion and
    // progress events carry. Without it, closing any other tab posts a
    // completion that drops this editor out of Running and wipes its output.
    std::string    m_running_source_id;
    ScriptApproval m_approval;

    EventManager::SubscriptionToken m_complete_token;
    EventManager::SubscriptionToken m_progress_token;
};

}  // namespace View
}  // namespace RocProfVis
