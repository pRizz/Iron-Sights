"""Compare a game font with an open font, glyph by glyph (local review only).

    python scripts/port/compare_open_font.py --open-font PATH.ttf [--game bankgothic]

Reads the game's glyph bitmaps from the decompilation's font sources
(assets/font/font*.c) and draws, for each printable character, four panels:
the game glyph, the open font's glyph scaled to the same cap height and
baseline, an overlay (overlap in red, game-only grey, open-only black) and
the difference (pixels covered by only one font, in red), with the share of
overlapping ink.

The output is ROM-derived and stays in build_win/font-compare/, which Git
ignores; never commit it. See the open assets plan's provenance rules.
"""
import argparse
import re
from pathlib import Path

import numpy as np
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
GAME_FONTS = {'bankgothic': 'fontBankGothic', 'zurichbold': 'fontZurichBold'}
PANEL = 12          # screen pixels per game pixel
SUPERSAMPLE = 4     # anti-aliasing for the open font's raster
INK = 0.5           # coverage counted as ink


def read_game_font(name):
    """{char: (bitmap float array, baseline, height, width)} from the C source."""
    source = (ROOT / 'assets' / 'font' / f'{name}.c').read_text()
    tables = {}
    for label in ('kerning', 'fontchartable', 'fontbytes'):
        body = re.search(rf'{name}_{label}\[\]\s*=\s*\{{(.*?)\}};', source, re.S).group(1)
        tables[label] = [int(v, 16) for v in re.findall(r'0x([0-9A-Fa-f]+)', body)]
    data = b''.join(w.to_bytes(4, 'big') for w in tables['kerning'] + tables['fontchartable'] + tables['fontbytes'])
    glyphs = {}
    rows = [v - (1 << 32) if v >= 1 << 31 else v for v in tables['fontchartable']]  # s32 fields
    for i in range(94):
        _, baseline, height, width, _, offset = rows[i * 6:i * 6 + 6]
        stride = (width + 7) & ~7
        pixels = np.frombuffer(data[offset:offset + stride * height], dtype=np.uint8).reshape(height, stride)
        glyphs[chr(0x21 + i)] = (pixels[:, :width] / 255.0, baseline, height, width)
    return glyphs


def open_font_outlines(path):
    """{char: (contours in font units, cap height)} from a TrueType font."""
    font = TTFont(str(path))
    cmap = font.getBestCmap()
    glyf = font['glyf']
    cap = font['OS/2'].sCapHeight
    out = {}
    for code, name in cmap.items():
        g = glyf[name]
        if g.numberOfContours <= 0:
            continue
        coords, ends, _ = g.getCoordinates(glyf)
        contours, start = [], 0
        for end in ends:
            contours.append([tuple(coords[k]) for k in range(start, end + 1)])
            start = end + 1
        out[chr(code)] = contours
    return out, cap


def rasterize(contours, scale, origin, size):
    """Coverage (0..1) of contours drawn at scale px/unit, origin = font-unit
    point at the image's top-left, size = (w, h) in output pixels."""
    w, h = size
    img = Image.new('L', (w * SUPERSAMPLE, h * SUPERSAMPLE), 0)
    draw = ImageDraw.Draw(img)
    s = scale * SUPERSAMPLE
    # Nonzero fill: draw each contour with XOR so holes cut out of outers.
    layers = []
    for c in contours:
        layer = Image.new('1', img.size, 0)
        ImageDraw.Draw(layer).polygon([((x - origin[0]) * s, (origin[1] - y) * s) for x, y in c], fill=1)
        layers.append(np.asarray(layer, dtype=bool))
    acc = np.zeros((img.size[1], img.size[0]), dtype=bool)
    for layer in layers:
        acc ^= layer
    cov = acc.reshape(h, SUPERSAMPLE, w, SUPERSAMPLE).mean(axis=(1, 3))
    return cov


def compare(game, ours, cap_units, ch, ref):
    """The four panels for one character as an RGB array, and the overlap ratio."""
    bitmap, baseline, height, width = game[ch]
    ref_baseline_y = ref[1] + ref[2]          # line-relative y of the text baseline
    cap_px = ref[2]                            # game cap height in game pixels
    units_per_px = cap_units / cap_px
    # Game glyph in a frame: rows from its top to the baseline, padded.
    pad = 2
    contours = ours.get(ch, [])
    xs = [x for c in contours for x, _ in c] or [0]
    ox_units = min(xs)
    our_w = (max(xs) - ox_units) / units_per_px if contours else 0
    frame_w = int(max(width, np.ceil(our_w))) + 2 * pad
    top = min(baseline - ref_baseline_y, -cap_px) - pad          # relative to baseline, game px
    ys = [y for c in contours for _, y in c] or [0]
    our_top = -max(ys) / units_per_px
    our_bottom = -min(ys) / units_per_px
    top = int(np.floor(min(top, our_top - pad)))
    bottom = int(np.ceil(max(baseline + height - ref_baseline_y, our_bottom, 0) + pad))
    frame_h = bottom - top
    W, H = frame_w * PANEL, frame_h * PANEL

    game_img = np.zeros((frame_h, frame_w))
    gy = baseline - ref_baseline_y - top
    game_img[gy:gy + height, pad:pad + width] = bitmap
    game_cov = np.kron(game_img, np.ones((PANEL, PANEL)))

    scale = PANEL / units_per_px
    origin = (ox_units - pad * units_per_px, -top * units_per_px)
    our_cov = rasterize(contours, scale, origin, (W, H)) if contours else np.zeros((H, W))

    g, o = game_cov >= INK, our_cov >= INK
    both = g & o
    union = g | o
    ratio = both.sum() / union.sum() if union.any() else 1.0

    def mono(cov):
        v = (1.0 - cov) * 255
        return np.stack([v, v, v], axis=-1)

    overlay = np.full((H, W, 3), 255.0)
    overlay[g & ~o] = (160, 166, 176)
    overlay[o & ~g] = (20, 20, 20)
    overlay[both] = (220, 30, 30)
    diff = np.full((H, W, 3), 255.0)
    diff[g ^ o] = (220, 30, 30)
    gap = np.full((H, 6, 3), 255.0)
    row = np.concatenate([mono(game_cov), gap, mono(our_cov), gap, overlay, gap, diff], axis=1)
    return row.astype(np.uint8), ratio


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--open-font', type=Path, required=True)
    parser.add_argument('--game', choices=sorted(GAME_FONTS), default='bankgothic')
    parser.add_argument('--out', type=Path, default=ROOT / 'build_win' / 'font-compare')
    parser.add_argument('--chars', default=''.join(chr(c) for c in range(0x21, 0x7F)))
    parser.add_argument('--columns', type=int, default=4)
    args = parser.parse_args()

    game = read_game_font(GAME_FONTS[args.game])
    ours, cap_units = open_font_outlines(args.open_font)
    ref = game['H']
    label_font = ImageFont.load_default(18)
    cells = []
    for ch in args.chars:
        panels, ratio = compare(game, ours, cap_units, ch, ref)
        label_h = 28
        cell = Image.new('RGB', (panels.shape[1] + 24, panels.shape[0] + label_h + 24), 'white')
        cell.paste(Image.fromarray(panels), (12, label_h))
        ImageDraw.Draw(cell).text((12, 4), f'{ch!r}  overlap {ratio:.0%}', fill=(60, 60, 60), font=label_font)
        cells.append((ch, cell, ratio))

    cols = args.columns
    cell_w = max(c.width for _, c, _ in cells)
    rows = [cells[i:i + cols] for i in range(0, len(cells), cols)]
    row_h = [max(c.height for _, c, _ in r) for r in rows]
    header = 70
    sheet = Image.new('RGB', (cell_w * cols, header + sum(row_h)), 'white')
    d = ImageDraw.Draw(sheet)
    title_font = ImageFont.load_default(22)
    mean = sum(r for *_, r in cells) / len(cells)
    d.text((12, 8), f'{args.game} (game) vs {args.open_font.stem}: game | ours | overlay | difference. '
                    f'Mean overlap {mean:.0%}.', fill='black', font=title_font)
    d.text((12, 38), 'Overlay: red = both, grey = game only, black = ours only. Difference: red = only one has ink.',
           fill=(60, 60, 60), font=label_font)
    y = header
    for r, h in zip(rows, row_h):
        for i, (_, cell, _) in enumerate(r):
            sheet.paste(cell, (i * cell_w, y))
        y += h
    args.out.mkdir(parents=True, exist_ok=True)
    path = args.out / f'{args.game}-vs-{args.open_font.stem}.png'
    sheet.save(path)
    worst = sorted(cells, key=lambda c: c[2])[:8]
    print(f'wrote {path}')
    print(f'mean overlap {mean:.1%}; lowest: ' + ', '.join(f'{ch!r} {r:.0%}' for ch, _, r in worst))


if __name__ == '__main__':
    main()
