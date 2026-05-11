// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_auth_modal.h"

#include "rocprofvis_ssh_auth_bridge.h"
#include "rocprofvis_widget.h"

#include "imgui.h"
#include <spdlog/spdlog.h>

#include <cstring>
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
    bool                     opened = false;
};

KbdintState& KbdintForFrame() { static KbdintState s; return s; }
}  // namespace

void RenderSshAuthModal(DataModel::AuthBridge& bridge)
{
    using namespace DataModel;
    auto pending = bridge.Peek();

    // ---- kbdint ----
    if(auto* req = std::get_if<PromptRequest>(&pending))
    {
        auto& st = KbdintForFrame();
        if(!st.opened)
        {
            st.answers.assign(req->prompts.size(), "");
            st.opened = true;
            spdlog::info("[ssh-ui] kbdint pending: {} prompt(s); calling OpenPopup",
                         req->prompts.size());
            ImGui::OpenPopup("SSH Authentication");
        }

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(ImVec2(480, 0));

        if(ImGui::BeginPopupModal("SSH Authentication", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            if(!req->name.empty())        ImGui::TextWrapped("%s", req->name.c_str());
            if(!req->instruction.empty()) ImGui::TextWrapped("%s", req->instruction.c_str());
            ImGui::Spacing();

            for(size_t i = 0; i < req->prompts.size(); i++)
            {
                ImGui::TextUnformatted(req->prompts[i].text.c_str());
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGuiInputTextFlags flags =
                    req->prompts[i].echo ? 0 : ImGuiInputTextFlags_Password;
                char buf[256];
                std::strncpy(buf, st.answers[i].c_str(), sizeof(buf));
                buf[sizeof(buf) - 1] = '\0';
                std::string id = "##kbd" + std::to_string(i);
                if(ImGui::InputText(id.c_str(), buf, sizeof(buf), flags))
                {
                    st.answers[i] = buf;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if(ImGui::Button("Submit", ImVec2(110, 0)))
            {
                std::vector<std::string> resp = st.answers;
                bridge.SubmitPromptResponses(std::move(resp));
                st = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(110, 0)))
            {
                bridge.Cancel();
                st = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        popup_style.PopStyles();
        return;
    }

    // ---- host key confirmation ----
    if(auto* req = std::get_if<HostKeyRequest>(&pending))
    {
        static bool opened = false;
        if(!opened) { ImGui::OpenPopup("SSH Host Key"); opened = true; }

        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(ImVec2(520, 0));

        if(ImGui::BeginPopupModal("SSH Host Key", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
        {
            if(req->state == HostKeyState::Mismatch)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                   "WARNING: server host key has CHANGED");
                ImGui::TextWrapped("Someone could be eavesdropping on you right now "
                                   "(man-in-the-middle attack), or the server's key was "
                                   "rotated. Continue only if you know this is expected.");
            }
            else
            {
                ImGui::TextWrapped("This is the first time you are connecting to this host. "
                                   "Verify the fingerprint matches what the server administrator "
                                   "expects before continuing.");
            }
            ImGui::Spacing();
            ImGui::Text("Host:        %s:%d", req->host.c_str(), req->port);
            ImGui::Text("Key type:    %s", req->key_type.c_str());
            ImGui::Text("Fingerprint: %s", req->fingerprint_sha256_b64.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if(ImGui::Button("Trust permanently", ImVec2(160, 0)))
            {
                bridge.SubmitHostKeyDecision(HostKeyDecision::TrustPermanently);
                opened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Trust once", ImVec2(110, 0)))
            {
                bridge.SubmitHostKeyDecision(HostKeyDecision::TrustOnce);
                opened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Reject", ImVec2(110, 0)))
            {
                bridge.SubmitHostKeyDecision(HostKeyDecision::Reject);
                opened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        popup_style.PopStyles();
        return;
    }

    // No pending request; reset latched modal state in case a previous
    // one was just dismissed.
    KbdintForFrame() = {};
}

}  // namespace View
}  // namespace RocProfVis
