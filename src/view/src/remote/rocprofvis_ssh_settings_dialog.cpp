// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ssh_settings_dialog.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_remote_trace_orchestrator.h"
#include "rocprofvis_secret_store.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_ssh_auth_modal.h"
#include "rocprofvis_ssh_uri.h"
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

static const char* kSshSettingsPopupName = "Remote Settings";

SshSettingsDialog::SshSettingsDialog(SshConnectionStore& store, const std::string& initial_id,
                                     std::function<void(const std::string&)> on_commit)
: m_store(store)
, m_working()
, m_on_commit(std::move(on_commit))
, m_test_uri(std::make_shared<RemoteUri>())
, m_test(nullptr)
, m_test_result()
, m_test_ok(false)
, m_secrets_persist(SecretStore::IsAvailable())
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
    ClearConnectionTest();
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
    ClearConnectionTest();
    // Left unnamed so the profile is labelled by its endpoint (user@host:port).
    m_working    = SshConnectionConfig();
    m_working.id = SshConnectionConfig::GenerateId();
}

void
SshSettingsDialog::ClearConnectionTest()
{
    // Dropping the orchestrator cancels an in-flight test. The auth modal
    // closes on its own once this dialog stops beginning it.
    m_test.reset();
    m_test_result.clear();
    m_test_ok = false;
}

void
SshSettingsDialog::StartConnectionTest()
{
    // With no command or result path on the URI, Start() stops after a
    // successful authentication.
    ClearConnectionTest();
    m_test_uri->SetConnection(m_working);
    m_test = std::make_unique<RemoteTraceOrchestrator>(m_test_uri, nullptr);
    m_test->Start();
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
    popup_style.CenterPopup(ImGuiCond_Always);

    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();

    // Fixed width, auto height, so no section clips and no scrollbar is needed.
    const float dialog_width =
        GetResponsiveWindowSize(ImVec2(560.0f, 0.0f), ImVec2(480.0f, 0.0f)).x;
    ImGui::SetNextWindowSizeConstraints(ImVec2(dialog_width, 0.0f),
                                        ImVec2(dialog_width, FLT_MAX));

    if(ImGui::BeginPopupModal(
           kSshSettingsPopupName, nullptr,
           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        // Keep only a finished test's verdict, so its connection is not left open.
        if(m_test && !m_test->IsRunning())
        {
            m_test_ok     = !m_test->HasFailed();
            m_test_result = m_test_ok ? "Connection succeeded." : m_test->GetStatusMessage();
            m_test.reset();
        }

        const float button_width =
            ImGui::CalcTextSize("Cancel").x + 2.0f * style.FramePadding.x;
        const float profile_button_w =
            ImGui::CalcTextSize("Delete").x + 2.0f * style.FramePadding.x;

        const ImU32 text_dim  = settings.GetColor(Colors::kTextDim);
        ImFont*     icon_font = settings.GetFontManager().GetFont(FontType::kIcon);

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
                                 bool& show) -> bool {
            const float eye_w = ImGui::GetFrameHeight();
            ImGui::SetNextItemWidth(-(eye_w + style.ItemInnerSpacing.x));
            ImGuiInputTextFlags flags   = show ? 0 : ImGuiInputTextFlags_Password;
            bool                changed = InputTextStringWithHint(id, hint, value, flags);
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            // Scope by the field id: both toggles render the same eye glyph, and
            // IconButton derives its ID from the glyph, so without this they would
            // collide (same ImGui ID) inside the shared field column.
            ImGui::PushID(id);
            if(IconButton(show ? ICON_EYE_SLASH : ICON_EYE, icon_font,
                          ImVec2(eye_w, eye_w), show ? "Hide" : "Show", false,
                          style.FramePadding))
            {
                show = !show;
            }
            ImGui::PopID();
            return changed;
        };

        // Delegate to the shared panel-card helper.
        auto begin_card = [&](const char* id) {
            BeginPanelCard(id, PanelCardTone::kPanel, style.WindowPadding, true,
                           &settings);
        };

        auto end_card = []() { EndPanelCard(); };

        bool close_popup = false;
        bool accept      = false;

        begin_card("##ssh_connection_card");
        {
            PanelIcon(ICON_CHAIN, Colors::kAccent, &settings,
                      settings.GetFontManager().GetFontSize(FontSize::kMedium));
            ImGui::SameLine();
            section_title("SSH Connection");
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextWrapped("Saved connections for opening traces and profiling "
                               "on a remote machine.");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            // Label and field columns as groups: every row is one frame high, so
            // row i of each group lines up at normal item spacing.
            ImGui::BeginGroup();
            label("Profile");
            label("Name");
            label("Host");
            label("User");
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            {
                ImGui::SetNextItemWidth(
                    -(profile_button_w * 2.0f + style.ItemSpacing.x * 2.0f));
                const std::vector<SshConnectionConfig>& connections = m_store.List();
                auto profile_label = [](const SshConnectionConfig& cfg) {
                    std::string text = cfg.DisplayLabel();
                    return text.empty() ? std::string("New connection") : text;
                };
                std::string combo_label = profile_label(m_working);
                PushComboStyles();
                if(ImGui::BeginCombo("##sshprofile", combo_label.c_str()))
                {
                    for(const SshConnectionConfig& cfg : connections)
                    {
                        bool        selected = (cfg.id == m_working.id);
                        std::string item     = profile_label(cfg) + "##" + cfg.id;
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

                ImGui::SameLine();
                if(ImGui::Button("New", ImVec2(profile_button_w, 0.0f)))
                {
                    BeginNewConnection();
                }
                ImGui::SameLine();
                bool exists_in_store = m_store.Get(m_working.id) != nullptr;
                if(!exists_in_store)
                {
                    ImGui::BeginDisabled();
                }
                if(ImGui::Button("Delete", ImVec2(profile_button_w, 0.0f)))
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

                ImGui::SetNextItemWidth(-FLT_MIN);
                InputTextStringWithHint("##rname", "Optional, e.g. Lab server",
                                        m_working.display_name);

                const float port_w =
                    ImGui::CalcTextSize("65535").x + style.FramePadding.x * 4.0f;
                ImGui::SetNextItemWidth(-(port_w + ImGui::CalcTextSize("Port").x +
                                          style.ItemSpacing.x * 2.0f));
                if(InputTextStringWithHint("##rhost", "Hostname or IP address",
                                           m_working.host))
                {
                    ClearConnectionTest();
                }
                ImGui::SameLine();
                label("Port");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-FLT_MIN);
                if(InputTextString("##rport", m_working.port))
                {
                    ClearConnectionTest();
                }

                ImGui::SetNextItemWidth(-FLT_MIN);
                if(InputTextStringWithHint("##ruser", "Username on the remote machine",
                                           m_working.user))
                {
                    ClearConnectionTest();
                }
            }
            ImGui::EndGroup();
        }
        end_card();

        begin_card("##ssh_auth_card");
        {
            PanelIcon(ICON_LOCKED, Colors::kAccent, &settings,
                      settings.GetFontManager().GetFontSize(FontSize::kMedium));
            ImGui::SameLine();
            section_title("Authentication");
            ImGui::PushStyleColor(ImGuiCol_Text, text_dim);
            ImGui::TextWrapped(
                "%s", m_secrets_persist
                          ? "All optional. Blank fields fall back to ssh-agent and "
                            "your ~/.ssh keys, and you are asked for a password if "
                            "needed. Saved passwords go to the system credential "
                            "store."
                          : "All optional. Blank fields fall back to ssh-agent and "
                            "your ~/.ssh keys, and you are asked for a password if "
                            "needed. Passwords can't be saved on this system.");
            ImGui::PopStyleColor();
            ImGui::Spacing();

            ImGui::BeginGroup();
            label("Password");
            label("SSH key");
            label("Passphrase");
            ImGui::EndGroup();
            ImGui::SameLine();
            ImGui::BeginGroup();
            {
                if(reveal_toggle("##rpass", m_working.password,
                                 "Leave blank to use keys or be asked "
                                 "when connecting",
                                 m_show_password))
                {
                    ClearConnectionTest();
                }

                ImGui::SetNextItemWidth(-FLT_MIN);
                if(InputTextStringWithHint("##rkey",
                                           "Private key path, e.g. ~/.ssh/id_ed25519",
                                           m_working.identity_file))
                {
                    ClearConnectionTest();
                }

                if(reveal_toggle("##rkeypass", m_working.passphrase,
                                 "Only if the key is encrypted", m_show_passphrase))
                {
                    ClearConnectionTest();
                }
            }
            ImGui::EndGroup();
        }
        end_card();

        const float test_width =
            ImGui::CalcTextSize("Test connection").x + style.FramePadding.x * 4.0f;
        const float action_width =
            test_width + button_width * 2.0f + style.ItemSpacing.x * 2.0f;
        // Status note on the left, actions right-aligned.
        const float note_width =
            ImGui::GetContentRegionAvail().x - action_width - style.ItemSpacing.x;
        {
            const bool  testing  = m_test != nullptr;
            ImU32       note_col = text_dim;
            std::string note;
            if(testing)
            {
                note_col = settings.GetColor(Colors::kAccent);
                note     = m_test->GetStatusMessage();
            }
            else if(!m_test_result.empty())
            {
                note_col = settings.GetColor(m_test_ok ? Colors::kTextSuccess
                                                       : Colors::kTextError);
                note     = m_test_result;
            }
            ImGui::BeginGroup();
            if(!note.empty())
            {
                ImGui::AlignTextToFramePadding();
                ImGui::PushStyleColor(ImGuiCol_Text, note_col);
                ElidedText(note.c_str(), note_width, 360.0f, Alignment_Left, true);
                ImGui::PopStyleColor();
            }
            ImGui::EndGroup();

            ImGui::SameLine(ImGui::GetContentRegionMax().x - action_width);
            ImGui::BeginGroup();
            const bool can_test = !testing && !m_working.HostTrimmed().empty() &&
                                  !m_working.UserTrimmed().empty();
            if(!can_test)
            {
                ImGui::BeginDisabled();
            }
            if(ImGui::Button(testing ? "Testing...##test" : "Test connection##test",
                             ImVec2(test_width, 0.0f)))
            {
                StartConnectionTest();
            }
            if(!can_test)
            {
                ImGui::EndDisabled();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(button_width, 0.0f)))
            {
                close_popup = true;
            }
            ImGui::SameLine();
            if(AccentButton("Save", ImVec2(button_width, 0.0f), &settings))
            {
                accept      = true;
                close_popup = true;
            }
            ImGui::EndGroup();
        }

        // This dialog is itself a modal, so the test session's prompts are drawn
        // nested inside it instead of by the centralized path.
        if(m_test)
        {
            if(SshSession* session = m_test->GetSession())
            {
                session->SetAuthModalSelfManaged(true);
                RenderSshAuthModal(session);
            }
        }

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

        ImGui::EndPopup();
    }
    return m_open;
}

}  // namespace View
}  // namespace RocProfVis
