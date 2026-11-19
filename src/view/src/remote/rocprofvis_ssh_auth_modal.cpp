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

// Header band shared by the auth modals: glyph, title, and a dim one-line
// subtitle, matching the "Open Remote Trace" dialog header.
void
RenderAuthHeaderCard(const char* id, const char* glyph, Colors glyph_color,
                     const char* title)
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    BeginPanelCard(id, PanelCardTone::kPanel, style.WindowPadding, true, &settings);
    PanelIcon(glyph, glyph_color, &settings,
              settings.GetFontManager().GetFontSize(FontSize::kMedium));
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImGui::BeginGroup();
    ImGui::PushFont(nullptr, settings.GetFontManager().GetFontSize(FontSize::kMedium));
    ImGui::TextWrapped("%s", title);
    ImGui::PopFont();
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
        const ImGuiStyle& style    = settings.GetDefaultStyle();

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup(ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(480, 0));

        if(ImGui::BeginPopupModal(
               "SSH Authentication", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            BeginPanelCard("##ssh_auth_body", PanelCardTone::kPanel, style.WindowPadding,
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

            float button_width =
                ImGui::CalcTextSize("Cancel").x + 2.0f * style.FramePadding.x;
            float total = button_width * 2.0f + style.ItemSpacing.x;
            float avail = ImGui::GetContentRegionAvail().x;
            if(avail > total)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - total));
            }
            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                ssh_session->CancelRequest();
                st = {};
                ImGui::CloseCurrentPopup();
                ssh_session->GetPromptRequest()->ClearUpdated();
            }
            ImGui::SameLine();
            if(AccentButton("Submit", ImVec2(button_width, 0), &settings))
            {
                std::vector<std::string> resp = st.answers;

                ssh_session->SubmitPromptResponses(resp);
                st = {};
                ImGui::CloseCurrentPopup();
                ssh_session->GetPromptRequest()->ClearUpdated();
            }

            ImGui::EndPopup();
        }
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
        const ImGuiStyle& style    = settings.GetDefaultStyle();

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup(ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(440, 0));

        if(ImGui::BeginPopupModal(
               "SSH Host Key", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            const bool        mismatch = req->state == HostKeyState::Mismatch;
            const std::string server =
                req->port == 22 ? req->host : req->host + ":" + std::to_string(req->port);

            RenderAuthHeaderCard(
                "##ssh_hostkey_header", mismatch ? ICON_UNLOCKED : ICON_CHAIN,
                mismatch ? Colors::kBgError : Colors::kAccent,
                mismatch ? "This server's key is different from last time. That "
                           "is normal after a reinstall. If you did not expect "
                           "it, cancel."
                         : "First time connecting to this server. Optiq will "
                           "remember it.");

            BeginPanelCard("##ssh_hostkey_body", PanelCardTone::kPanel,
                           style.WindowPadding, true, &settings);
            {
                ImGui::PushFont(nullptr,
                                settings.GetFontManager().GetFontSize(FontSize::kSmall));
                ImGui::TextDisabled("Host");
                ImGui::TextWrapped("%s", server.c_str());
                ImGui::TextDisabled("Key fingerprint");
                ImGui::TextWrapped("%s", req->fingerprint_sha256_b64.c_str());
                ImGui::PopFont();
            }
            EndPanelCard();

            constexpr float BUTTON_WIDTH = 104.0f;
            const char* trust_label = mismatch ? "Trust new key" : "Trust and connect";
            const float trust_width =
                ImGui::CalcTextSize(trust_label).x + style.FramePadding.x * 4.0f;
            const float total = trust_width + BUTTON_WIDTH + style.ItemSpacing.x;
            const float avail = ImGui::GetContentRegionAvail().x;
            if(avail > total)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - total));
            }

            auto decide = [&](HostKeyDecision decision) {
                ssh_session->SubmitHostKeyDecision(decision);
                ImGui::CloseCurrentPopup();
                ssh_session->GetHostKeyRequest()->ClearUpdated();
            };
            // A changed key makes Cancel the highlighted, safe choice.
            if(mismatch ? AccentButton("Cancel", ImVec2(BUTTON_WIDTH, 0), &settings)
                        : ImGui::Button("Cancel", ImVec2(BUTTON_WIDTH, 0)))
            {
                decide(HostKeyDecision::Reject);
            }
            ImGui::SameLine();
            if(mismatch ? ImGui::Button(trust_label, ImVec2(trust_width, 0))
                        : AccentButton(trust_label, ImVec2(trust_width, 0), &settings))
            {
                decide(HostKeyDecision::TrustPermanently);
            }

            ImGui::EndPopup();
        }
        return true;
    }

    // No pending request; reset latched modal state in case a previous
    // one was just dismissed.
    KbdintForFrame() = {};
    return false;
}

void
RenderSshAuthModals()
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
