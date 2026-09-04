// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_auth_modal.h"

#include "rocprofvis_ssh_session.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"
#include "imgui.h"
#include <spdlog/spdlog.h>

#include <cfloat>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{
// Per-frame buffers backing the kbdint inputs. Sized once, reused.
struct KbdintState
{
    std::vector<std::string> answers;
};

KbdintState& KbdintForFrame() { static KbdintState s; return s; }

// Header band shared by the auth modals: accent SSH glyph, title, and a dim
// one-line subtitle, matching the "Open Remote Trace" dialog header.
void RenderAuthHeaderCard(const char* id, const char* title, const char* subtitle)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = ImGui::GetStyle();

    BeginPanelCard(id, PanelCardTone::kFrame, ImVec2(16.0f, 10.0f), true, &settings);
    PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(subtitle);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    EndPanelCard();
}
}  // namespace

bool RenderSshAuthModal(SshSession* ssh_session)
{
    if (!ssh_session)
    {
        return false;
    }

    // ---- kbdint ----
    if (auto req = ssh_session->GetPromptRequest()->ConsumeIfUpdated())
    {
        auto& st = KbdintForFrame();
        // The kbdint answer buffer is a single process-wide scratch shared by
        // every session's prompt (only one auth modal is ever visible at a
        // time). Reconcile it to the current request's prompt count every frame
        // - not just when opening - so a buffer left over from a different
        // session's prompt can never be too small to index below. Only reassign
        // when the count changes, so typed input is preserved across frames for
        // the same prompt.
        if(st.answers.size() != req->prompts.size())
        {
            st.answers.assign(req->prompts.size(), "");
        }
        // Gate on ImGui's own popup state rather than a persistent latch: if the
        // op is torn down without a button press, the un-Begin'd modal
        // auto-closes and a fresh request re-opens it (no wedged latch).
        if(!ImGui::IsPopupOpen("SSH Authentication"))
        {
            spdlog::debug("[ssh-ui] kbdint pending: {} prompt(s); calling OpenPopup",
                         req->prompts.size());
            ImGui::OpenPopup("SSH Authentication");
        }

        SettingsManager&  settings = SettingsManager::GetInstance();
        const ImGuiStyle& style    = ImGui::GetStyle();

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        // Borderless card-stack: the window hugs the cards (like the remote
        // trace dialog) and the header band carries the title instead of the
        // native title bar.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::SetNextWindowSize(ImVec2(480, 0));

        if(ImGui::BeginPopupModal("SSH Authentication", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
        {
            constexpr float BUTTON_WIDTH = 104.0f;

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(style.ItemSpacing.x, 4.0f));

            RenderAuthHeaderCard("##ssh_auth_header", "SSH Authentication",
                                 "Enter your credentials to continue.");

            BeginPanelCard("##ssh_auth_body", PanelCardTone::kPanel, ImVec2(14.0f, 10.0f),
                           true, &settings);
            {
                if(!req->name.empty())
                {
                    ImGui::TextWrapped("%s", req->name.c_str());
                }
                if(!req->instruction.empty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
                    ImGui::TextWrapped("%s", req->instruction.c_str());
                    ImGui::PopStyleColor();
                }

                for(size_t i = 0; i < req->prompts.size(); i++)
                {
                    PanelFieldLabel(req->prompts[i].text.c_str(), false, &settings);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGuiInputTextFlags flags =
                        req->prompts[i].echo ? 0 : ImGuiInputTextFlags_Password;
                    std::string id = "##kbd" + std::to_string(i);
                    // Writes directly into the answer string via CallbackResize.
                    InputTextString(id.c_str(), st.answers[i], flags);
                }
            }
            EndPanelCard();

            BeginPanelCard("##ssh_auth_footer", PanelCardTone::kFrame, ImVec2(14.0f, 8.0f),
                           true, &settings);
            {
                const float total = BUTTON_WIDTH * 2.0f + style.ItemSpacing.x;
                const float avail = ImGui::GetContentRegionAvail().x;
                if(avail > total)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - total));
                }
                if(ImGui::Button("Cancel", ImVec2(BUTTON_WIDTH, 0)))
                {
                    ssh_session->CancelRequest();
                    st = {};
                    ImGui::CloseCurrentPopup();
                    ssh_session->GetPromptRequest()->ClearUpdated();
                }
                ImGui::SameLine();
                if(AccentButton("Submit", ImVec2(BUTTON_WIDTH, 0), &settings))
                {
                    std::vector<std::string> resp = st.answers;

                    ssh_session->SubmitPromptResponses(resp);
                    st = {};
                    ImGui::CloseCurrentPopup();
                    ssh_session->GetPromptRequest()->ClearUpdated();
                }
            }
            EndPanelCard();

            ImGui::PopStyleVar();  // ItemSpacing
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding
        popup_style.PopStyles();
        return true;
    }

    // ---- host key confirmation ----
    if(auto req = ssh_session->GetHostKeyRequest()->ConsumeIfUpdated())
    {
        // Gate on ImGui's own popup state (no persistent latch): if the op is
        // cancelled/torn down without a button press, the un-Begin'd modal
        // auto-closes and a subsequent host-key request re-opens it, instead of
        // a stuck latch permanently suppressing future prompts.
        if(!ImGui::IsPopupOpen("SSH Host Key"))
        {
            ImGui::OpenPopup("SSH Host Key");
        }

        SettingsManager&  settings = SettingsManager::GetInstance();
        const ImGuiStyle& style    = ImGui::GetStyle();

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::SetNextWindowSize(ImVec2(520, 0));

        if(ImGui::BeginPopupModal("SSH Host Key", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
        {
            const bool  mismatch = req->state == HostKeyState::Mismatch;
            const char* subtitle = mismatch ? "The server's host key does not match."
                                            : "Verify this host before connecting.";

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                                ImVec2(style.ItemSpacing.x, 4.0f));

            RenderAuthHeaderCard("##ssh_hostkey_header", "SSH Host Key", subtitle);

            BeginPanelCard("##ssh_hostkey_body", PanelCardTone::kPanel, ImVec2(14.0f, 10.0f),
                           true, &settings);
            {
                if(mismatch)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kBgError));
                    ImGui::TextWrapped("WARNING: server host key has CHANGED");
                    ImGui::PopStyleColor();
                    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
                    ImGui::TextWrapped("Someone could be eavesdropping on you right now "
                                       "(man-in-the-middle attack), or the server's key was "
                                       "rotated. Continue only if you know this is expected.");
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
                    ImGui::TextWrapped(
                        "This is the first time you are connecting to this host. "
                        "Verify the fingerprint matches what the server administrator "
                        "expects before continuing.");
                    ImGui::PopStyleColor();
                }

                ImGui::Spacing();
                if(ImGui::BeginTable("##ssh_hostkey_fields", 2,
                                     ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    PanelFieldLabel("Host", true, &settings);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("%s:%lu", req->host.c_str(), req->port);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    PanelFieldLabel("Key type", true, &settings);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(req->key_type.c_str());

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    PanelFieldLabel("Fingerprint", true, &settings);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextWrapped("%s", req->fingerprint_sha256_b64.c_str());
                    ImGui::EndTable();
                }
            }
            EndPanelCard();

            BeginPanelCard("##ssh_hostkey_footer", PanelCardTone::kFrame, ImVec2(14.0f, 8.0f),
                           true, &settings);
            {
                constexpr float TRUST_WIDTH  = 150.0f;
                constexpr float BUTTON_WIDTH = 104.0f;
                const float     total =
                    TRUST_WIDTH + BUTTON_WIDTH * 2.0f + style.ItemSpacing.x * 2.0f;
                const float avail = ImGui::GetContentRegionAvail().x;
                if(avail > total)
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - total));
                }
                if(ImGui::Button("Reject", ImVec2(BUTTON_WIDTH, 0)))
                {
                    ssh_session->SubmitHostKeyDecision(HostKeyDecision::Reject);
                    ImGui::CloseCurrentPopup();
                    ssh_session->GetHostKeyRequest()->ClearUpdated();
                }
                ImGui::SameLine();
                if(ImGui::Button("Trust once", ImVec2(BUTTON_WIDTH, 0)))
                {
                    ssh_session->SubmitHostKeyDecision(HostKeyDecision::TrustOnce);
                    ImGui::CloseCurrentPopup();
                    ssh_session->GetHostKeyRequest()->ClearUpdated();
                }
                ImGui::SameLine();
                if(AccentButton("Trust permanently", ImVec2(TRUST_WIDTH, 0), &settings))
                {
                    ssh_session->SubmitHostKeyDecision(HostKeyDecision::TrustPermanently);
                    ImGui::CloseCurrentPopup();
                    ssh_session->GetHostKeyRequest()->ClearUpdated();
                }
            }
            EndPanelCard();

            ImGui::PopStyleVar();  // ItemSpacing
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding
        popup_style.PopStyles();
        return true;
    }

    // No pending request; reset latched modal state in case a previous
    // one was just dismissed.
    KbdintForFrame() = {};
    return false;
}

void RenderSshAuthModals()
{
    // A session whose owner renders its own auth modal nested inside its own
    // modal window (e.g. the remote file browser) must not be rendered here:
    // opening a second top-level modal would dismiss the owner's modal. While
    // any such self-managed modal is up, defer every other session's prompt too
    // for the same reason - requests are level-held, so they resurface once the
    // owning modal closes.
    for (SshSession* session : SshSession::ActiveSessions())
    {
        if (session->IsAuthModalSelfManaged())
        {
            return;
        }
    }

    // ImGui modals are exclusive and each request is level-held (the flag
    // persists until the user submits/cancels), so render at most one session's
    // modal per frame; other pending sessions are serviced on later frames once
    // this one resolves. This also keeps the shared popup IDs / kbdint input
    // buffer owned by a single session at a time.
    for (SshSession* session : SshSession::ActiveSessions())
    {
        if (RenderSshAuthModal(session))
        {
            break;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
