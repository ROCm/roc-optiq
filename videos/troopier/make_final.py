"""Builds the ROCm Optiq tutorial playlist: a few videos of five to ten minutes,
each a group of voiced chapters between a title card and a closing card on the
AMD title screen, joined with short crossfades, with YouTube timestamps.

  cards  renders every video's title and closing cards from the voiced lines
         (<clips>/<id>.wav, ids from frame_lines()) and the title screen image.
  join   joins each video's cards and chapters, and writes its timestamps.

Usage: python make_final.py cards --clips <folder> --screen <image> --out <folder>
       python make_final.py join --voiced <folder> --cards <folder> --out <folder>
"""

import argparse
import subprocess
import sys
from pathlib import Path

import make_masters as mm

INTRO_STEM = "00_introduction"
SERIES = "rocm optiq tutorials_"
PRESENTER = "Sajeeth Wimalasuriyan"
# The series introduction, said on the first video's title card.
INTRO_LINES = [
    "Hi, I'm Sajeeth, and I'm one of the developers of ROCm Optiq.",
    "Optiq is a desktop tool for visualizing and analyzing data from the ROCm profilers.",
    "It turns a trace into an interactive timeline of every CPU thread and GPU queue. It also "
    "brings compute profiler results together in charts and tables, so you can see where your "
    "application spends its time, and how well it uses the GPU.",
    "Let's jump into it.",
]
CLOSING = ("That completes our tour of ROCm Optiq. Thanks for watching, and to learn more, see the "
           "ROCm Optiq documentation.")
VIDEOS = [
    {"title": ["Getting Started", "with ROCm Optiq"], "file": "Getting Started",
     "chapters": ["01_opening_a_trace", "02_interface_tour", "03_navigating_the_timeline",
                  "04_system_topology_panel"],
     "welcome": INTRO_LINES,
     "farewell": ["Thanks for watching. In the next video, we'll work with tracks, events, and time ranges."],
     "next": "Up next: Tracks, Events, and Time Ranges"},
    {"title": ["Tracks, Events,", "and Time Ranges"], "file": "Tracks, Events, and Time Ranges",
     "chapters": ["05_working_with_tracks", "06_events_and_flows", "07_time_ranges_and_measuring"],
     "welcome": ["Hi, I'm Sajeeth. In this video, we'll work with tracks, events, and time ranges in ROCm Optiq."],
     "farewell": ["Thanks for watching. In the next video, we'll explore the Advanced Details tables, "
                  "search, annotations, and ways to customize Optiq."],
     "next": "Up next: Tables, Search, Annotations, and Settings"},
    {"title": ["Tables, Search,", "Annotations, and Settings"], "file": "Tables, Search, Annotations, and Settings",
     "chapters": ["08_advanced_details_tables", "09_search_minimap_and_summary",
                  "10_annotations_bookmarks_and_projects", "11_customizing_optiq"],
     "welcome": ["Hi, I'm Sajeeth. In this video, we'll explore the Advanced Details tables, search, "
                 "annotations and bookmarks, and ways to customize ROCm Optiq."],
     "farewell": ["Thanks for watching. In the next video, we'll analyze ROCm Compute Profiler data."],
     "next": "Up next: Analyzing ROCm Compute Profiler Data"},
    {"title": ["Analyzing ROCm Compute", "Profiler Data"], "file": "Analyzing ROCm Compute Profiler Data",
     "chapters": ["12_compute_opening_and_summary", "13_compute_kernel_details",
                  "14_compute_tables_workload_and_comparison"],
     "welcome": ["Hi, I'm Sajeeth. In this final video, we'll analyze ROCm Compute Profiler data in ROCm Optiq."],
     "farewell": [CLOSING],
     "next": "Learn more in the ROCm Optiq documentation"},
]
CHAPTER_TITLES = {
    "01_opening_a_trace": "Opening a Trace",
    "02_interface_tour": "Interface Tour",
    "03_navigating_the_timeline": "Navigating the Timeline",
    "04_system_topology_panel": "System Topology Panel",
    "05_working_with_tracks": "Working with Tracks",
    "06_events_and_flows": "Events and Flows",
    "07_time_ranges_and_measuring": "Time Ranges and Measuring",
    "08_advanced_details_tables": "Advanced Details Tables",
    "09_search_minimap_and_summary": "Search, Minimap, and Summary",
    "10_annotations_bookmarks_and_projects": "Annotations, Bookmarks, and Projects",
    "11_customizing_optiq": "Customizing Optiq",
    "12_compute_opening_and_summary": "Opening Compute Data and the Summary View",
    "13_compute_kernel_details": "Kernel Details",
    "14_compute_tables_workload_and_comparison": "Table View, Workload Details, and Baseline Comparison",
}

WIDTH, HEIGHT, FPS = 1920, 1080, 60
LEAD_S = 1.2
LINE_GAP_S = 0.45
TAIL_S = 1.0
FAREWELL_TAIL_S = 2.2
CROSSFADE_S = 0.5
# The shortest chapter YouTube shows in a video's chapter list.
MIN_CHAPTER_S = 10
# The title panel: a dark veil over the left of the screen with an accent bar
# along its edge, in the teal of the screen's circuitry.
PANEL_W = 980
ACCENT = "0x1ED2E6"
TEXT_X = 110
FONT_LIGHT = "C\\:/Windows/Fonts/segoeuil.ttf"
FONT_SEMILIGHT = "C\\:/Windows/Fonts/segoeuisl.ttf"
FONT_SEMIBOLD = "C\\:/Windows/Fonts/seguisb.ttf"
FONT_BOLD = "C\\:/Windows/Fonts/segoeuib.ttf"
AUDIO_ARGS = ["-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "1"]


def frame_lines():
    """(id, text) of every line the cards say: the series introduction as 00_NN,
    then each other welcome and farewell as 90_NN, in order."""
    lines = [(f"00_{n:02d}", text) for n, text in enumerate(INTRO_LINES, 1)]
    others = []
    for video in VIDEOS:
        for text in video["welcome"] + video["farewell"]:
            if text not in INTRO_LINES and text not in others:
                others.append(text)
    return lines + [(f"90_{n:02d}", text) for n, text in enumerate(others, 1)]


def video_codec_args():
    return mm.VIDEO_ARGS[mm.VIDEO_ARGS.index("-c:v"):mm.VIDEO_ARGS.index("-an")]


def escape(text):
    return text.replace("\\", "\\\\").replace(":", "\\:").replace("'", "\u2019")


def text_at(font, text, size, color, y, start):
    fade = f"alpha='min(1,max(0,(t-{start:.2f})/0.7))'"
    return (f"drawtext=fontfile='{font}':text='{escape(text)}':fontcolor={color}:fontsize={size}:"
            f"x={TEXT_X}:y={y}:{fade}")


def card(screen, length, rows):
    """Filter chain of a card: the still screen, the title panel, and its rows of text."""
    chain = [f"[0:v]scale={WIDTH}:{HEIGHT}:flags=lanczos",
             f"trim=duration={length:.3f}",
             f"drawbox=x=0:y=0:w={PANEL_W}:h=ih:color=black@0.66:t=fill",
             f"drawbox=x=0:y=0:w=12:h=ih:color={ACCENT}@1:t=fill"]
    chain += [text_at(*row) for row in rows]
    return ",".join(chain)


def title_rows(video, part, parts):
    rows = [(FONT_SEMILIGHT, SERIES, 34, ACCENT, 300, 0.5)]
    y = 360
    for line in video["title"]:
        rows.append((FONT_BOLD, line, 70, "white", y, 0.8))
        y += 90
    rows.append((FONT_LIGHT, f"Part {part} of {parts}", 36, "0xC9D3DB", y + 20, 1.1))
    rows.append((FONT_LIGHT, "Presented by", 28, "0xC9D3DB", 820, 1.4))
    rows.append((FONT_SEMIBOLD, PRESENTER, 44, "white", 858, 1.4))
    return rows


def closing_rows(video, part, parts):
    return [(FONT_SEMILIGHT, SERIES, 34, ACCENT, 300, 0.4),
            (FONT_BOLD, "Thanks for watching", 76, "white", 360, 0.6),
            (FONT_LIGHT, f"Part {part} of {parts}", 36, "0xC9D3DB", 476, 0.8),
            (FONT_SEMILIGHT, video["next"], 32, "white", 560, 1.0),
            (FONT_LIGHT, "Presented by", 28, "0xC9D3DB", 820, 1.0),
            (FONT_SEMIBOLD, PRESENTER, 44, "white", 858, 1.0)]


def render_card(ffmpeg, ffprobe, screen, clips, rows, tail_s, fades, out):
    starts, t = [], LEAD_S
    for clip in clips:
        starts.append(t)
        t += mm.duration(ffprobe, clip) + LINE_GAP_S
    length = t - LINE_GAP_S + tail_s
    video = card(screen, length, rows)
    if "in" in fades:
        video += ",fade=t=in:st=0:d=1.0"
    if "out" in fades:
        video += f",fade=t=out:st={length - 1.2:.3f}:d=1.2"
    delays = "".join(f"[{n + 1}:a]volume={mm.line_gain(ffmpeg, clip):.2f}dB,adelay=delays={int(s * 1000)}:all=1[a{n}];"
                     for n, (clip, s) in enumerate(zip(clips, starts)))
    mix = "".join(f"[a{n}]" for n in range(len(clips)))
    voice = f"{delays}{mix}amix=inputs={len(clips)}:normalize=0:dropout_transition=0,{mm.NARRATION_POLISH}"
    clip_inputs = [arg for clip in clips for arg in ("-i", str(clip))]
    # The loudness pass measures the voice alone; a tiny stand-in keeps input 0.
    loudness = mm.loudness_filter(ffmpeg, ["-f", "lavfi", "-t", "0.1", "-i", "anullsrc", *clip_inputs], voice)
    audio = f"{voice},{loudness},aresample=48000,apad,atrim=0:{length:.3f}"
    if "out" in fades:
        audio += f",afade=t=out:st={length - 1.0:.3f}:d=1.0"
    inputs = ["-framerate", str(FPS), "-loop", "1", "-i", str(screen), *clip_inputs]
    graph = f"{video},scale=out_color_matrix=bt709:out_range=tv,format=yuv420p[v];{audio}[a]"
    subprocess.run([ffmpeg, "-v", "error", "-y", *inputs, "-filter_complex", graph, "-map", "[v]", "-map", "[a]",
                    "-t", f"{length:.3f}", *video_codec_args(), *AUDIO_ARGS, "-movflags", "+faststart",
                    str(out)], check=True)
    print(f"{out.name}: {length:.1f} s", flush=True)


def cards(args, ffmpeg, ffprobe):
    ids = {text: line_id for line_id, text in frame_lines()}
    args.out.mkdir(parents=True, exist_ok=True)
    for part, video in enumerate(VIDEOS, 1):
        if args.part and part != args.part:
            continue
        for kind in ("welcome", "farewell"):
            clips = [args.clips / f"{ids[text]}.wav" for text in video[kind]]
            missing = [c.name for c in clips if not c.exists()]
            if missing:
                sys.exit(f"part {part} {kind}: missing voiced lines {', '.join(missing)}")
            if kind == "welcome" and "welcome" in args.kinds:
                render_card(ffmpeg, ffprobe, args.screen, clips, title_rows(video, part, len(VIDEOS)),
                            TAIL_S, "in", args.out / f"part{part}_welcome.mp4")
            elif kind == "farewell" and "farewell" in args.kinds:
                last = part == len(VIDEOS)
                render_card(ffmpeg, ffprobe, args.screen, clips, closing_rows(video, part, len(VIDEOS)),
                            FAREWELL_TAIL_S, "out" if last else "", args.out / f"part{part}_farewell.mp4")


def stamp(seconds):
    whole = int(seconds)
    return f"{whole // 60}:{whole % 60:02d}"


def join(args, ffmpeg, ffprobe):
    args.out.mkdir(parents=True, exist_ok=True)
    for part, video in enumerate(VIDEOS, 1):
        if args.part and part != args.part:
            continue
        parts = ([args.cards / f"part{part}_welcome.mp4"] + [args.voiced / f"{stem}.mp4" for stem in video["chapters"]]
                 + [args.cards / f"part{part}_farewell.mp4"])
        missing = [p.name for p in parts if not p.exists()]
        if missing:
            sys.exit(f"part {part}: missing {', '.join(missing)}")
        lengths = [mm.duration(ffprobe, p) for p in parts]
        inputs = [arg for p in parts for arg in ("-i", str(p))]
        # One frame rate, time base and sample format throughout, as the crossfades need.
        graph = [f"[{n}:v]fps={FPS},settb=AVTB,format=yuv420p[v{n}in];"
                 f"[{n}:a]aresample=48000,aformat=sample_fmts=fltp:channel_layouts=mono[a{n}in]"
                 for n in range(len(parts))]
        v, a, offset, marks = "[v0in]", "[a0in]", 0.0, [0.0]
        for n in range(1, len(parts)):
            offset += lengths[n - 1] - CROSSFADE_S
            marks.append(offset)
            graph.append(f"{v}[v{n}in]xfade=transition=fade:duration={CROSSFADE_S}:offset={offset:.3f}[v{n}]")
            graph.append(f"{a}[a{n}in]acrossfade=d={CROSSFADE_S}:c1=tri:c2=tri[a{n}]")
            v, a = f"[v{n}]", f"[a{n}]"
        # The crossfades work in 4:4:4; the video goes out in 4:2:0 like its parts.
        graph.append(f"{v}format=yuv420p[vout]")
        v = "[vout]"
        name = f"ROCm Optiq Tutorial {part} - {video['file']}"
        out = args.out / f"{name}.mp4"
        # The parts are already encoded at a high quality; this pass is fast,
        # at a CRF low enough that screen text stays crisp.
        codec = video_codec_args()
        codec[codec.index("-preset") + 1] = "veryfast"
        codec[codec.index("-crf") + 1] = "11"
        subprocess.run([ffmpeg, "-v", "error", "-y", *inputs, "-filter_complex", ";".join(graph), "-map", v,
                        "-map", a, *codec, *AUDIO_ARGS, "-movflags", "+faststart", str(out)], check=True)
        # The title card is its own chapter only when YouTube would list it.
        titles = ["Introduction"] + [CHAPTER_TITLES[s] for s in video["chapters"]]
        starts = marks[:len(titles)]
        if starts[1] < MIN_CHAPTER_S:
            titles, starts = titles[1:], [0.0] + starts[2:]
        rows = [f"{stamp(s + (CROSSFADE_S / 2 if s else 0))} {t}" for s, t in zip(starts, titles)]
        (args.out / f"{name} - timestamps.txt").write_text("\n".join(rows) + "\n", encoding="utf-8")
        print(f"{out.name}: {mm.clock(mm.duration(ffprobe, out))}\n  " + "\n  ".join(rows), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("cards", "join"))
    parser.add_argument("--clips", type=Path, help="voiced lines (cards)")
    parser.add_argument("--screen", type=Path, help="title screen image (cards)")
    parser.add_argument("--voiced", type=Path, help="voiced chapters (join)")
    parser.add_argument("--cards", type=Path, help="rendered cards (join)")
    parser.add_argument("--part", type=int, default=0, help="only this video's cards or join")
    parser.add_argument("--kinds", nargs="+", default=["welcome", "farewell"], choices=("welcome", "farewell"),
                        help="which cards to render")
    parser.add_argument("--out", type=Path, required=True, help="output folder")
    args = parser.parse_args()
    ffmpeg, ffprobe = mm.tools()
    {"cards": cards, "join": join}[args.command](args, ffmpeg, ffprobe)


if __name__ == "__main__":
    main()
