"""Voices suspect lines again and keeps, per line, the take Whisper hears closest
to the line's text: the current take, other seeds, or the text starting as its own
sentence (VoxCPM joins the prompt's transcript and the line without a space).

Runs beside voxcpm_narrate.py, in the voice-clone project's environment, on the
lines hear_all.py flags:
  [VOICE_STEPS=30 VOICE_CFG=2.0] python revoice_best.py lines.json clips voices/conversational_clean.json 03_07 ...
"""

import json
import os
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from faster_whisper import WhisperModel

import voxcpm_narrate as vn
from hear_all import edit_distance, words

SEEDS = (7, 1234, 99)
TAKE = Path("/tmp/revoice_take.wav")
STEPS = int(os.environ.get("VOICE_STEPS", "30"))
CFG = float(os.environ.get("VOICE_CFG", "2.0"))


def main():
    lines = {line["id"]: line for line in json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))}
    clips, voice_path, ids = Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4:]
    voice = vn.vl.load_voice(voice_path)
    cache_path = clips / "cache.json"
    cache = json.loads(cache_path.read_text(encoding="utf-8"))
    asr = WhisperModel("large-v3", device="cpu", compute_type="int8",
                       cpu_threads=int(os.environ.get("ASR_THREADS", "24")))
    tts = vn.vl.load_tts()
    sr = tts.tts_model.sample_rate

    def heard_errors(wav, text):
        sf.write(TAKE, wav, sr)
        segments, _ = asr.transcribe(str(TAKE), language="en", beam_size=5, condition_on_previous_text=False)
        heard = " ".join(s.text.strip() for s in segments)
        return edit_distance(words(text), words(heard)), heard

    for line_id in ids:
        line = lines[line_id]
        current, _ = sf.read(clips / f"{line_id}.wav", dtype="float32")
        errors, heard = heard_errors(current, line["text"])
        best = {"errors": errors, "heard": heard, "wav": current, "take": "current"}
        print(f"{line_id} current: {errors} off  | {heard}", flush=True)
        spoken = line["spoken"]
        own_sentence = " " + spoken[0].upper() + spoken[1:]
        variants = [(spoken, seed, "") for seed in SEEDS] + \
                   [(own_sentence, seed, " as a sentence") for seed in (42,) + SEEDS]
        for text, seed, how in variants:
            if best["errors"] == 0:
                break
            wav = vn.speech_only(np.asarray(vn.vl.synthesize(tts, text, voice, seed=seed, cfg_value=CFG,
                                                             inference_timesteps=STEPS), dtype=np.float32), sr)
            errors, heard = heard_errors(wav, line["text"])
            print(f"{line_id} seed {seed}{how}: {errors} off  | {heard}", flush=True)
            if errors < best["errors"]:
                best = {"errors": errors, "heard": heard, "wav": wav, "take": f"seed {seed}{how}"}
        if best["take"] != "current":
            sf.write(clips / f"{line_id}.wav", best["wav"], sr, subtype="PCM_24")
            # Another process may be voicing other lines into the same folder.
            cache = json.loads(cache_path.read_text(encoding="utf-8"))
            cache[line_id]["seconds"] = round(len(best["wav"]) / sr, 3)
            cache[line_id]["take"] = best["take"]
            cache_path.write_text(json.dumps(cache, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"{line_id} KEPT {best['take']} ({best['errors']} off): {best['heard']}", flush=True)


if __name__ == "__main__":
    main()
