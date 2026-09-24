"""Adds a narration track to the silent tutorial masters.

  lines  writes every narration line of the recorded chapters, as shown and as a
         voice should say it, with the stretch of the video it has, to a JSON
         file for voxcpm_narrate.py. With --from-script, the lines come from the
         chapter script and the introduction instead, so they can be voiced
         before the chapters are recorded.
  mix    lays each voiced line (<clips>/<id>.wav) over its master at the time the
         line starts, each brought to the same level, polishes the narration
         (make_masters.NARRATION_POLISH), sets its loudness from a first pass,
         and writes the voiced masters.
         Lines longer than their stretch are listed: they would run into the
         next line, so that chapter needs recording again with the real line
         lengths (record_tutorials.ps1 -Narration <clips>).

Usage: python voice_masters.py lines (--masters <folder> | --from-script) --out lines.json
       python voice_masters.py mix --masters <folder> --clips <folder> --out <folder> [--chapter 03]
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import make_final
import make_masters as mm

# A line may run this close to the start of the next one.
MIN_GAP_S = 0.15


def chapters(masters, chapter):
    return sorted(p.stem for p in masters.glob(f"{chapter}*.mp4"))


def stretches(stem, raw, master, ffprobe):
    """(id, start, end, text) for each line, ending where the next begins."""
    cues = mm.spoken_lines(stem, raw)
    ends = [start for start, _ in cues[1:]] + [mm.duration(ffprobe, master)]
    return [(f"{stem[:2]}_{n:02d}", start, end, text)
            for n, ((start, text), end) in enumerate(zip(cues, ends), start=1)]


def clip_ids(clips, stem, texts):
    """Ids of the clips voiced from `texts`, in order, from <clips>/cache.json, or
    None without a match for each. Clips are numbered by the chapter script, where
    a line of a branch the recording skipped still has one, so counting the lines
    the recording said would pick its neighbour's clip."""
    cache_path = clips / "cache.json"
    if not cache_path.exists():
        return None
    cache = json.loads(cache_path.read_text(encoding="utf-8"))
    chapter = sorted(line_id for line_id in cache if line_id.startswith(f"{stem[:2]}_"))
    ids, cursor = [], 0
    for text in texts:
        spoken = mm.as_spoken(text)
        found = next((i for i in range(cursor, len(chapter)) if cache[chapter[i]].get("spoken") == spoken), None)
        if found is None:
            return None
        ids.append(chapter[found])
        cursor = found + 1
    return ids


def script_stretches():
    """(stem, id, text) for the lines of the videos' cards and every line of the chapter script, in order."""
    yield from ((make_final.INTRO_STEM, line_id, text) for line_id, text in make_final.frame_lines())
    for stem, texts in sorted(mm.script_lines().items()):
        yield from ((stem, f"{stem[:2]}_{n:02d}", text) for n, text in enumerate(texts, 1))


def write_lines(args, ffprobe):
    lines = []
    if args.from_script:
        for stem, line_id, text in script_stretches():
            if stem.startswith(args.chapter):
                lines.append({"id": line_id, "chapter": stem, "start": 0.0, "end": 0.0,
                              "text": text, "spoken": mm.as_spoken(text)})
    for stem in [] if args.from_script else chapters(args.masters, args.chapter):
        for line_id, start, end, text in stretches(stem, args.raw, args.masters / f"{stem}.mp4", ffprobe):
            lines.append({"id": line_id, "chapter": stem, "start": round(start, 3), "end": round(end, 3),
                          "text": text, "spoken": mm.as_spoken(text)})
    args.out.write_text(json.dumps(lines, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"{len(lines)} lines from {len({l['chapter'] for l in lines})} chapters written to {args.out}")


def mix(args, ffmpeg, ffprobe):
    args.out.mkdir(parents=True, exist_ok=True)
    overruns = 0
    for stem in chapters(args.masters, args.chapter):
        master = args.masters / f"{stem}.mp4"
        placed = []
        lines = stretches(stem, args.raw, master, ffprobe)
        ids = clip_ids(args.clips, stem, [text for _, _, _, text in lines])
        if ids is None:
            print(f"{stem}: no voiced clip matches every line; numbering the clips by the lines said")
            ids = [line_id for line_id, _, _, _ in lines]
        for line_id, (_, start, end, text) in zip(ids, lines):
            clip = args.clips / f"{line_id}.wav"
            if not clip.exists():
                sys.exit(f"{stem}: no voiced line {clip.name} for: {text}")
            length = mm.duration(ffprobe, clip)
            if start + length > end - MIN_GAP_S:
                print(f"{line_id}: {length:.1f} s of voice for {end - start:.1f} s of video: {text}")
                overruns += 1
            placed.append((clip, start))
        inputs = [arg for clip, _ in placed for arg in ("-i", str(clip))]
        delays = "".join(f"[{n}:a]volume={mm.line_gain(ffmpeg, clip):.2f}dB,"
                         f"adelay=delays={int(start * 1000)}:all=1[a{n}];"
                         for n, (clip, start) in enumerate(placed, start=1))
        joined = "".join(f"[a{n}]" for n in range(1, len(placed) + 1))
        voice = (f"{delays}{joined}amix=inputs={len(placed)}:normalize=0:dropout_transition=0,"
                 f"{mm.NARRATION_POLISH}")
        loudness = mm.loudness_filter(ffmpeg, ["-i", str(master), *inputs], voice)
        graph = f"{voice},{loudness},aresample=48000,apad[voice]"
        subprocess.run([ffmpeg, "-v", "error", "-y", "-i", str(master), *inputs, "-filter_complex", graph,
                        "-map", "0:v", "-map", "[voice]", "-c:v", "copy", "-c:a", "aac", "-b:a", "192k",
                        "-ar", "48000", "-shortest", "-movflags", "+faststart", str(args.out / master.name)],
                       check=True)
        print(f"{stem}: {len(placed)} lines voiced", flush=True)
    if overruns:
        sys.exit(f"{overruns} lines run into the next one; record those chapters again with the real lengths")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=("lines", "mix"))
    parser.add_argument("--raw", type=Path, default=mm.HERE / "raw")
    parser.add_argument("--masters", type=Path, help="folder with the silent masters")
    parser.add_argument("--from-script", action="store_true",
                        help="lines: take the lines from the chapter script and the introduction "
                             "instead of from recorded chapters, before recording")
    parser.add_argument("--clips", type=Path, help="folder with the voiced lines (mix)")
    parser.add_argument("--out", type=Path, required=True, help="lines.json (lines) or a folder (mix)")
    parser.add_argument("--chapter", default="", help="only chapters starting with this, like 03")
    args = parser.parse_args()

    ffmpeg, ffprobe = mm.tools()
    if args.command == "lines":
        write_lines(args, ffprobe)
    elif args.clips is None:
        sys.exit("mix needs --clips")
    else:
        mix(args, ffmpeg, ffprobe)


if __name__ == "__main__":
    main()
