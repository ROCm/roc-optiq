"""Turns the recorded chapters into silent masters and timestamped scripts.

For every chapter record_tutorials.ps1 wrote into raw/, this
  - checks that every narration line of the chapter script ran; a missing line
    means a step was skipped on camera,
  - encodes the lossless recording once into a 1080p60 H.264 master (no audio),
  - writes the chapter's narration with the time each line starts, spelled the
    way a voice should say it, ready for Trupeer's script editor or any other
    text-to-speech tool.

Usage: python make_masters.py --videos <folder> --scripts <folder> [--chapter 03]
       [--check-only | --scripts-only]
"""

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
CHAPTERS_CPP = REPO / "src" / "app" / "test" / "tutorial_chapters.cpp"

SAY = re.compile(r'\bSay\(\s*((?:"(?:[^"\\\n]|\\.)*"\s*)+)\)', re.S)
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
CHAPTER_FUNC = re.compile(r"^(chapter_\w+)\(ImGuiTestContext\* ctx\)", re.M)
REGISTER = re.compile(r'register_chapter\(\s*engine,\s*"(\w+)",\s*(\w+)\s*\)')

VIDEO_ARGS = [
    "-vf", "scale=out_color_matrix=bt709:out_range=tv:flags=lanczos+accurate_rnd+full_chroma_int,"
           "format=yuv420p,setparams=color_primaries=bt709:color_trc=bt709:colorspace=bt709:range=tv",
    "-c:v", "libx264", "-preset", "slow", "-crf", "12", "-profile:v", "high", "-level:v", "4.2",
    "-g", "30", "-keyint_min", "30", "-bf", "2", "-flags", "+cgop",
    "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709", "-color_range", "tv",
    "-an", "-movflags", "+faststart",
]

HEADER = """{title}
Video: {video} ({length})

Each numbered section is one line of narration, with the stretch of the video
it covers: it starts at the first time, and the next line starts at the second.
Words a voice would get wrong are spelled the way they're said (Rock-em for
ROCm, Optic for Optiq, Wimala-suriyan for Wimalasuriyan), so paste the text
as it is.
"""

# How the narrator says words a text-to-speech voice gets wrong, applied to the
# script text only; the footage keeps the real spellings.
SPOKEN = [
    (r"\bROCm\b", "Rock-em"),
    # Said as one flowing name; as two words the voice stresses each like a separate name.
    (r"\bWimalasuriyan\b", "Wimala-suriyan"),
    (r"\bOptiq\b", "Optic"),
    (r"\bhipMemcpy\b", "hip mem copy"),
    (r"\bHIP\b", "hip"),
    (r"\bAPI\b", "A-P-I"),
    (r"\bIDs\b", "I-Ds"),
    (r"\bID\b", "I-D"),
    (r"\bGFX\b", "G-F-X"),
    (r"\bL2\b", "L-two"),
    (r"\bOK\b", "okay"),
    (r"\bSQL\b", "sequel"),
    (r"\bLIKE\b", "like"),
    (r"\bZ key\b", "Zee key"),
]


# A light broadcast chain for the voiced narration: low cut, less boxiness, a
# touch of presence, and gentle de-essing. No compression: it made the voice
# score less natural and lifted the room tone between words.
NARRATION_POLISH = ("highpass=f=75,equalizer=f=250:t=q:w=1.0:g=-1.5,equalizer=f=3500:t=q:w=1.2:g=1.5,"
                    "deesser=i=0.35")
# Every voiced line is first brought to one level, as the takes vary by several dB.
LINE_LUFS = -23.0
# The narration is then raised to this loudness, and a fast limiter shaves the
# sparse peaks that pass PEAK_LIMIT; that leaves it near -18 LUFS. Pushing it
# louder took more limiting, which made the voice score less natural.
LOUDNESS_LUFS = -16.0
PEAK_LIMIT = 0.8  # -1.9 dBFS


def loudness(ffmpeg, inputs, graph):
    """Integrated loudness of the audio `graph` produces from `inputs`."""
    err = subprocess.run([ffmpeg, "-hide_banner", "-nostats", *inputs, "-filter_complex",
                          f"{graph},loudnorm=print_format=json", "-f", "null", "-"],
                         capture_output=True, text=True, check=True).stderr
    return float(json.loads(err[err.rindex("{"):err.rindex("}") + 1])["input_i"])


def line_gain(ffmpeg, clip):
    """Gain in dB that brings one voiced line to LINE_LUFS."""
    return LINE_LUFS - loudness(ffmpeg, ["-i", str(clip)], "[0:a]anull")


def loudness_filter(ffmpeg, inputs, graph):
    """A fixed gain to LOUDNESS_LUFS for the audio `graph` produces from
    `inputs`, measured in a first pass, then the peak limiter. (loudnorm would
    ride the gain whenever a fixed one would clip.)"""
    return (f"volume={LOUDNESS_LUFS - loudness(ffmpeg, inputs, graph):.2f}dB,"
            f"alimiter=limit={PEAK_LIMIT}:attack=2:release=40:asc=0:level=disabled")


def unescape(literal):
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t"}.get(m[1], m[1]), literal)


def script_lines():
    """Returns {chapter: [line, ...]} in the order the chapter script says them."""
    code = CHAPTERS_CPP.read_text(encoding="utf-8")
    stems = {func: stem for stem, func in REGISTER.findall(code)}
    starts = [(m.start(), stems.get(m[1], m[1])) for m in CHAPTER_FUNC.finditer(code)]
    lines = {}
    for say in SAY.finditer(code):
        chapter = next((stem for start, stem in reversed(starts) if start < say.start()), None)
        if chapter:
            lines.setdefault(chapter, []).append("".join(unescape(p) for p in LITERAL.findall(say[1])))
    return lines


def as_spoken(text):
    for pattern, replacement in SPOKEN:
        text = re.sub(pattern, replacement, text)
    return text


def spoken_lines(stem, raw):
    cues = []
    for row in (raw / "timing" / f"{stem}.narration.tsv").read_text(encoding="utf-8").splitlines():
        start, _, text = row.partition("\t")
        if text:
            cues.append((float(start), text))
    return cues


def clock(seconds):
    total = int(round(seconds))
    return f"{total // 60}:{total % 60:02d}"


def duration(ffprobe, path):
    out = subprocess.run([ffprobe, "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0",
                          str(path)], capture_output=True, text=True, check=True).stdout
    return float(out.strip())


def tools():
    """(ffmpeg, ffprobe) paths, from PATH or the winget install."""
    ffmpeg = shutil.which("ffmpeg") or next(
        (str(p) for p in (Path.home() / "AppData/Local/Microsoft/WinGet/Packages").glob("**/ffmpeg.exe")), None)
    if ffmpeg is None:
        sys.exit("ffmpeg was not found")
    return ffmpeg, str(Path(ffmpeg).with_name("ffprobe" + Path(ffmpeg).suffix))


def title_of(stem):
    number, _, name = stem.partition("_")
    words = name.replace("_", " ")
    return f"ROCm Optiq tutorial {int(number)}: {words[0].upper()}{words[1:]}"


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--raw", type=Path, default=HERE / "raw")
    parser.add_argument("--videos", type=Path, required=True, help="folder for the silent masters")
    parser.add_argument("--scripts", type=Path, required=True, help="folder for the scripts")
    parser.add_argument("--chapter", default="", help="only chapters starting with this, like 03")
    parser.add_argument("--check-only", action="store_true", help="only check that every line ran")
    parser.add_argument("--scripts-only", action="store_true",
                        help="rewrite the scripts for masters that are already encoded")
    args = parser.parse_args()

    ffmpeg, ffprobe = tools()
    expected = script_lines()
    problems = 0
    for raw_video in sorted(args.raw.glob(f"{args.chapter}*.mkv")):
        stem = raw_video.stem
        cues = spoken_lines(stem, args.raw)
        said = [text for _, text in cues]
        missing = [line for line in expected.get(stem, []) if line not in said]
        for line in missing:
            print(f"{stem}: SKIPPED STEP, this line never ran: {line}")
        problems += len(missing)
        if args.check_only:
            print(f"{stem}: {len(said)} of {len(expected.get(stem, []))} lines ran")
            continue

        args.videos.mkdir(parents=True, exist_ok=True)
        args.scripts.mkdir(parents=True, exist_ok=True)
        master = args.videos / f"{stem}.mp4"
        if not args.scripts_only:
            subprocess.run([ffmpeg, "-v", "error", "-y", "-i", str(raw_video), *VIDEO_ARGS, str(master)],
                           check=True)
        elif not master.exists():
            print(f"{stem}: no master to write a script for")
            problems += 1
            continue
        length = duration(ffprobe, master)
        ends = [start for start, _ in cues[1:]] + [length]
        sections = [f"{n}. [{clock(start)} - {clock(end)}]\n{as_spoken(text)}"
                    for n, ((start, text), end) in enumerate(zip(cues, ends), start=1)]
        script = HEADER.format(title=title_of(stem), video=master.resolve(), length=clock(length))
        (args.scripts / f"{stem}.txt").write_text(script + "\n" + "\n\n".join(sections) + "\n", encoding="utf-8")
        print(f"{stem}: {clock(length)}, {len(cues)} lines, {master.stat().st_size / 2**20:.0f} MB", flush=True)
    if problems:
        sys.exit(f"{problems} narrated steps were skipped; see above")


if __name__ == "__main__":
    main()
