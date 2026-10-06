#!/usr/bin/env python3
"""
Codey Rocky / Bruce CHAR_FONT editor (6x8 glyphs)
================================================
Edit the Makeblock authentic font in the terminal with arrow keys.
Saves back into codey_fonts.h (CHAR_FONT array).

Controls
--------
  Arrows     move pixel cursor
  Space      toggle pixel ON/OFF
  n / p      next / previous character
  0-9 A-Z    jump to that glyph (if present)
  c          clear current glyph
  i          invert current glyph
  f          flip vertical
  h          flip horizontal
  s          save to codey_fonts.h
  q          quit (asks to save if dirty)

Usage
-----
  python3 font_editor.py [path/to/codey_fonts.h]
"""

from __future__ import annotations

import curses
import re
import sys
from pathlib import Path

COLS = 6
ROWS = 8

GLYPH_RE = re.compile(
    r"\{'((?:\\.|[^'\\])*)',\s*\{([^}]+)\}\}",
    re.MULTILINE,
)


def unescape_char(s: str) -> str:
    if s.startswith("\\"):
        mapping = {"\\n": "\n", "\\t": "\t", "\\'": "'", "\\\\": "\\", "\\0": "\0"}
        if s in mapping:
            return mapping[s]
        if len(s) >= 2:
            return s[1]
    return s


def escape_char(ch: str) -> str:
    if ch == "'":
        return "\\'"
    if ch == "\\":
        return "\\\\"
    if ch == "\n":
        return "\\n"
    if ord(ch) < 32 or ord(ch) > 126:
        return f"\\x{ord(ch):02x}"
    return ch


def parse_fonts(text: str) -> tuple[list[tuple[str, list[int]]], int, int]:
    """Return glyphs, start index of CHAR_FONT array body, end index."""
    start = text.find("static const CharFont CHAR_FONT[]")
    if start < 0:
        raise SystemExit("CHAR_FONT not found in file")
    brace = text.find("{", start)
    # find matching close of array — look for "};" after entries
    end_marker = text.find("static const int CHAR_FONT_LEN", brace)
    if end_marker < 0:
        end_marker = text.find("};", brace) + 2
    body = text[brace:end_marker]
    glyphs: list[tuple[str, list[int]]] = []
    for m in GLYPH_RE.finditer(body):
        ch = unescape_char(m.group(1))
        nums = [int(x.strip(), 0) for x in m.group(2).split(",") if x.strip()]
        if len(nums) != COLS:
            # pad or trim
            nums = (nums + [0] * COLS)[:COLS]
        glyphs.append((ch, nums))
    if not glyphs:
        raise SystemExit("No glyphs parsed from CHAR_FONT")
    return glyphs, brace, end_marker


def format_glyph_line(ch: str, cols: list[int]) -> str:
    hexes = ",".join(f"0x{b:02X}" for b in cols)
    return f"  {{'{escape_char(ch)}', {{{hexes}}}}},"


def write_fonts(path: Path, text: str, glyphs: list[tuple[str, list[int]]], brace: int, end: int) -> None:
    lines = ["{"]
    for ch, cols in glyphs:
        lines.append(format_glyph_line(ch, cols))
    lines.append("};")
    new_body = "\n".join(lines)
    # end points at CHAR_FONT_LEN line; body was from brace to end_marker
    # Keep anything between } and CHAR_FONT_LEN
    after = text[end:]
    # If body ended with }; before CHAR_FONT_LEN, strip leading };
    new_text = text[:brace] + new_body + "\n" + after
    # Ensure CHAR_FONT_LEN still valid — leave as-is
    path.write_text(new_text)


# Display is vertically flipped vs stored bits (matches device after lm_rev8):
#   editor row 0 (top of grid)  <-> bit 7 in the file
#   editor row 7 (bottom)       <-> bit 0 in the file
def _bit_for_row(y: int) -> int:
    return 1 << (ROWS - 1 - y)


def pixel_on(cols: list[int], x: int, y: int) -> bool:
    return bool(cols[x] & _bit_for_row(y))


def set_pixel(cols: list[int], x: int, y: int, on: bool) -> None:
    mask = _bit_for_row(y)
    if on:
        cols[x] |= mask
    else:
        cols[x] &= ~mask


def toggle_pixel(cols: list[int], x: int, y: int) -> None:
    set_pixel(cols, x, y, not pixel_on(cols, x, y))


def run(stdscr: "curses._CursesWindow", path: Path) -> None:
    curses.curs_set(0)
    curses.use_default_colors()
    if curses.has_colors():
        curses.init_pair(1, curses.COLOR_GREEN, -1)
        curses.init_pair(2, curses.COLOR_YELLOW, -1)
        curses.init_pair(3, curses.COLOR_CYAN, -1)
        curses.init_pair(4, curses.COLOR_RED, -1)

    text = path.read_text()
    glyphs, brace, end = parse_fonts(text)
    # work on mutable copies
    data = [(ch, list(cols)) for ch, cols in glyphs]
    idx = 0
    cx, cy = 0, 0
    dirty = False
    status = f"Loaded {len(data)} glyphs from {path.name}"

    def current():
        return data[idx][1]

    while True:
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        ch, cols = data[idx]
        label = ch if ch.isprintable() and ch != " " else f"0x{ord(ch):02X}"
        title = f" Font editor [FLIP] — '{label}'  ({idx + 1}/{len(data)}) "
        if dirty:
            title += " *modified* "
        stdscr.addnstr(0, 0, title.ljust(w), w, curses.color_pair(2) | curses.A_BOLD)

        # grid origin
        ox, oy = 4, 3
        cell = 2  # width of each cell in chars
        for y in range(ROWS):
            for x in range(COLS):
                on = pixel_on(cols, x, y)
                char = "██" if on else "··"
                attr = curses.A_REVERSE if (x == cx and y == cy) else 0
                if on:
                    attr |= curses.color_pair(1) | curses.A_BOLD
                try:
                    stdscr.addstr(oy + y, ox + x * cell, char, attr)
                except curses.error:
                    pass

        # border hints
        stdscr.addstr(oy - 1, ox, "0 1 2 3 4 5", curses.color_pair(3))
        for y in range(ROWS):
            try:
                stdscr.addstr(oy + y, ox - 2, str(y), curses.color_pair(3))
            except curses.error:
                pass

        help_lines = [
            "Arrows move  Space toggle  n/p next/prev  s save  q quit",
            "c clear  i invert  f flip-V  h flip-H  type char to jump",
        ]
        for i, line in enumerate(help_lines):
            try:
                stdscr.addnstr(oy + ROWS + 2 + i, 0, line, w, curses.color_pair(3))
            except curses.error:
                pass
        try:
            stdscr.addnstr(h - 1, 0, status[: w - 1].ljust(w - 1), w - 1, curses.color_pair(2))
        except curses.error:
            pass

        stdscr.refresh()
        key = stdscr.getch()

        if key in (curses.KEY_LEFT, ord("a")):
            cx = (cx - 1) % COLS
        elif key in (curses.KEY_RIGHT, ord("d")):
            cx = (cx + 1) % COLS
        elif key in (curses.KEY_UP, ord("w")):
            cy = (cy - 1) % ROWS
        elif key == curses.KEY_DOWN:
            cy = (cy + 1) % ROWS
        elif key == ord(" "):
            toggle_pixel(current(), cx, cy)
            dirty = True
            status = f"Toggled ({cx},{cy})"
        elif key in (ord("n"), curses.KEY_NPAGE):
            idx = (idx + 1) % len(data)
            cx = cy = 0
            status = f"Glyph '{data[idx][0]}'"
        elif key in (ord("p"), curses.KEY_PPAGE):
            idx = (idx - 1) % len(data)
            cx = cy = 0
            status = f"Glyph '{data[idx][0]}'"
        elif key == ord("c"):
            data[idx] = (data[idx][0], [0] * COLS)
            dirty = True
            status = "Cleared"
        elif key == ord("i"):
            data[idx] = (data[idx][0], [b ^ 0xFF for b in current()])
            dirty = True
            status = "Inverted"
        elif key == ord("f"):
            # flip vertical: reverse bits in each column
            newcols = []
            for b in current():
                nb = 0
                for y in range(ROWS):
                    if b & (1 << y):
                        nb |= 1 << (ROWS - 1 - y)
                newcols.append(nb)
            data[idx] = (data[idx][0], newcols)
            dirty = True
            status = "Flipped vertical"
        elif key == ord("h"):
            data[idx] = (data[idx][0], list(reversed(current())))
            dirty = True
            status = "Flipped horizontal"
        elif key == ord("s"):
            # rebuild text and write
            write_fonts(path, path.read_text(), data, brace, end)
            # re-parse positions after write (file changed)
            text = path.read_text()
            glyphs, brace, end = parse_fonts(text)
            dirty = False
            status = f"Saved → {path}"
        elif key in (ord("q"), 27):
            if dirty:
                status = "Unsaved changes! Press s to save, Q to quit anyway"
                stdscr.addnstr(h - 1, 0, status[: w - 1].ljust(w - 1), w - 1, curses.color_pair(4))
                stdscr.refresh()
                k2 = stdscr.getch()
                if k2 == ord("Q"):
                    break
                if k2 == ord("s"):
                    write_fonts(path, path.read_text(), data, brace, end)
                    break
                continue
            break
        elif 32 <= key < 127:
            # jump to character
            target = chr(key)
            for i, (gch, _) in enumerate(data):
                if gch == target:
                    idx = i
                    cx = cy = 0
                    status = f"Jumped to '{target}'"
                    break
            else:
                status = f"No glyph for '{target}'"


def main() -> None:
    default = Path(__file__).resolve().parents[1] / "bruce" / "lib" / "HAL" / "display" / "codey_fonts.h"
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else default
    if not path.exists():
        # try relative to cwd
        alt = Path("lib/HAL/display/codey_fonts.h")
        if alt.exists():
            path = alt
        else:
            print(f"File not found: {path}", file=sys.stderr)
            print("Usage: python3 font_editor.py [path/to/codey_fonts.h]", file=sys.stderr)
            sys.exit(1)
    curses.wrapper(lambda scr: run(scr, path))


if __name__ == "__main__":
    main()
