// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_shared_tabs.h"
#include "../icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include <algorithm>
#include <cstring>

namespace RocProfVis
{
namespace View
{
namespace
{

static char s_save_preset_name[128] = {};

// Color tints and intrinsic widget geometry only. Padding/spacing come from the
// shared app style (GetDefaultStyle / frame padding) so the launcher matches the
// rest of the application rather than inventing its own spacing rules.
constexpr float kAccentBarWidth     = 4.0f;   // card-header leading accent bar
constexpr float kPillCloseScale     = 0.75f;  // close glyph size, fraction of font
constexpr float kPillBgAlpha        = 0.16f;
constexpr float kPillBgHoverAlpha   = 0.30f;
constexpr float kPillCloseIdleAlpha = 0.6f;
constexpr float kToggleHeightScale  = 0.82f;  // of frame height
constexpr float kToggleWidthScale   = 1.75f;  // of toggle height
constexpr float kToggleAnimSpeed    = 10.0f;
constexpr float kToggleKnobInset    = 2.0f;   // knob padding inside the track

// Minimum height for the run-output console child.
constexpr float kMinOutputConsoleHeight = 60.0f;

// Wrapped tooltip shared by every launcher "(?)", attached to the last item.
void LaunchHelpTooltip(const char* text)
{
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * kLaunchTooltipWrapEm);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

// Linear blend between two packed colors (t in [0,1]).
ImU32 LerpColor(ImU32 a, ImU32 b, float t)
{
    ImVec4 av = ImGui::ColorConvertU32ToFloat4(a);
    ImVec4 bv = ImGui::ColorConvertU32ToFloat4(b);
    ImVec4 cv(av.x + (bv.x - av.x) * t, av.y + (bv.y - av.y) * t,
              av.z + (bv.z - av.z) * t, av.w + (bv.w - av.w) * t);
    return ImGui::ColorConvertFloat4ToU32(cv);
}

} // namespace

void BeginLaunchCard(const char* id)
{
    // Delegate to the shared design-language panel card so the launcher tracks
    // the same rounded/bordered/tiered look as the remote dialogs. Padding and
    // row spacing are the app's default style.
    SettingsManager&  settings = SettingsManager::Get();
    BeginPanelCard(id, PanelCardTone::kPanel, settings.GetDefaultStyle().WindowPadding,
                   true, &settings);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        settings.GetDefaultStyle().ItemSpacing);
}

void EndLaunchCard()
{
    ImGui::PopStyleVar();  // ItemSpacing
    EndPanelCard();
    // Cards are separated by the surrounding item spacing only.
}

float LaunchActionButtonWidth(const char* label)
{
    const float text_w = ImGui::CalcTextSize(label, nullptr, true).x;
    return std::max(kLaunchActionButtonWidth,
                    text_w + ImGui::GetStyle().FramePadding.x * 2.0f);
}

void LaunchCardHeader(const char* icon, const char* title, const char* help)
{
    SettingsManager& settings = SettingsManager::Get();
    FontManager&     fonts    = settings.GetFontManager();

    // Leading accent bar sized to the header text.
    const float bar_h = ImGui::GetFrameHeight();
    ImVec2      p     = ImGui::GetCursorScreenPos();
    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y + 1.0f),
                      ImVec2(p.x + kAccentBarWidth, p.y + bar_h - 1.0f),
                      settings.GetColor(Colors::kAccent), 2.0f);

    const float bar_gap = settings.GetDefaultStyle().ItemSpacing.x;
    ImGui::Indent(kAccentBarWidth + bar_gap);

    // Optional leading icon (drawn from the icon font, in the accent color).
    if (icon && icon[0])
    {
        ImGui::PushFont(fonts.GetFont(FontType::kIcon));
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kAccent));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SameLine(0.0f, bar_gap);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();

    if (help && help[0])
    {
        HelpTip(help);
    }
    ImGui::Unindent(kAccentBarWidth + bar_gap);
}

// A dimmed "(?)" tooltip marker, frame-aligned to sit level with its field.
void
HelpTip(const char* tooltip)
{
    ImGui::SameLine();
    ImGui::PushFont(
        SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon));
    ImGui::TextDisabled(ICON_QUESTION_MARK);
    ImGui::PopFont();
    LaunchHelpTooltip(tooltip);
}

void
HelpMarker(char const* env_var, char const* desc)
{
    // Frame-align so the "(?)" sits level with the field/toggle/label it follows.
    ImGui::SameLine();
    ImGui::PushFont(
        SettingsManager::GetInstance().GetFontManager().GetFont(FontType::kIcon));
    ImGui::TextDisabled(ICON_QUESTION_MARK);
    ImGui::PopFont();
    if(ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * kLaunchTooltipWrapEm);
        ImGui::TextUnformatted(env_var);
        ImGui::Separator();
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

PillAction
EditablePill(const char* label, ImU32 accent_color)
{
    const ImVec2 pad       = ImGui::GetStyle().FramePadding;
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const float  x_size    = ImGui::GetFontSize() * kPillCloseScale;

    // The label-to-"x" gap is spacing inside one widget, hence ItemInnerSpacing.
    ImVec2 size(pad.x * 2.0f + text_size.x +
                    SettingsManager::Get().GetDefaultStyle().ItemInnerSpacing.x + x_size,
                text_size.y + pad.y * 2.0f);
    ImVec2 p = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(label, size);
    bool  hovered = ImGui::IsItemHovered();
    bool  clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    float x_left  = p.x + size.x - pad.x - x_size;
    bool  over_x  = hovered && ImGui::GetIO().MousePos.x >= x_left;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    float       rnd = size.y * 0.5f;
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                      ApplyAlpha(accent_color, hovered ? kPillBgHoverAlpha : kPillBgAlpha),
                      rnd);
    dl->AddText(ImVec2(p.x + pad.x, p.y + pad.y), accent_color, label);

    // Trailing "x" - brightens when hovered so removal is obvious.
    ImVec2 xc(x_left + x_size * 0.5f, p.y + size.y * 0.5f);
    float  arm  = x_size * 0.3f;
    ImU32  xcol = ApplyAlpha(accent_color, over_x ? 1.0f : kPillCloseIdleAlpha);
    dl->AddLine(ImVec2(xc.x - arm, xc.y - arm), ImVec2(xc.x + arm, xc.y + arm), xcol, 1.6f);
    dl->AddLine(ImVec2(xc.x - arm, xc.y + arm), ImVec2(xc.x + arm, xc.y - arm), xcol, 1.6f);

    if (clicked)
    {
        return over_x ? PillAction::kRemove : PillAction::kEdit;
    }
    return PillAction::kNone;
}

void StatusPill(const char* label, ImU32 bg_color)
{
    const ImVec2 pad       = ImGui::GetStyle().FramePadding;
    ImVec2       text_size = ImGui::CalcTextSize(label);
    ImVec2       p         = ImGui::GetCursorScreenPos();
    ImVec2       size(text_size.x + pad.x * 2.0f, text_size.y + pad.y * 2.0f);
    ImDrawList*  dl  = ImGui::GetWindowDrawList();
    float        rnd = size.y * 0.5f;

    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), bg_color, rnd);
    dl->AddText(ImVec2(p.x + pad.x, p.y + pad.y),
                SettingsManager::Get().GetColor(Colors::kTextOnAccent), label);

    ImGui::Dummy(size);
}

void LaunchSubHeader(const char* text, const char* help)
{
    SettingsManager& settings = SettingsManager::Get();
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    if (help && help[0])
    {
        HelpTip(help);
    }
}

bool ToggleSwitch(const char* label, bool* value)
{
    SettingsManager& settings = SettingsManager::Get();

    const float frame_h = ImGui::GetFrameHeight();
    const float height  = frame_h * kToggleHeightScale;
    const float width   = height * kToggleWidthScale;
    const float radius  = height * 0.5f;

    ImGui::PushID(label);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##switch", ImVec2(width, frame_h));

    bool hovered = ImGui::IsItemHovered();
    bool changed = false;
    if (ImGui::IsItemClicked())
    {
        *value  = !*value;
        changed = true;
    }

    // Animate the knob/fill between states.
    ImGuiID       id      = ImGui::GetItemID();
    ImGuiStorage* storage = ImGui::GetStateStorage();
    float         target  = *value ? 1.0f : 0.0f;
    float         t       = storage->GetFloat(id, target);
    float         step    = ImGui::GetIO().DeltaTime * kToggleAnimSpeed;
    if (t < target) { t = std::min(t + step, target); }
    else if (t > target) { t = std::max(t - step, target); }
    storage->SetFloat(id, t);

    const float y_off = (frame_h - height) * 0.5f;
    ImVec2      bar_min(p.x, p.y + y_off);
    ImVec2      bar_max(p.x + width, p.y + y_off + height);

    ImU32 off_col = settings.GetColor(Colors::kBorderGray);
    ImU32 on_col  = settings.GetColor(hovered ? Colors::kAccentHover : Colors::kAccent);
    ImU32 bar_col = LerpColor(off_col, on_col, t);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(bar_min, bar_max, bar_col, radius);

    float  knob_x = bar_min.x + radius + t * (width - 2.0f * radius);
    ImVec2 knob_c(knob_x, bar_min.y + radius);
    dl->AddCircleFilled(knob_c, radius - kToggleKnobInset,
                        settings.GetColor(Colors::kTextOnAccent));

    ImGui::PopID();

    if (label && label[0] && label[0] != '#')
    {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
    }

    return changed;
}

float ToggleSwitchWidth()
{
    return ImGui::GetFrameHeight() * kToggleHeightScale * kToggleWidthScale;
}

float
TargetLabelColumnWidth(const char* widest_label)
{
    return std::max(kLaunchLabelColumnMinWidth,
                    ImGui::CalcTextSize(widest_label).x +
                        ImGui::GetStyle().ItemSpacing.x * 2.0f);
}

bool
RenderTargetPathRow(TargetPathRow const& row, std::string& path,
                    ConnectionType connection, TargetBrowseFn const& on_browse,
                    float label_w)
{
    bool modified = false;

    // Recents are local paths, so they are not offered for a remote host.
    const bool is_remote        = (connection == ConnectionType::kSsh);
    const bool has_recent = !is_remote && row.recents != nullptr && !row.recents->empty();
    const float browse_w  = LaunchActionButtonWidth("Browse");
    const float spacing   = ImGui::GetStyle().ItemSpacing.x;

    ImGui::PushID(row.id);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(row.label);
    ImGui::SameLine(label_w);

    ImGui::SetNextItemWidth(-(browse_w + spacing +
                              (has_recent ? (ImGui::GetFrameHeight() + spacing) : 0.0f)));
    if(InputTextStringWithHint("##Path", row.hint, path))
    {
        modified = true;
    }

    // The recents dropdown sits before Browse so Browse stays right-anchored and
    // lines up with the other rows' Browse buttons.
    if (has_recent)
    {
        ImGui::SameLine();
        if(ImGui::ArrowButton("##Recent", ImGuiDir_Down))
        {
            ImGui::OpenPopup("RecentPopup");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", row.recents_tooltip);
        }
        if(ImGui::BeginPopup("RecentPopup"))
        {
            for(std::string const& recent : *row.recents)
            {
                std::string short_name = recent;
                if (short_name.size() > 48)
                {
                    short_name = "..." + short_name.substr(short_name.size() - 45);
                }
                if (ImGui::Selectable(short_name.c_str(), false))
                {
                    path     = recent;
                    modified          = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", recent.c_str());
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!on_browse);
    if(ImGui::Button("Browse", ImVec2(browse_w, 0.0f)))
    {
        on_browse(path, row.kind, row.browse_title);
    }
    ImGui::EndDisabled();

    ImGui::PopID();
    return modified;
}

bool
RenderTargetArgumentsRow(std::string& arguments, float label_w)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Arguments");
    ImGui::SameLine(label_w);
    // Reserve the Browse-row trailing width so this box's right edge lines up.
    ImGui::SetNextItemWidth(
        -(LaunchActionButtonWidth("Browse") + ImGui::GetStyle().ItemSpacing.x));
    return InputTextStringWithHint("##TargetArgs",
                                   "e.g.  --iterations 100 --input data.bin", arguments);
}

bool
RenderTargetAutoLoadRow(bool& auto_load_trace)
{
    ImGui::Spacing();
    return ImGui::Checkbox("Open the trace automatically when profiling finishes",
                           &auto_load_trace);
}

// The tools folder and Python venv rows sit together, so they share a label
// column wide enough for both.
static float
ToolLocationLabelColumnWidth()
{
    return std::max(TargetLabelColumnWidth("Tools Directory"),
                    TargetLabelColumnWidth("Python Venv"));
}

bool
RenderPythonEnvironmentSection(std::string& python_environment, ConnectionType connection,
                               TargetBrowseFn const& on_browse)
{
    TargetPathRow row;
    row.label        = "Python Venv";
    row.id           = "PythonEnv";
    row.hint         = connection == ConnectionType::kSsh
                           ? "remote /path/to/venv (leave empty for the system Python)"
                           : "/path/to/venv (leave empty for the system Python)";
    row.browse_title = "Choose Python Virtual Environment";
    row.kind         = Directory;

    return RenderTargetPathRow(row, python_environment, connection, on_browse,
                               ToolLocationLabelColumnWidth());
}

bool
RenderToolLocationSection(std::string& tool_directory, ConnectionType connection,
                          std::string const&    resolved_hint,
                          TargetBrowseFn const& on_browse)
{
    bool modified = false;

    const bool is_remote     = (connection == ConnectionType::kSsh);

    // Shares its label column with the Python venv row beneath it.
    const float label_w  = ToolLocationLabelColumnWidth();
    const float browse_w = LaunchActionButtonWidth("Browse");
    const float spacing  = ImGui::GetStyle().ItemSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tools Directory");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-(browse_w + spacing));
    if (InputTextStringWithHint(
            "##ToolDir",
            is_remote ? "leave empty to use the remote $ROCM_PATH or $PATH"
                      : "leave empty to use $ROCM_PATH or $PATH",
            tool_directory))
    {
        modified = true;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Folder containing the profiler executables, for a ROCm install in a\n"
                          "non-standard location. The executable name is chosen by Optiq, so\n"
                          "the tool must be present in this folder for the run to start.");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!on_browse);
    if(ImGui::Button("Browse##ToolDir", ImVec2(browse_w, 0)))
    {
        on_browse(tool_directory, Directory, "Choose Profiler Tools Directory");
    }
    ImGui::EndDisabled();

    if (!resolved_hint.empty())
    {
        ImGui::Dummy(ImVec2(label_w - ImGui::GetStyle().ItemSpacing.x, 0.0f));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", resolved_hint.c_str());
    }

    return modified;
}

void PushSlimScrollbarStyle()
{
    constexpr float SLIM_SCROLLBAR_SIZE    = 10.0f;
    constexpr float SLIM_SCROLLBAR_PADDING = 2.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, SLIM_SCROLLBAR_SIZE);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarPadding, SLIM_SCROLLBAR_PADDING);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,
                          SettingsManager::Get().GetColor(Colors::kTransparent));
}

void PopSlimScrollbarStyle()
{
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

void BeginCodePanel(const char* id, ImVec2 size)
{
    // The launch cards' surface, at a size the caller chooses so the panel can
    // fill a column or the run view. The style applies to this card only; the
    // code box inside sets its own.
    SettingsManager& settings = SettingsManager::Get();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        settings.GetDefaultStyle().ItemSpacing);
}

void EndCodePanel()
{
    ImGui::PopStyleVar();  // ItemSpacing
    ImGui::EndChild();
}

void RenderCodeBox(const char* id, std::string const& text, bool follow_tail)
{
    // Rounded like an input field rather than like the card around it, so the
    // nested corners stay concentric.
    SettingsManager& settings = SettingsManager::Get();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kPanelBorderSubtle));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    PushSlimScrollbarStyle();
    ImGui::BeginChild(id, ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    PopSlimScrollbarStyle();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::PushFont(settings.GetFontManager().GetFont(FontType::kCode), 0.0f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopFont();
    if (follow_tail && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
}

void SameLineRightAligned(float width)
{
    ImGui::SameLine();
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > width)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - width);
    }
}

float ButtonWidth(const char* label)
{
    return ImGui::CalcTextSize(label, nullptr, true).x +
           ImGui::GetStyle().FramePadding.x * 2.0f;
}

float CheckboxWidth(const char* label)
{
    return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
           ImGui::CalcTextSize(label, nullptr, true).x;
}

void RenderCommandPreview(std::string const& preview_text)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Command Preview");
    SameLineRightAligned(ButtonWidth("Copy"));
    if (ImGui::Button("Copy##CmdPreview"))
    {
        ImGui::SetClipboardText(preview_text.c_str());
    }
    RenderCodeBox("CmdPreview", preview_text, false);
}

bool RenderOutputConsole(
    std::string const& output_text,
    std::string const& error_message,
    std::string const& state_label,
    ConsoleStatusLevel state_level,
    std::string const& detail,
    bool&              auto_scroll)
{
    bool clear_requested = false;
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Output");

    // The badge color follows the semantic level, sourced from the theme so it
    // stays consistent with the rest of the app (and tracks light/dark themes).
    SettingsManager& settings = SettingsManager::Get();
    Colors color_id = Colors::kTextMain;
    switch (state_level)
    {
        case ConsoleStatusLevel::kSuccess: color_id = Colors::kTextSuccess; break;
        case ConsoleStatusLevel::kError:   color_id = Colors::kTextError;   break;
        case ConsoleStatusLevel::kIdle:
        case ConsoleStatusLevel::kRunning:
        default:                           color_id = Colors::kTextMain;    break;
    }
    ImVec4 state_color = ImGui::ColorConvertU32ToFloat4(settings.GetColor(color_id));

    float spacing = 4.0f;
    ImGui::SameLine(0.0f, spacing);
    ImGui::TextColored(state_color, "[%s]", state_label.c_str());

    // Optional phase detail (e.g. the remote download path) next to the badge.
    if (!detail.empty())
    {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextDisabled("%s", detail.c_str());
    }

    VerticalSeparator();

    if (ImGui::Button("Copy##OutputCopy"))
    {
        std::string clip;
        if (!error_message.empty())
        {
            clip = error_message + "\n\n";
        }
        clip += output_text;
        ImGui::SetClipboardText(clip.c_str());
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear##OutputClear"))
    {
        clear_requested = true;
    }

    VerticalSeparator();
    ImGui::Checkbox("Auto-scroll##Output", &auto_scroll);

    FontManager&     fonts       = settings.GetFontManager();
    ImGuiWindowFlags output_flags = ImGuiWindowFlags_HorizontalScrollbar;
    // Fill the remaining height (no uneven bottom gap below the console).
    float output_height =
        std::max(ImGui::GetContentRegionAvail().y, kMinOutputConsoleHeight);

    // Terminal-style panel: darker background, soft corners, monospaced text.
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    ImGui::BeginChild("OutputText", ImVec2(0, output_height), ImGuiChildFlags_Borders,
                      output_flags);
    ImGui::PushFont(fonts.GetFont(FontType::kCode), 0.0f);

    if (!error_message.empty())
    {
        ImVec4 err = ImGui::ColorConvertU32ToFloat4(settings.GetColor(Colors::kTextError));
        ImGui::TextColored(err, "%s", error_message.c_str());
        ImGui::Separator();
    }

    ImGui::TextUnformatted(output_text.c_str());

    if (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    return clear_requested;
}

std::string
RenderSavedProfileBar(LaunchPresetManager& preset_mgr, std::string const& profiler_id,
                      std::string& current_preset_name, LaunchConfig& config,
                      IProfilerBackend const* backend, AppWindow* /*app_window*/,
                      bool& config_reset)
{
    std::string load_name;
    config_reset = false;

    std::vector<PresetInfo> presets = preset_mgr.ListPresets(profiler_id);

    PanelFieldLabel("Saved config");
    ImGui::SameLine();

    // Combo for selecting a saved configuration.
    ImGui::PushItemWidth(170);
    if (ImGui::BeginCombo("##PresetCombo",
                          current_preset_name.empty() ? "Unsaved" : current_preset_name.c_str()))
    {
        if (ImGui::Selectable("Unsaved", current_preset_name.empty()))
        {
            current_preset_name.clear();
        }
        for (auto const& p : presets)
        {
            bool selected = (p.name == current_preset_name);
            if (ImGui::Selectable(p.name.c_str(), selected))
            {
                load_name           = p.name;
                current_preset_name = p.name;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Save##Preset"))
    {
        if (!current_preset_name.empty())
        {
            preset_mgr.SavePreset(current_preset_name, config, backend);
        }
        else
        {
            ImGui::OpenPopup("SavePresetPopup");
        }
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(current_preset_name.empty()
                              ? "Save these settings as a named config"
                              : "Update the selected config");
    }

    // Overflow menu keeps the less-common actions out of the toolbar.
    ImGui::SameLine();
    if (ImGui::Button("More...##Preset"))
    {
        ImGui::OpenPopup("ProfileMenu");
    }

    bool open_save_as = false;
    bool do_reset     = false;
    if (ImGui::BeginPopup("ProfileMenu"))
    {
        if (ImGui::MenuItem("Save As New Config..."))
        {
            open_save_as = true;
        }
        if (ImGui::MenuItem("Delete Config", nullptr, false,
                            !current_preset_name.empty()))
        {
            preset_mgr.DeletePreset(current_preset_name, profiler_id);
            current_preset_name.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Options to Defaults"))
        {
            do_reset = true;
        }
        ImGui::EndPopup();
    }

    if (open_save_as)
    {
        ImGui::OpenPopup("SavePresetPopup");
    }
    if (do_reset && backend)
    {
        // Reset every backend option to its default, the backend's target and
        // extra arguments included. Only the selected profile name is kept.
        const_cast<IProfilerBackend*>(backend)->LoadSettings(jt::Json());
        config.backend_payload = backend->SaveSettings();
        config_reset = true;
    }

    // Save-As popup
    SettingsManager& settings = SettingsManager::Get();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        settings.GetDefaultStyle().ItemSpacing);
    if (ImGui::BeginPopup("SavePresetPopup"))
    {
        ImGui::TextUnformatted("Save configuration");
        ImGui::TextDisabled("Give these settings a name.");
        ImGui::SetNextItemWidth(280.0f);
        bool commit = ImGui::InputTextWithHint(
            "##SaveName", "Configuration name", s_save_preset_name,
            sizeof(s_save_preset_name), ImGuiInputTextFlags_EnterReturnsTrue);

        constexpr float BUTTON_WIDTH = 90.0f;
        const float     actions_w    = BUTTON_WIDTH * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             std::max(0.0f, ImGui::GetContentRegionAvail().x - actions_w));
        if (ImGui::Button("Cancel##SavePreset", ImVec2(BUTTON_WIDTH, 0.0f)))
        {
            s_save_preset_name[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if ((AccentButton("Save##SavePreset", ImVec2(BUTTON_WIDTH, 0.0f), &settings) ||
             commit) &&
            std::strlen(s_save_preset_name) > 0)
        {
            current_preset_name = s_save_preset_name;
            preset_mgr.SavePreset(current_preset_name, config, backend);
            s_save_preset_name[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    return load_name;
}

} // namespace View
} // namespace RocProfVis
