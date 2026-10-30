"""Renders scripted terminal footage, paced to synthesized narration.

A Take types commands into a terminal drawn at 1080p60 and replays the output
systems_capture.sh saved from real runs, line by line at the pace it appeared.
Like the Optiq recorder's Director, every step is paced to the narration: a line
starts once the previous one has finished (lengths from <narration>/lines.tsv,
estimated until the lines are voiced), and a command can wait for the word that
names it (<narration>/words.tsv). It writes what the recorder writes, so
make_masters.py and voice_masters.py take it as they are: a lossless
<out>/<chapter>.mkv, and <out>/timing/<chapter>.txt and .narration.tsv.
"""

import random
import re
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT, FPS = 1920, 1080, 60
COLS, ROWS = 132, 33
TAB_STOP = 8
FONT_SIZE = 24
LINE_H = 30
MARGIN_X = 26
TITLE_H = 48
TOP = TITLE_H + 14
FONT_DIR = Path("C:/Windows/Fonts")
MONO_FONTS = ("CascadiaMono.ttf", "consola.ttf")
TITLE_FONTS = ("Klavika-Regular.otf", "arial.ttf")

BACKGROUND = (12, 12, 12)
TITLE_BAR = (31, 31, 31)
TITLE_TEXT = (170, 176, 182)
TEXT = (204, 204, 204)
PROMPT_USER = (99, 199, 64)
PROMPT_PATH = (97, 152, 236)
CURSOR = (204, 204, 204)
SPOTLIGHT = (255, 140, 0)
DIM_ALPHA = 0.45
# The Campbell palette, plain and bright, for SGR colors 30-37 and 90-97.
PALETTE = [(12, 12, 12), (197, 15, 31), (19, 161, 14), (193, 156, 0),
           (0, 55, 218), (136, 23, 152), (58, 150, 221), (204, 204, 204)]
BRIGHT = [(118, 118, 118), (231, 72, 86), (22, 198, 12), (249, 241, 165),
          (59, 120, 255), (180, 0, 158), (97, 214, 214), (242, 242, 242)]

PROMPT_HOST = "user@demo-node"

TYPING_CPS = 17.0
TYPING_JITTER = 0.35
ENTER_DELAY_S = 0.3
# Output appears at the pace it was captured, but no gap between two lines
# lasts longer than this on screen.
MAX_OUTPUT_GAP_S = 2.0
LINE_GAP_S = 0.35
POST_VOICE_HOLD_S = 0.5
PAUSE_S = 0.5
OPENING_HOLD_S = 0.5
CLOSING_HOLD_S = 1.0
ARRIVE_EARLY_S = 0.1
WORD_PREFIX_MIN = 3
SPOTLIGHT_FADE_S = 0.3
CURSOR_BLINK_S = 0.53
ESTIMATED_WORD_S = 0.37
ESTIMATED_EXTRA_S = 0.4

SGR = re.compile(r"\x1b\[([0-9;]*)m")
OTHER_ESCAPE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]|\x1b\][^\x07]*\x07")


def font_file(names):
    for name in names:
        if (FONT_DIR / name).exists():
            return str(FONT_DIR / name)
    raise SystemExit(f"none of {', '.join(names)} is installed")


def word_key(text):
    return "".join(c for c in text.lower() if c.isalnum())


def estimated_length(text):
    return (text.count(" ") + 1) * ESTIMATED_WORD_S + ESTIMATED_EXTRA_S


def read_narration(folder):
    """{text: seconds} and {text: [(word key, seconds), ...]} of the voiced lines."""
    lengths, words = {}, {}
    if folder and (folder / "lines.tsv").exists():
        for row in (folder / "lines.tsv").read_text(encoding="utf-8").splitlines():
            seconds, _, text = row.partition("\t")
            if text:
                lengths[text] = float(seconds)
    if folder and (folder / "words.tsv").exists():
        for row in (folder / "words.tsv").read_text(encoding="utf-8").splitlines():
            text, _, timed = row.partition("\t")
            line = []
            for field in timed.split():
                word, _, at = field.rpartition("@")
                if word:
                    line.append((word_key(word), float(at)))
            words[text] = line
    return lengths, words


class Screen:
    """Rows of styled spans, scrolled like a terminal COLS wide."""

    def __init__(self):
        self.lines = [[]]

    def column(self):
        return sum(len(text) for text, _ in self.lines[-1])

    def write(self, text, color=TEXT):
        if "\t" in text:
            expanded = ""
            for char in text:
                if char == "\t":
                    expanded += " " * (TAB_STOP - (self.column() + len(expanded)) % TAB_STOP)
                else:
                    expanded += char
            text = expanded
        while text:
            room = COLS - self.column()
            if room <= 0:
                self.lines.append([])
                continue
            self.lines[-1].append((text[:room], color))
            text = text[room:]

    def newline(self):
        self.lines.append([])

    def write_ansi(self, line):
        """Writes one captured line, in its SGR colors, and ends it."""
        line = OTHER_ESCAPE.sub("", line)
        color, bold, at = TEXT, False, 0
        for match in SGR.finditer(line):
            self.write(line[at:match.start()], color)
            at = match.end()
            for code in [int(c) for c in match[1].split(";") if c] or [0]:
                if code == 0:
                    color, bold = TEXT, False
                elif code == 1:
                    bold = True
                elif 30 <= code <= 37:
                    color = (BRIGHT if bold else PALETTE)[code - 30]
                elif 90 <= code <= 97:
                    color = BRIGHT[code - 90]
        self.write(line[at:], color)
        self.newline()

    def visible(self):
        return self.lines[-ROWS:]

    def clear(self):
        self.lines = [[]]


class Renderer:
    def __init__(self, title):
        self.font = ImageFont.truetype(font_file(MONO_FONTS), FONT_SIZE)
        self.title_font = ImageFont.truetype(font_file(TITLE_FONTS), 22)
        self.cell = self.font.getlength("M")
        self.title = title
        self.last_key = None
        self.last_frame = None

    def frame(self, screen, cursor_on, spotlight_rows, spotlight_alpha):
        rows = screen.visible()
        key = (tuple(tuple(r) for r in rows), cursor_on, spotlight_rows, round(spotlight_alpha, 3),
               self.title)
        if key == self.last_key:
            return self.last_frame
        image = Image.new("RGB", (WIDTH, HEIGHT), BACKGROUND)
        draw = ImageDraw.Draw(image)
        draw.rectangle((0, 0, WIDTH, TITLE_H), fill=TITLE_BAR)
        draw.text((WIDTH / 2, TITLE_H / 2), self.title, font=self.title_font, fill=TITLE_TEXT, anchor="mm")
        for row, spans in enumerate(rows):
            x, y = MARGIN_X, TOP + row * LINE_H
            for text, color in spans:
                draw.text((x, y), text, font=self.font, fill=color)
                x += self.cell * len(text)
        if cursor_on:
            column = sum(len(text) for text, _ in rows[-1])
            x0, y0 = MARGIN_X + self.cell * column, TOP + (len(rows) - 1) * LINE_H
            draw.rectangle((x0, y0 + 2, x0 + self.cell - 1, y0 + LINE_H - 3), fill=CURSOR)
        if spotlight_rows and spotlight_alpha > 0.0:
            first, last = spotlight_rows
            top, bottom = TOP + first * LINE_H - 4, TOP + (last + 1) * LINE_H + 2
            shade = Image.new("L", (WIDTH, HEIGHT), int(255 * DIM_ALPHA * spotlight_alpha))
            ImageDraw.Draw(shade).rectangle((0, top, WIDTH, bottom), fill=0)
            image = Image.composite(Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0)), image, shade)
            draw = ImageDraw.Draw(image)
            edge = tuple(int(BACKGROUND[i] + (SPOTLIGHT[i] - BACKGROUND[i]) * spotlight_alpha) for i in range(3))
            draw.rounded_rectangle((MARGIN_X - 10, top, WIDTH - MARGIN_X + 10, bottom), radius=6,
                                   outline=edge, width=3)
        self.last_key, self.last_frame = key, image.tobytes()
        return self.last_frame


class Take:
    """One chapter of terminal footage, timed like the Optiq recorder's Director."""

    def __init__(self, stem, out, narration, capture, ffmpeg, cwd="~/transpose"):
        self.stem, self.out, self.capture = stem, out, capture
        self.lengths, self.words = read_narration(narration)
        self.narrated = narration is not None and bool(self.lengths)
        self.screen = Screen()
        self.cwd = cwd
        self.renderer = Renderer(self.title())
        self.frames = 0
        self.voice_end = 0.0
        self.line_text, self.line_start, self.line_length = "", 0.0, 0.0
        self.word_cursor = self.text_cursor = 0
        self.cues, self.timeline, self.problems = [], [], []
        # The spotlit lines, as indexes into screen.lines, so they scroll with their text.
        self.spotlight_lines, self.spotlight_from, self.spotlight_to = None, 0.0, -1.0
        # The output of the command running now, still to be shown.
        self.pending, self.pending_at = [], 0.0
        self.typing = False
        self.random = random.Random(stem)
        (out / "timing").mkdir(parents=True, exist_ok=True)
        self.encoder = subprocess.Popen(
            [ffmpeg, "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{WIDTH}x{HEIGHT}",
             "-r", str(FPS), "-i", "-", "-c:v", "libx264rgb", "-preset", "ultrafast", "-qp", "0",
             str(out / f"{stem}.mkv")], stdin=subprocess.PIPE)
        self.prompt()
        self.wait(OPENING_HOLD_S)

    # -- time and frames
    def now(self):
        return self.frames / FPS

    def title(self):
        return f"{PROMPT_HOST}: {self.cwd}"

    def spotlight_alpha(self):
        t = self.now()
        if self.spotlight_lines is None or t < self.spotlight_from:
            return 0.0
        alpha = min(1.0, (t - self.spotlight_from) / SPOTLIGHT_FADE_S)
        if self.spotlight_to >= 0.0 and t > self.spotlight_to:
            alpha = min(alpha, max(0.0, 1.0 - (t - self.spotlight_to) / SPOTLIGHT_FADE_S))
        return alpha

    def spotlight_rows(self):
        """The spotlit lines' rows on screen, cut to what is still shown, or None."""
        if self.spotlight_lines is None:
            return None
        hidden = len(self.screen.lines) - len(self.screen.visible())
        first, last = (line - hidden for line in self.spotlight_lines)
        return (max(first, 0), last) if last >= 0 else None

    def tick(self, count=1):
        for _ in range(max(count, 0)):
            blink = self.typing or int(self.now() / CURSOR_BLINK_S) % 2 == 0
            self.encoder.stdin.write(self.renderer.frame(self.screen, blink, self.spotlight_rows(),
                                                         self.spotlight_alpha()))
            self.frames += 1

    def wait(self, seconds):
        self.tick(round(max(seconds, 0.0) * FPS))

    def log(self, at, text):
        total = round(at * 10)
        self.timeline.append(f"{total // 600}:{total // 10 % 60:02d}.{total % 10}  {text}")

    def problem(self, text):
        self.problems.append(text)
        print(f"[{self.stem}] {text}", flush=True)

    # -- narration
    def Say(self, text):
        if self.cues:
            self.wait(self.voice_end + LINE_GAP_S - self.now())
        length = self.lengths.get(text)
        if length is None:
            length = estimated_length(text)
            if self.narrated:
                self.problem(f"no synthesized narration for: {text}")
        self.line_text, self.line_start, self.line_length = text, self.now(), length
        self.word_cursor = self.text_cursor = 0
        self.voice_end = self.now() + length
        self.cues.append((self.now(), text))
        self.log(self.now(), f'say "{text}"')

    def word_time(self, word):
        wanted = word_key(word)

        def starts_like(key):
            return len(key) >= WORD_PREFIX_MIN and len(wanted) >= WORD_PREFIX_MIN and (
                key.startswith(wanted) or wanted.startswith(key))

        heard = self.words.get(self.line_text, [])
        for exact in (True, False):
            for i in range(self.word_cursor, len(heard)):
                if heard[i][0] == wanted if exact else starts_like(heard[i][0]):
                    self.word_cursor = i + 1
                    return self.line_start + heard[i][1]
        for exact in (True, False):
            offset = 0
            for each in self.line_text.split():
                offset = self.line_text.find(each, offset)
                key = word_key(each)
                if offset >= self.text_cursor and (key == wanted if exact else starts_like(key)):
                    self.text_cursor = offset + len(each)
                    return self.line_start + self.line_length * offset / max(len(self.line_text), 1)
                offset += len(each)
        self.problem(f"'{word}' is not in the line: {self.line_text}")
        return -1.0

    def wait_until_said(self, word):
        at = self.word_time(word)
        if at >= 0.0:
            self.wait(at - ARRIVE_EARLY_S - self.now())

    def wait_for_voice(self):
        self.wait(self.voice_end - self.now())

    def hold(self, seconds):
        self.wait(min(seconds, max(self.voice_end - self.now(), 0.0) + POST_VOICE_HOLD_S))

    def pause(self):
        self.wait_for_voice()
        self.log(self.now(), "pause")
        self.hold(PAUSE_S)

    # -- terminal
    def prompt(self):
        self.screen.write(PROMPT_HOST, PROMPT_USER)
        self.screen.write(":")
        self.screen.write(self.cwd, PROMPT_PATH)
        self.screen.write("$ ")

    def type(self, command, word=None):
        if word:
            self.wait_until_said(word)
        self.log(self.now(), f'type "{command}"')
        self.typing = True
        for char in command:
            self.screen.write(char)
            self.wait(self.random.uniform(1.0 - TYPING_JITTER, 1.0 + TYPING_JITTER) / TYPING_CPS)
        self.wait(ENTER_DELAY_S)
        self.typing = False

    def enter(self, cwd=None):
        """Ends a command that prints nothing, at `cwd` when it changes directory."""
        self.screen.newline()
        if cwd:
            self.cwd = cwd
            self.renderer.title = self.title()
        self.prompt()
        self.tick()

    def run(self, command, capture, word=None):
        """Types `command`, then replays the output captured for it as `capture`."""
        self.type(command, word)
        self.output(capture)

    def output(self, capture, pause_at=None):
        """Runs the command typed last: replays the output captured for it as
        `capture`. With `pause_at`, the replay stops after the first line
        containing it, as if the program were still running, until resume()."""
        self.screen.newline()
        path = self.capture / f"{capture}.tsv"
        if not path.exists():
            self.problem(f"no captured output {path.name}")
            self.pending = []
        else:
            self.pending = [row.partition("\t") for row in path.read_text(encoding="utf-8").splitlines()]
        self.pending_at = 0.0
        self.replay(pause_at)

    def resume(self):
        """Replays the rest of the output an output(pause_at=...) stopped."""
        self.replay(None)

    def replay(self, pause_at):
        while self.pending:
            at, _, text = self.pending.pop(0)
            self.wait(min(float(at) - self.pending_at, MAX_OUTPUT_GAP_S))
            self.pending_at = float(at)
            self.screen.write_ansi(text)
            if pause_at and pause_at in text:
                return
        if pause_at:
            self.problem(f"the output never showed '{pause_at}'")
        self.prompt()
        self.tick()

    def spotlight(self, first, last=None):
        """Lights up the rows from the one containing `first` through the one
        containing `last` (or `first` again), among the rows on screen. A
        spotlight already lit moves there without fading."""
        rows = ["".join(text for text, _ in spans) for spans in self.screen.visible()]
        start = next((i for i, row in enumerate(rows) if first in row), None)
        end = next((i for i in range(len(rows) - 1, -1, -1) if (last or first) in rows[i]), None)
        if start is None or end is None or end < start:
            self.problem(f"nothing on screen to spotlight for '{first}'")
            return
        if self.spotlight_lines is None or self.spotlight_to >= 0.0:
            self.spotlight_from = self.now()
        hidden = len(self.screen.lines) - len(rows)
        self.spotlight_lines, self.spotlight_to = (start + hidden, end + hidden), -1.0
        self.log(self.now(), f'spotlight "{rows[start].strip()}"')

    def clear_spotlight(self):
        if self.spotlight_lines is not None and self.spotlight_to < 0.0:
            self.spotlight_to = self.now()

    def finish(self):
        self.clear_spotlight()
        self.wait_for_voice()
        self.wait(CLOSING_HOLD_S)
        self.log(self.now(), "end")
        self.encoder.stdin.close()
        self.encoder.wait()
        timing = self.out / "timing"
        (timing / f"{self.stem}.txt").write_text(
            f"# {self.stem}: each action and when it starts, as m:ss.s into the video\n" +
            "\n".join(self.timeline) + "\n", encoding="utf-8")
        (timing / f"{self.stem}.narration.tsv").write_text(
            "".join(f"{start:.3f}\t{text}\n" for start, text in self.cues), encoding="utf-8")
        status = "Every step ran as scripted" if not self.problems else f"{len(self.problems)} step(s) did not run as scripted"
        print(f"[{self.stem}] {self.now():.1f} s, {len(self.cues)} lines. {status}", flush=True)
        return not self.problems
