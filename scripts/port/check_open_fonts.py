"""Check how an open font's glyphs fit the game font's spaces (local only).

    python scripts/port/check_open_fonts.py --open-font PATH.ttf [--game zurichbold]

Places each glyph as the renderer does (port/fast3d/open_fonts.cpp): scaled so
our capital H matches the game H's ink as it reads on screen, with the ink
centred in the game glyph's width and squeezed horizontally when it is wider
than that width plus a little slack. A squeeze narrows the strokes too, so the
check lists glyphs squeezed below --min-squeeze, and glyphs whose ink reaches
past the game glyph's box by more than a texel, which can crowd neighbours.

Reads the game's glyphs from the decompilation's font sources
(assets/font/font*.c, extracted from the player's ROM). It prints numbers only;
nothing it reads may be committed (see the open assets plan).
"""
import argparse
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from compare_open_font import GAME_FONTS, open_font_outlines, read_game_font  # noqa: E402

WIDTH_SLACK = 0.4  # texels; matches kWidthSlack in open_fonts.cpp


def rom_ink(bitmap):
    """(height, bottom) of the glyph's ink in texels, as rom_ink() measures it:
    rows' peak intensity crossing a quarter of the glyph's peak."""
    rows = bitmap.max(axis=1) * 255
    peak = rows.max()
    level = 0.25 * peak
    inked = np.nonzero(rows >= level)[0]
    first, last = int(inked[0]), int(inked[-1])

    def edge(inside, outside):
        out = rows[outside] if 0 <= outside < len(rows) else 0.0
        t = (rows[inside] - level) / max(rows[inside] - out, 1.0)
        return inside + 0.5 + (outside - inside) * t

    top, bottom = edge(first, first - 1), edge(last, last + 1)
    return bottom - top, bottom


def fit(game, ours, cap_units):
    """{char: (squeeze, overhang)}: the horizontal scale applied (1 = none) and
    how far the ink reaches past the game glyph's sides, in texels."""
    cap_texels, _ = rom_ink(game['H'][0])
    scale = cap_texels / cap_units
    out = {}
    for ch, contours in ours.items():
        if ch not in game:
            continue
        xs = [x for c in contours for x, _ in c]
        ink = (max(xs) - min(xs)) * scale
        width = game[ch][3]
        room = width + WIDTH_SLACK
        squeeze = room / ink if ink > room else 1.0
        overhang = max(0.0, (min(ink, room) - width) / 2)
        out[ch] = (squeeze, overhang)
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--open-font', type=Path, required=True)
    parser.add_argument('--game', choices=sorted(GAME_FONTS), default='zurichbold')
    parser.add_argument('--min-squeeze', type=float, default=0.85)
    args = parser.parse_args()

    game = read_game_font(GAME_FONTS[args.game])
    ours, cap_units = open_font_outlines(args.open_font)
    results = fit(game, ours, cap_units)
    squeezed = sorted((s, ch) for ch, (s, _) in results.items() if s < args.min_squeeze)
    mean = sum(s for s, _ in results.values()) / len(results)
    print(f'{len(results)} glyphs; mean horizontal scale {mean:.2f}; '
          f'{sum(1 for s, _ in results.values() if s < 1)} squeezed at all')
    for s, ch in squeezed:
        print(f'  {ch!r} squeezed to {s:.0%}')
    if squeezed:
        print(f'{len(squeezed)} glyph(s) squeezed below {args.min_squeeze:.0%}: narrow their designs')
        return 1
    print(f'fit check clean: no glyph squeezed below {args.min_squeeze:.0%}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
