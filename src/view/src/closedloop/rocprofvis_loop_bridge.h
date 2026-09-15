// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

#ifdef ROCPROFVIS_ENABLE_REMOTE
class RemoteTraceOrchestrator;
class RemoteUri;
#endif

// How far an offer has got. The model proposes, the user answers, and only then
// does Optiq touch the source tree or start a run - the same contract the
// analysis-script tool already works under.
enum class LoopState : uint8_t
{
    kIdle,
    // With the user, who has not answered yet.
    kPending,
    // Approved: the editor is applying and building, or the profiler is running.
    kWorking,
    // Finished, failed, or refused. TakeResult() says which.
    kSettled
};

// What the loop is doing. Only kEdit and kRun are ever put to the user; kAttach
// reads one config file over a connection they already authorised, so it starts
// on its own.
enum class LoopAction : uint8_t
{
    kNone,
    kEdit,
    kRun,
    kAttach
};

/**
 * @brief The closed loop: profile, analyze, edit, profile again.
 *
 * Optiq is the analyst; the VS Code / Cursor extension is the only thing that
 * writes source. So an approved edit leaves here as one loopback HTTP call to
 * the editor rather than as a file write, and an approved run goes through the
 * launcher dialog the user already knows rather than a hidden process. This
 * class is to the loop what OptiqActions is to the trace view: the single place
 * that acts on the assistant's behalf.
 *
 * Nothing here acts on its own. ProposeEdit / ProposeRun park an offer,
 * Render() draws the Approve / Reject card, and Pending() - polled from the
 * panel's Update() like a script the user is reading - is what starts the work
 * and later reports it finished. Neither wait has a data request behind it,
 * which is why the panel asks here rather than polling the data provider.
 */
class LoopBridge
{
public:
    static LoopBridge& GetInstance();

    // --- Tool side ---------------------------------------------------------

    // Offers a search-and-replace edit and a build of the workspace behind it.
    // Returns the refusal to hand the model when the offer cannot be made, or
    // an empty string once the offer is parked with the user.
    std::string ProposeEdit(const std::string& file, const std::string& find,
                            const std::string& replace, const std::string& why);

    // Offers a run of a saved launch profile. Same contract as ProposeEdit: the
    // model never names a command line, only a profile the user already
    // authored in File > Launch Profiler.
    std::string ProposeRun(const std::string& profile, const std::string& why);

    // True while the offer is with the user, or the work they approved is still
    // running. Drives the state machine, so it must be called every frame.
    bool Pending();

    // What to tell the model, and clears the offer.
    std::string TakeResult();

    // Read-only and answered in the same call: each is one loopback round trip
    // rather than a query over the trace.
    std::string FindSource(const std::string& query) const;
    std::string ReadSource(const std::string& file) const;
    std::string Status() const;

    // True once an editor has been located, locally or on the remote host.
    bool Attached() const;

    // Goes looking for an editor running under VS Code Remote-SSH: reads its
    // handshake over the SSH profile already configured for profiling. Returns
    // the refusal to hand the model when the search cannot start, or an empty
    // string once it is under way.
    std::string Attach();

    // Saved launch profiles, by name, or the reason there are none to offer.
    std::string LaunchProfiles() const;

    // --- UI side -----------------------------------------------------------

    // The lap ring and the Approve / Reject card, above the composer. Records
    // the user's answer rather than acting on it, so a tool never runs halfway
    // through the frame that draws it.
    void Render();

private:
    LoopBridge() = default;

    void StartApprovedWork();
    // Each returns true once the work behind the approval has settled.
    bool PollEdit();
    bool PollRun();
    bool PollAttach();
    // Where to send a request and the token to send with it. False when no
    // editor has been found. Prefers a local handshake, read fresh each time so
    // restarting the editor needs no reattach; falls back to the remote one
    // Attach() resolved.
    bool Endpoint(std::string& url_out, std::string& token_out) const;
    void RecordLap(const std::string& trace_path);
    // The three-dot cycle and the lap counter beside it. stage is which dot is
    // lit; detail is the line under the counter.
    void RenderRing(uint32_t lap, int32_t stage, const char* detail) const;

    LoopState  m_state  = LoopState::kIdle;
    LoopAction m_action = LoopAction::kNone;

    std::string m_file;
    std::string m_find;
    std::string m_replace;
    std::string m_profile;
    std::string m_why;

    std::string m_result;

    // Traces this loop produced, oldest first. A second one is what makes the
    // comparison - and so the visible delta - possible.
    std::vector<std::string> m_laps;

    // Set by Render(), acted on by Pending().
    bool m_approve_requested = false;
    bool m_reject_requested  = false;

    // The apply-and-build call, which runs for as long as the build does.
    std::future<std::string> m_edit_call;

    // A remote editor, once Attach() has found one. The URL is the tunnelled
    // localhost address VS Code handed the extension, so requests still never
    // leave this machine.
    bool        m_remote_attached = false;
    std::string m_remote_url;
    std::string m_remote_token;
    std::string m_remote_workspace;

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // Reuses the profiling SSH pipeline rather than opening a second kind of
    // connection: connect, authenticate, then one command. Auth prompts are
    // drawn by AppWindow's RenderSshAuthModals, which walks every live session.
    std::shared_ptr<RemoteUri>               m_attach_uri;
    std::unique_ptr<RemoteTraceOrchestrator> m_attach;
#endif
};

}  // namespace View
}  // namespace RocProfVis
