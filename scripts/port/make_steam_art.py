"""Regenerate the Steam library artwork shipped in the Steam Deck ZIP.

Usage (from the repo root, with Pillow installed):

    python scripts/port/make_steam_art.py

Writes scripts/port/linux/steam-art/{cover,banner,hero,logo}.png. The artwork is
original: an iron-sight picture drawn from polygons and the Iron Sights title in
Black Ops One (SIL Open Font License 1.1), downloaded from a pinned Google Fonts
commit. Run it again after changing the design and commit the outputs; packaging
only copies them, so the release container needs no Pillow.
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

from linux_build import download

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "scripts/port/linux/steam-art"
FONT_COMMIT = "d50a0d29b8b01a0a5d2cc7f6fd0fee1fbdbe4ce5"
FONT_URL = f"https://raw.githubusercontent.com/google/fonts/{FONT_COMMIT}/ofl/blackopsone/BlackOpsOne-Regular.ttf"
FONT_SHA256 = "282a825b5f294377387e3969f765408157dbea8da0f5d0aae68c6bc704b145b3"
SUPERSAMPLE = 2
RADIUS_VALUE = 181  # Pillow's radial_gradient reaches 181 at its inscribed circle, 255 at the corners

SKY_TOP, SKY_BOTTOM = (34, 39, 31), (7, 8, 7)
GOLD_TOP, GOLD_MID, GOLD_BOTTOM = (246, 228, 146), (234, 184, 48), (240, 132, 28)
GLOW = (240, 176, 60)
STEEL_TOP, STEEL_BOTTOM = (46, 51, 54), (17, 19, 20)
SLIDE_TOP, SLIDE_BOTTOM = (24, 27, 29), (9, 10, 11)
POST = (58, 62, 63)
EDGE = (92, 99, 103)
OUTLINE = (26, 18, 6)


def gradient(size, stops):
    """Vertical RGB gradient through evenly spaced colour stops."""
    width, height = size
    column = Image.new("RGB", (1, height))
    spans = len(stops) - 1
    for y in range(height):
        position = y / max(height - 1, 1) * spans
        index = min(int(position), spans - 1)
        t = position - index
        a, b = stops[index], stops[index + 1]
        column.putpixel((0, y), tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3)))
    return column.resize(size).convert("RGBA")


def paint(image, mask, fill):
    """Paint a solid colour or an image through an L mask."""
    layer = fill if isinstance(fill, Image.Image) else Image.new("RGBA", image.size, fill + (255,))
    image.paste(layer, (0, 0), mask)


def glow(image, center, radius, color, strength):
    """Add a soft round bloom whose alpha falls off towards its radius."""
    radius = max(int(radius), 1)
    falloff = Image.radial_gradient("L").resize((radius * 2, radius * 2))
    alpha = falloff.point(lambda v: round(strength * 255 * max(0.0, 1 - v / RADIUS_VALUE) ** 2.2))
    layer = Image.new("RGBA", image.size, color + (0,))
    layer.paste(Image.new("RGBA", alpha.size, color + (255,)), (int(center[0]) - radius, int(center[1]) - radius), alpha)
    image.alpha_composite(layer)


def vignette(image, strength):
    width, height = image.size
    falloff = Image.radial_gradient("L").resize((width, height))
    alpha = falloff.point(lambda v: round(strength * 255 * min(1.0, v / RADIUS_VALUE) ** 2.5))
    shade = Image.new("RGBA", image.size, (0, 0, 0, 255))
    shade.putalpha(alpha)
    image.alpha_composite(shade)


def sight_picture(image, cx, tip, width):
    """Three-dot pistol sights: a far front post centred in the rear notch."""
    w, h = image.size
    notch, depth, chamfer = 0.16 * width, 0.11 * width, 0.03 * width
    left, right, bottom = cx - width / 2, cx + width / 2, tip + 0.22 * width
    post = 0.055 * width

    glow(image, (cx, tip - 0.02 * width), 0.75 * width, GLOW, 0.55)
    glow(image, (cx, tip - 0.02 * width), 0.16 * width, GOLD_TOP, 0.65)
    ring = Image.new("RGBA", image.size, (0, 0, 0, 0))
    r = 0.2 * width
    ImageDraw.Draw(ring).ellipse((cx - r, tip - r, cx + r, tip + r), outline=GOLD_TOP + (90,), width=max(1, round(0.008 * width)))
    image.alpha_composite(ring.filter(ImageFilter.GaussianBlur(0.006 * width)))

    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).rectangle((cx - post / 2, tip, cx + post / 2, bottom), fill=255)
    paint(image, mask, POST)

    spread = 0.40 * width + (h - bottom) * 0.35
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).polygon([(cx - 0.40 * width, bottom), (cx + 0.40 * width, bottom),
                                  (cx + spread, h), (cx - spread, h)], fill=255)
    paint(image, mask, gradient(image.size, (SLIDE_TOP, SLIDE_BOTTOM)))

    blade = [(left, tip + chamfer), (left + chamfer, tip), (cx - notch / 2, tip), (cx - notch / 2, tip + depth),
             (cx + notch / 2, tip + depth), (cx + notch / 2, tip), (right - chamfer, tip), (right, tip + chamfer),
             (right, bottom), (left, bottom)]
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).polygon(blade, fill=255)
    steel = gradient(image.size, (STEEL_TOP, STEEL_BOTTOM))
    serrations = ImageDraw.Draw(steel)
    step, line = 0.018 * width, max(1, round(0.004 * width))
    y = tip + depth + step
    while y < bottom - step / 2:
        serrations.line((left, y, right, y), fill=SLIDE_BOTTOM + (255,), width=line)
        y += step
    paint(image, mask, steel)
    ImageDraw.Draw(image).line(blade[:8], fill=EDGE + (255,), width=max(1, round(0.006 * width)), joint="curve")

    dot = 0.022 * width
    for x, y, color in ((cx, tip + 0.05 * width, GOLD_TOP),
                        (cx - notch / 2 - 0.07 * width, tip + depth * 0.45, GOLD_MID),
                        (cx + notch / 2 + 0.07 * width, tip + depth * 0.45, GOLD_MID)):
        size = dot * (0.75 if color == GOLD_TOP else 1)
        glow(image, (x, y), size * 4, GLOW, 0.5)
        ImageDraw.Draw(image).ellipse((x - size, y - size, x + size, y + size), fill=color + (255,))


def fit_font(font_path, text, width):
    probe = ImageFont.truetype(str(font_path), 100)
    left, _, right, _ = probe.getbbox(text)
    return ImageFont.truetype(str(font_path), max(1, int(100 * width / (right - left))))


def title(image, font_path, lines, box):
    """Gold title lines filling box=(left, top, width), each scaled to the box width."""
    left, top, width = box
    y = top
    for text in lines:
        font = fit_font(font_path, text, width)
        x0, y0, x1, y1 = font.getbbox(text)
        stroke = max(2, round(font.size * 0.035))
        origin = (left - x0, y - y0)
        mask = Image.new("L", image.size)
        ImageDraw.Draw(mask).text(origin, text, font=font, fill=255)
        outline = Image.new("L", image.size)
        ImageDraw.Draw(outline).text(origin, text, font=font, fill=255, stroke_width=stroke, stroke_fill=255)
        shadow = Image.new("RGBA", image.size, (0, 0, 0, 0))
        shadow.paste(Image.new("RGBA", image.size, (0, 0, 0, 200)), (0, round(stroke * 1.5)), outline)
        image.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(stroke * 2)))
        paint(image, outline, OUTLINE)
        fill = Image.new("RGBA", image.size, GOLD_BOTTOM + (255,))
        fill.paste(gradient((image.width, y1 - y0), (GOLD_TOP, GOLD_MID, GOLD_BOTTOM)), (0, y))
        paint(image, mask, fill)
        y += (y1 - y0) + round(font.size * 0.12)


def finish(image, size, path):
    image = image.resize(size, Image.LANCZOS)
    if image.getchannel("A").getextrema() == (255, 255):
        image = image.convert("RGB")
    image.save(path, optimize=True)
    print(path)


def render(font_path):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    s = SUPERSAMPLE

    size = (600, 900)
    image = gradient((size[0] * s, size[1] * s), (SKY_TOP, SKY_BOTTOM))
    sight_picture(image, 300 * s, 580 * s, 560 * s)
    vignette(image, 0.55)
    title(image, font_path, ("IRON", "SIGHTS"), (60 * s, 70 * s, 480 * s))
    finish(image, size, OUTPUT / "cover.png")

    size = (920, 430)
    image = gradient((size[0] * s, size[1] * s), (SKY_TOP, SKY_BOTTOM))
    sight_picture(image, 680 * s, 250 * s, 380 * s)
    vignette(image, 0.5)
    title(image, font_path, ("IRON", "SIGHTS"), (50 * s, 95 * s, 370 * s))
    finish(image, size, OUTPUT / "banner.png")

    size = (1920, 620)
    image = gradient((size[0] * s, size[1] * s), (SKY_TOP, SKY_BOTTOM))
    sight_picture(image, 1350 * s, 300 * s, 760 * s)
    vignette(image, 0.45)
    finish(image, size, OUTPUT / "hero.png")

    size = (1280, 260)
    image = Image.new("RGBA", (size[0] * s, size[1] * s), (0, 0, 0, 0))
    title(image, font_path, ("IRON SIGHTS",), (40 * s, 30 * s, 1200 * s))
    left, top, right, bottom = image.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()
    pad = 12 * s
    image = image.crop((left - pad, top - pad, right + pad, bottom + pad))
    finish(image, (image.width // s, image.height // s), OUTPUT / "logo.png")


if __name__ == "__main__":
    render(download(FONT_URL, FONT_SHA256, ROOT / "build/steam-art/BlackOpsOne-Regular.ttf"))
