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
        GetResponsiveWindowSize(ImVec2(560.0f, 0.0f), ImVec2(480.0f, 0.0f)).x;
    ImGui::SetNextWindowSizeConstraints(ImVec2(dialog_width, 0.0f),
                                        ImVec2(dialog_width, FLT_MAX));
    // Borderless card-stack: the header band carries the title (and its own
    // close button) instead of the native title bar.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);

    if(ImGui::BeginPopupModal(kSshSettingsPopupName, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_NoSavedSettings |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoTitleBar))
    {
        constexpr float CONTENT_PADDING_X = 14.0f;
        constexpr float CONTENT_PADDING_Y = 8.0f;
        constexpr float LABEL_WIDTH       = 104.0f;
        constexpr float BUTTON_WIDTH      = 104.0f;
        constexpr float PROFILE_BUTTON_W  = 78.0f;

        const ImU32 text_dim  = settings.GetColor(Colors::kTextDim);
        ImFont*     icon_font = settings.GetFontManager().GetFont(FontType::kIcon);

        // Restored inside each card so the tighter panel gaps do not cramp fields.
        const ImVec2 default_item_spacing = style.ItemSpacing;

        auto label = [&](const char* text) {
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextUnformatted(text);
            ImGui::PopStyleColor();
        };

        auto section_title = [&](const char* title) {
            ImGui::PushFont(nullptr,
                            settings.GetFontManager().GetFontSize(FontSize::kMedium));
            ImGui::TextUnformatted(title);
            ImGui::PopFont();
        };

        // Compact reveal toggle that lives inside the field cell, so it can never
        // overflow past the card like a trailing "Show" checkbox column would.
        auto reveal_toggle = [&](const char* id, std::string& value, const char* hint,
                                 bool& show) {
            const float eye_w = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(-(eye_w + style.ItemInnerSpacing.x));
            ImGuiInputTextFlags flags = show ? 0 : ImGuiInputTextFlags_Password;
            if(hint)
            {
                InputTextStringWithHint(id, hint, value, flags);
            }
            else
            {
                InputTextString(id, value, flags);
            }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            // Scope by the field id: both toggles render the same eye glyph, and
            // IconButton derives its ID from the glyph, so without this they would
            // collide (same ImGui ID) inside the shared table column.
            ImGui::PushID(id);
            if(IconButton(show ? ICON_EYE_SLASH : ICON_EYE, icon_font,
                          ImVec2(eye_w, eye_w), show ? "Hide" : "Show"))
            {
                show = !show;
            }
            ImGui::PopID();
        };

        // Delegate to the shared panel-card helper; restore default item spacing
        // inside the card so the tighter inter-card gap does not cramp fields.
        auto begin_card = [&](const char* id) {
            BeginPanelCard(id, PanelCardTone::kPanel,
                           ImVec2(CONTENT_PADDING_X, CONTENT_PADDING_Y), true, &settings);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, default_item_spacing);
        };

        auto end_card = []() {
            ImGui::PopStyleVar();
            EndPanelCard();
        };

        bool close_popup = false;
        bool accept      = false;

        // Tighten the vertical gaps between the stacked panels (header/cards/footer).
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            ImVec2(default_item_spacing.x, 4.0f));

        BeginPanelCard("##ssh_settings_header", PanelCardTone::kFrame, ImVec2(16.0f, 10.0f),
                       true, &settings);
        {
            if(ImGui::BeginTable("##ssh_settings_header_table", 2,
                                  ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Close", ImGuiTableColumnFlags_WidthFixed, 24.0f);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                PanelIcon(ICON_COMPASS, Colors::kAccent, &settings);
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

                ImGui::TableSetColumnIndex(1);
                if(XButton("##ssh_settings_close", "Close", &settings))
                {
                    close_popup = true;
                }
                ImGui::EndTable();
            }
        }
        EndPanelCard();

        BeginPanelCard("##ssh_settings_body", PanelCardTone::kMain, ImVec2(12.0f, 4.0f),
                       false, &settings);
        {
            begin_card("##ssh_profile_card");
            {
                section_title("Profile");
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
                section_title("Connection");
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
                section_title("Authentication");
                ImGui::Spacing();

                if(ImGui::BeginTable("##ssh_auth_table", 2,
                                      ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed,
                                            LABEL_WIDTH);
                    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    label("Password");
                    ImGui::TableSetColumnIndex(1);
                    reveal_toggle("##rpass", m_working.password, nullptr, m_show_password);

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
                    reveal_toggle("##rkeypass", m_working.passphrase,
                                  "Leave blank for unencrypted keys or ssh-agent",
                                  m_show_passphrase);

                    ImGui::EndTable();
                }
            }
            end_card();
        }
        EndPanelCard();

        BeginPanelCard("##ssh_settings_footer", PanelCardTone::kFrame, ImVec2(14.0f, 8.0f),
                       true, &settings);
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
                if(ColoredButton("Save", settings.GetColor(Colors::kAccent),
                                 settings.GetColor(Colors::kAccentHover),
                                 settings.GetColor(Colors::kAccentActive),
                                 settings.GetColor(Colors::kTextOnAccent), nullptr,
                                 ImVec2(BUTTON_WIDTH, 0.0f)))
                {
                    accept      = true;
                    close_popup = true;
                }
                ImGui::EndTable();
            }
        }
        EndPanelCard();

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

    ImGui::PopStyleVar(2);  // WindowPadding, WindowRounding
    popup_style.PopStyles();

    return m_open;
}

}  // namespace View
}  // namespace RocProfVis
