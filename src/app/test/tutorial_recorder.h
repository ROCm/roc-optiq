// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"
#include <functional>
#include <string>
#include <vector>

struct ImGuiTestEngine;

namespace RocProfVis
{
namespace Tutorial
{

// Settings of a --record-tutorials run, filled from the command line.
struct RecorderConfig
{
    std::string output_dir;     // Receives a lossless .mkv and timing files per chapter.
    std::string samples_dir;    // Holds the traces the chapters open.
    std::string encoder_path;   // ffmpeg executable the capture tool pipes frames into.
    std::string filter;         // Test-engine filter choosing which chapters to record.
    std::string narration_dir;  // Synthesized narration (lines.tsv) that paces the chapters.
    float       ui_scale = 1.25f;
};

inline constexpr int   VIDEO_WIDTH  = 1920;
inline constexpr int   VIDEO_HEIGHT = 1080;
inline constexpr int   VIDEO_FPS    = 60;
// Every frame advances the UI clock by this much regardless of how long it took
// to render, so pointer motion and pauses play back at the intended pace.
inline constexpr float SIMULATION_STEP_S = 1.0f / 60.0f;

void                  SetConfig(const RecorderConfig& config);
const RecorderConfig& GetConfig();

// Returns the ffmpeg executable to use: `hint` when it names an existing file,
// otherwise the first ffmpeg found on PATH. Empty when neither exists.
std::string ResolveEncoderPath(const std::string& hint);

// Points the capture tool at the OpenGL back buffer and at the encoder.
void InstallCapture(ImGuiTestEngine* engine);

// Draws click ripples, key captions and the spotlight over the finished frame.
// Call once per frame between the view render and ImGui::Render().
void RenderOverlay();

// Records one chapter and paces it to its narration: each line starts once the
// previous one has finished, and the actions after it play while it is spoken.
// Every action is logged with its video time to timing/<chapter>.txt, and every
// line of narration to timing/<chapter>.narration.tsv for the final mix.
class Director
{
public:
    Director(ImGuiTestContext* ctx, const std::string& file_stem);
    ~Director();

    bool Start();
    void Finish();

    ImGuiTestContext* Context() const { return m_ctx; }

    // Holds for `seconds`, but no longer than a moment past the end of the
    // current line of narration.
    void Hold(float seconds);
    // Short gap between related actions.
    void Beat();
    // Ends a topic: waits for the narration to finish, then takes a breath.
    void Pause();

    // Starts a line of narration once the previous line has finished. Its length
    // comes from the synthesized narration, or is estimated until it exists.
    void Say(const char* text);
    // Holds until the current line of narration has finished.
    void WaitForVoice();
    // Holds until the pointer should set off to reach what the current line
    // names as it says `word` (its next occurrence in the line).
    void WaitForWord(const char* word);
    // Holds until just before the current line says `word` (its next
    // occurrence), for an action that takes no travel, such as a key press.
    void WaitUntilSaid(const char* word);
    // Glides the pointer to `pos` so it settles just before the current line
    // says `word` (its next occurrence): it sets off in time for the glide,
    // and glides faster, within reason, when running late. Without a word it
    // glides there at once, without the engine's pause.
    void PointAt(const ImVec2& pos, const char* word = nullptr);
    // PointAt() the centre of the item `ref`, which must be on screen.
    void PointAtItem(ImGuiTestRef ref, const char* word = nullptr);
    // Clicks where the pointer is.
    void ClickHere(ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Runs `work` with its frames cut from the video, for setting a scene up
    // off camera.
    void Cut(const std::function<void()>& work);

    void MoveTo(const ImVec2& pos);
    void Hover(const ImVec2& pos, float seconds);
    void HoverItem(ImGuiTestRef ref, float seconds);
    void Click(const ImVec2& pos, ImGuiMouseButton button = ImGuiMouseButton_Left);
    void DoubleClick(const ImVec2& pos);
    void ClickItem(ImGuiTestRef ref, ImGuiMouseButton button = ImGuiMouseButton_Left);
    // Presses at `from`, glides to `to` over `seconds`, then releases.
    void Drag(const ImVec2& from, const ImVec2& to, float seconds);
    // Scrolls the wheel over `pos`, one notch per step, `step_gap` apart.
    void Wheel(const ImVec2& pos, float notches_per_step, int steps, float step_gap);

    // Presses `chord` `repeats` times while its caption is shown on screen.
    void Key(ImGuiKeyChord chord, const char* caption, int repeats = 1,
             float gap = 0.45f);
    // Holds `chord` for `seconds`, so shortcuts that repeat (pan, zoom, scroll)
    // keep firing, with its caption on screen.
    void KeyHold(ImGuiKeyChord chord, const char* caption, float seconds);
    // Holds modifier keys down (with a caption) until ReleaseKeys().
    void HoldKeys(ImGuiKeyChord chord, const char* caption);
    void ReleaseKeys();
    void Type(const char* text);

    void Spotlight(const ImRect& rect);
    void ClearSpotlight();
    void Caption(const char* text, float seconds);

    // Yields until `ready` holds or `timeout_s` of wall time passes. Only the
    // first `visible_s` of the wait is recorded, so long loads become a cut.
    bool WaitFor(const std::function<bool()>& ready, float visible_s, float timeout_s);
    // Clicks at `pos` and cuts from the button's release until `ready` holds or
    // `timeout_s` of wall time passes, for a click whose result draws half
    // built at first.
    bool ClickAndCutUntil(const ImVec2& pos, const std::function<bool()>& ready,
                          float timeout_s);

private:
    struct NarrationCue
    {
        double      start_s;
        std::string text;
    };

    void HoldExactly(float seconds);
    // Video time at which the current line says `word` (its next occurrence),
    // or a negative value when it doesn't.
    double WordTime(const char* word);
    // Glides the pointer to `to` in `seconds`.
    void Glide(const ImVec2& to, float seconds);
    void SetFramesSaved(bool saved);
    // Seconds into the video: UI time since Start() minus the cut frames.
    double VideoTime() const;
    void   Log(double video_s, const std::string& text);
    void   WriteTimeline() const;
    // Label of `ref`, or of what is under the pointer. The lookup takes a frame
    // or two, which are cut from the video.
    std::string LabelOf(ImGuiTestRef ref);
    std::string LabelAtPointer();
    void        ShowCaption(const char* text, float seconds);

    ImGuiTestContext*         m_ctx;
    std::string               m_file_stem;
    bool                      m_recording;
    bool                      m_frames_saved;
    ImGuiKeyChord             m_held_keys;
    double                    m_origin_s;
    double                    m_cut_s;
    double                    m_cut_start_s;
    double                    m_voice_end_s;
    std::string               m_line_text;
    double                    m_line_start_s  = 0.0;
    float                     m_line_length_s = 0.0f;
    size_t                    m_word_cursor   = 0;
    size_t                    m_text_cursor   = 0;
    std::vector<std::string>  m_timeline;
    std::vector<NarrationCue> m_narration;
};

}  // namespace Tutorial
}  // namespace RocProfVis
