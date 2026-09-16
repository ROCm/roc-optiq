// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The closed loop's only moving part. It holds the offer the model made, draws
// the card the user answers, and - once they approve - either asks the editor
// extension to apply and build, or starts a saved profiler run. Nothing else in
// Optiq writes source or launches a profiler on the assistant's behalf.
//
// The editor is reached over loopback because the extension already has the
// workspace open, and going through it means there is one writer for the source
// tree rather than two.
//
// Under VS Code Remote-SSH the extension host runs on the GPU box, so it writes
// its handshake on that filesystem and listens on that loopback - neither of
// which this machine can see. Attach() closes the gap with the pipeline that is
// already here: the profiling SSH profile runs one command to read the remote
// handshake, in which the extension has put a VS Code-tunnelled localhost URL.
// So Optiq still only ever dials its own loopback, and neither port forwarding
// nor file upload had to be invented for it.

#include "rocprofvis_loop_bridge.h"

// Vendored single-header library; it does not compile clean at /W4.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100 4127 4244 4267 4456 4458 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "imgui.h"
#include "json.h"
// Brings in spdlog with the log level the rest of the app builds against.
// Including spdlog directly here would redefine SPDLOG_ACTIVE_LEVEL once a
// project header pulled this in behind it.
#include "rocprofvis_core.h"

#include "icons/rocprovfis_icon_defines.h"
#include "profiler/rocprofvis_profiler_launcher_dialog.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#ifdef ROCPROFVIS_ENABLE_REMOTE
#    include "remote/rocprofvis_remote_trace_orchestrator.h"
#    include "remote/rocprofvis_ssh_connection_store.h"
#    include "remote/rocprofvis_ssh_session.h"
#    include "remote/rocprofvis_ssh_uri.h"
#endif

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{

// Opening the socket is loopback, so this only has to cover an extension host
// that is still waking up. A dead port fails here, fast.
constexpr int LOOP_CONNECT_TIMEOUT_SECONDS = 10;
// Waiting for the answer is a different matter. A search runs through VS Code's
// file index, and the first one after a window opens blocks until that index is
// built - which over a Remote-SSH filesystem is tens of seconds on a real tree.
// Timing that out looks exactly like "no editor there", so it is sized for the
// cold case rather than the warm one.
constexpr int LOOP_QUERY_TIMEOUT_SECONDS = 180;
// Applying the edit is instant; the build behind it is not, and a cold build of
// a real project is the case this has to survive.
constexpr int LOOP_BUILD_TIMEOUT_SECONDS = 1800;
// Enough compiler output to fix the error it names, without spending the
// model's context on a wall of warnings.
constexpr size_t LOOP_MAX_OUTPUT_CHARS = 6000;
// Cap on what one read hands back, for the same reason.
constexpr size_t LOOP_MAX_SOURCE_CHARS = 24000;

// The extension owns this file: it writes it on activate and removes it on
// shutdown, so a missing file is the honest answer to "is an editor attached".
constexpr char LOOP_HANDSHAKE_FILE[] = "optiq-ide.json";
constexpr char LOOP_TOKEN_HEADER[]   = "X-Optiq-Token";
constexpr char LOOP_JSON_TYPE[]      = "application/json";
// Every route here exists in the extension that ships beside this file, so a
// 404 means the editor is running an older one - which otherwise degrades into
// a tool that silently answers nothing.
constexpr int LOOP_HTTP_NOT_FOUND = 404;

constexpr float LOOP_RING_RADIUS = 14.0f;
constexpr float LOOP_RING_DOT    = 3.5f;
constexpr int   LOOP_RING_STAGES = 3;
// IM_PI is declared in imgui_internal.h, which nothing in the view includes.
constexpr float LOOP_PI            = 3.14159265f;
constexpr int   LOOP_STAGE_PROFILE = 0;
constexpr int   LOOP_STAGE_ANALYZE = 1;
constexpr int   LOOP_STAGE_EDIT    = 2;

/*
 * Copies the handshake the extension wrote on the remote host somewhere with a
 * fixed absolute path, so it can then be downloaded.
 *
 * Copy-then-download rather than reading the command's own output: stdout
 * arrives in chunks that the session only drains one per frame, and a `cat` of
 * a 200-byte file finishes long before a frame boundary - so the completion is
 * seen with nothing collected. Download is the path traces already come back
 * through, and it either produces the file or fails loudly.
 *
 * The command honours XDG_CONFIG_HOME the same way get_application_config_path
 * does on Linux, which is the platform a GPU box runs.
 */
constexpr char LOOP_REMOTE_HANDSHAKE_PATH[] = "/tmp/optiq-ide-handshake.json";
// Removes the previous copy first: without that, an editor that has since shut
// down still leaves a file to download, and a dead port reads as an attach.
constexpr char LOOP_REMOTE_HANDSHAKE_COMMAND[] =
    "rm -f /tmp/optiq-ide-handshake.json; "
    "cp \"${XDG_CONFIG_HOME:-$HOME/.config}/rocm-optiq/optiq-ide.json\" "
    "/tmp/optiq-ide-handshake.json";

constexpr char LOOP_NO_EDITOR[] =
    "No editor is attached. Call loop_status, which looks on this machine and "
    "then on the profiling host. If it finds nothing, tell the user to install "
    "the Optiq extension in VS Code or Cursor and open the workspace holding "
    "this source.";

struct IdeEndpoint
{
    // Always an address on this machine: either the extension's own loopback
    // port, or the localhost port VS Code tunnelled for a Remote-SSH one.
    std::string url;
    std::string token;
    std::string workspace;
    std::string remote;
};

// Optiq only ever dials loopback. The handshake is a file that another process
// wrote, so this is what stops a stray or edited one from pointing source at
// somewhere else.
bool
IsLoopbackUrl(const std::string& url)
{
    return url.rfind("http://127.0.0.1:", 0) == 0 ||
           url.rfind("http://localhost:", 0) == 0;
}

/*
 * Reads one handshake document, wherever it came from.
 *
 * require_tunnel is set for a handshake read off the remote host. Its bare port
 * is a port on *that* machine, so accepting it would send source at whatever
 * happens to be listening on the same number here. Only externalUrl - which the
 * extension gets from asExternalUri, and which VS Code forwards - is meaningful
 * from this side.
 */
bool
ParseHandshake(const std::string& text, bool require_tunnel, IdeEndpoint& endpoint_out)
{
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(text);
    if(parsed.first != jt::Json::success || !parsed.second.isObject())
    {
        return false;
    }

    std::string url = JsonUtils::GetString(parsed.second, "externalUrl", "");
    if(url.empty())
    {
        const int32_t port = JsonUtils::GetInt(parsed.second, "port", 0);
        if(require_tunnel || port <= 0)
        {
            return false;
        }
        url = "http://127.0.0.1:" + std::to_string(port);
    }
    while(!url.empty() && url.back() == '/')
    {
        url.pop_back();
    }
    if(!IsLoopbackUrl(url))
    {
        return false;
    }

    endpoint_out.url       = url;
    endpoint_out.token     = JsonUtils::GetString(parsed.second, "token", "");
    endpoint_out.workspace = JsonUtils::GetString(parsed.second, "workspace", "");
    endpoint_out.remote    = JsonUtils::GetString(parsed.second, "remote", "");
    return true;
}

// The editor on this machine, if one is running. Read fresh on every call, so
// restarting the editor needs no reattach.
bool
ReadLocalHandshake(IdeEndpoint& endpoint_out)
{
    const std::filesystem::path path =
        std::filesystem::path(get_application_config_path(false)) / LOOP_HANDSHAKE_FILE;
    std::ifstream in(path);
    if(!in.is_open())
    {
        return false;
    }
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    return ParseHandshake(text, false, endpoint_out);
}

// One POST to the extension, at an address Endpoint() has already checked is
// loopback.
bool
CallIde(const std::string& base_url, const std::string& token, const char* path,
        jt::Json body, int timeout_seconds, jt::Json& reply_out,
        std::string& error_out)
{
    httplib::Client client(base_url);
    if(!client.is_valid())
    {
        error_out = "The editor handshake named an address that cannot be reached.";
        return false;
    }
    client.set_connection_timeout(LOOP_CONNECT_TIMEOUT_SECONDS);
    client.set_read_timeout(timeout_seconds);
    client.set_write_timeout(timeout_seconds);

    httplib::Headers headers;
    headers.emplace(LOOP_TOKEN_HEADER, token);

    const std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
    const httplib::Result response =
        client.Post(path, headers, body.toString(), LOOP_JSON_TYPE);
    if(!response)
    {
        // The elapsed time is what tells a refused port from a request that was
        // answered too slowly, and the two need opposite fixes.
        const int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - began)
                                       .count();
        spdlog::warn("Closed loop: POST {}{} failed after {} ms: {}", base_url, path,
                     elapsed_ms, httplib::to_string(response.error()));
        error_out = "The editor did not answer (" +
                    httplib::to_string(response.error()) + " after " +
                    std::to_string(elapsed_ms / 1000) +
                    "s). Check that the workspace is still open.";
        return false;
    }

    if(response->status == LOOP_HTTP_NOT_FOUND)
    {
        error_out =
            "The attached editor is running an older Optiq extension that does "
            "not have this feature. Tell the user to reload their editor window "
            "(Developer: Reload Window); until they do, the loop cannot search "
            "or change code.";
        return false;
    }

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(response->body);
    if(parsed.first != jt::Json::success || !parsed.second.isObject())
    {
        error_out = "The editor replied with HTTP " + std::to_string(response->status) +
                    " and no usable JSON.";
        return false;
    }
    reply_out = parsed.second;
    return true;
}

// Keeps the head, which is where both a compiler log and a source file put the
// part worth reading first.
std::string
Trim(std::string text, size_t max_chars)
{
    if(text.size() > max_chars)
    {
        text.resize(max_chars);
        text += "\n... truncated ...";
    }
    return text;
}

// Turns the editor's reply into what the model needs to know next. Each branch
// ends by naming the next move, so a failure is a step rather than a dead end.
std::string
DescribeApply(const jt::Json& reply)
{
    if(!JsonUtils::GetBool(reply, "applied", false))
    {
        const std::string error = JsonUtils::GetString(reply, "error", "");
        return "The edit was not applied. " +
               (error.empty() ? std::string("The editor gave no reason.") : error) +
               " Read the file again before offering another change: what you "
               "matched on is not what is in the file.";
    }

    const std::string output =
        Trim(JsonUtils::GetString(reply, "output", ""), LOOP_MAX_OUTPUT_CHARS);
    if(!JsonUtils::GetBool(reply, "built", false))
    {
        return "The edit was applied but the build failed:\n" + output +
               "\nFix the error it names and offer another change. Do not offer a "
               "re-profile until it builds.";
    }
    return "The edit was applied and the workspace built.\n" + output +
           "\nOffer a re-profile with the same launch profile, so the change can "
           "be measured rather than assumed.";
}

// The launcher dialog, created on demand. Null in a build without the profiler.
ProfilerLauncherDialog*
Launcher()
{
    AppWindow* app = AppWindow::GetInstance();
    return app != nullptr ? app->GetProfilerLauncher() : nullptr;
}

}  // namespace

LoopBridge&
LoopBridge::GetInstance()
{
    // Deliberately never destroyed: a build the user approved can still be
    // running at exit, and ~future would block the shutdown until it finished.
    static LoopBridge* s_instance = new LoopBridge();
    return *s_instance;
}

std::string
LoopBridge::ProposeEdit(const std::string& file, const std::string& find,
                        const std::string& replace, const std::string& why)
{
    if(m_state != LoopState::kIdle)
    {
        return "Another proposal is already with the user. Wait for their answer "
               "before offering anything else.";
    }
    if(file.empty() || find.empty())
    {
        return "propose_code_change needs a file and the exact text to find.";
    }

    std::string url;
    std::string token;
    if(!Endpoint(url, token))
    {
        return LOOP_NO_EDITOR;
    }

    m_action  = LoopAction::kEdit;
    m_state   = LoopState::kPending;
    m_file    = file;
    m_find    = find;
    m_replace = replace;
    m_why     = why;
    return std::string();
}

std::string
LoopBridge::ProposeRun(const std::string& profile, const std::string& why)
{
    if(m_state != LoopState::kIdle)
    {
        return "Another proposal is already with the user. Wait for their answer "
               "before offering anything else.";
    }
    if(profile.empty())
    {
        return "propose_profile_run needs the name of a saved launch profile. Call "
               "list_launch_profiles to see them.";
    }
    if(Launcher() == nullptr)
    {
        return "The profiler launcher is not available, so no run can be offered.";
    }

    m_action  = LoopAction::kRun;
    m_state   = LoopState::kPending;
    m_profile = profile;
    m_why     = why;
    return std::string();
}

bool
LoopBridge::Pending()
{
    if(m_state == LoopState::kPending)
    {
        if(m_reject_requested)
        {
            m_reject_requested = false;
            // Their call, and not a failure. Said plainly so the model takes
            // another route instead of offering the same thing again.
            m_result = "The user read the proposal and refused it. Do not offer the "
                       "same one again unless they ask. Take another route, or "
                       "answer with what you already have.";
            m_state  = LoopState::kSettled;
        }
        else if(m_approve_requested)
        {
            m_approve_requested = false;
            StartApprovedWork();
        }
    }
    else if(m_state == LoopState::kWorking)
    {
        bool settled = false;
        switch(m_action)
        {
            case LoopAction::kEdit: settled = PollEdit(); break;
            case LoopAction::kAttach: settled = PollAttach(); break;
            default: settled = PollRun(); break;
        }
        if(settled)
        {
            m_state = LoopState::kSettled;
        }
    }

    return m_state == LoopState::kPending || m_state == LoopState::kWorking;
}

std::string
LoopBridge::TakeResult()
{
    std::string result = m_result;
    if(result.empty())
    {
        result = "The proposal was not answered, so nothing was changed. Carry on "
                 "without it.";
    }

    m_state             = LoopState::kIdle;
    m_action            = LoopAction::kNone;
    m_approve_requested = false;
    m_reject_requested  = false;
    m_result.clear();
    m_file.clear();
    m_find.clear();
    m_replace.clear();
    m_profile.clear();
    m_why.clear();
    return result;
}

void
LoopBridge::StartApprovedWork()
{
    m_state = LoopState::kWorking;

    if(m_action == LoopAction::kEdit)
    {
        jt::Json body;
        body["file"]    = m_file;
        body["find"]    = m_find;
        body["replace"] = m_replace;
        body["build"]   = true;

        std::string url;
        std::string token;
        if(!Endpoint(url, token))
        {
            m_result = LOOP_NO_EDITOR;
            m_state  = LoopState::kSettled;
            return;
        }
        // The build behind the edit runs for minutes, so it cannot be waited on
        // from the frame loop the way a read can.
        m_edit_call = std::async(std::launch::async, [url, token, body]() {
            jt::Json    reply;
            std::string error;
            if(!CallIde(url, token, "/apply", body, LOOP_BUILD_TIMEOUT_SECONDS, reply,
                        error))
            {
                return error;
            }
            return DescribeApply(reply);
        });
        spdlog::info("Closed loop: applying an approved edit to {}", m_file);
        return;
    }

    ProfilerLauncherDialog* launcher = Launcher();
    if(launcher == nullptr || !launcher->LaunchNamedPreset(m_profile))
    {
        const std::string error =
            launcher != nullptr ? launcher->LastRunError() : std::string();
        m_result = "The run could not be started. " +
                   (error.empty() ? std::string("Check the launcher for why.")
                                  : error);
        m_state = LoopState::kSettled;
        return;
    }
    spdlog::info("Closed loop: launched profile {}", m_profile);
}

bool
LoopBridge::PollEdit()
{
    if(!m_edit_call.valid())
    {
        m_result = "The call to the editor was lost before it answered.";
        return true;
    }
    if(m_edit_call.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
    {
        return false;
    }
    m_result = m_edit_call.get();
    return true;
}

bool
LoopBridge::PollRun()
{
    ProfilerLauncherDialog* launcher = Launcher();
    if(launcher == nullptr)
    {
        m_result = "The profiler launcher went away before the run finished.";
        return true;
    }
    if(launcher->IsRunActive())
    {
        return false;
    }

    // Launch() clears the previous path, so anything here belongs to this run.
    const std::string trace = launcher->LastTracePath();
    if(trace.empty())
    {
        const std::string error = launcher->LastRunError();
        m_result = "The run finished without producing a trace. " +
                   (error.empty() ? std::string("Check the launcher console.")
                                  : error);
        return true;
    }

    RecordLap(trace);
    m_result = "The run finished and " + trace + " is open as lap " +
               std::to_string(m_laps.size()) +
               ". Start again at trace_overview on it, and say whether the number "
               "you were chasing actually moved.";
    return true;
}

void
LoopBridge::RecordLap(const std::string& trace_path)
{
    const std::string previous = m_laps.empty() ? std::string() : m_laps.back();
    m_laps.push_back(trace_path);

    // Two traces is what makes the change measurable, so the second lap opens
    // the comparison itself rather than leaving the user to pair tabs by hand.
    if(!previous.empty() && previous != trace_path)
    {
        AppWindow* app = AppWindow::GetInstance();
        if(app != nullptr)
        {
            app->OpenCompare(previous, trace_path);
        }
    }
}

void
LoopBridge::SetEditorChoice(EditorChoice choice)
{
    m_editor_choice = choice;
}

bool
LoopBridge::Endpoint(std::string& url_out, std::string& token_out) const
{
    // Asked for the remote one, answer only with the remote one. Falling back
    // to whatever editor happens to be open here would apply a change to a
    // different checkout than the one that was profiled.
    if(m_editor_choice != EditorChoice::kRemote)
    {
        IdeEndpoint local;
        if(ReadLocalHandshake(local))
        {
            url_out   = local.url;
            token_out = local.token;
            return true;
        }
    }
    if(m_editor_choice != EditorChoice::kLocal && m_remote_attached)
    {
        url_out   = m_remote_url;
        token_out = m_remote_token;
        return true;
    }
    return false;
}

bool
LoopBridge::Attached() const
{
    std::string url;
    std::string token;
    return Endpoint(url, token);
}

std::string
LoopBridge::Attach()
{
#ifndef ROCPROFVIS_ENABLE_REMOTE
    return "No editor is attached on this machine, and this build has no remote "
           "support, so none can be looked for on the profiling host.";
#else
    if(m_state != LoopState::kIdle)
    {
        return "Something else is already in flight. Wait for it to finish.";
    }

    // The host to ask is the one the user already profiles on, so the loop never
    // invents a connection: it takes the profile's, or the only one saved.
    SshConnectionStore store;
    store.Load();
    const SshConnectionConfig* connection = nullptr;
    ProfilerLauncherDialog*    launcher   = Launcher();
    if(launcher != nullptr)
    {
        connection = store.Get(launcher->CurrentSshConnectionId());
    }
    if(connection == nullptr && store.List().size() == 1)
    {
        connection = &store.List().front();
    }
    if(connection == nullptr)
    {
        return "No editor is attached here, and there is no single SSH connection "
               "to look for one on. Tell the user to load the remote launch "
               "profile they profile with, so the loop knows which host to ask.";
    }

    m_attach_uri = std::make_shared<RemoteUri>();
    m_attach_uri->SetConnection(*connection);
    m_attach_uri->GetRemoteCommandLine() = LOOP_REMOTE_HANDSHAKE_COMMAND;
    // Setting a result path is what makes the orchestrator download after the
    // command, which is how the handshake actually gets here.
    m_attach_uri->SetRemoteResultPathString(LOOP_REMOTE_HANDSHAKE_PATH);
    // And the local side of the same trap: the cache path is derived from the
    // connection, so a failed download would otherwise leave last time's answer
    // sitting there to be read as this time's.
    std::error_code discard;
    std::filesystem::remove(m_attach_uri->GetLocalResultPathString(), discard);

    m_attach = std::make_unique<RemoteTraceOrchestrator>(m_attach_uri, nullptr);
    if(!m_attach->Start())
    {
        m_attach.reset();
        m_attach_uri.reset();
        return "The SSH connection used to look for the editor could not be started.";
    }

    m_action = LoopAction::kAttach;
    m_state  = LoopState::kWorking;
    spdlog::info("Closed loop: looking for a remote editor on {}",
                 m_attach_uri->GetRemoteHostString());
    return std::string();
#endif
}

bool
LoopBridge::PollAttach()
{
#ifndef ROCPROFVIS_ENABLE_REMOTE
    m_result = "This build has no remote support.";
    return true;
#else
    if(m_attach == nullptr)
    {
        m_result = "The search for a remote editor was lost.";
        return true;
    }
    if(m_attach->IsRunning())
    {
        return false;
    }

    const std::string status = m_attach->GetStatusMessage();
    const std::string local_copy =
        m_attach_uri != nullptr ? m_attach_uri->GetLocalResultPathString() : std::string();

    std::string   text;
    std::ifstream in(local_copy);
    if(in.is_open())
    {
        text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    IdeEndpoint endpoint;
    // A handshake from over there must carry the tunnelled URL: its own loopback
    // port means nothing on this machine.
    const bool found = ParseHandshake(text, true, endpoint);
    spdlog::info("Closed loop: remote handshake {} bytes from {}, parsed={}",
                 text.size(), local_copy, found);
    m_attach.reset();
    m_attach_uri.reset();

    if(!found)
    {
        m_result =
            "No editor could be reached on the profiling host. Either nothing is "
            "running the Optiq extension there, or it could not open a tunnel "
            "back to this machine. Tell the user to open that host in VS Code or "
            "Cursor over Remote-SSH with the Optiq extension installed. (" +
            status + ")";
        return true;
    }

    m_remote_attached  = true;
    m_remote_url       = endpoint.url;
    m_remote_token     = endpoint.token;
    m_remote_workspace = endpoint.workspace;
    spdlog::info("Closed loop: attached to a remote editor, workspace {}",
                 m_remote_workspace);
    m_result = "Found the editor on the profiling host.\n" + Status();
    return true;
#endif
}

void
LoopBridge::InvalidateRemote()
{
    if(m_remote_attached)
    {
        spdlog::info("Closed loop: remote editor at {} stopped answering, dropping it",
                     m_remote_url);
    }
    m_remote_attached = false;
    m_remote_url.clear();
    m_remote_token.clear();
    m_remote_workspace.clear();
}

std::string
LoopBridge::FindSource(const std::string& query)
{
    if(query.empty())
    {
        return "find_source needs a file name, or part of one, to search for.";
    }

    std::string url;
    std::string token;
    if(!Endpoint(url, token))
    {
        return LOOP_NO_EDITOR;
    }

    jt::Json body;
    body["query"] = query;

    jt::Json    reply;
    std::string error;
    if(!CallIde(url, token, "/find", body, LOOP_QUERY_TIMEOUT_SECONDS, reply, error))
    {
        // An editor window that reloaded or opened another folder comes back on
        // a different port, so the address we were given is routinely stale
        // rather than exceptionally so.
        InvalidateRemote();
        return error + " Call loop_status again to find the editor at its "
                       "current address before giving up.";
    }
    if(!JsonUtils::GetBool(reply, "ok", false))
    {
        const std::string reason = JsonUtils::GetString(reply, "error", "");
        return reason.empty() ? std::string("The editor could not search.") : reason;
    }

    const std::vector<std::string> files = JsonUtils::GetStringArray(reply, "files");

    // The lines a symbol actually appears on, which is what a kernel name from
    // the trace resolves to. Reported first: a file whose name happens to
    // contain the query is a weaker lead than the call site itself.
    std::ostringstream out;
    jt::Json&          matches = reply["matches"];
    size_t             shown   = 0;
    if(matches.isArray())
    {
        for(jt::Json& match : matches.getArray())
        {
            if(shown == 0)
            {
                out << "\"" << query << "\" appears at:\n";
            }
            out << "  " << JsonUtils::GetString(match, "file", "") << ":"
                << JsonUtils::GetInt(match, "line", 0) << ": "
                << JsonUtils::GetString(match, "text", "") << "\n";
            ++shown;
        }
    }

    if(!files.empty())
    {
        out << "Files whose name matches \"" << query << "\":\n";
        for(const std::string& file : files)
        {
            out << "  " << file << "\n";
        }
    }

    if(shown == 0 && files.empty())
    {
        return "Nothing in the workspace matches \"" + query +
               "\", by file name or in any source file. Try a shorter fragment, "
               "or a different name from the trace.";
    }
    return out.str();
}

std::string
LoopBridge::ReadSource(const std::string& file)
{
    if(file.empty())
    {
        return "read_source needs a file path, relative to the workspace root.";
    }

    std::string url;
    std::string token;
    if(!Endpoint(url, token))
    {
        return LOOP_NO_EDITOR;
    }

    jt::Json body;
    body["file"] = file;

    jt::Json    reply;
    std::string error;
    if(!CallIde(url, token, "/read", body, LOOP_QUERY_TIMEOUT_SECONDS, reply, error))
    {
        InvalidateRemote();
        return error + " Call loop_status again to find the editor at its "
                       "current address before giving up.";
    }
    if(!JsonUtils::GetBool(reply, "ok", false))
    {
        const std::string reason = JsonUtils::GetString(reply, "error", "");
        return "The editor could not read " + file + ". " +
               (reason.empty() ? std::string("Check the path.") : reason);
    }
    return Trim(JsonUtils::GetString(reply, "text", ""), LOOP_MAX_SOURCE_CHARS);
}

std::string
LoopBridge::Status() const
{
    std::ostringstream out;

    IdeEndpoint local;
    const bool  local_open = ReadLocalHandshake(local);

    if(local_open && m_editor_choice != EditorChoice::kRemote)
    {
        out << "editor: attached on this machine, workspace=" << local.workspace
            << "\n";
        // The loop will edit and build in that workspace, which is only right if
        // it is the code the trace came from. Said plainly so a trace profiled
        // on another box is not quietly matched against the wrong checkout.
        out << "note: if this is not the source the trace was built from, call "
               "loop_status again with editor=\"remote\" to use the editor on "
               "the profiling host instead\n";
    }
    else if(m_remote_attached)
    {
        out << "editor: attached on the profiling host over the SSH profile, "
            << "workspace=" << m_remote_workspace << "\n";
    }
    else
    {
        out << "editor: not attached\n";
        if(local_open)
        {
            out << "note: an editor is open on this machine (workspace="
                << local.workspace
                << ") but editor=\"remote\" was asked for and none was found "
                   "there\n";
        }
    }

    out << "laps: " << m_laps.size() << "\n";
    for(size_t i = 0; i < m_laps.size(); ++i)
    {
        out << "  " << (i + 1) << ". " << m_laps[i] << "\n";
    }
    return out.str();
}

std::string
LoopBridge::LaunchProfiles() const
{
    ProfilerLauncherDialog* launcher = Launcher();
    if(launcher == nullptr)
    {
        return "The profiler launcher is not available in this build.";
    }

    const std::vector<std::string> names = launcher->ListPresetNames();
    if(names.empty())
    {
        return "No launch profiles are saved. A run can only be offered by profile "
               "name, so tell the user to author one in File > Launch Profiler and "
               "save it first.";
    }

    std::ostringstream out;
    out << "Saved launch profiles:\n";
    for(const std::string& name : names)
    {
        out << "  " << name << "\n";
    }
    return out.str();
}

void
LoopBridge::RenderRing(uint32_t lap, int32_t stage, const char* detail) const
{
    SettingsManager& settings = SettingsManager::GetInstance();

    const float  extent = (LOOP_RING_RADIUS + LOOP_RING_DOT) * 2.0f;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 center(origin.x + extent * 0.5f, origin.y + extent * 0.5f);

    const ImU32 dim    = settings.GetColor(Colors::kBorderGray);
    const ImU32 accent = settings.GetColor(Colors::kAccent);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircle(center, LOOP_RING_RADIUS, dim, 0, 1.5f);
    for(int32_t i = 0; i < LOOP_RING_STAGES; ++i)
    {
        const float angle = -LOOP_PI * 0.5f + LOOP_PI * 2.0f * static_cast<float>(i) /
                                                  static_cast<float>(LOOP_RING_STAGES);
        const ImVec2 dot(center.x + std::cos(angle) * LOOP_RING_RADIUS,
                         center.y + std::sin(angle) * LOOP_RING_RADIUS);
        draw->AddCircleFilled(dot, LOOP_RING_DOT, i == stage ? accent : dim);
    }
    ImGui::Dummy(ImVec2(extent, extent));

    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Text("Lap %u", lap);
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(detail);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
}

void
LoopBridge::Render()
{
    if(m_state == LoopState::kIdle && m_laps.empty())
    {
        return;
    }

    SettingsManager& settings = SettingsManager::GetInstance();
    BeginPanelCard("##optiq_loop", PanelCardTone::kPanel, ImVec2(14.0f, 10.0f), true,
                   &settings);

    int32_t     stage  = LOOP_STAGE_ANALYZE;
    const char* detail = "Analyzing";
    if(m_action == LoopAction::kEdit)
    {
        stage  = LOOP_STAGE_EDIT;
        detail = m_state == LoopState::kWorking ? "Applying and building"
                                                : "Change proposed";
    }
    else if(m_action == LoopAction::kRun)
    {
        stage  = LOOP_STAGE_PROFILE;
        detail = m_state == LoopState::kWorking ? "Profiling" : "Run proposed";
    }
    else if(m_action == LoopAction::kAttach)
    {
        stage  = LOOP_STAGE_EDIT;
        detail = "Looking for the editor";
    }

    // The lap being worked on, which is one past what has landed.
    const uint32_t lap = static_cast<uint32_t>(m_laps.size()) +
                         (m_state == LoopState::kIdle ? 0u : 1u);
    RenderRing(lap, stage, detail);

    if(m_state != LoopState::kPending)
    {
        EndPanelCard();
        return;
    }

    ImGui::Spacing();
    if(m_action == LoopAction::kEdit)
    {
        ImGui::TextWrapped("Apply this change to %s?", m_file.c_str());
    }
    else
    {
        ImGui::TextWrapped("Run the saved profile \"%s\"?", m_profile.c_str());
    }
    if(!m_why.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
        ImGui::TextWrapped("%s", m_why.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    if(AccentButton("Approve", ImVec2(0.0f, 0.0f), &settings))
    {
        m_approve_requested = true;
    }
    ImGui::SameLine();
    if(ColoredButton("Reject", settings.GetColor(Colors::kButton),
                     settings.GetColor(Colors::kButtonHovered),
                     settings.GetColor(Colors::kButtonActive),
                     settings.GetColor(Colors::kTextMain)))
    {
        m_reject_requested = true;
    }

    EndPanelCard();
}

}  // namespace View
}  // namespace RocProfVis
