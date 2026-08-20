// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_launch_shared_tabs.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_font_manager.h"
#include "imgui.h"
#include <algorithm>
#include <cfloat>
#include <sstream>
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
constexpr float kAccentBarGap       = 8.0f;
constexpr float kChipBgAlpha        = 0.16f;
constexpr float kChipEdgeAlpha      = 0.55f;
constexpr float kPillGap            = 7.0f;   // gap between pill label and close "x"
constexpr float kPillCloseScale     = 0.75f;  // close glyph size, fraction of font
constexpr float kPillBgAlpha        = 0.16f;
constexpr float kPillBgHoverAlpha   = 0.30f;
constexpr float kPillCloseIdleAlpha = 0.6f;
constexpr float kToggleHeightScale  = 0.82f;  // of frame height
constexpr float kToggleWidthScale   = 1.75f;  // of toggle height
constexpr float kToggleAnimSpeed    = 10.0f;
constexpr float kToggleKnobInset    = 2.0f;   // knob padding inside the track

// Card interior vertical padding (horizontal padding still follows the app
// style). Tighter than the app default so the stacked cards read as one compact
// form instead of a column of tall panels.
constexpr float kCardPadY = 5.0f;

// A dimmed "(?)" that shows a wrapped tooltip on hover.
void HelpTip(const char* tooltip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip())
    {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(tooltip);
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
    // the same rounded/bordered/tiered look as the remote dialogs. The tighter
    // kCardPadY keeps the stacked launcher cards reading as one compact form.
    SettingsManager&  settings = SettingsManager::Get();
    const ImGuiStyle& def      = settings.GetDefaultStyle();
    BeginPanelCard(id, PanelCardTone::kPanel, ImVec2(def.WindowPadding.x, kCardPadY), true,
                   &settings);
}

void EndLaunchCard()
{
    EndPanelCard();
    // Cards are separated by the surrounding item spacing only.
}

void LaunchCardHeader(const char* icon, const char* title, const char* help)
{
    SettingsManager& settings = SettingsManager::Get();
    FontManager&     fonts    = settings.GetFontManager();

    ImGui::PushFont(fonts.GetFont(FontType::kMainText),
                    fonts.GetFontSize(FontSize::kMedLarge));

    // Leading accent bar sized to the (larger) header text.
    const float  bar_h = ImGui::GetFontSize();
    ImVec2       p     = ImGui::GetCursorScreenPos();
    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y), ImVec2(p.x + kAccentBarWidth, p.y + bar_h),
                      settings.GetColor(Colors::kAccent), 2.0f);

    ImGui::Indent(kAccentBarWidth + kAccentBarGap);

    // Optional leading icon (drawn from the icon font, in the accent color).
    if (icon && icon[0])
    {
        ImGui::PushFont(fonts.GetFont(FontType::kIcon),
                        fonts.GetFontSize(FontSize::kMedLarge));
        ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kAccent));
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SameLine(0.0f, kAccentBarGap);
    }

    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
    ImGui::TextUnformatted(title);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (help && help[0])
    {
        HelpTip(help);
    }
    ImGui::Unindent(kAccentBarWidth + kAccentBarGap);
}

void Chip(const char* label, ImU32 accent_color)
{
    const ImVec2 pad       = ImGui::GetStyle().FramePadding;
    ImVec2       text_size = ImGui::CalcTextSize(label);
    ImVec2       p         = ImGui::GetCursorScreenPos();
    ImVec2       size(text_size.x + pad.x * 2.0f, text_size.y + pad.y * 2.0f);
    ImDrawList*  dl  = ImGui::GetWindowDrawList();
    float        rnd = size.y * 0.5f;

    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                      ApplyAlpha(accent_color, kChipBgAlpha), rnd);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y),
                ApplyAlpha(accent_color, kChipEdgeAlpha), rnd);
    dl->AddText(ImVec2(p.x + pad.x, p.y + pad.y), accent_color, label);

    ImGui::Dummy(size);
}

PillAction EditablePill(const char* label, ImU32 accent_color)
{
    const ImVec2 pad       = ImGui::GetStyle().FramePadding;
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const float  x_size    = ImGui::GetFontSize() * kPillCloseScale;

    ImVec2 size(pad.x * 2.0f + text_size.x + kPillGap + x_size,
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

void RenderConfigChips(const char* lead_label, std::vector<std::string> const& tags)
{
    SettingsManager& settings = SettingsManager::Get();

    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", lead_label);

    if (tags.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("nothing selected yet");
        return;
    }

    const ImU32  accent    = settings.GetColor(Colors::kAccent);
    ImGuiStyle&  style     = ImGui::GetStyle();
    const float  window_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

    // Chip width matches Chip(): text width plus its horizontal frame padding.
    const float chip_pad_x = ImGui::GetStyle().FramePadding.x;
    auto chip_width = [chip_pad_x](std::string const& s)
    { return ImGui::CalcTextSize(s.c_str()).x + chip_pad_x * 2.0f; };

    ImGui::SameLine();
    for (size_t i = 0; i < tags.size(); i++)
    {
        Chip(tags[i].c_str(), accent);
        if (i + 1 < tags.size())
        {
            float last_x2 = ImGui::GetItemRectMax().x;
            float next_x2 = last_x2 + style.ItemSpacing.x + chip_width(tags[i + 1]);
            if (next_x2 < window_x2)
            {
                ImGui::SameLine();
            }
        }
    }
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
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kAccent));
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

bool RenderTargetSection(TargetSpec& target, ConnectionType connection, AppWindow* app_window,
                         const std::function<void()>& on_remote_browse_program,
                         const std::function<void()>& on_remote_browse_output)
{
    bool modified = false;

    // In remote (SSH) mode the executable / output refer to paths on the remote
    // host. When the caller supplies a remote-browse callback we drive the shared
    // remote file browser; otherwise the local pickers don't apply and the Browse
    // button stays disabled.
    const bool is_remote        = (connection == ConnectionType::kSsh);
    const bool remote_browse_exe = is_remote && static_cast<bool>(on_remote_browse_program);
    const bool remote_browse_out = is_remote && static_cast<bool>(on_remote_browse_output);

    SettingsManager&  settings      = SettingsManager::Get();
    ProfilerSettings& prof_settings = settings.GetProfilerSettings();

    const float label_w  = 105.0f;
    const float browse_w = 84.0f;
    const float spacing  = ImGui::GetStyle().ItemSpacing.x;
    const float arrow_w  = ImGui::GetFrameHeight();

    // --- Executable (the headline field) --------------------------------------
    const bool has_recent = (!is_remote && !prof_settings.recent_targets.empty());

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Program");
    ImGui::SameLine(label_w);

    float exe_trailing = browse_w + spacing + (has_recent ? (arrow_w + spacing) : 0.0f);
    ImGui::SetNextItemWidth(-exe_trailing);
    if (InputTextStringWithHint(
            "##TargetExe",
            is_remote ? "/remote/path/to/app" : "/path/to/app  (or a name on $PATH)",
            target.executable))
    {
        modified = true;
    }

    ImGui::SameLine();
    // Disabled only in remote mode without a remote-browse handler; local mode
    // and remote-with-handler both keep the button live.
    const bool exe_disabled = is_remote && !remote_browse_exe;
    if (exe_disabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Browse##TargetExe", ImVec2(browse_w, 0)))
    {
        if (remote_browse_exe)
        {
            on_remote_browse_program();
        }
        else if (!is_remote && app_window)
        {
            app_window->ShowOpenFileDialog(
                "Choose Program", {}, "",
                [&target](std::string const& path) { target.executable = path; });
        }
    }
    if (exe_disabled)
    {
        ImGui::EndDisabled();
    }

    if (has_recent)
    {
        ImGui::SameLine();
        if (ImGui::ArrowButton("##RecentTargetExe", ImGuiDir_Down))
        {
            ImGui::OpenPopup("RecentTargetsPopup");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Recent programs");
        }
        if (ImGui::BeginPopup("RecentTargetsPopup"))
        {
            for (auto const& t : prof_settings.recent_targets)
            {
                std::string short_name = t;
                if (short_name.size() > 48)
                {
                    short_name = "..." + short_name.substr(short_name.size() - 45);
                }
                if (ImGui::Selectable(short_name.c_str(), false))
                {
                    target.executable = t;
                    modified          = true;
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", t.c_str());
                }
            }
            ImGui::EndPopup();
        }
    }

    // --- Arguments (free-form, passed verbatim to the program) ----------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Arguments");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (InputTextStringWithHint(
            "##TargetArgs", "e.g.  --iterations 100 --input data.bin", target.arguments))
    {
        modified = true;
    }

    // --- Output folder (where the profiler writes the trace) ------------------
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Output folder");
    ImGui::SameLine(label_w);
    ImGui::SetNextItemWidth(-(browse_w + spacing));
    if (InputTextStringWithHint(
            "##OutputDir",
            is_remote ? "remote folder for results" : "folder for results",
            target.output_directory))
    {
        modified = true;
    }
    ImGui::SameLine();
    const bool out_disabled = is_remote && !remote_browse_out;
    if (out_disabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Browse##OutputDir", ImVec2(browse_w, 0)))
    {
        if (remote_browse_out)
        {
            on_remote_browse_output();
        }
        else if (!is_remote && app_window)
        {
            app_window->ShowPathPickerDialog(
                "Choose Output Directory", "",
                [&target](std::string const& path) { target.output_directory = path; });
        }
    }
    if (out_disabled)
    {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    if (ImGui::Checkbox("Open the trace automatically when profiling finishes",
                        &target.auto_load_trace))
    {
        modified = true;
    }

    return modified;
}

bool RenderToolLocationSection(std::string& tool_directory, ConnectionType connection,
                               AppWindow* app_window, std::string const& resolved_hint,
                               const std::function<void()>& on_remote_browse_directory)
{
    bool modified = false;

    const bool is_remote     = (connection == ConnectionType::kSsh);
    const bool remote_browse = is_remote && static_cast<bool>(on_remote_browse_directory);

    const float label_w  = 105.0f;
    const float browse_w = 84.0f;
    const float spacing  = ImGui::GetStyle().ItemSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Tools folder");
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
    const bool browse_disabled = is_remote && !remote_browse;
    if (browse_disabled)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Browse##ToolDir", ImVec2(browse_w, 0)))
    {
        if (remote_browse)
        {
            on_remote_browse_directory();
        }
        else if (!is_remote && app_window)
        {
            app_window->ShowPathPickerDialog(
                "Choose Profiler Tools Directory", "",
                [&tool_directory](std::string const& path) { tool_directory = path; });
        }
    }
    if (browse_disabled)
    {
        ImGui::EndDisabled();
    }

    if (!resolved_hint.empty())
    {
        ImGui::Dummy(ImVec2(label_w - ImGui::GetStyle().ItemSpacing.x, 0.0f));
        ImGui::SameLine();
        ImGui::TextDisabled("%s", resolved_hint.c_str());
    }

    return modified;
}

std::string BuildCommandPreviewString(
    std::string const& tool_path,
    std::vector<std::pair<std::string, std::string>> const& env_vars,
    std::vector<std::string> const& argv)
{
    std::ostringstream preview;

    for (auto const& kv : env_vars)
    {
        if (!kv.second.empty())
        {
            preview << kv.first << "=" << kv.second << " \\\n";
        }
    }

    // argv is already the complete argument list (see
    // IProfilerBackend::FlattenToExecution), so the preview renders it as-is
    // rather than re-deriving any part of the command. Anything appended here
    // would be shown but not run.
    preview << tool_path;
    for (auto const& arg : argv)
    {
        preview << " " << arg;
    }

    return preview.str();
}

void RenderCommandPreview(std::string const& preview_text)
{
    SettingsManager& settings = SettingsManager::Get();
    FontManager&     fonts    = settings.GetFontManager();

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Command Preview");
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy##CmdPreview"))
    {
        ImGui::SetClipboardText(preview_text.c_str());
    }

    // Monospaced, softly-rounded panel so the command reads like a terminal.
    // Fills the remaining height of its container (the launcher's preview panel).
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kBgMain));
    ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar;
    ImGui::BeginChild("CmdPreview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, flags);
    ImGui::PushFont(fonts.GetFont(FontType::kCode), 0.0f);
    ImGui::TextUnformatted(preview_text.c_str());
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
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
    float output_height = std::max(ImGui::GetContentRegionAvail().y - 30.0f, 60.0f);

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

std::string RenderSavedProfileBar(
    LaunchPresetManager& preset_mgr,
    std::string const& profiler_id,
    std::string& current_preset_name,
    LaunchConfig& config,
    IProfilerBackend const* backend,
    AppWindow* /*app_window*/)
{
    std::string load_name;

    std::vector<PresetInfo> presets = preset_mgr.ListPresets(profiler_id);

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Saved Config");
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
        // Reset backend settings to defaults while keeping the selected profile
        // name and target intact.
        const_cast<IProfilerBackend*>(backend)->LoadSettings(jt::Json());
        config.backend_payload = backend->SaveSettings();
        config.extra_env.clear();
        config.extra_argv.clear();
    }

    // Save-As popup
    if (ImGui::BeginPopup("SavePresetPopup"))
    {
        ImGui::TextUnformatted("Config name:");
        ImGui::SetNextItemWidth(240.0f);
        bool commit = ImGui::InputText("##SaveName", s_save_preset_name,
                                       sizeof(s_save_preset_name),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        if ((ImGui::Button("Save##SavePreset") || commit) &&
            std::strlen(s_save_preset_name) > 0)
        {
            current_preset_name = s_save_preset_name;
            preset_mgr.SavePreset(current_preset_name, config, backend);
            s_save_preset_name[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##SavePreset"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    return load_name;
}

} // namespace View
} // namespace RocProfVis
