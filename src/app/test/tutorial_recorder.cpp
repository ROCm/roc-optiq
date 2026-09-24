// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#include "tutorial_recorder.h"

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
// winbase.h defines Yield() as an empty macro, which would erase every
// ImGuiTestContext::Yield() call below.
#    undef Yield
#endif
#ifdef __APPLE__
#    include <OpenGL/gl.h>
#else
#    include <GL/gl.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "imgui_capture_tool.h"
#include "imgui_te_engine.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace Tutorial
{

// Template for the capture tool; it substitutes $FPS/$WIDTH/$HEIGHT/$OUTPUT and
// must stay under ImGuiTestEngineIO::VideoCaptureEncoderParams' 256 chars.
// Frames are kept losslessly as RGB, so the one lossy encode is the delivery
// master, which also does the conversion to BT.709 YUV.
constexpr const char* ENCODER_PARAMS =
    "-v error -r $FPS -f rawvideo -pix_fmt rgba -s $WIDTHx$HEIGHT -i - "
    "-c:v libx264rgb -preset ultrafast -qp 0 -y \"$OUTPUT\"";
constexpr const char* VIDEO_EXTENSION = ".mkv";

constexpr float MOUSE_SPEED_PX_PER_S     = 1150.0f;
constexpr float MOUSE_WOBBLE             = 0.2f;
constexpr float TYPING_SPEED_CHARS_PER_S = 16.0f;
constexpr float ACTION_DELAY_SHORT_S     = 0.2f;
constexpr float ACTION_DELAY_STANDARD_S  = 0.3f;
constexpr float CURSOR_SCALE             = 1.5f;
constexpr float BEAT_S                   = 1.0f;
constexpr float PAUSE_S                  = 0.5f;
constexpr float OPENING_HOLD_S           = 0.5f;
constexpr float CLOSING_HOLD_S           = 1.0f;
constexpr float CAPTION_LINGER_S         = 0.9f;
constexpr float PERSISTENT_S             = 3600.0f;

// How long a chapter waits for the window to return to the video size.
constexpr int WINDOW_SIZE_WAIT_FRAMES = 120;

// Silence between two lines of narration.
constexpr double LINE_GAP_S = 0.35;
// A chapter's holds end at most this long after the narration falls silent,
// so the screen never sits still with nothing being said.
constexpr double POST_VOICE_HOLD_S = 0.5;
// How far ahead of a word the pointer sets off for what the word names: the
// engine's pause before a move plus a typical glide.
constexpr double WORD_LEAD_S = 0.55;
// A pointer sent to what a word names settles this long before the word.
constexpr double ARRIVE_EARLY_S = 0.1;
// Such a glide takes GLIDE_BASE_S plus a second per GLIDE_PX_PER_S of distance,
// at most GLIDE_MAX_S. Running behind the narration, it may shrink to
// GLIDE_MIN_S: quick, but still easy to follow.
constexpr float GLIDE_BASE_S   = 0.3f;
constexpr float GLIDE_PX_PER_S = 1800.0f;
constexpr float GLIDE_MAX_S    = 0.8f;
constexpr float GLIDE_MIN_S    = 0.2f;
// Spoken forms differ from the text ("GPU's" for "GPU"), so a heard word that
// starts like the wanted one, over at least this many letters, also counts.
constexpr size_t WORD_PREFIX_MIN = 3;
// Pace used for lines that have not been synthesized yet.
constexpr float ESTIMATED_WORD_S  = 0.37f;
constexpr float ESTIMATED_EXTRA_S = 0.4f;

constexpr const char* NARRATION_INDEX    = "lines.tsv";
// "<text>\t<word>@<seconds> <word>@<seconds> ...": when each word of a
// synthesized line starts, from speech recognition of its voice.
constexpr const char* WORD_INDEX         = "words.tsv";
constexpr const char* NARRATION_SUFFIX   = ".narration.tsv";
constexpr const char* TIMING_DIR         = "timing";
constexpr float       MIN_LOGGED_WAIT_S  = 0.25f;
constexpr int32_t     TENTHS_PER_SECOND  = 10;
constexpr int32_t     SECONDS_PER_MINUTE = 60;
constexpr size_t      TEXT_BUFFER_SIZE   = 64;
// Child windows are named "<parent>/<name>_%08X" after their id.
constexpr size_t      WINDOW_ID_DIGITS   = 8;
constexpr const char* HEX_DIGITS         = "0123456789ABCDEF";
constexpr char        FIRST_PRINTABLE    = ' ';
constexpr char        LAST_PRINTABLE     = '~';

constexpr double RIPPLE_DURATION_S   = 0.55;
constexpr float  RIPPLE_START_RADIUS = 8.0f;
constexpr float  RIPPLE_END_RADIUS   = 34.0f;
constexpr float  RIPPLE_THICKNESS    = 3.5f;
constexpr float  RIPPLE_FILL_ALPHA   = 0.18f;
constexpr float  PRESSED_DOT_RADIUS  = 11.0f;
constexpr float  PRESSED_DOT_ALPHA   = 0.35f;
constexpr float  CAPTION_FONT_SIZE   = 26.0f;
constexpr float  CAPTION_PADDING_X   = 20.0f;
constexpr float  CAPTION_PADDING_Y   = 10.0f;
constexpr float  CAPTION_ROUNDING    = 10.0f;
constexpr float  CAPTION_BOTTOM_GAP  = 96.0f;
constexpr double CAPTION_FADE_IN_S   = 0.12;
constexpr double CAPTION_FADE_OUT_S  = 0.35;
constexpr const char* SPOTLIGHT_WINDOW = "##tutorial_spotlight";
constexpr double SPOTLIGHT_FADE_S    = 0.3;
constexpr float  SPOTLIGHT_DIM_ALPHA = 0.38f;
constexpr float  SPOTLIGHT_THICKNESS = 3.0f;
constexpr float  SPOTLIGHT_ROUNDING  = 6.0f;
constexpr float  SPOTLIGHT_MARGIN    = 3.0f;

static const ImVec4 LEFT_CLICK_COLOR  = ImVec4(1.0f, 0.38f, 0.13f, 1.0f);
static const ImVec4 RIGHT_CLICK_COLOR = ImVec4(0.16f, 0.55f, 1.0f, 1.0f);
static const ImVec4 CAPTION_BG_COLOR  = ImVec4(0.08f, 0.08f, 0.10f, 0.86f);
static const ImVec4 CAPTION_TEXT      = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
static const ImVec4 SPOTLIGHT_COLOR   = ImVec4(1.0f, 0.55f, 0.0f, 1.0f);
static const ImVec4 DIM_COLOR         = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

struct Ripple
{
    ImVec2 pos;
    double start;
    bool   right_button;
};

// Overlay state. Written from the test coroutine and read by the main loop;
// the engine never runs the two at the same time.
struct OverlayState
{
    std::vector<Ripple> ripples;
    std::string         caption;
    double              caption_start   = -1.0;
    double              caption_end     = -1.0;
    ImRect              spotlight;
    double              spotlight_start = -1.0;
    double              spotlight_end   = -1.0;
};

static RecorderConfig g_config;
static OverlayState   g_overlay;

static std::string
read_environment(const char* name)
{
#ifdef _WIN32
    char*  buffer = nullptr;
    size_t length = 0;
    if(_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr)
    {
        return std::string();
    }
    std::unique_ptr<char, decltype(&std::free)> owner(buffer, &std::free);
    return std::string(owner.get());
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

static ImU32
color_with_alpha(const ImVec4& color, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(color.x, color.y, color.z, color.w * ImClamp(alpha, 0.0f, 1.0f)));
}

// Opacity of an element shown from `start` to `end`; zero outside that span.
static float
fade(double now, double start, double end, double fade_in, double fade_out)
{
    if(start < 0.0 || now < start || now > end + fade_out)
    {
        return 0.0f;
    }
    float alpha = 1.0f;
    if(fade_in > 0.0 && now - start < fade_in)
    {
        alpha = static_cast<float>((now - start) / fade_in);
    }
    if(now > end)
    {
        alpha = ImMin(alpha, 1.0f - static_cast<float>((now - end) / fade_out));
    }
    return ImClamp(alpha, 0.0f, 1.0f);
}

// Keeps printable ASCII, so icon glyphs drop out of the plain-text timeline,
// and swaps double quotes for single ones because labels are quoted there.
static std::string
printable(const std::string& text)
{
    std::string out;
    for(char c : text)
    {
        const bool visible = c >= FIRST_PRINTABLE && c <= LAST_PRINTABLE;
        const bool repeated_space = c == ' ' && (out.empty() || out.back() == ' ');
        if(visible && !repeated_space)
        {
            out.push_back(c == '"' ? '\'' : c);
        }
    }
    while(!out.empty() && out.back() == ' ')
    {
        out.pop_back();
    }
    return out;
}

// "Open##open_trace" -> "Open", and "##contract" -> "contract".
static std::string
readable_label(const char* label)
{
    const std::string text   = label != nullptr ? label : "";
    const size_t      hidden = text.find("##");
    std::string       shown  = printable(text.substr(0, hidden));
    if(shown.empty() && hidden != std::string::npos)
    {
        const size_t id = text.find_first_not_of('#', hidden);
        if(id != std::string::npos)
        {
            shown = printable(text.substr(id));
        }
    }
    return shown;
}

static bool
is_window_id(const std::string& text)
{
    return text.size() == WINDOW_ID_DIGITS &&
           text.find_first_not_of(HEX_DIGITS) == std::string::npos;
}

// "Main Window/welcome_card_left_1A2B3C4D" -> "welcome_card_left". Anonymous
// children are skipped in favour of the nearest named ancestor.
static std::string
window_label(const ImGuiWindow* window)
{
    std::string rest = window->Name;
    std::string label;
    while(label.empty() && !rest.empty())
    {
        const size_t slash   = rest.rfind('/');
        std::string  segment = slash == std::string::npos ? rest : rest.substr(slash + 1);
        rest = slash == std::string::npos ? std::string() : rest.substr(0, slash);
        if(segment.size() > WINDOW_ID_DIGITS &&
           segment[segment.size() - WINDOW_ID_DIGITS - 1] == '_' &&
           is_window_id(segment.substr(segment.size() - WINDOW_ID_DIGITS)))
        {
            segment.resize(segment.size() - WINDOW_ID_DIGITS - 1);
        }
        if(!is_window_id(segment))
        {
            label = readable_label(segment.c_str());
        }
    }
    return label;
}

static std::string
quoted(const std::string& label)
{
    return label.empty() ? std::string("(unlabeled)") : "\"" + label + "\"";
}

// "m:ss.s", the form narration scripts use to place their lines.
static std::string
format_time(double seconds)
{
    const int32_t tenths =
        static_cast<int32_t>(std::lround(ImMax(seconds, 0.0) * TENTHS_PER_SECOND));
    const int32_t tenths_per_minute = TENTHS_PER_SECOND * SECONDS_PER_MINUTE;
    char          buffer[TEXT_BUFFER_SIZE];
    ImFormatString(buffer, IM_COUNTOF(buffer), "%d:%02d.%d", tenths / tenths_per_minute,
                   (tenths % tenths_per_minute) / TENTHS_PER_SECOND, tenths % TENTHS_PER_SECOND);
    return buffer;
}

static std::string
format_seconds(float seconds)
{
    char buffer[TEXT_BUFFER_SIZE];
    ImFormatString(buffer, IM_COUNTOF(buffer), "%.1f s", seconds);
    return buffer;
}

static std::string
click_verb(ImGuiMouseButton button)
{
    std::string verb = "click";
    if(button == ImGuiMouseButton_Right)
    {
        verb = "right-click";
    }
    else if(button == ImGuiMouseButton_Middle)
    {
        verb = "middle-click";
    }
    return verb;
}

// The main direction and length of a drag, like "right 160 px".
static std::string
drag_direction(const ImVec2& from, const ImVec2& to)
{
    const float dx         = to.x - from.x;
    const float dy         = to.y - from.y;
    const bool  horizontal = ImFabs(dx) >= ImFabs(dy);
    const char* way = horizontal ? (dx >= 0.0f ? "right" : "left") : (dy >= 0.0f ? "down" : "up");
    char        buffer[TEXT_BUFFER_SIZE];
    ImFormatString(buffer, IM_COUNTOF(buffer), "%s %.0f px", way,
                   horizontal ? ImFabs(dx) : ImFabs(dy));
    return buffer;
}

static std::string
wheel_amount(float notches)
{
    char buffer[TEXT_BUFFER_SIZE];
    ImFormatString(buffer, IM_COUNTOF(buffer), "%s %g notches", notches >= 0.0f ? "up" : "down",
                   ImFabs(notches));
    return buffer;
}

// Seconds each synthesized line of narration lasts, read once from the index
// the voice tool writes: one "<seconds>\t<text>" row per line.
static const std::unordered_map<std::string, float>&
narration_lengths()
{
    static std::unordered_map<std::string, float> lengths;
    static bool                                   loaded = false;
    if(!loaded && !g_config.narration_dir.empty())
    {
        loaded = true;
        const std::filesystem::path index =
            std::filesystem::path(g_config.narration_dir) / NARRATION_INDEX;
        std::ifstream file(index);
        std::string   row;
        while(std::getline(file, row))
        {
            if(!row.empty() && row.back() == '\r')
            {
                row.pop_back();
            }
            const size_t tab = row.find('\t');
            if(tab != std::string::npos)
            {
                lengths[row.substr(tab + 1)] = std::strtof(row.c_str(), nullptr);
            }
        }
        spdlog::info("[tutorial] {} narration lines read from {}", lengths.size(),
                     index.string());
    }
    return lengths;
}

// Lowercase letters and digits of `text`, how spoken words are matched.
static std::string
word_key(const std::string& text)
{
    std::string key;
    for(const char c : text)
    {
        if(std::isalnum(static_cast<unsigned char>(c)) != 0)
        {
            key += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return key;
}

struct TimedWord
{
    std::string key;
    float       start_s = 0.0f;
};

// When each word of each synthesized line starts, read once from the index
// the voice tool writes beside the line lengths.
static const std::unordered_map<std::string, std::vector<TimedWord>>&
narration_words()
{
    static std::unordered_map<std::string, std::vector<TimedWord>> words;
    static bool                                                    loaded = false;
    if(!loaded && !g_config.narration_dir.empty())
    {
        loaded = true;
        std::ifstream file(std::filesystem::path(g_config.narration_dir) / WORD_INDEX);
        std::string   row;
        while(std::getline(file, row))
        {
            if(!row.empty() && row.back() == '\r')
            {
                row.pop_back();
            }
            const size_t tab = row.find('\t');
            if(tab == std::string::npos)
            {
                continue;
            }
            std::vector<TimedWord>& line = words[row.substr(0, tab)];
            std::istringstream      fields(row.substr(tab + 1));
            std::string             field;
            while(fields >> field)
            {
                const size_t at = field.rfind('@');
                if(at != std::string::npos)
                {
                    line.push_back(
                        { word_key(field.substr(0, at)), std::strtof(field.c_str() + at + 1, nullptr) });
                }
            }
        }
        spdlog::info("[tutorial] word timings for {} narration lines", words.size());
    }
    return words;
}

static float
estimated_length(const std::string& text)
{
    const size_t words = static_cast<size_t>(std::count(text.begin(), text.end(), ' ')) + 1;
    return static_cast<float>(words) * ESTIMATED_WORD_S + ESTIMATED_EXTRA_S;
}

// The app detaches from the console at startup, so chapter progress and
// failures go to the application log instead of stdout.
static void
forward_engine_log(ImGuiTestEngine* engine, ImGuiTestContext* ctx, ImGuiTestVerboseLevel level,
                   const char* message, void* user_data)
{
    (void) engine;
    (void) user_data;
    const char* test_name = (ctx != nullptr && ctx->Test != nullptr) ? ctx->Test->Name : "-";
    if(level <= ImGuiTestVerboseLevel_Error)
    {
        spdlog::error("[tutorial {}] {}", test_name, message);
    }
    else if(level == ImGuiTestVerboseLevel_Warning)
    {
        spdlog::warn("[tutorial {}] {}", test_name, message);
    }
    else
    {
        spdlog::info("[tutorial {}] {}", test_name, message);
    }
}

static bool
capture_back_buffer(ImGuiID viewport_id, int x, int y, int w, int h, unsigned int* pixels,
                    void* user_data)
{
    (void) viewport_id;
    (void) user_data;

    const ImGuiIO& io = ImGui::GetIO();
    if(io.DisplayFramebufferScale.x != 1.0f || io.DisplayFramebufferScale.y != 1.0f)
    {
        return false;
    }

    const int framebuffer_height = static_cast<int>(io.DisplaySize.y);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(x, framebuffer_height - y - h, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    // OpenGL returns rows bottom-up; the capture tool expects them top-down.
    const size_t              row_length = static_cast<size_t>(w);
    std::vector<unsigned int> row(row_length);
    for(int top = 0, bottom = h - 1; top < bottom; ++top, --bottom)
    {
        unsigned int* top_row    = pixels + static_cast<size_t>(top) * row_length;
        unsigned int* bottom_row = pixels + static_cast<size_t>(bottom) * row_length;
        std::copy(top_row, top_row + row_length, row.begin());
        std::copy(bottom_row, bottom_row + row_length, top_row);
        std::copy(row.begin(), row.end(), bottom_row);
    }
    return true;
}

static void
render_ripples(ImDrawList* draw_list, double now)
{
    const ImGuiIO& io = ImGui::GetIO();
    if(io.MouseClicked[ImGuiMouseButton_Left])
    {
        g_overlay.ripples.push_back({ io.MousePos, now, false });
    }
    if(io.MouseClicked[ImGuiMouseButton_Right])
    {
        g_overlay.ripples.push_back({ io.MousePos, now, true });
    }

    g_overlay.ripples.erase(
        std::remove_if(g_overlay.ripples.begin(), g_overlay.ripples.end(),
                       [now](const Ripple& r) { return now - r.start > RIPPLE_DURATION_S; }),
        g_overlay.ripples.end());

    for(const Ripple& ripple : g_overlay.ripples)
    {
        const float   t      = static_cast<float>((now - ripple.start) / RIPPLE_DURATION_S);
        const float   eased  = 1.0f - (1.0f - t) * (1.0f - t);
        const float   radius = RIPPLE_START_RADIUS + (RIPPLE_END_RADIUS - RIPPLE_START_RADIUS) * eased;
        const ImVec4& color  = ripple.right_button ? RIGHT_CLICK_COLOR : LEFT_CLICK_COLOR;
        draw_list->AddCircleFilled(ripple.pos, radius,
                                   color_with_alpha(color, RIPPLE_FILL_ALPHA * (1.0f - t)));
        draw_list->AddCircle(ripple.pos, radius, color_with_alpha(color, 1.0f - t), 0,
                             RIPPLE_THICKNESS);
    }

    if(io.MouseDown[ImGuiMouseButton_Left] || io.MouseDown[ImGuiMouseButton_Right])
    {
        const ImVec4& color =
            io.MouseDown[ImGuiMouseButton_Right] ? RIGHT_CLICK_COLOR : LEFT_CLICK_COLOR;
        draw_list->AddCircleFilled(io.MousePos, PRESSED_DOT_RADIUS,
                                   color_with_alpha(color, PRESSED_DOT_ALPHA));
    }
}

// The dimming is drawn from a window kept in front of the app's windows but
// behind open popups. Tooltips always draw above windows, so the tooltips of
// spotlighted controls stay readable.
static void
render_spotlight(double now)
{
    const float alpha = fade(now, g_overlay.spotlight_start, g_overlay.spotlight_end,
                             SPOTLIGHT_FADE_S, SPOTLIGHT_FADE_S);
    if(alpha <= 0.0f)
    {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    constexpr ImGuiWindowFlags FLAGS =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoDocking;
    if(ImGui::Begin(SPOTLIGHT_WINDOW, nullptr, FLAGS))
    {
        ImGuiWindow* overlay = ImGui::GetCurrentWindow();
        ImGui::BringWindowToDisplayFront(overlay);
        for(const ImGuiPopupData& popup : ImGui::GetCurrentContext()->OpenPopupStack)
        {
            if(popup.Window != nullptr)
            {
                ImGui::BringWindowToDisplayFront(popup.Window);
            }
        }

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->PushClipRectFullScreen();
        ImRect rect = g_overlay.spotlight;
        rect.Expand(SPOTLIGHT_MARGIN);
        const ImU32 dim = color_with_alpha(DIM_COLOR, SPOTLIGHT_DIM_ALPHA * alpha);
        draw_list->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(display.x, rect.Min.y), dim);
        draw_list->AddRectFilled(ImVec2(0.0f, rect.Max.y), display, dim);
        draw_list->AddRectFilled(ImVec2(0.0f, rect.Min.y), ImVec2(rect.Min.x, rect.Max.y), dim);
        draw_list->AddRectFilled(ImVec2(rect.Max.x, rect.Min.y), ImVec2(display.x, rect.Max.y),
                                 dim);
        draw_list->AddRect(rect.Min, rect.Max, color_with_alpha(SPOTLIGHT_COLOR, alpha),
                           SPOTLIGHT_ROUNDING, 0, SPOTLIGHT_THICKNESS);
        draw_list->PopClipRect();
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

static void
render_caption(ImDrawList* draw_list, double now)
{
    const float alpha = fade(now, g_overlay.caption_start, g_overlay.caption_end,
                             CAPTION_FADE_IN_S, CAPTION_FADE_OUT_S);
    if(alpha <= 0.0f || g_overlay.caption.empty())
    {
        return;
    }

    ImFont*      font = ImGui::GetFont();
    const ImVec2 text_size =
        font->CalcTextSizeA(CAPTION_FONT_SIZE, FLT_MAX, 0.0f, g_overlay.caption.c_str());
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 box_size(text_size.x + CAPTION_PADDING_X * 2.0f,
                          text_size.y + CAPTION_PADDING_Y * 2.0f);
    const ImVec2 box_min((display.x - box_size.x) * 0.5f,
                         display.y - CAPTION_BOTTOM_GAP - box_size.y);
    const ImVec2 box_max(box_min.x + box_size.x, box_min.y + box_size.y);
    draw_list->AddRectFilled(box_min, box_max, color_with_alpha(CAPTION_BG_COLOR, alpha),
                             CAPTION_ROUNDING);
    draw_list->AddText(font, CAPTION_FONT_SIZE,
                       ImVec2(box_min.x + CAPTION_PADDING_X, box_min.y + CAPTION_PADDING_Y),
                       color_with_alpha(CAPTION_TEXT, alpha), g_overlay.caption.c_str());
}

// Keeps a wheel caption that is up from fading for `seconds` more, so it lasts
// as long as the wheel turns: in cinematic mode the engine pauses before every
// notch, however short the gap the caller asked for.
static void
extend_wheel_caption(double seconds)
{
    const double now = ImGui::GetTime();
    const bool   up  = g_overlay.caption_start >= 0.0 && now <= g_overlay.caption_end;
    if(up && g_overlay.caption.find("wheel") != std::string::npos)
    {
        g_overlay.caption_end = ImMax(g_overlay.caption_end, now + seconds);
    }
}

void
SetConfig(const RecorderConfig& config)
{
    g_config = config;
}

const RecorderConfig&
GetConfig()
{
    return g_config;
}

std::string
ResolveEncoderPath(const std::string& hint)
{
    std::error_code ec;
    if(!hint.empty() && std::filesystem::is_regular_file(hint, ec))
    {
        return hint;
    }

#ifdef _WIN32
    constexpr char        PATH_SEPARATOR = ';';
    constexpr const char* ENCODER_NAME   = "ffmpeg.exe";
#else
    constexpr char        PATH_SEPARATOR = ':';
    constexpr const char* ENCODER_NAME   = "ffmpeg";
#endif
    const std::string path_env = read_environment("PATH");
    std::string       found;
    size_t            begin = 0;
    while(found.empty() && begin <= path_env.size())
    {
        size_t end = path_env.find(PATH_SEPARATOR, begin);
        if(end == std::string::npos)
        {
            end = path_env.size();
        }
        const std::string dir = path_env.substr(begin, end - begin);
        if(!dir.empty())
        {
            const std::filesystem::path candidate = std::filesystem::path(dir) / ENCODER_NAME;
            if(std::filesystem::is_regular_file(candidate, ec))
            {
                found = candidate.string();
            }
        }
        begin = end + 1;
    }
    return found;
}

void
InstallCapture(ImGuiTestEngine* engine)
{
    ImGuiTestEngineIO& io    = ImGuiTestEngine_GetIO(engine);
    io.ScreenCaptureFunc     = capture_back_buffer;
    io.ScreenCaptureUserData = nullptr;
    io.ConfigCaptureEnabled  = true;
    io.ConfigMouseDrawCursor = true;
    ImStrncpy(io.VideoCaptureEncoderPath, g_config.encoder_path.c_str(),
              IM_COUNTOF(io.VideoCaptureEncoderPath));
    ImStrncpy(io.VideoCaptureEncoderParams, ENCODER_PARAMS,
              IM_COUNTOF(io.VideoCaptureEncoderParams));
    ImStrncpy(io.VideoCaptureExtension, VIDEO_EXTENSION, IM_COUNTOF(io.VideoCaptureExtension));
    // Chapters run for minutes; the default watchdog would abort them at 60 s.
    io.ConfigWatchdogWarning   = FLT_MAX;
    io.ConfigWatchdogKillTest  = FLT_MAX;
    io.ConfigWatchdogKillApp   = FLT_MAX;
    io.ConfigVerboseLevel      = ImGuiTestVerboseLevel_Info;
    io.ConfigVerboseLevelOnError = ImGuiTestVerboseLevel_Debug;
    io.ConfigLogToFunc         = forward_engine_log;
    ImGui::GetStyle().MouseCursorScale = CURSOR_SCALE;
    // Switching to another window would otherwise release the simulated keys
    // and buttons mid-chapter.
    ImGui::GetIO().ConfigDebugIgnoreFocusLoss = true;
}

void
RenderOverlay()
{
    const double now = ImGui::GetTime();
    render_spotlight(now);
    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    render_ripples(draw_list, now);
    render_caption(draw_list, now);
}

Director::Director(ImGuiTestContext* ctx, const std::string& file_stem)
: m_ctx(ctx)
, m_file_stem(file_stem)
, m_recording(false)
, m_frames_saved(true)
, m_held_keys(ImGuiKey_None)
, m_origin_s(0.0)
, m_cut_s(0.0)
, m_cut_start_s(0.0)
, m_voice_end_s(0.0)
{}

Director::~Director()
{
    Finish();
}

bool
Director::Start()
{
    ImGuiTestEngineIO& io    = *m_ctx->EngineIO;
    io.ConfigRunSpeed        = ImGuiTestRunSpeed_Cinematic;
    io.ConfigFixedDeltaTime  = SIMULATION_STEP_S;
    io.ConfigMouseDrawCursor = true;
    io.MouseSpeed            = MOUSE_SPEED_PX_PER_S;
    io.MouseWobble           = MOUSE_WOBBLE;
    io.TypingSpeed           = TYPING_SPEED_CHARS_PER_S;
    io.ActionDelayShort      = ACTION_DELAY_SHORT_S;
    io.ActionDelayStandard   = ACTION_DELAY_STANDARD_S;

    g_overlay = OverlayState();

    // The main loop puts the window back to the video size after a display
    // change; footage at any other size is useless.
    const auto at_video_size = []() {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        return static_cast<int>(size.x) == VIDEO_WIDTH && static_cast<int>(size.y) == VIDEO_HEIGHT;
    };
    for(int i = 0; i < WINDOW_SIZE_WAIT_FRAMES && !at_video_size(); i++)
    {
        m_ctx->Yield();
    }
    if(!at_video_size())
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        m_ctx->LogError("The window is %.0fx%.0f instead of %dx%d, so %s is not recorded",
                        size.x, size.y, VIDEO_WIDTH, VIDEO_HEIGHT, m_file_stem.c_str());
        return false;
    }

    m_ctx->CaptureReset();
    ImGuiCaptureArgs*           args = m_ctx->CaptureArgs;
    const std::filesystem::path output =
        std::filesystem::path(g_config.output_dir) / (m_file_stem + VIDEO_EXTENSION);
    ImStrncpy(args->InOutputFile, output.string().c_str(), IM_COUNTOF(args->InOutputFile));
    args->InRecordFPSTarget = VIDEO_FPS;
    // Alignment would trim a full-window capture by two pixels on each axis.
    args->InSizeAlign = 1;

    m_recording = m_ctx->CaptureBeginVideo();
    if(m_recording)
    {
        m_frames_saved = true;
        m_origin_s     = ImGui::GetTime();
        m_cut_s        = 0.0;
        m_voice_end_s  = 0.0;
        m_timeline.clear();
        m_narration.clear();
        HoldExactly(OPENING_HOLD_S);
    }
    return m_recording;
}

void
Director::Finish()
{
    if(!m_recording)
    {
        return;
    }
    ReleaseKeys();
    ClearSpotlight();
    WaitForVoice();
    HoldExactly(CLOSING_HOLD_S);
    Log(VideoTime(), "end");
    m_ctx->CaptureEndVideo();
    m_recording                     = false;
    m_ctx->EngineIO->ConfigRunSpeed = ImGuiTestRunSpeed_Fast;
    WriteTimeline();
}

void
Director::Hold(float seconds)
{
    const double voice_left_s = m_voice_end_s - VideoTime();
    HoldExactly(static_cast<float>(ImMin<double>(seconds, ImMax(voice_left_s, 0.0) + POST_VOICE_HOLD_S)));
}

void
Director::HoldExactly(float seconds)
{
    m_ctx->Sleep(seconds);
}

void
Director::Beat()
{
    Hold(BEAT_S);
}

void
Director::Pause()
{
    WaitForVoice();
    Log(VideoTime(), "pause");
    Hold(PAUSE_S);
}

void
Director::Say(const char* text)
{
    if(!m_narration.empty())
    {
        const double wait_s = m_voice_end_s + LINE_GAP_S - VideoTime();
        if(wait_s > 0.0)
        {
            HoldExactly(static_cast<float>(wait_s));
        }
    }
    const std::unordered_map<std::string, float>& lengths = narration_lengths();
    const auto  found    = lengths.find(text);
    float       length_s = 0.0f;
    if(found != lengths.end())
    {
        length_s = found->second;
    }
    else
    {
        length_s = estimated_length(text);
        spdlog::warn("[tutorial {}] no synthesized narration yet, estimating {:.1f} s for: {}",
                     m_file_stem, length_s, text);
    }
    const double start_s = VideoTime();
    m_voice_end_s        = start_s + length_s;
    m_line_text          = text;
    m_line_start_s       = start_s;
    m_line_length_s      = length_s;
    m_word_cursor        = 0;
    m_text_cursor        = 0;
    m_narration.push_back({ start_s, text });
    Log(start_s, "say " + quoted(printable(text)));
}

double
Director::WordTime(const char* word)
{
    const std::string wanted = word_key(word);
    const auto&       timed  = narration_words();
    const auto        found  = timed.find(m_line_text);
    if(found != timed.end())
    {
        const std::vector<TimedWord>& heard = found->second;
        const auto                    starts_like = [&wanted](const std::string& key) {
            return key.size() >= WORD_PREFIX_MIN && wanted.size() >= WORD_PREFIX_MIN &&
                   (key.rfind(wanted, 0) == 0 || wanted.rfind(key, 0) == 0);
        };
        for(const bool exact : { true, false })
        {
            for(size_t i = m_word_cursor; i < heard.size(); i++)
            {
                if(exact ? heard[i].key == wanted : starts_like(heard[i].key))
                {
                    m_word_cursor = i + 1;
                    return m_line_start_s + heard[i].start_s;
                }
            }
        }
    }
    // Without a timing, a word is placed by where it falls in the text.
    std::istringstream words(m_line_text);
    std::string        each;
    size_t             offset = 0;
    while(words >> each)
    {
        offset = m_line_text.find(each, offset);
        if(offset >= m_text_cursor && word_key(each) == wanted)
        {
            m_text_cursor = offset + each.size();
            return m_line_start_s + m_line_length_s * static_cast<double>(offset) /
                                        static_cast<double>(ImMax<size_t>(m_line_text.size(), 1));
        }
        offset += each.size();
    }
    spdlog::warn("[tutorial {}] '{}' is not in the line: {}", m_file_stem, word, m_line_text);
    return -1.0;
}

void
Director::WaitForWord(const char* word)
{
    const double at_s = WordTime(word);
    if(at_s < 0.0)
    {
        return;
    }
    const double wait_s = at_s - WORD_LEAD_S - VideoTime();
    if(wait_s > 0.0)
    {
        HoldExactly(static_cast<float>(wait_s));
    }
}

void
Director::WaitUntilSaid(const char* word)
{
    const double at_s = WordTime(word);
    if(at_s < 0.0)
    {
        return;
    }
    const double wait_s = at_s - ARRIVE_EARLY_S - VideoTime();
    if(wait_s > 0.0)
    {
        HoldExactly(static_cast<float>(wait_s));
    }
}

void
Director::PointAt(const ImVec2& pos, const char* word)
{
    const ImVec2 from    = ImGui::GetIO().MousePos;
    const float  length  = ImSqrt(ImLengthSqr(ImVec2(pos.x - from.x, pos.y - from.y)));
    const float  natural = ImMin(GLIDE_BASE_S + length / GLIDE_PX_PER_S, GLIDE_MAX_S);
    float        seconds = natural;
    const double word_s  = word != nullptr ? WordTime(word) : -1.0;
    if(word_s >= 0.0)
    {
        const double arrive_s = word_s - ARRIVE_EARLY_S;
        const double wait_s   = arrive_s - natural - VideoTime();
        if(wait_s > 0.0)
        {
            HoldExactly(static_cast<float>(wait_s));
        }
        seconds = static_cast<float>(ImClamp(arrive_s - VideoTime(),
                                             static_cast<double>(GLIDE_MIN_S),
                                             static_cast<double>(natural)));
    }
    const double start_s = VideoTime();
    Glide(pos, seconds);
    std::string entry = "point at " + quoted(LabelAtPointer());
    if(word != nullptr)
    {
        entry += " on " + quoted(word);
    }
    Log(start_s, entry);
}

void
Director::PointAtItem(ImGuiTestRef ref, const char* word)
{
    const bool was_saved = m_frames_saved;
    SetFramesSaved(false);
    const ImGuiTestItemInfo info = m_ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
    SetFramesSaved(was_saved);
    if(info.ID == 0)
    {
        spdlog::warn("[tutorial {}] nothing to point at for '{}'", m_file_stem,
                     ref.Path != nullptr ? ref.Path : "");
        return;
    }
    PointAt(info.RectClipped.GetCenter(), word);
}

void
Director::ClickHere(ImGuiMouseButton button)
{
    Log(VideoTime(), click_verb(button) + " " + quoted(LabelAtPointer()));
    m_ctx->MouseClick(button);
}

void
Director::Cut(const std::function<void()>& work)
{
    const bool was_saved = m_frames_saved;
    SetFramesSaved(false);
    work();
    SetFramesSaved(was_saved);
}

void
Director::Glide(const ImVec2& to, float seconds)
{
    // At normal speed the engine neither pauses before a move nor stretches a
    // short one to half a second, so the glide takes `seconds`.
    ImGuiTestEngineIO&      io          = *m_ctx->EngineIO;
    const ImGuiTestRunSpeed run_speed   = io.ConfigRunSpeed;
    const float             mouse_speed = io.MouseSpeed;
    const ImVec2            from        = ImGui::GetIO().MousePos;
    const float length = ImSqrt(ImLengthSqr(ImVec2(to.x - from.x, to.y - from.y)));
    io.ConfigRunSpeed  = ImGuiTestRunSpeed_Normal;
    io.MouseSpeed      = ImMax(length / ImMax(seconds, SIMULATION_STEP_S), 1.0f);
    m_ctx->MouseMoveToPos(to);
    io.MouseSpeed     = mouse_speed;
    io.ConfigRunSpeed = run_speed;
}

void
Director::WaitForVoice()
{
    const double remaining_s = m_voice_end_s - VideoTime();
    if(remaining_s > 0.0)
    {
        HoldExactly(static_cast<float>(remaining_s));
    }
}

void
Director::MoveTo(const ImVec2& pos)
{
    // Already there, the engine would still pause, then hold still for half a
    // second.
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    if(ImLengthSqr(ImVec2(pos.x - mouse.x, pos.y - mouse.y)) < 1.0f)
    {
        return;
    }
    m_ctx->MouseMoveToPos(pos);
}

void
Director::Hover(const ImVec2& pos, float seconds)
{
    const double start_s = VideoTime();
    MoveTo(pos);
    Log(start_s, "hover " + quoted(LabelAtPointer()));
    Hold(seconds);
}

void
Director::HoverItem(ImGuiTestRef ref, float seconds)
{
    const std::string label = LabelOf(ref);
    Log(VideoTime(), "hover " + quoted(label));
    m_ctx->MouseMove(ref);
    Hold(seconds);
}

void
Director::Click(const ImVec2& pos, ImGuiMouseButton button)
{
    const double start_s = VideoTime();
    MoveTo(pos);
    Log(start_s, click_verb(button) + " " + quoted(LabelAtPointer()));
    m_ctx->MouseClick(button);
}

void
Director::DoubleClick(const ImVec2& pos)
{
    const double start_s = VideoTime();
    MoveTo(pos);
    Log(start_s, "double-click " + quoted(LabelAtPointer()));
    m_ctx->MouseDoubleClick(ImGuiMouseButton_Left);
}

void
Director::ClickItem(ImGuiTestRef ref, ImGuiMouseButton button)
{
    const std::string label = LabelOf(ref);
    Log(VideoTime(), click_verb(button) + " " + quoted(label));
    m_ctx->ItemClick(ref, button);
}

void
Director::Drag(const ImVec2& from, const ImVec2& to, float seconds)
{
    const double start_s = VideoTime();
    MoveTo(from);
    Log(start_s, "drag " + quoted(LabelAtPointer()) + " " + drag_direction(from, to));
    m_ctx->MouseDown(ImGuiMouseButton_Left);
    Glide(to, seconds);
    m_ctx->MouseUp(ImGuiMouseButton_Left);
}

void
Director::Wheel(const ImVec2& pos, float notches_per_step, int steps, float step_gap)
{
    const double start_s = VideoTime();
    MoveTo(pos);
    Log(start_s, "scroll " + quoted(LabelAtPointer()) + " " +
                     wheel_amount(notches_per_step * static_cast<float>(steps)));
    // The engine pauses before every turn of the wheel; `step_gap` alone paces
    // them.
    ImGuiTestEngineIO& io    = *m_ctx->EngineIO;
    const float        pause = io.ActionDelayStandard;
    io.ActionDelayStandard   = 0.0f;
    for(int i = 0; i < steps; i++)
    {
        m_ctx->MouseWheelY(notches_per_step);
        extend_wheel_caption(step_gap + CAPTION_LINGER_S);
        HoldExactly(step_gap);
    }
    io.ActionDelayStandard = pause;
}

void
Director::Key(ImGuiKeyChord chord, const char* caption, int repeats, float gap)
{
    std::string entry = "press " + quoted(printable(caption));
    if(repeats > 1)
    {
        entry += " x" + std::to_string(repeats);
    }
    Log(VideoTime(), entry);
    const float press_time = gap + ACTION_DELAY_SHORT_S * 2.0f;
    ShowCaption(caption, static_cast<float>(repeats) * press_time + CAPTION_LINGER_S);
    for(int i = 0; i < repeats; i++)
    {
        m_ctx->KeyPress(chord);
        HoldExactly(gap);
    }
}

void
Director::KeyHold(ImGuiKeyChord chord, const char* caption, float seconds)
{
    Log(VideoTime(), "hold " + quoted(printable(caption)) + " for " + format_seconds(seconds));
    ShowCaption(caption, seconds + CAPTION_LINGER_S);
    m_ctx->KeyDown(chord);
    HoldExactly(seconds);
    m_ctx->KeyUp(chord);
    HoldExactly(ACTION_DELAY_SHORT_S);
}

void
Director::HoldKeys(ImGuiKeyChord chord, const char* caption)
{
    ReleaseKeys();
    Log(VideoTime(), "hold down " + quoted(printable(caption)));
    m_ctx->KeyDown(chord);
    m_held_keys = chord;
    ShowCaption(caption, PERSISTENT_S);
}

void
Director::ReleaseKeys()
{
    if(m_held_keys == ImGuiKey_None)
    {
        return;
    }
    Log(VideoTime(), "release keys");
    m_ctx->KeyUp(m_held_keys);
    m_held_keys           = ImGuiKey_None;
    g_overlay.caption_end = ImGui::GetTime();
}

void
Director::Type(const char* text)
{
    Log(VideoTime(), "type " + quoted(printable(text)));
    m_ctx->KeyChars(text);
}

void
Director::Spotlight(const ImRect& rect)
{
    ImGuiWindow* window = nullptr;
    ImGui::FindHoveredWindowEx(rect.GetCenter(), true, &window, nullptr);
    Log(VideoTime(), "spotlight " + quoted(window != nullptr ? window_label(window) : ""));

    const double now     = ImGui::GetTime();
    const bool   visible = g_overlay.spotlight_start >= 0.0 && now <= g_overlay.spotlight_end;
    g_overlay.spotlight  = rect;
    if(!visible)
    {
        g_overlay.spotlight_start = now;
    }
    g_overlay.spotlight_end = now + PERSISTENT_S;
}

void
Director::ClearSpotlight()
{
    const double now = ImGui::GetTime();
    if(g_overlay.spotlight_start >= 0.0 && now < g_overlay.spotlight_end)
    {
        g_overlay.spotlight_end = now;
    }
}

void
Director::Caption(const char* text, float seconds)
{
    Log(VideoTime(), "caption " + quoted(printable(text)));
    ShowCaption(text, seconds);
}

void
Director::ShowCaption(const char* text, float seconds)
{
    const double now     = ImGui::GetTime();
    const bool   visible = g_overlay.caption_start >= 0.0 && now <= g_overlay.caption_end;
    if(!visible || g_overlay.caption != text)
    {
        g_overlay.caption_start = now;
    }
    g_overlay.caption     = text;
    g_overlay.caption_end = now + seconds;
}

bool
Director::WaitFor(const std::function<bool()>& ready, float visible_s, float timeout_s)
{
    using Clock             = std::chrono::steady_clock;
    const auto   started    = Clock::now();
    const double start_s    = VideoTime();
    const bool   was_saved  = m_frames_saved;
    float        recorded_s = 0.0f;
    bool         paused     = false;
    while(!ready())
    {
        const float waited_s = std::chrono::duration<float>(Clock::now() - started).count();
        if(waited_s > timeout_s || m_ctx->IsError())
        {
            break;
        }
        if(!paused && recorded_s >= visible_s)
        {
            SetFramesSaved(false);
            paused = true;
        }
        m_ctx->Yield();
        recorded_s += ImGui::GetIO().DeltaTime;
    }
    if(paused)
    {
        SetFramesSaved(was_saved);
        Log(start_s, "wait " + format_seconds(visible_s) + ", the rest of the wait is cut");
    }
    else if(recorded_s >= MIN_LOGGED_WAIT_S)
    {
        Log(start_s, "wait " + format_seconds(recorded_s));
    }
    return ready();
}

bool
Director::ClickAndCutUntil(const ImVec2& pos, const std::function<bool()>& ready,
                           float timeout_s)
{
    const double start_s = VideoTime();
    MoveTo(pos);
    Log(start_s, "click " + quoted(LabelAtPointer()));
    // The pointer has just arrived, so the press skips the engine's pause, and
    // the button is held on camera as long as a plain click holds it.
    ImGuiTestEngineIO& io    = *m_ctx->EngineIO;
    const float        pause = io.ActionDelayStandard;
    io.ActionDelayStandard   = 0.0f;
    m_ctx->MouseDown(ImGuiMouseButton_Left);
    io.ActionDelayStandard = pause;
    m_ctx->SleepShort();
    const double cut_s     = VideoTime();
    const bool   was_saved = m_frames_saved;
    SetFramesSaved(false);
    m_ctx->MouseUp(ImGuiMouseButton_Left);

    using Clock        = std::chrono::steady_clock;
    const auto started = Clock::now();
    while(!ready() && !m_ctx->IsError() &&
          std::chrono::duration<float>(Clock::now() - started).count() <= timeout_s)
    {
        m_ctx->Yield();
    }
    SetFramesSaved(was_saved);
    Log(cut_s, "wait " + format_seconds(0.0f) + ", the rest of the wait is cut");
    return ready();
}

void
Director::SetFramesSaved(bool saved)
{
    if(!m_recording || saved == m_frames_saved)
    {
        return;
    }
    m_frames_saved   = saved;
    const double now = ImGui::GetTime();
    if(saved)
    {
        m_ctx->CaptureArgs->InFlags &= ~ImGuiCaptureFlags_NoSave;
        m_cut_s += now - m_cut_start_s;
    }
    else
    {
        m_ctx->CaptureArgs->InFlags |= ImGuiCaptureFlags_NoSave;
        m_cut_start_s = now;
    }
}

double
Director::VideoTime() const
{
    const double now = ImGui::GetTime();
    const double cut = m_frames_saved ? m_cut_s : m_cut_s + (now - m_cut_start_s);
    return now - m_origin_s - cut;
}

void
Director::Log(double video_s, const std::string& text)
{
    if(m_recording)
    {
        m_timeline.push_back(format_time(video_s) + "  " + text);
    }
}

void
Director::WriteTimeline() const
{
    const std::filesystem::path dir = std::filesystem::path(g_config.output_dir) / TIMING_DIR;
    std::error_code             ec;
    std::filesystem::create_directories(dir, ec);
    std::ofstream file(dir / (m_file_stem + ".txt"), std::ios::trunc);
    std::ofstream cues(dir / (m_file_stem + NARRATION_SUFFIX), std::ios::trunc);
    if(file && cues)
    {
        file << "# " << m_file_stem << ": each action and when it starts, as m:ss.s into the video\n";
        for(const std::string& line : m_timeline)
        {
            file << line << '\n';
        }
        for(const NarrationCue& cue : m_narration)
        {
            char start[TEXT_BUFFER_SIZE];
            ImFormatString(start, IM_COUNTOF(start), "%.3f", cue.start_s);
            cues << start << '\t' << cue.text << '\n';
        }
    }
    else
    {
        spdlog::warn("[tutorial {}] could not write its timing files to {}", m_file_stem,
                     dir.string());
    }
}

std::string
Director::LabelOf(ImGuiTestRef ref)
{
    const bool was_saved = m_frames_saved;
    SetFramesSaved(false);
    const ImGuiTestItemInfo info = m_ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
    SetFramesSaved(was_saved);
    std::string label = readable_label(info.DebugLabel);
    if(label.empty() && ref.Path != nullptr)
    {
        label = printable(ref.Path);
    }
    return label;
}

std::string
Director::LabelAtPointer()
{
    const ImGuiContext& g       = *ImGui::GetCurrentContext();
    ImGuiWindow*        window  = g.HoveredWindow;
    const ImGuiID       item_id = g.HoveredIdPreviousFrame;
    std::string         label;
    if(item_id != 0)
    {
        label = LabelOf(item_id);
    }
    if(label.empty() && window != nullptr)
    {
        label = window_label(window);
    }
    return label;
}

}  // namespace Tutorial
}  // namespace RocProfVis
