// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_settings_dialog.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"

#include <cfloat>
#include <utility>
#include <vector>

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

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = ImGui::GetStyle();

    // Fixed width, auto height, so no section clips and no scrollbar is needed.
    const float dialog_width =
        GetResponsiveWindowSize(ImVec2(720.0f, 0.0f), ImVec2(620.0f, 0.0f)).x;
    ImGui::SetNextWindowSizeConstraints(ImVec2(dialog_width, 0.0f),
                                        ImVec2(dialog_width, FLT_MAX));

    if(ImGui::BeginPopupModal(kSshSettingsPopupName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoScrollbar))
    {
        constexpr float CONTENT_PADDING_X = 18.0f;
        constexpr float CONTENT_PADDING_Y = 12.0f;
        constexpr float LABEL_WIDTH       = 118.0f;
        constexpr float BUTTON_WIDTH      = 112.0f;
        constexpr float PROFILE_BUTTON_W  = 88.0f;
        constexpr float SHOW_TOGGLE_W     = 76.0f;

        const ImU32 text_dim       = settings.GetColor(Colors::kTextDim);
        const ImU32 accent         = settings.GetColor(Colors::kAccent);
        const ImU32 accent_hover   = settings.GetColor(Colors::kAccentHover);
        const ImU32 accent_active  = settings.GetColor(Colors::kAccentActive);
        const ImU32 text_on_accent = settings.GetColor(Colors::kTextOnAccent);
        ImFont*     icon_font      = settings.GetFontManager().GetFont(FontType::kIcon);

        // Restored inside each card so the tighter panel gaps do not cramp fields.
        const ImVec2 default_item_spacing = style.ItemSpacing;

        auto label = [&](const char* text) {
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextUnformatted(text);
            ImGui::PopStyleColor();
        };

        auto section_title = [&](const char* title, const char* subtitle) {
            ImGui::PushFont(nullptr,
                            settings.GetFontManager().GetFontSize(FontSize::kMedium));
            ImGui::TextUnformatted(title);
            ImGui::PopFont();
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextWrapped("%s", subtitle);
            ImGui::PopStyleColor();
        };

        auto begin_card = [&](const char* id) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
            ImGui::PushStyleColor(ImGuiCol_Border,
                                  settings.GetColor(Colors::kPanelBorderSubtle));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                                ImVec2(CONTENT_PADDING_X, CONTENT_PADDING_Y));
            ImGui::BeginChild(id, ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, default_item_spacing);
        };

        auto end_card = []() {
            ImGui::PopStyleVar();
            ImGui::EndChild();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
        };

        bool close_popup = false;
        bool accept      = false;

        // Tighten the vertical gaps between the stacked panels (header/cards/footer).
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(default_item_spacing.x, 4.0f));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
        ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 14.0f));
        ImGui::BeginChild("##ssh_settings_header", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            ImGui::PushFont(icon_font, ImGui::GetFontSize());
            ImGui::PushStyleColor(ImGuiCol_Text, accent);
            ImGui::TextUnformatted(ICON_COMPASS);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            ImGui::BeginGroup();
            ImGui::PushFont(nullptr,
                            settings.GetFontManager().GetFontSize(FontSize::kMedLarge));
            ImGui::TextUnformatted("Remote SSH Profile");
            ImGui::PopFont();

            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextWrapped(
                "Save the connection details used when opening profiler traces over SSH.");
            ImGui::PopStyleColor();
            ImGui::EndGroup();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 6.0f));
        ImGui::BeginChild("##ssh_settings_body", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            begin_card("##ssh_profile_card");
            {
                section_title("Profile", "Choose an existing profile or create a new one.");
                ImGui::Spacing();

                if(ImGui::BeginTable("##ssh_profile_table", 3,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                            LABEL_WIDTH);
                    ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                            PROFILE_BUTTON_W * 2.0f + style.ItemSpacing.x);
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    label("Profile");

                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    const std::vector<SshConnectionConfig>& connections = m_store.List();
                    std::string combo_label =
                        m_working.display_name.empty() ? "(unnamed)" : m_working.display_name;
                    PushComboStyles();
                    if(ImGui::BeginCombo("##sshprofile", combo_label.c_str()))
                    {
                        for(const SshConnectionConfig& cfg : connections)
                        {
                            bool        selected = (cfg.id == m_working.id);
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
                    PopComboStyles();

                    ImGui::TableSetColumnIndex(2);
                    if(ImGui::Button("New", ImVec2(PROFILE_BUTTON_W, 0.0f)))
                    {
                        BeginNewConnection();
                    }
                    ImGui::SameLine();
                    bool exists_in_store = m_store.Get(m_working.id) != nullptr;
                    if(!exists_in_store)
                    {
                        ImGui::BeginDisabled();
                    }
                    if(ImGui::Button("Delete", ImVec2(PROFILE_BUTTON_W, 0.0f)))
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
                    if(!exists_in_store)
                    {
                        ImGui::EndDisabled();
                    }

                    ImGui::EndTable();
                }
            }
            end_card();

            begin_card("##ssh_connection_card");
            {
                section_title("Connection", "Name the profile and set the remote host.");
                ImGui::Spacing();

                if(ImGui::BeginTable("##ssh_connection_table", 2,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                            LABEL_WIDTH);
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Name");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextString("##rname", m_working.display_name);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Host");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextString("##rhost", m_working.host);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Port");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextString("##rport", m_working.port);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("User");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextString("##ruser", m_working.user);

                    ImGui::EndTable();
                }
            }
            end_card();

            begin_card("##ssh_auth_card");
            {
                section_title("Authentication",
                              "Use a password, SSH key, or agent-backed key for login.");
                ImGui::Spacing();

                if(ImGui::BeginTable("##ssh_auth_table", 3,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                            LABEL_WIDTH);
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Toggle", ImGuiTableColumnFlags_WidthFixed,
                                            SHOW_TOGGLE_W);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Password");
                    ImGui::TableSetColumnIndex(1);
                    ImGuiInputTextFlags pwd_flags =
                        m_show_password ? 0 : ImGuiInputTextFlags_Password;
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextString("##rpass", m_working.password, pwd_flags);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Checkbox("Show##password", &m_show_password);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("SSH Key");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextStringWithHint(
                        "##rkey", "Optional private key path, e.g. ~/.ssh/id_ed25519",
                        m_working.identity_file);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Passphrase");
                    ImGui::TableSetColumnIndex(1);
                    ImGuiInputTextFlags pass_flags =
                        m_show_passphrase ? 0 : ImGuiInputTextFlags_Password;
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    InputTextStringWithHint(
                        "##rkeypass", "Leave blank for unencrypted keys or ssh-agent",
                        m_working.passphrase, pass_flags);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Checkbox("Show##passphrase", &m_show_passphrase);

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                ImGui::TextWrapped(
                    "Open Remote Trace requires a host, user, result database path, and either "
                    "a password or SSH key.");
                ImGui::PopStyleColor();
            }
            end_card();
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgFrame));
        ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::BeginChild("##ssh_settings_footer", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            const float action_width = BUTTON_WIDTH * 2.0f + style.ItemSpacing.x;
            if(ImGui::BeginTable("##ssh_settings_footer_table", 2,
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Note", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                                        action_width);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
                ElidedText("Profiles are saved locally and reused by remote trace open.",
                           ImGui::GetContentRegionAvail().x, 360.0f, Alignment_Left, true);
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                if(ImGui::Button("Cancel", ImVec2(BUTTON_WIDTH, 0.0f)))
                {
                    close_popup = true;
                }
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_active);
                ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
                if(ImGui::Button("Save", ImVec2(BUTTON_WIDTH, 0.0f)))
                {
                    accept      = true;
                    close_popup = true;
                }
                ImGui::PopStyleColor(4);
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(2);

        if(accept)
        {
            m_store.Save(m_working);
            if(m_on_commit)
            {
                m_on_commit(m_working.id);
            }
        }

        if(close_popup)
        {
            m_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleVar();

        ImGui::EndPopup();
    }

    popup_style.PopStyles();

    return m_open;
}

}  // namespace View
}  // namespace RocProfVis
