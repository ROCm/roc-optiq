"""Transcribes every voiced line with Whisper large-v3 and lists the lines whose
words differ from their text, after normalizing how names and terms are spelled.

Runs beside voxcpm_narrate.py, in the voice-clone project's environment:
  python hear_all.py lines.json clips
"""

import json
import re
import sys
from pathlib import Path

from faster_whisper import WhisperModel

# Spellings Whisper may choose for a word the voice said right, mapped to one form.
CANON = [
    (r"\ba\.? ?m\.? ?d\b\.?|\ba-m-d\b", "amd"),
    (r"\bi\.? ?s\.? ?a\b\.?|\bi-s-a\b", "isa"),
    (r"\bp\.? ?c\b\.?|\bp-c\b", "pc"),
    (r"\brock[- ']?[eu]m\b|\bro(?:c?k+h?)[aiu]?m\b|\brokom\b|\bra?ckham\b|\brocm\b", "rocm"),
    (r"\bopti[ckq]\b", "optiq"),
    (r"\bsaa?j[ae]?[ey]?[ei]?th+\b|\bsajith\b|\bsajid\b|\bsajeet\b", "sajeeth"),
    (r"\b[wv][iau]m+[aeo]l[ae][- ]?s[uoa]+r[iy]*[ae]?n\b", "wimalasuriyan"),
    (r"\bcue\b", "queue"),
    (r"\bctrl\b", "control"),
    (r"\bmini-?map\b", "minimap"),
    (r"\broof line\b", "roofline"),
    (r"\bwave fronts\b", "wavefronts"),
    (r"\bhip ?mem ?copy\b|\bhipmemcpy\b", "hipmemcpy"),
    (r"\bokay\b", "ok"),
    (r"\ba-p-i\b|\bapi\b", "api"),
    (r"\bi-ds\b|\bids\b", "ids"),
    (r"\bi-d\b", "id"),
    (r"\bg-f-x\b", "gfx"),
    (r"\bl-two\b|\bl 2\b", "l2"),
    (r"\bsequel\b", "sql"),
    (r"\bopentrace\b", "open trace"),
    (r"\bay and dee\b", "a and d"),
    (r"\bzee key\b", "z key"),
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
    lines = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    clips = Path(sys.argv[2])
    model = WhisperModel("large-v3", device="cpu", compute_type="int8", cpu_threads=24)
    flagged = 0
    for line in lines:
        segments, _ = model.transcribe(str(clips / f"{line['id']}.wav"), language="en", beam_size=5,
                                       condition_on_previous_text=False)
        heard = " ".join(s.text.strip() for s in segments)
        said, got = words(line["text"]), words(heard)
        errors = edit_distance(said, got)
        if errors:
            flagged += 1
            print(f"{line['id']}  {errors} word(s) differ\n  text:  {line['text']}\n  heard: {heard}", flush=True)
        else:
            print(f"{line['id']}  ok", flush=True)
    print(f"done: {flagged} of {len(lines)} lines differ")


if __name__ == "__main__":
    main()
