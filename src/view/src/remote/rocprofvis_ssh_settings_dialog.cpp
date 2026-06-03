// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_settings_dialog.h"
#include "widgets/rocprofvis_widget.h"

#include "imgui.h"

#include <cfloat>
#include <utility>

namespace RocProfVis
{
namespace View
{

static const char* kSshSettingsPopupName = "SSH Connection Settings";

SshSettingsDialog::SshSettingsDialog(const RemoteUri& current,
                                     std::function<void(RemoteUri)> on_commit)
: m_working(current)
, m_on_commit(std::move(on_commit))
, m_show_password(false)
, m_show_passphrase(false)
, m_open(true)
, m_requested_open(false)
{
}

SshSettingsDialog::~SshSettingsDialog() = default;

bool
SshSettingsDialog::Render()
{
    if(!m_open)
    {
        return false;
    }

    // Open the modal once on first render.
    if(!m_requested_open)
    {
        ImGui::OpenPopup(kSshSettingsPopupName);
        m_requested_open = true;
    }

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSize(ImVec2(560, 0));

    if(ImGui::BeginPopupModal(kSshSettingsPopupName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float label_w = 110.0f;
        ImGui::AlignTextToFramePadding(); ImGui::Text("Host"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##rhost", m_working.GetRemoteHostBuffer(), m_working.GetRemoteHostBufferSize());

        ImGui::AlignTextToFramePadding(); ImGui::Text("Port"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##rport", m_working.GetRemotePortBuffer(), m_working.GetRemotePortBufferSize());

        ImGui::AlignTextToFramePadding(); ImGui::Text("User"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##ruser", m_working.GetRemoteUserBuffer(), m_working.GetRemoteUserBufferSize());

        ImGui::AlignTextToFramePadding(); ImGui::Text("Password"); ImGui::SameLine(label_w);
        ImGuiInputTextFlags pwd_flags = m_show_password ? 0 : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(-FLT_MIN - 90.0f);
        ImGui::InputText("##rpass", m_working.GetRemotePasswordBuffer(), m_working.GetRemotePasswordBufferSize(), pwd_flags);
        ImGui::SameLine();
        ImGui::Checkbox("Show", &m_show_password);

        ImGui::AlignTextToFramePadding(); ImGui::Text("SSH Key"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint(
            "##rkey",
            "Optional - path to private key (e.g. ~/.ssh/id_ed25519). Leave blank for default keys or password.",
            m_working.GetRemoteIdentityFileBuffer(), m_working.GetRemoteIdentityFileBufferSize());

        ImGui::AlignTextToFramePadding(); ImGui::Text("Key Passphrase"); ImGui::SameLine(label_w);
        ImGuiInputTextFlags pass_flags = m_show_passphrase ? 0 : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(-FLT_MIN - 90.0f);
        ImGui::InputTextWithHint(
            "##rkeypass", "Leave blank if key is unencrypted or loaded in ssh-agent",
            m_working.GetPassphraseBuffer(), m_working.GetPassphraseBufferSize(), pass_flags);
        ImGui::SameLine();
        ImGui::Checkbox("Show##kpshow", &m_show_passphrase);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if(ImGui::Button("OK", ImVec2(110, 0)))
        {
            if(m_on_commit)
            {
                m_on_commit(std::move(m_working));
            }
            m_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(110, 0)))
        {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    popup_style.PopStyles();

    return m_open;
}

}  // namespace View
}  // namespace RocProfVis
