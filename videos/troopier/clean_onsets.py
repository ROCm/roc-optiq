"""Removes the mouth noise and hiss the cloned voice leaves before its lines.

VoxCPM continues its voice prompt, so a voiced line opens with what follows the
prompt's last sentence: a lip smack, then about half a second of breath and
room hiss, then the words. Voice detection counts the smack as speech, so
voxcpm_narrate.py keeps all of it. This finds where each part of a line starts
speaking (keeping a short run-in, and consonants that lead into the speech
after brief gaps) and silences any lone click inside a pause.

By default everything before the speech is silenced and every line keeps its
length, for lines a chapter was already recorded to. With --trim it is cut
instead, and the parts of a line voiced separately are joined with a natural
pause, for lines voiced before recording; the recorder then paces the
chapters to the trimmed lengths.

  python clean_onsets.py <clips folder> [--trim] [--out <folder>] [--only 01_02 ...]
"""

import argparse
import wave
from pathlib import Path

import numpy as np

STEP_S = 0.005
LOUD_BELOW_PEAK_DB = 14.0   # speech: this close to the line's typical level...
LOUD_FRAMES = 10            # ...for this many steps in a row
QUIET_ABOVE_NOISE_DB = 8.0  # anything this far above the hiss counts as sound
DEFAULT_NOISE_DB = -48.0
GAP_FRAMES = 20             # a quiet stretch this long ends the speech before it
RUN_IN_FRAMES = 4
FADE_FRAMES = 2
CLICK_MAX_FRAMES = 12       # a sound this short, alone in a pause, is a click
PART_GAP_S = 0.2            # silence voxcpm_narrate.py puts between parts of a line
PART_PAUSE_S = 0.45         # pause between the parts of a trimmed line


def read(path):
    with wave.open(str(path), "rb") as w:
        params = w.getparams()
        raw = np.frombuffer(w.readframes(params.nframes), dtype=np.uint8)
    if params.sampwidth != 3 or params.nchannels != 1:
        raise ValueError(f"{path.name}: expected mono 24-bit PCM")
    b = raw.reshape(-1, 3).astype(np.int32)
    v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
    v = np.where(v & 0x800000, v - 0x1000000, v)
    return v.astype(np.float64) / 8388608.0, params


def write(path, samples, params):
    v = np.clip(np.round(samples * 8388608.0), -8388608, 8388607).astype(np.int32)
    b = np.stack([v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF], axis=1).astype(np.uint8)
    with wave.open(str(path), "wb") as w:
        w.setparams(params._replace(nframes=len(v)))
        w.writeframes(b.tobytes())


def segments(samples, sr):
    """(start, end) sample ranges of the parts of a line, split at the exact
    silence voxcpm_narrate.py inserts between parts."""
    silent = samples == 0.0
    edges = np.flatnonzero(np.diff(np.concatenate([[0], silent.astype(np.int8), [0]])))
    gaps = [(a, b) for a, b in zip(edges[::2], edges[1::2]) if b - a >= PART_GAP_S * sr]
    bounds, start = [], 0
    for a, b in gaps:
        if a > start:
            bounds.append((start, a))
        start = b
    if start < len(samples):
        bounds.append((start, len(samples)))
    return bounds


def analyse(samples, sr):
    """Per part of the line: (part start, part end, sample where it starts
    speaking, [(click start, click end)...]), all in samples."""
    hop = int(STEP_S * sr)
    frames = samples[: len(samples) // hop * hop].reshape(-1, hop)
    level = 20 * np.log10(np.sqrt(np.mean(frames ** 2, axis=1)) + 1e-12)
    loud = level > np.percentile(level, 90) - LOUD_BELOW_PEAK_DB
    parts = []
    for seg_start, seg_end in segments(samples, sr):
        f0, f1 = seg_start // hop, min(seg_end // hop, len(level))
        onset = next((i for i in range(f0, f1 - LOUD_FRAMES) if loud[i:i + LOUD_FRAMES].all()), None)
        if onset is None:
            parts.append((seg_start, seg_end, seg_start, []))
            continue
        lead = level[f0 + int(0.15 / STEP_S): onset - int(0.1 / STEP_S)]
        noise = float(np.median(lead)) if len(lead) >= int(0.2 / STEP_S) else DEFAULT_NOISE_DB
        sound = level > noise + QUIET_ABOVE_NOISE_DB
        start, quiet = onset, 0
        for i in range(onset - 1, f0 - 1, -1):
            if sound[i]:
                start, quiet = i, 0
            else:
                quiet += 1
                if quiet >= GAP_FRAMES:
                    break
        cut = max(f0, start - RUN_IN_FRAMES)
        clicks = []
        i = start
        while i < f1:
            if sound[i]:
                i += 1
                continue
            j = i
            while j < f1 and not sound[j]:
                j += 1
            k = j
            while k < f1 and sound[k]:
                k += 1
            m = k
            while m < f1 and not sound[m]:
                m += 1
            if j - i >= GAP_FRAMES and 0 < k - j <= CLICK_MAX_FRAMES and m - k >= GAP_FRAMES:
                clicks.append((j * hop, k * hop))
            i = k if k > j else j + 1
        parts.append((seg_start, seg_end, cut * hop, clicks))
    return parts


def clean(samples, sr, trim):
    hop = int(STEP_S * sr)
    fade = np.linspace(0.0, 1.0, FADE_FRAMES * hop, endpoint=False)
    out = samples.copy()
    notes = []
    pieces = []
    for seg_start, seg_end, cut, clicks in analyse(samples, sr):
        for a, b in clicks:
            out[a:b] = 0.0
            notes.append(f"silenced a click at {a / sr:.2f} s")
        n = min(len(fade), seg_end - cut)
        out[cut:cut + n] *= fade[:n]
        if trim:
            pieces.append(out[cut:seg_end])
            notes.append(f"cut {(cut - seg_start) / sr:.2f} s before speech")
        else:
            out[seg_start:cut] = 0.0
            notes.append(f"silenced {(cut - seg_start) / sr:.2f} s before speech at {cut / sr:.2f} s")
    if trim and pieces:
        pause = np.zeros(int(PART_PAUSE_S * sr))
        joined = [pieces[0]]
        for piece in pieces[1:]:
            joined += [pause, piece]
        out = np.concatenate(joined)
    return out, notes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("clips", type=Path)
    ap.add_argument("--trim", action="store_true", help="cut the lead-in instead of silencing it")
    ap.add_argument("--out", type=Path, help="write here instead of over the clips")
    ap.add_argument("--only", nargs="*", default=None, metavar="ID")
    args = ap.parse_args()
    out = args.out or args.clips
    out.mkdir(parents=True, exist_ok=True)
    for path in sorted(args.clips.glob("*.wav")):
        if args.only is not None and path.stem not in args.only:
            continue
        samples, params = read(path)
        cleaned, notes = clean(samples, params.framerate, args.trim)
        write(out / path.name, cleaned, params)
        length = f" ({len(samples) / params.framerate:.2f} -> {len(cleaned) / params.framerate:.2f} s)" \
            if args.trim else ""
        print(f"{path.stem}{length}: {'; '.join(notes) if notes else 'no speech found, unchanged'}")


if __name__ == "__main__":
    main()
