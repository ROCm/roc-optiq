"""Builds the AMD ROCm Optiq tutorial playlist: a few videos of five to ten minutes,
each a group of voiced chapters between a title card and a closing card, then the
legal disclaimer and, when given, the AMD outro bumper. The parts are joined with
short crossfades, and each video gets its YouTube timestamps.

The cards follow the AMD brand: white Klavika type on black (Arial where Klavika
is not installed), the AMD ROCm Optiq lockup under the title, and the full
"AMD ROCm(TM)" name at the first mention of a ROCm product on every card.

  cards  renders every video's title and closing cards from the voiced lines
         (<clips>/<id>.wav, ids from frame_lines()) and the white lockup image,
         and the legal disclaimer card.
  join   joins each video's cards, chapters and disclaimer, then the outro
         bumper, and writes its timestamps.

Usage: python make_final.py cards --clips <folder> --lockup <white lockup png> --out <folder>
       python make_final.py join --voiced <folder> --cards <folder> --out <folder> [--outro <video>]
"""

import argparse
import os
import subprocess
import sys
import textwrap
from pathlib import Path

import make_masters as mm

INTRO_STEM = "00_introduction"
TM = "\u2122"
# The first mention on every card: the full product name with its mark.
SERIES = f"AMD ROCm{TM} Optiq Tutorials"
PRESENTER = "Sajeeth Wimalasuriyan"
# The series introduction, said on the first video's title card.
INTRO_LINES = [
    "Hi, I'm Sajeeth, and I'm one of the developers of AMD ROCm Optiq.",
    "Optiq is a desktop tool for visualizing and analyzing data from the ROCm profilers.",
    "It turns a trace into an interactive timeline of every CPU thread and GPU queue. It also "
    "brings compute profiler results together in charts and tables, so you can see where your "
    "application spends its time, and how well it uses the GPU.",
    "Let's jump into it.",
]
CLOSING = ("That completes our tour of AMD ROCm Optiq. Thanks for watching, and to learn more, see "
           "the ROCm Optiq documentation.")
VIDEOS = [
    {"title": ["Getting Started", "with ROCm Optiq"], "file": "Getting Started",
     "chapters": ["01_opening_a_trace", "02_interface_tour", "03_navigating_the_timeline",
                  "04_system_topology_panel"],
     "welcome": INTRO_LINES,
     "farewell": ["Thanks for watching. In the next video, we'll work with tracks, events, and time ranges."],
     "next": "Up next: Tracks, Events, and Time Ranges"},
    {"title": ["Tracks, Events,", "and Time Ranges"], "file": "Tracks, Events, and Time Ranges",
     "chapters": ["05_working_with_tracks", "06_events_and_flows", "07_time_ranges_and_measuring"],
     "welcome": ["Hi, I'm Sajeeth. In this video, we'll work with tracks, events, and time ranges in "
                 "AMD ROCm Optiq."],
     "farewell": ["Thanks for watching. In the next video, we'll explore the Advanced Details tables, "
                  "search, annotations, and ways to customize Optiq."],
     "next": "Up next: Tables, Search, Annotations, and Settings"},
    {"title": ["Tables, Search,", "Annotations, and Settings"], "file": "Tables, Search, Annotations, and Settings",
     "chapters": ["08_advanced_details_tables", "09_search_minimap_and_summary",
                  "10_annotations_bookmarks_and_projects", "11_customizing_optiq"],
     "welcome": ["Hi, I'm Sajeeth. In this video, we'll explore the Advanced Details tables, search, "
                 "annotations and bookmarks, and ways to customize AMD ROCm Optiq."],
     "farewell": ["Thanks for watching. In the next video, we'll analyze ROCm Compute Profiler data."],
     "next": f"Up next: Analyzing AMD ROCm{TM} Compute Profiler Data"},
    {"title": [f"Analyzing AMD ROCm{TM}", "Compute Profiler Data"], "file": "Analyzing ROCm Compute Profiler Data",
     "chapters": ["12_compute_opening_and_summary", "13_compute_kernel_details",
                  "14_compute_tables_workload_and_comparison"],
     "welcome": ["Hi, I'm Sajeeth. In this final video, we'll analyze AMD ROCm Compute Profiler data in "
                 "ROCm Optiq."],
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
    "14_compute_tables_workload_and_comparison": "Table View, Profile Details, and Baseline Comparison",
}
# The short-form disclaimer legal approved for the end of every video, alone on a
# black frame before the outro bumper.
DISCLAIMER = ("Certain AMD technologies may require third-party enablement or activation. Supported "
              "features may vary by operating system. Please confirm with the system manufacturer for "
              "specific features. No technology or product can be completely secure. GD-18b.")
COPYRIGHT = "\u00a9 2026 Advanced Micro Devices, Inc. All rights reserved."

WIDTH, HEIGHT, FPS = 1920, 1080, 60
LEAD_S = 1.2
LINE_GAP_S = 0.45
TAIL_S = 1.0
FAREWELL_TAIL_S = 2.2
LEGAL_S = 8.0
CROSSFADE_S = 0.5
# The shortest chapter YouTube shows in a video's chapter list.
MIN_CHAPTER_S = 10
# Every card is white type on black in one left-aligned column, the lockup under
# its title and the presenter at its foot.
TEXT_X = 160
WHITE = "white"
GREY = "0xB9C0C7"
LOCKUP_H = 190
LOCKUP_GAP = 64
PRESENTER_Y = 892
LEGAL_SIZE = 32
LEGAL_LINE_H = 48
LEGAL_WRAP = 74
# Klavika is the AMD brand typeface; the brand falls back to Arial without it.
FONT_FILES = {
    "light": ("Klavika-Light.otf", "arial.ttf"),
    "regular": ("Klavika-Regular.otf", "arial.ttf"),
    "medium": ("Klavika-Medium.otf", "arialbd.ttf"),
    "bold": ("Klavika-Bold.otf", "arialbd.ttf"),
}
AUDIO_ARGS = ["-c:a", "aac", "-b:a", "192k", "-ar", "48000", "-ac", "2"]


def font_path(weight):
    """The installed file for `weight`, Klavika first, as drawtext wants it."""
    dirs = [Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts"]
    if os.environ.get("LOCALAPPDATA"):
        dirs.append(Path(os.environ["LOCALAPPDATA"]) / "Microsoft" / "Windows" / "Fonts")
    for name in FONT_FILES[weight]:
        for folder in dirs:
            if (folder / name).exists():
                return str(folder / name).replace("\\", "/").replace(":", "\\:")
    sys.exit(f"neither Klavika nor Arial is installed for the {weight} weight")


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
    return text.replace("\\", "\\\\").replace(":", "\\:").replace("'", "\u2019").replace("%", "\\%")


def text_at(weight, text, size, color, y, start, centered=False):
    x = "(w-text_w)/2" if centered else str(TEXT_X)
    fade = f"alpha='min(1,max(0,(t-{start:.2f})/0.7))'"
    return (f"drawtext=fontfile='{font_path(weight)}':text='{escape(text)}':fontcolor={color}:"
            f"fontsize={size}:x={x}:y={y}:{fade}")


def title_rows(video, part, parts):
    """Rows of the title card, and the top of the lockup under them."""
    rows = [("regular", SERIES, 38, GREY, 232, 0.5)]
    y = 292
    for line in video["title"]:
        rows.append(("bold", line, 86, WHITE, y, 0.8))
        y += 104
    rows.append(("light", f"Part {part} of {parts}", 40, GREY, y + 24, 1.1))
    rows.append(("light", "Presented by", 30, GREY, PRESENTER_Y, 1.4))
    rows.append(("medium", PRESENTER, 46, WHITE, PRESENTER_Y + 40, 1.4))
    return rows, y + 24 + 40 + LOCKUP_GAP


def closing_rows(video, part, parts):
    rows = [("regular", SERIES, 38, GREY, 232, 0.4),
            ("bold", "Thanks for watching", 86, WHITE, 292, 0.6),
            ("light", f"Part {part} of {parts}", 40, GREY, 420, 0.8),
            ("regular", video["next"], 36, WHITE, 486, 1.0),
            ("light", "Presented by", 30, GREY, PRESENTER_Y, 1.0),
            ("medium", PRESENTER, 46, WHITE, PRESENTER_Y + 40, 1.0)]
    return rows, 486 + 36 + LOCKUP_GAP


def render_card(ffmpeg, ffprobe, lockup, clips, rows, lockup_y, lockup_start, tail_s, out):
    starts, t = [], LEAD_S
    for clip in clips:
        starts.append(t)
        t += mm.duration(ffprobe, clip) + LINE_GAP_S
    length = t - LINE_GAP_S + tail_s
    text = ",".join(text_at(*row) for row in rows)
    logo_fade = f"fade=t=in:st={lockup_start:.2f}:d=0.7:alpha=1"
    video = (f"[0:v]{text}[base];[1:v]scale=-1:{LOCKUP_H}:flags=lanczos,format=rgba,{logo_fade}[logo];"
             f"[base][logo]overlay=x={TEXT_X}:y={lockup_y}:shortest=1,fade=t=in:st=0:d=1.0")
    delays = "".join(f"[{n + 2}:a]volume={mm.line_gain(ffmpeg, clip):.2f}dB,adelay=delays={int(s * 1000)}:all=1[a{n}];"
                     for n, (clip, s) in enumerate(zip(clips, starts)))
    mix = "".join(f"[a{n}]" for n in range(len(clips)))
    voice = f"{delays}{mix}amix=inputs={len(clips)}:normalize=0:dropout_transition=0,{mm.NARRATION_POLISH}"
    clip_inputs = [arg for clip in clips for arg in ("-i", str(clip))]
    # The loudness pass measures the voice alone; tiny stand-ins keep inputs 0 and 1.
    loudness = mm.loudness_filter(ffmpeg, ["-f", "lavfi", "-t", "0.1", "-i", "anullsrc",
                                           "-f", "lavfi", "-t", "0.1", "-i", "anullsrc", *clip_inputs], voice)
    audio = f"{voice},{loudness},aresample=48000,apad,atrim=0:{length:.3f}"
    inputs = ["-f", "lavfi", "-i", f"color=c=black:s={WIDTH}x{HEIGHT}:r={FPS}:d={length:.3f}",
              "-loop", "1", "-framerate", str(FPS), "-t", f"{length:.3f}", "-i", str(lockup), *clip_inputs]
    graph = f"{video},format=yuv420p[v];{audio}[a]"
    subprocess.run([ffmpeg, "-v", "error", "-y", *inputs, "-filter_complex", graph, "-map", "[v]", "-map", "[a]",
                    "-t", f"{length:.3f}", *video_codec_args(), *AUDIO_ARGS, "-movflags", "+faststart",
                    str(out)], check=True)
    print(f"{out.name}: {length:.1f} s", flush=True)


def render_legal(ffmpeg, out):
    """The disclaimer, centered on black, silent, fading in and out."""
    lines = textwrap.wrap(DISCLAIMER, width=LEGAL_WRAP)
    block_h = (len(lines) + 1) * LEGAL_LINE_H + LEGAL_SIZE
    y = (HEIGHT - block_h) // 2
    rows = []
    for line in lines:
        rows.append(("regular", line, LEGAL_SIZE, WHITE, y, 0.0, True))
        y += LEGAL_LINE_H
    rows.append(("regular", COPYRIGHT, LEGAL_SIZE, GREY, y + LEGAL_SIZE, 0.0, True))
    video = (",".join(text_at(*row) for row in rows) +
             f",fade=t=in:st=0:d=0.5,fade=t=out:st={LEGAL_S - 1.0:.2f}:d=1.0,format=yuv420p")
    subprocess.run([ffmpeg, "-v", "error", "-y",
                    "-f", "lavfi", "-i", f"color=c=black:s={WIDTH}x{HEIGHT}:r={FPS}:d={LEGAL_S}",
                    "-f", "lavfi", "-i", f"anullsrc=r=48000:cl=stereo:d={LEGAL_S}",
                    "-filter_complex", f"[0:v]{video}[v]", "-map", "[v]", "-map", "1:a", "-t", f"{LEGAL_S}",
                    *video_codec_args(), *AUDIO_ARGS, "-movflags", "+faststart", str(out)], check=True)
    print(f"{out.name}: {LEGAL_S:.1f} s", flush=True)


def cards(args, ffmpeg, ffprobe):
    ids = {text: line_id for line_id, text in frame_lines()}
    args.out.mkdir(parents=True, exist_ok=True)
    if not args.lockup.exists():
        sys.exit(f"no lockup image at {args.lockup}")
    if "legal" in args.kinds:
        render_legal(ffmpeg, args.out / "legal.mp4")
    for part, video in enumerate(VIDEOS, 1):
        if args.part and part != args.part:
            continue
        for kind in ("welcome", "farewell"):
            if kind not in args.kinds:
                continue
            clips = [args.clips / f"{ids[text]}.wav" for text in video[kind]]
            missing = [c.name for c in clips if not c.exists()]
            if missing:
                sys.exit(f"part {part} {kind}: missing voiced lines {', '.join(missing)}")
            if kind == "welcome":
                rows, lockup_y = title_rows(video, part, len(VIDEOS))
                render_card(ffmpeg, ffprobe, args.lockup, clips, rows, lockup_y, 1.2, TAIL_S,
                            args.out / f"part{part}_welcome.mp4")
            else:
                rows, lockup_y = closing_rows(video, part, len(VIDEOS))
                render_card(ffmpeg, ffprobe, args.lockup, clips, rows, lockup_y, 1.0, FAREWELL_TAIL_S,
                            args.out / f"part{part}_farewell.mp4")


def has_audio(ffprobe, path):
    out = subprocess.run([ffprobe, "-v", "error", "-select_streams", "a", "-show_entries", "stream=index",
                          "-of", "csv=p=0", str(path)], capture_output=True, text=True, check=True).stdout
    return bool(out.strip())


def stamp(seconds):
    whole = int(seconds)
    return f"{whole // 60}:{whole % 60:02d}"


def join(args, ffmpeg, ffprobe):
    args.out.mkdir(parents=True, exist_ok=True)
    if args.outro and not args.outro.exists():
        sys.exit(f"no outro bumper at {args.outro}")
    if not args.outro:
        print("No outro bumper given: the videos end on the disclaimer. Pass --outro with the AMD "
              "bumper from the brand team to close them with it.")
    for part, video in enumerate(VIDEOS, 1):
        if args.part and part != args.part:
            continue
        parts = ([args.cards / f"part{part}_welcome.mp4"] + [args.voiced / f"{stem}.mp4" for stem in video["chapters"]]
                 + [args.cards / f"part{part}_farewell.mp4", args.cards / "legal.mp4"])
        missing = [p.name for p in parts if not p.exists()]
        if missing:
            sys.exit(f"part {part}: missing {', '.join(missing)}")
        lengths = [mm.duration(ffprobe, p) for p in parts]
        inputs = [arg for p in parts for arg in ("-i", str(p))]
        # One size, frame rate, time base and sample format throughout, as the
        # crossfades and the concat need.
        fit = (f"scale={WIDTH}:{HEIGHT}:force_original_aspect_ratio=decrease,"
               f"pad={WIDTH}:{HEIGHT}:(ow-iw)/2:(oh-ih)/2,setsar=1,fps={FPS},settb=AVTB,format=yuv420p")
        sound = "aresample=48000,aformat=sample_fmts=fltp:channel_layouts=stereo"
        graph = [f"[{n}:v]{fit}[v{n}in];[{n}:a]{sound}[a{n}in]" for n in range(len(parts))]
        v, a, offset, marks = "[v0in]", "[a0in]", 0.0, [0.0]
        for n in range(1, len(parts)):
            offset += lengths[n - 1] - CROSSFADE_S
            marks.append(offset)
            graph.append(f"{v}[v{n}in]xfade=transition=fade:duration={CROSSFADE_S}:offset={offset:.3f}[v{n}]")
            graph.append(f"{a}[a{n}in]acrossfade=d={CROSSFADE_S}:c1=tri:c2=tri[a{n}]")
            v, a = f"[v{n}]", f"[a{n}]"
        if args.outro:
            # The bumper starts on black after the disclaimer fades out, so it is
            # cut in whole rather than crossfaded.
            n = len(parts)
            inputs += ["-i", str(args.outro)]
            if has_audio(ffprobe, args.outro):
                graph.append(f"[{n}:v]{fit}[v{n}in];[{n}:a]{sound}[a{n}in]")
            else:
                outro_s = mm.duration(ffprobe, args.outro)
                graph.append(f"[{n}:v]{fit}[v{n}in];anullsrc=r=48000:cl=stereo:d={outro_s:.3f},{sound}[a{n}in]")
            graph.append(f"{v}{a}[v{n}in][a{n}in]concat=n=2:v=1:a=1[vcat][acat]")
            v, a = "[vcat]", "[acat]"
        # The crossfades work in 4:4:4; the video goes out in 4:2:0 like its parts.
        graph.append(f"{v}format=yuv420p[vout]")
        v = "[vout]"
        name = f"AMD ROCm Optiq Tutorial {part} - {video['file']}"
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
    parser.add_argument("--lockup", type=Path, help="the white AMD ROCm Optiq lockup PNG (cards)")
    parser.add_argument("--voiced", type=Path, help="voiced chapters (join)")
    parser.add_argument("--cards", type=Path, help="rendered cards (join)")
    parser.add_argument("--outro", type=Path, help="the AMD outro bumper video (join)")
    parser.add_argument("--part", type=int, default=0, help="only this video's cards or join")
    parser.add_argument("--kinds", nargs="+", default=["welcome", "farewell", "legal"],
                        choices=("welcome", "farewell", "legal"), help="which cards to render")
    parser.add_argument("--out", type=Path, required=True, help="output folder")
    args = parser.parse_args()
    if args.command == "cards" and (args.clips is None or args.lockup is None):
        sys.exit("cards needs --clips and --lockup")
    ffmpeg, ffprobe = mm.tools()
    {"cards": cards, "join": join}[args.command](args, ffmpeg, ffprobe)


if __name__ == "__main__":
    main()
