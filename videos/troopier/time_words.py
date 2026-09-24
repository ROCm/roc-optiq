#!/usr/bin/env python3
"""Times the words of voiced lines for the tutorial recorder, and checks them.

Runs beside voxcpm_narrate.py, in the voice-clone project's environment.
Transcribes each voiced line (<clips>/<id>.wav) with Whisper, adds a row
"<text>\t<word>@<seconds> ..." per line to <clips>/words.tsv (the recorder
points at what a line names as it says it), and lists lines whose words differ
from their text, after allowing for how names and terms are spelled.

  python time_words.py lines.json clips [--only 00_01 02_02 ...]
"""

import argparse
import json
import re
from pathlib import Path

from faster_whisper import WhisperModel

# Spellings Whisper may choose for a word the voice said right, mapped to one form.
CANON = [
    (r"\brock[- ']?[eu]m\b|\bro(?:c?k+h?)[aiu]?m\b|\brokom\b|\bra?ckham\b|\brocm\b", "rocm"),
    (r"\bopti[ckq]\b", "optiq"),
    (r"\bsaa?j[ae]?[ey]?[ei]?th+\b|\bsajith\b|\bsajid\b|\bsajeet\b", "sajeeth"),
    (r"\b[wv][iau]m+[aeo]l[ae][- ]?s[uoa]+r[iy]*[ae]?n\b", "wimalasuriyan"),
    (r"\bhip ?mem ?copy\b|\bhipmemcpy\b", "hipmemcpy"),
    (r"\bokay\b", "ok"),
    (r"\bcue\b", "queue"),
    (r"\bctrl\b", "control"),
    (r"\bmini-?map\b", "minimap"),
    (r"\broof line\b", "roofline"),
    (r"\bwave fronts\b", "wavefronts"),
    (r"\bopentrace\b", "open trace"),
]
NUMBERS = {"zero": "0", "one": "1", "two": "2", "three": "3", "four": "4", "five": "5", "six": "6",
           "seven": "7", "eight": "8", "nine": "9", "ten": "10"}


def words(text):
    text = text.lower()
    for pattern, replacement in CANON:
        text = re.sub(pattern, replacement, text)
    text = re.sub(r"[^a-z0-9%.' ]+", " ", text).replace(". ", " ")
    return [NUMBERS.get(w.strip(".'"), w.strip(".'")) for w in text.split() if w.strip(".'")]


def edit_distance(a, b):
    row = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        previous, row[0] = row[0], i
        for j, y in enumerate(b, 1):
            previous, row[j] = row[j], min(row[j] + 1, row[j - 1] + 1, previous + (x != y))
    return row[len(b)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("lines", type=Path, help="lines.json from voice_masters.py")
    ap.add_argument("clips", type=Path, help="folder with the voiced lines")
    ap.add_argument("--only", nargs="*", default=None, metavar="ID", help="line ids to time")
    args = ap.parse_args()

    lines = [line for line in json.loads(args.lines.read_text(encoding="utf-8"))
             if args.only is None or line["id"] in args.only]
    index = args.clips / "words.tsv"
    rows = {}
    if index.exists():
        for row in index.read_text(encoding="utf-8").splitlines():
            text, _, timed = row.partition("\t")
            rows[text] = timed
    model = WhisperModel("large-v3", device="cpu", compute_type="int8", cpu_threads=24)
    flagged = 0
    for line in lines:
        segments, _ = model.transcribe(str(args.clips / f"{line['id']}.wav"), language="en", beam_size=5,
                                       word_timestamps=True, condition_on_previous_text=False)
        spoken = [w for s in segments for w in s.words]
        rows[line["text"]] = " ".join(f"{w.word.strip().replace(' ', '')}@{w.start:.2f}" for w in spoken)
        heard = " ".join(w.word.strip() for w in spoken)
        errors = edit_distance(words(line["text"]), words(heard))
        if errors:
            flagged += 1
            print(f"{line['id']}  {errors} word(s) differ\n  text:  {line['text']}\n  heard: {heard}", flush=True)
        else:
            print(f"{line['id']}  ok", flush=True)
    index.write_text("".join(f"{text}\t{timed}\n" for text, timed in rows.items()), encoding="utf-8")
    print(f"done: {flagged} of {len(lines)} lines differ; {len(rows)} lines timed in {index}")


if __name__ == "__main__":
    main()
