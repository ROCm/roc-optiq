// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Assistant settings: the Settings-dialog page, and the credential-store access
// behind its API-key field. These are SettingsPanel and SettingsManager members
// because the page edits the same UserSettings copy and OK/Cancel state as every
// other page, but the bodies live here so the assistant owns its own settings
// and the shared files carry none of it.
//
// Reading and writing the endpoint list stays in rocprofvis_settings_manager.cpp
// with every other Serialize/Deserialize pair: it runs unguarded, so a build
// with the assistant off still round-trips a saved configuration instead of
// erasing it.
#include "imgui.h"

#include "icons/rocprovfis_icon_defines.h"
#include "remote/rocprofvis_secret_store.h"
#include "rocprofvis_ai_assistant.h"
#include "rocprofvis_ai_client.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

namespace
{
// Rows the Model field's list shows before it scrolls.
constexpr int   ASSISTANT_MODEL_LIST_ROWS       = 12;
constexpr float ASSISTANT_MODEL_LIST_MAX_SCREEN = 0.75f;

// A model id is data, not an ImGui label: `##` inside a valid id must stay
// visible and must not become the item's identifier.
bool
ModelSelectable(const std::string& name, bool selected, size_t index, float row_width)
{
    ImGui::PushID(static_cast<int>(index));
    const ImVec2 text_pos = ImGui::GetCursorScreenPos();
    const bool picked = ImGui::Selectable("##model", selected, ImGuiSelectableFlags_None,
                                          ImVec2(row_width, ImGui::GetTextLineHeight()));
    ImGui::GetWindowDrawList()->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text),
                                        name.c_str());
    if(ImGui::IsItemHovered() && ImGui::CalcTextSize(name.c_str()).x > row_width)
    {
        SetTooltipStyled("%s", name.c_str());
    }
    ImGui::PopID();
    return picked;
}

// Credential-store entry for one endpoint. Every endpoint gets its own entry,
// and since the URL under a name can change, the value also records the origin
// the key was saved for (SplitStoredToken).
std::string
AssistantTokenKey(const std::string& provider_name)
{
    if(provider_name.empty())
    {
        return ASSISTANT_TOKEN_SECRET_KEY;
    }
    return std::string(ASSISTANT_TOKEN_SECRET_KEY) + "/" + provider_name;
}

// Separates the origin from the key in a stored value; neither field can hold
// a line break.
constexpr char ASSISTANT_TOKEN_ORIGIN_SEPARATOR = '\n';

// Drops the session copy of an endpoint's key, if there is one.
void
ForgetSessionToken(std::map<std::string, std::string>& session_tokens,
                   const std::string&                  provider_name)
{
    const std::map<std::string, std::string>::iterator session =
        session_tokens.find(provider_name);
    if(session != session_tokens.end())
    {
        AssistantWipeSecret(session->second);
        session_tokens.erase(session);
    }
}

// The origin the named endpoint points at now. Empty when there is no such
// endpoint or its URL has no scheme.
std::string
AssistantProviderOrigin(const AssistantSettings& assistant,
                        const std::string&       provider_name)
{
    for(const AssistantProvider& provider : assistant.providers)
    {
        if(provider.name == provider_name)
        {
            return AssistantUrlOrigin(provider.endpoint_url);
        }
    }
    return std::string();
}

// What is kept for a key: the origin it was saved for, a line break, the key.
std::string
JoinStoredToken(const std::string& origin, const std::string& token)
{
    return origin + ASSISTANT_TOKEN_ORIGIN_SEPARATOR + token;
}

// A value written before origins were recorded has no line break: it is all
// key, and its origin comes back empty. Surrounding whitespace is skipped,
// because such a key may have been pasted with a trailing newline, which would
// otherwise read as the separator with the key on the origin's side of it.
void
SplitStoredToken(const std::string& stored, std::string& origin_out,
                 std::string& token_out)
{
    origin_out.clear();
    token_out.clear();
    const char* const blank = " \t\r\n";
    const size_t      first = stored.find_first_not_of(blank);
    if(first == std::string::npos)
    {
        return;
    }
    const size_t last      = stored.find_last_not_of(blank);
    const size_t separator = stored.find(ASSISTANT_TOKEN_ORIGIN_SEPARATOR, first);
    const bool   has_origin = separator != std::string::npos && separator < last;
    if(has_origin)
    {
        origin_out = stored.substr(first, separator - first);
    }
    const size_t token_start = has_origin ? separator + 1 : first;
    token_out                = stored.substr(token_start, last + 1 - token_start);
}
}  // namespace

// Moves a key written before endpoints were named onto the first endpoint, once
// at load, then deletes the unnamed entry. Reading it as a live fallback
// instead would let any endpoint without a key of its own inherit this one -
// which is how a key for one provider ends up posted to another's host.
void
SettingsManager::MigrateLegacyAssistantToken()
{
    if(!SecretStore::IsAvailable() || m_usersettings.assistant.providers.empty())
    {
        return;
    }

    std::string legacy_token;
    if(!SecretStore::Get(ASSISTANT_TOKEN_SECRET_KEY, legacy_token) ||
       legacy_token.empty())
    {
        return;
    }

    // The single endpoint that existed before the list did is the first one.
    const std::string key =
        AssistantTokenKey(m_usersettings.assistant.providers.front().name);
    if(key != ASSISTANT_TOKEN_SECRET_KEY)
    {
        std::string existing_token;
        const bool  already_migrated =
            SecretStore::Get(key, existing_token) && !existing_token.empty();
        AssistantWipeSecret(existing_token);
        if(already_migrated || SecretStore::Set(key, legacy_token))
        {
            SecretStore::Erase(ASSISTANT_TOKEN_SECRET_KEY);
        }
        else
        {
            // Leave the old entry alone rather than destroying the only copy.
            spdlog::warn("Could not migrate the saved assistant key; leaving it in "
                         "place");
        }
    }
    // Like every other copy of the key, wiped once its job is done.
    AssistantWipeSecret(legacy_token);
}

bool
SettingsManager::HasAssistantToken(const std::string& provider_name) const
{
    std::string unused;
    const bool  has_token = GetAssistantToken(provider_name, unused);
    AssistantWipeSecret(unused);
    return has_token;
}

/*
 * Reads this endpoint's key from this session's memory, then from the
 * credential store. A session copy exists only when the store could not take
 * the key, so it is the newer one. Never falls through to another endpoint's
 * key: an endpoint with nothing saved has no key.
 *
 * The key is returned only while the endpoint's URL still has the origin saved
 * with it, so an edited URL cannot carry the key to a new host. A key with no
 * origin - saved by an older build, or while the URL had no scheme - is given
 * the current origin here, once, and is not used while the store refuses that.
 */
bool
SettingsManager::GetAssistantToken(const std::string& provider_name,
                                   std::string&       out_token) const
{
    out_token.clear();

    const std::string origin =
        AssistantProviderOrigin(m_usersettings.assistant, provider_name);
    if(origin.find(ASSISTANT_TOKEN_ORIGIN_SEPARATOR) != std::string::npos)
    {
        // A hand-edited URL: this origin could not be split from the key again.
        return false;
    }

    const std::string key = AssistantTokenKey(provider_name);
    std::string       stored;
    const std::map<std::string, std::string>::iterator session =
        m_assistant_token_session.find(provider_name);
    const bool in_session =
        session != m_assistant_token_session.end() && !session->second.empty();
    if(in_session)
    {
        stored = session->second;
    }
    else if(!SecretStore::IsAvailable() || !SecretStore::Get(key, stored) ||
            stored.empty())
    {
        return false;
    }

    std::string saved_origin;
    SplitStoredToken(stored, saved_origin, out_token);
    AssistantWipeSecret(stored);
    if(saved_origin.empty() && !origin.empty() && !out_token.empty())
    {
        stored = JoinStoredToken(origin, out_token);
        if(in_session)
        {
            AssistantWipeSecret(session->second);
            session->second = stored;
        }
        else if(!SecretStore::Set(key, stored))
        {
            // Still unbound, the key would follow whatever URL is set at the next
            // read, an edited one included. Not used until the binding is kept.
            spdlog::warn("Could not record which host the saved assistant key "
                         "belongs to, so it is not used");
            AssistantWipeSecret(stored);
            AssistantWipeSecret(out_token);
            return false;
        }
        AssistantWipeSecret(stored);
        saved_origin = origin;
    }

    if(saved_origin == origin && !out_token.empty())
    {
        return true;
    }
    AssistantWipeSecret(out_token);
    return false;
}

// Saves the key with the origin of the endpoint's URL, or clears it when the
// token is empty. Returns false when the credential store refused it; the key
// is then kept for this session only.
bool
SettingsManager::SetAssistantToken(const std::string& provider_name,
                                   const std::string& token)
{
    // A key of nothing but whitespace is no key, and stored it would read back
    // as an entry with no separator.
    if(token.find_first_not_of(" \t\r\n") == std::string::npos)
    {
        return ClearAssistantToken(provider_name);
    }

    std::string stored = JoinStoredToken(
        AssistantProviderOrigin(m_usersettings.assistant, provider_name), token);
    // When the vault takes the key it owns it: a second copy in process memory
    // would only widen the window where the key can be read back out.
    ForgetSessionToken(m_assistant_token_session, provider_name);
    const bool store_available = SecretStore::IsAvailable();
    const bool saved =
        !store_available || SecretStore::Set(AssistantTokenKey(provider_name), stored);
    if(!saved)
    {
        spdlog::warn("Could not save the assistant key to the credential store; "
                     "keeping it for this session only");
    }
    if(!store_available || !saved)
    {
        m_assistant_token_session[provider_name] = stored;
    }
    AssistantWipeSecret(stored);
    return saved;
}

// Forgets this endpoint's key. Other endpoints keep theirs.
bool
SettingsManager::ClearAssistantToken(const std::string& provider_name)
{
    ForgetSessionToken(m_assistant_token_session, provider_name);
    if(!SecretStore::IsAvailable() ||
       SecretStore::Erase(AssistantTokenKey(provider_name)))
    {
        return true;
    }
    spdlog::warn("Could not remove the assistant key from the credential store");
    return false;
}

void
SettingsPanel::RenderAssistantSettings()
{
    ImGuiStyle& style = ImGui::GetStyle();

    AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.providers.empty())
    {
        assistant.providers.push_back(MakeDefaultAssistantProvider());
        assistant.active = 0;
    }
    if(assistant.active >= assistant.providers.size())
    {
        assistant.active = 0;
    }
    AssistantProvider& provider = assistant.providers[assistant.active];
    ApplyAssistantEndpointDefaults(provider);

    ImGui::TextUnformatted("Assistant");
    ImGui::Separator();
    ImGui::TextWrapped(
        "OpenAI-compatible chat endpoint. The key entered below is stored in "
        "the OS credential store, never in the settings file.");

    const float label_width =
        ImGui::CalcTextSize("Model").x + 2.0f * style.ItemSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("URL");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(-1.0f);
    if(InputTextStringWithHint("##assistant_url", "https://<host>/<path>",
                               provider.endpoint_url))
    {
        m_settings_changed = true;
    }
    if(ImGui::IsItemDeactivatedAfterEdit())
    {
        RequestAssistantModels();
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled(
            "Base URL, e.g. https://api.openai.com/v1. /chat/completions is "
            "appended, and a plain-http URL with no path, such as a local "
            "Ollama, uses /v1. Model is the deployment id when the path ends in "
            "/azure, /azure/engines, /openai, or /openai/deployments. A path "
            "that already contains /engines/ or /deployments/ keeps that one.");
    }
    // Shown under the field for as long as they apply, not in the tooltip.
    ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextWarning));
    if(AssistantUrlIsCleartext(provider.endpoint_url))
    {
        ImGui::TextWrapped(
            "Unencrypted connection: questions and trace data sent to this "
            "endpoint can be read or altered on the network.");
    }
    if(AssistantStorableUrl(provider.endpoint_url) != provider.endpoint_url)
    {
        ImGui::TextWrapped(
            "The settings file is plain text, so credentials, a fragment, and "
            "every query parameter except api-version are removed from this "
            "URL when you press OK. Put the key in the Key field instead.");
    }
    ImGui::PopStyleColor();

    // The field always takes a typed name. The arrow beside it appears only
    // when this URL listed its models, and picking one just fills the field.
    AssistantModelCatalog& catalog = AssistantPanel::GetInstance()->ModelCatalog();
    const std::vector<std::string>& models = catalog.Models();
    const bool  listed  = !models.empty() && catalog.Url() == provider.endpoint_url;
    const float arrow_w = ImGui::GetFrameHeight();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Model");
    ImGui::SameLine(label_width);
    ImGui::SetNextItemWidth(listed ? -(arrow_w + style.ItemInnerSpacing.x) : -1.0f);
    if(InputTextStringWithHint("##assistant_model", ASSISTANT_DEFAULT_MODEL,
                               provider.model))
    {
        m_settings_changed = true;
    }
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled(
            "Model name sent with an OpenAI-style request. When the URL path "
            "ends in /azure, /azure/engines, /openai, or /openai/deployments, "
            "this is the deployment id, and it is unused when that path "
            "already names one. Default is %s.",
            ASSISTANT_DEFAULT_MODEL);
    }
    if(listed)
    {
        // The list opens leftward, under the whole field, rather than under
        // the arrow alone.
        const float field_x = ImGui::GetItemRectMin().x;
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        const float list_w = ImGui::GetCursorScreenPos().x + arrow_w - field_x;
        const float widest = AssistantPanel::GetInstance()->ModelNameWidth();
        const float max_w =
            ImGui::GetMainViewport()->WorkSize.x * ASSISTANT_MODEL_LIST_MAX_SCREEN;
        const float popup_w =
            std::min(max_w, std::max(list_w, widest + style.WindowPadding.x * 2.0f));
        const float row_w = popup_w - style.WindowPadding.x * 2.0f;
        const float list_h =
            ImGui::GetTextLineHeightWithSpacing() * ASSISTANT_MODEL_LIST_ROWS +
            style.WindowPadding.y * 2.0f;
        ImGui::SetNextWindowSizeConstraints(ImVec2(popup_w, 0.0f),
                                            ImVec2(popup_w, list_h));
        if(ImGui::BeginCombo("##assistant_model_list", nullptr,
                             ImGuiComboFlags_NoPreview | ImGuiComboFlags_PopupAlignLeft))
        {
            for(size_t i = 0; i < models.size(); ++i)
            {
                const std::string& name     = models[i];
                const bool         selected = name == provider.model;
                if(ModelSelectable(name, selected, i, row_w))
                {
                    provider.model     = name;
                    m_settings_changed = true;
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Dummy(ImVec2(0.0f, style.ItemSpacing.y));
    ImGui::TextUnformatted("API key");
    ImGui::Separator();

    if(m_assistant_token_stored < 0)
    {
        m_assistant_token_stored    = m_settings.HasAssistantToken(provider.name) ? 1 : 0;
        m_assistant_store_available = SecretStore::IsAvailable();
    }
    // A key saved for the origin the dialog opened with is refused once the URL
    // names another, so OK removes it and the page says so instead of offering
    // to keep it.
    const bool token_present = m_assistant_token_stored == 1 && !m_assistant_clear_token;
    const bool token_stale   = token_present && AssistantOriginChanged();
    const bool token_stored  = token_present && !token_stale;
    if(token_stale && m_assistant_token_draft.empty())
    {
        ImGui::TextWrapped(
            "The stored key belongs to the previous host and is removed when you "
            "press OK. Enter the key for this endpoint.");
    }
    else if(token_stored && m_assistant_token_draft.empty())
    {
        ImGui::TextUnformatted("A key is stored. Leave the field blank to keep it.");
    }
    else if(m_assistant_store_available)
    {
        ImGui::TextWrapped(
            "The key typed here is saved to the OS credential store when you "
            "press OK, never to the settings file.");
    }
    else
    {
        ImGui::TextWrapped(
            "No OS credential store is available. The key is kept in memory "
            "for this session only.");
    }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Key");
    ImGui::SameLine();
    const float eye_w = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(-(eye_w + style.ItemInnerSpacing.x));
    ImGuiInputTextFlags token_flags =
        m_assistant_show_token ? 0 : ImGuiInputTextFlags_Password;
    if(InputTextStringWithHint("##assistant_token",
                               token_stored ? "Stored — type to replace" : "API key",
                               m_assistant_token_draft, token_flags))
    {
        m_assistant_clear_token = false;
        m_settings_changed      = true;
    }
    if(ImGui::IsItemDeactivatedAfterEdit())
    {
        RequestAssistantModels();
    }
    ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
    ImFont* icon_font = m_fonts.GetFont(FontType::kIcon);
    ImGui::PushID("assistant_token_reveal");
    if(IconButton(m_assistant_show_token ? ICON_EYE_SLASH : ICON_EYE, icon_font,
                  ImVec2(eye_w, eye_w), m_assistant_show_token ? "Hide" : "Show"))
    {
        m_assistant_show_token = !m_assistant_show_token;
    }
    ImGui::PopID();

    if(ImGui::Button("Clear stored key"))
    {
        AssistantWipeSecret(m_assistant_token_draft);
        m_assistant_clear_token = true;
        m_settings_changed      = true;
        RequestAssistantModels();
    }
}

// Called from OK, so nothing typed on this page reaches the credential store
// until the user accepts the dialog.
void
SettingsPanel::ApplyAssistantTokenEdits()
{
    // Endpoints dropped by Reset go first: their keys have nothing pointing at
    // them once the configuration is gone.
    bool erase_failed = false;
    for(const std::string& orphan : m_assistant_orphaned_providers)
    {
        erase_failed |= !m_settings.ClearAssistantToken(orphan);
    }

    // The dialog closes on OK, so a notification is the only way the user
    // learns that the store refused a change or that a key was dropped.
    const auto warn = [](const char* message) {
        NotificationManager::GetInstance().Show(message, NotificationLevel::Warning);
    };
    AssistantProvider* provider = nullptr;
    if(m_usersettings.assistant.active < m_usersettings.assistant.providers.size())
    {
        provider = &m_usersettings.assistant.providers[m_usersettings.assistant.active];
        ApplyAssistantEndpointDefaults(*provider);
        // Kept as it can be saved, so this session sends what the next one
        // loads. A key's origin holds neither userinfo nor query: unchanged.
        const std::string storable = AssistantStorableUrl(provider->endpoint_url);
        if(storable != provider->endpoint_url)
        {
            provider->endpoint_url = storable;
            warn("Credentials and query parameters other than api-version were "
                 "removed from the assistant URL, because the settings file is "
                 "plain text. Put the key in the Key field.");
        }
    }
    const std::string provider_name = ActiveAssistantProviderName();
    const AssistantProvider* saved_provider = nullptr;
    for(const AssistantProvider& saved : m_usersettings_initial.assistant.providers)
    {
        if(saved.name == provider_name)
        {
            saved_provider = &saved;
            break;
        }
    }
    const bool origin_changed = provider != nullptr && saved_provider != nullptr &&
                                !AssistantEndpointsShareOrigin(
                                    provider->endpoint_url, saved_provider->endpoint_url);

    // A provider name is its credential-store key. When its origin changes,
    // remove the old origin's key before accepting the new URL. If removal
    // fails, keep the old endpoint rather than risk posting that key elsewhere.
    if(origin_changed && !m_settings.ClearAssistantToken(provider_name))
    {
        *provider = *saved_provider;
        const std::string message =
            "Could not detach the saved assistant key, so the URL was not changed.";
        spdlog::warn(message);
        NotificationManager::GetInstance().Show(message, NotificationLevel::Error);
    }
    else if(m_assistant_clear_token)
    {
        // A host change has already cleared it above.
        if(!origin_changed)
        {
            erase_failed |= !m_settings.ClearAssistantToken(provider_name);
        }
    }
    else if(!m_assistant_token_draft.empty())
    {
        if(!m_settings.SetAssistantToken(provider_name, m_assistant_token_draft))
        {
            warn("Could not save the API key to the credential store. It will be "
                 "kept for this session only.");
        }
    }
    else if(origin_changed && m_assistant_token_stored == 1)
    {
        warn("The assistant URL now points at a different host, so the stored API "
             "key was removed. Enter the key for the new endpoint.");
    }
    if(erase_failed)
    {
        warn("Could not remove the API key from the credential store. It may "
             "still be stored on this machine.");
    }
    DiscardAssistantTokenEdits();
}

// True when OK will treat the edited URL as another host and remove the stored
// key: the same test ApplyAssistantTokenEdits makes, on the URL OK will keep.
bool
SettingsPanel::AssistantOriginChanged() const
{
    const AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.active >= assistant.providers.size())
    {
        return false;
    }
    const AssistantProvider& provider = assistant.providers[assistant.active];
    for(const AssistantProvider& saved : m_usersettings_initial.assistant.providers)
    {
        if(saved.name == provider.name)
        {
            return !AssistantEndpointsShareOrigin(
                AssistantStorableUrl(provider.endpoint_url), saved.endpoint_url);
        }
    }
    return false;
}

void
SettingsPanel::DiscardAssistantTokenEdits()
{
    AssistantWipeSecret(m_assistant_token_draft);
    m_assistant_orphaned_providers.clear();
    m_assistant_clear_token  = false;
    m_assistant_token_stored = -1;
}

// The endpoint being edited, which is the key its token is stored under.
std::string
SettingsPanel::ActiveAssistantProviderName() const
{
    const AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.active >= assistant.providers.size())
    {
        return std::string();
    }
    return assistant.providers[assistant.active].name;
}

// Endpoints that disappear here are remembered so OK can delete their keys too,
// rather than leaving entries in the credential store nothing refers to.
void
SettingsPanel::ResetAssistantOptions()
{
    const AssistantProvider default_provider = MakeDefaultAssistantProvider();
    for(const AssistantProvider& provider : m_usersettings.assistant.providers)
    {
        if(provider.name != default_provider.name)
        {
            m_assistant_orphaned_providers.push_back(provider.name);
        }
    }

    m_usersettings.assistant.providers.clear();
    m_usersettings.assistant.providers.push_back(default_provider);
    m_usersettings.assistant.active = 0;
    AssistantWipeSecret(m_assistant_token_draft);
    // The default endpoint has nothing configured now, so its key should go too.
    m_assistant_clear_token = true;
    // The endpoint being edited may have another name now: ask the store again.
    m_assistant_token_stored = -1;
    m_settings_changed       = true;
    RequestAssistantModels();
}

void
SettingsPanel::RequestAssistantModels(bool saved_settings)
{
    const AssistantSettings& assistant = m_usersettings.assistant;
    if(assistant.active >= assistant.providers.size())
    {
        return;
    }
    const AssistantProvider& provider = assistant.providers[assistant.active];

    // A typed key belongs to this draft URL. A stored key is safe only while
    // the URL still matches its saved provider, or after OK/Cancel has made
    // the settings current. This prevents a draft URL from receiving the old
    // endpoint's credential.
    std::string token             = m_assistant_token_draft;
    bool        same_saved_origin = false;
    for(const AssistantProvider& saved : m_usersettings_initial.assistant.providers)
    {
        if(saved.name == provider.name &&
           AssistantEndpointsShareOrigin(saved.endpoint_url, provider.endpoint_url))
        {
            same_saved_origin = true;
            break;
        }
    }
    if(token.empty() && !m_assistant_clear_token && (saved_settings || same_saved_origin))
    {
        m_settings.GetAssistantToken(provider.name, token);
    }
    AssistantPanel::GetInstance()->ModelCatalog().Request(provider.endpoint_url, token);
}

}  // namespace View
}  // namespace RocProfVis
