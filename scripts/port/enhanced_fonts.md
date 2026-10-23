# Enhanced fonts

1964 GEPD's enhanced text uses a graphics-plugin texture replacement pack.
The emulator still draws the game's original glyph rectangles, while the
plugin identifies the low-resolution textures and substitutes higher-resolution
images. It does not change the game's text layout or create detail by filtering
the original bitmap.

The [1964 bundle](https://github.com/Graslu/1964GEPD/releases/tag/latest)
ships `plugin/cache/GOLDENEYE_HIRESTEXTURES.htc`; its bundled changelog
describes the GoldenEye HUD texture pack for GLideN64. The plugin's
[replacement loader](https://github.com/gonetz/GLideN64/blob/master/src/GLideNHQ/TxHiResLoader.cpp)
and [Rice fingerprint implementation](https://github.com/gonetz/GLideN64/blob/master/src/GLideNHQ/TxUtil.cpp)
explain how those named replacement textures are matched. This implementation
uses that format only during preparation and has its own native renderer hook.

This port uses the same community Bank Gothic and Zurich Bold masks from
[Carnivorous's pack published by Graslu](https://github.com/Graslu/GEPDTexturePacks),
revision `ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63`, published under CC0.
There is no emulator dependency or runtime download. The executable embeds
186 replacement masks at eight times the native glyph resolution. `sources.json`
records their source paths and SHA256 fingerprints. The ZIP includes credits
and the CC0 license. Original ROM glyph pixels are not embedded by this feature.

**Options > Display > Video > Enhanced Fonts** is on by default, including for
existing configurations without the new key. Set `[display] enhanced_fonts=0`
for original text. Both the frontend and watch offer this setting. It is
independent of 2D Filtering: HD glyphs retain smooth filtering while other HUD
graphics keep their own setting. The Nintendo 64 preset disables enhanced
fonts; the Modern preset enables them. The separate Classic Mode toggle does
not own this setting.

The renderer only substitutes tagged Latin font draws whose padded native
glyph bytes and dimensions match a known fingerprint. The game's original
spacing, baselines, colours, outlines, clipping and HUD text scales remain.
Missing glyphs (including the pack's absent tildes), Japanese glyphs and
modified ROM fonts fall back to original rendering. Picture text/logos and
ammo icons are not replaced. Switching the setting keeps original and enhanced
textures in separate cache entries.

## Digit height: decision deferred

The enhanced Bank Gothic digits appear taller than uppercase letters in mixed
weapon labels such as `KF7` and `PP7`. This difference is already present in
the source pack's images, before this port's conversion or HUD scaling.

Measured at an intensity threshold of 128/255, the source `0` and `7` each have
58 rows of visible ink, while uppercase `K` and `F` have 52, in the same
64-pixel-high texture cells: approximately a 12% difference. For comparison,
the original game's `0`, `7`, `K` and `F` each have six visible rows at that
threshold and the same vertical alignment. Original lowercase glyphs are
smaller capitals, so numbers can still look taller beside those.

The pack's README and import history do not explain this size difference;
research found no author statement confirming deliberate intent. Bank Gothic's
small-cap conventions provide context, but do not establish why the pack's
digits also exceed its uppercase letters. Treat this as a documented visual
difference, rather than a confirmed defect or confirmed intentional design.

**Decision deferred (2026-10-23):** retain the current pack proportions for now.
Whether to normalize digit height, and whether any adjustment should apply
throughout the font or only to weapon labels, remains undecided.

Evidence:

- [Source digit 7](https://github.com/Graslu/GEPDTexturePacks/blob/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63/GoldenEye%20Texture%20Pack/Bank%20Gothic/Misc/GOLDENEYE%23A8DBE3A5%234%231_rgb.png)
  and [uppercase K](https://github.com/Graslu/GEPDTexturePacks/blob/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63/GoldenEye%20Texture%20Pack/Bank%20Gothic/Upper%20Case/GOLDENEYE%2308A740B3%234%231_rgb.png).
- [Original font tables and pixels](../../assets/font/fontBankGothic.c),
  compared with the per-character source paths in
  [sources.json](../../port/fast3d/fonts/sources.json).
- [Pack README](https://github.com/Graslu/GEPDTexturePacks/blob/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63/GoldenEye%20Texture%20Pack/readme.txt)
  and [Bank Gothic import](https://github.com/Graslu/GEPDTexturePacks/commit/d66ef97e5b3880242afa055ac618860b15c2fb3b).

## Regeneration

To regenerate, install Pillow in a developer environment, download the
[pinned source archive](https://codeload.github.com/Graslu/GEPDTexturePacks/zip/ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63),
then run:

```sh
python scripts/port/generate_enhanced_fonts.py --pack /path/to/source.zip
python scripts/port/test_enhanced_fonts.py
```

The generator verifies the archive SHA256, maps Rice-format pack identifiers
to the inherited font tables, adapts power-of-two padding to native cells,
and losslessly encodes the high-resolution intensity masks. It writes no
original glyph pixels into its output. Builds and CI use checked-in community
masks without requiring Pillow or a ROM. `GE_FONT_AUDIT=1` logs successful
replacement uploads for isolated visual audits.
