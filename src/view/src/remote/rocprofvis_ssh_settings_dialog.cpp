// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_settings_dialog.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"

#include <cfloat>
#include <utility>

namespace RocProfVis
{
namespace View
{

static const char* kSshSettingsPopupName = "SSH Connection Settings";

SshSettingsDialog::SshSettingsDialog(SshConnectionStore& store, const std::string& initial_id,
                                     std::function<void(const std::string&)> on_commit)
: m_store(store)
, m_working()
, m_on_commit(std::move(on_commit))
, m_show_password(false)
, m_show_passphrase(false)
, m_open(true)
, m_requested_open(false)
{
    if(!initial_id.empty())
    {
        SelectConnection(initial_id);
    }
    else if(!m_store.Empty())
    {
        SelectConnection(m_store.List().front().id);
    }
    else
    {
        BeginNewConnection();
    }
}

SshSettingsDialog::~SshSettingsDialog() = default;

void
SshSettingsDialog::SelectConnection(const std::string& id)
{
    const SshConnectionConfig* cfg = m_store.Get(id);
    if(cfg)
    {
        m_working = *cfg;
    }
    else
    {
        BeginNewConnection();
    }
}

void
SshSettingsDialog::BeginNewConnection()
{
    m_working              = SshConnectionConfig();
    m_working.id           = SshConnectionConfig::GenerateId();
    m_working.display_name = "New Connection";
}

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

        // --- Profile selector row ---
        ImGui::AlignTextToFramePadding(); ImGui::Text("Profile"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN - 170.0f);
        const auto& connections = m_store.List();
        std::string combo_label =
            m_working.display_name.empty() ? "(unnamed)" : m_working.display_name;
        if(ImGui::BeginCombo("##sshprofile", combo_label.c_str()))
        {
            for(const auto& cfg : connections)
            {
                bool selected = (cfg.id == m_working.id);
                std::string item =
                    cfg.display_name.empty() ? "(unnamed)" : cfg.display_name;
                if(ImGui::Selectable(item.c_str(), selected))
                {
                    SelectConnection(cfg.id);
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        if(ImGui::Button("New", ImVec2(80, 0)))
        {
            BeginNewConnection();
        }

        ImGui::SameLine();
        bool exists_in_store = m_store.Get(m_working.id) != nullptr;
        if(!exists_in_store) ImGui::BeginDisabled();
        if(ImGui::Button("Delete", ImVec2(80, 0)))
        {
            std::string removed_id = m_working.id;
            m_store.Remove(removed_id);
            if(!m_store.Empty())
            {
                SelectConnection(m_store.List().front().id);
            }
            else
            {
                BeginNewConnection();
            }
        }
        if(!exists_in_store) ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::AlignTextToFramePadding(); ImGui::Text("Name"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextString("##rname", m_working.display_name);

        ImGui::AlignTextToFramePadding(); ImGui::Text("Host"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextString("##rhost", m_working.host);

        ImGui::AlignTextToFramePadding(); ImGui::Text("Port"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextString("##rport", m_working.port);

        ImGui::AlignTextToFramePadding(); ImGui::Text("User"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextString("##ruser", m_working.user);

        ImGui::AlignTextToFramePadding(); ImGui::Text("Password"); ImGui::SameLine(label_w);
        ImGuiInputTextFlags pwd_flags = m_show_password ? 0 : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(-FLT_MIN - 90.0f);
        InputTextString("##rpass", m_working.password, pwd_flags);
        ImGui::SameLine();
        ImGui::Checkbox("Show", &m_show_password);

        ImGui::AlignTextToFramePadding(); ImGui::Text("SSH Key"); ImGui::SameLine(label_w);
        ImGui::SetNextItemWidth(-FLT_MIN);
        InputTextStringWithHint(
            "##rkey",
            "Optional - path to private key (e.g. ~/.ssh/id_ed25519). Leave blank for default keys or password.",
            m_working.identity_file);

        ImGui::AlignTextToFramePadding(); ImGui::Text("Key Passphrase"); ImGui::SameLine(label_w);
        ImGuiInputTextFlags pass_flags = m_show_passphrase ? 0 : ImGuiInputTextFlags_Password;
        ImGui::SetNextItemWidth(-FLT_MIN - 90.0f);
        InputTextStringWithHint(
            "##rkeypass", "Leave blank if key is unencrypted or loaded in ssh-agent",
            m_working.passphrase, pass_flags);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if(ImGui::Button("OK", ImVec2(110, 0)))
        {
            m_store.Save(m_working);
            if(m_on_commit)
            {
                m_on_commit(m_working.id);
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
