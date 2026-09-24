#!/usr/bin/env python3
"""Voices the tutorial narration with the cloned VoxCPM2 voice.

Runs on the machine with the voice-clone project (~/projects/voice-clone), in its
virtual environment, and uses its helpers and its voice (data/voice.json, or the
voice file given with --voice). Reads the lines voice_masters.py wrote and saves
one 48 kHz WAV per line as <out>/<id>.wav, cut to its speech, plus <out>/lines.tsv
with each line's length and on-screen text for the tutorial recorder. Finished
lines are cached, so a rerun only voices lines whose text or voice changed, plus
any listed with --redo (pass a different --seed for another take).

  python voxcpm_narrate.py lines.json clips [--voice voice.json] [--only 01] [--redo 01_03 --seed 7]
"""

import argparse
import hashlib
import json
import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf
from faster_whisper.vad import VadOptions, get_speech_timestamps

VOICE_CLONE = Path.home() / "projects" / "voice-clone"
sys.path.insert(0, str(VOICE_CLONE))

import voicelib as vl  # noqa: E402
from narrate import chunk_script  # noqa: E402

# Silence between the sentences of a line voiced in parts.
PART_PAUSE_S = 0.25

# VoxCPM continues the voice prompt, so every part it voices opens with the pause
# and breath that follow the prompt's last sentence, and may trail off in the
# room tone of the prompt's recording. Silero VAD finds the speech between them;
# the part keeps a little on either side, faded in and out. SPEECH_CUT is part
# of every line's cache key: change it when the cut changes.
SPEECH_CUT = "silero-vad-1"
VAD_RATE = 16000
VAD = VadOptions(threshold=0.5, min_speech_duration_ms=100, min_silence_duration_ms=100, speech_pad_ms=0)
LEAD_IN_S = 0.08
FADE_IN_S = 0.05
TAIL_S = 0.12
FADE_OUT_S = 0.04


def speech_only(wav, sr):
    speech = get_speech_timestamps(vl.to_16k(wav, sr), VAD)
    if not speech:
        return wav
    start = max(int((speech[0]["start"] / VAD_RATE - LEAD_IN_S) * sr), 0)
    end = min(int((speech[-1]["end"] / VAD_RATE + TAIL_S) * sr), len(wav))
    wav = np.array(wav[start:end], dtype=np.float32)
    fade_in = min(int(FADE_IN_S * sr), len(wav))
    fade_out = min(int(FADE_OUT_S * sr), len(wav))
    wav[:fade_in] *= 0.5 - 0.5 * np.cos(np.linspace(0.0, np.pi, fade_in, dtype=np.float32))
    wav[len(wav) - fade_out:] *= 0.5 + 0.5 * np.cos(np.linspace(0.0, np.pi, fade_out, dtype=np.float32))
    return wav


def line_key(spoken, setting):
    return hashlib.sha1(json.dumps([spoken, setting], sort_keys=True).encode()).hexdigest()[:16]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("lines", type=Path, help="lines.json from voice_masters.py")
    ap.add_argument("out", type=Path, help="folder for the voiced lines")
    ap.add_argument("--voice", type=Path, default=vl.VOICE,
                    help="voice file: name, prompt_wav + prompt_text and/or reference_wav")
    ap.add_argument("--only", default="", help="only lines whose id starts with this, like 01")
    ap.add_argument("--redo", nargs="*", default=[], metavar="ID", help="line ids to voice again")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--timesteps", type=int, default=10, help="diffusion steps per frame")
    ap.add_argument("--cfg", type=float, default=2.0, help="guidance scale")
    args = ap.parse_args()

    lines = json.loads(args.lines.read_text(encoding="utf-8"))
    voice = vl.load_voice(args.voice)
    lora = voice.get("lora")
    setting = {"voice": voice["name"], "lora": lora, "timesteps": args.timesteps, "cfg": args.cfg,
               "cut": SPEECH_CUT}
    args.out.mkdir(parents=True, exist_ok=True)
    cache_path = args.out / "cache.json"
    cache = json.loads(cache_path.read_text(encoding="utf-8")) if cache_path.exists() else {}

    todo = [line for line in lines if line["id"].startswith(args.only) and (
        line["id"] in args.redo or cache.get(line["id"], {}).get("key") != line_key(line["spoken"], setting)
        or not (args.out / f"{line['id']}.wav").exists())]

    # A line that moved to another id, or whose text another line already has,
    # keeps the take voiced for that text instead of being voiced anew.
    takes = {}
    for line_id, entry in cache.items():
        path = args.out / f"{line_id}.wav"
        if path.exists():
            takes.setdefault(entry["key"], (path.read_bytes(), dict(entry)))
    reused = []
    for line in [line for line in todo if line["id"] not in args.redo]:
        take = takes.get(line_key(line["spoken"], setting))
        if take is not None:
            (args.out / f"{line['id']}.wav").write_bytes(take[0])
            cache[line["id"]] = dict(take[1])
            reused.append(line["id"])
    if reused:
        cache_path.write_text(json.dumps(cache, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"{len(reused)} lines keep takes voiced for the same text: {' '.join(reused)}", flush=True)
    todo = [line for line in todo if line["id"] not in reused]
    print(f"{len(todo)} of {len(lines)} lines to voice (voice {voice['name']}"
          + (f", LoRA {lora})" if lora else ")"), flush=True)

    tts = vl.load_tts(lora=lora) if todo else None
    started = time.time()
    for done, line in enumerate(todo, start=1):
        sr = tts.tts_model.sample_rate
        parts = []
        for _, sentence in chunk_script(line["spoken"]):
            wav = vl.synthesize(tts, sentence, voice, seed=args.seed, cfg_value=args.cfg,
                                inference_timesteps=args.timesteps)
            if parts:
                parts.append(np.zeros(int(sr * PART_PAUSE_S), dtype=np.float32))
            parts.append(speech_only(np.asarray(wav, dtype=np.float32), sr))
        wav = np.concatenate(parts)
        sf.write(args.out / f"{line['id']}.wav", wav, sr, subtype="PCM_24")
        seconds = len(wav) / sr
        cache[line["id"]] = {"key": line_key(line["spoken"], setting), "seconds": round(seconds, 3),
                             "seed": args.seed, "spoken": line["spoken"]}
        cache_path.write_text(json.dumps(cache, indent=1, ensure_ascii=False), encoding="utf-8")
        slot = line["end"] - line["start"]
        flag = "  LONGER THAN ITS SLOT" if 0 < slot < seconds else ""
        print(f"[{done}/{len(todo)}] {line['id']} {seconds:5.1f} s of {slot:5.1f} s{flag}  {line['spoken']}",
              flush=True)

    rows = [f"{cache[line['id']]['seconds']:.3f}\t{line['text']}" for line in lines
            if line["id"] in cache and (args.out / f"{line['id']}.wav").exists()]
    (args.out / "lines.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")
    if todo:
        print(f"voiced {len(todo)} lines in {time.time() - started:.0f} s; {len(rows)} lines in lines.tsv")


if __name__ == "__main__":
    main()
