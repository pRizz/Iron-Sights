# Open assets and sharp text plan

Iron Sights will draw its text with original, MIT-licensed fonts through a
renderer that stays sharp at any resolution. The fonts are the first of a
planned set of open game assets, kept in their own repository so other projects
can use them too. The owner accepted this plan and its recommended decisions on
2026-11-05. Acceptance is not evidence that any milestone works; the
[milestone status](#milestone-status) below records what has been built and
verified.

## Why

- The fork removed every bundled third-party art set on 2026-11-05: the
  GoldenEye 007 HD textures and ammo icons (FORK-033, FORK-047), whose maintainer
  disallowed bundling in ports, and the community enhanced fonts (FORK-021),
  which were redrawn from Rare's glyphs. Every target now draws the ROM's own
  fonts.
- The ROM fonts are small bitmaps drawn for the game's 440x330 page, enlarged
  about 2 times at 720p and 6.5 times at 4K, so full-size text stays soft at
  high resolutions. FORK-049 fixed scaled text's spacing, but not its sharpness.
- Assets the fork owns outright need no permission, can ship in every build
  (including the browser) and can be shared with other ports and emulators.

## Licensing and provenance rules

- Everything we create is MIT licensed: font sources, generators and generated
  outputs. Third-party tools used to build them (for example `msdfgen`, MIT, or
  `fontTools`, MIT) are build tools, not shipped content.
- Draw from scratch. Never trace the ROM's glyphs, the removed community masks
  or any other pack, and never copy their outlines, coordinates, pixels or
  metrics into our sources. Capturing the general style of a 1990s bold
  grotesque and a squared geometric display face is the aim; reproducing any
  particular design is not.
- Local comparison is allowed (owner's decision, 2026-11-05). The owner may
  compare our glyphs with the game's own using
  `scripts/port/compare_open_font.py`, which draws each game glyph, ours, an
  overlay (overlap in red) and a difference, with an overlap share, into the
  ignored `build_win/font-compare/`. The output is game-derived and is never
  committed anywhere. The owner may use what it shows to adjust our fonts toward
  the original's spirit; such changes are redrawn in our own sources and their
  provenance note says a comparison informed them. Title font v0.1.0 was drawn
  before the first comparison.
- Do not use the names Zurich, Bank Gothic or GoldenEye for the fonts.
- Keep ROM-derived data out of the assets repository, including glyph metrics.
  Iron Sights reads the ROM's character widths, baselines and kerning at run
  time from the player's ROM, as it does today.
- Fonts derived from an existing open font keep that font's license (OFL or
  Apache-2.0) and cannot be relabelled MIT. Such fonts are allowed only as
  development placeholders and do not ship.
- Contributions to the assets repository are accepted under MIT with a
  Developer Certificate of Origin sign-off and a short provenance note per
  change (who drew it, from what, with which tools).
- This is not legal advice. Typeface designs are protected in some countries
  (for example the UK and Germany) and not in others (the US), which is why the
  plan insists on original drawings.

## Repository layout: hybrid

A separate MIT repository is the source of truth. Iron Sights vendors its built
outputs at a pinned version.

| | Assets repository | Iron Sights |
| --- | --- | --- |
| Holds | Font sources, generators, tests, CI, versioned releases | A pinned copy of the release outputs it uses |
| License | MIT throughout | Vendored outputs keep the assets repository's MIT notice |
| Builds | Deterministic: sources to fonts, atlases and metrics | Offline and reproducible; no network at build time |
| Updates | Tagged releases with checksums | `scripts/port/update_open_assets.py` fetches a tag, verifies its SHA-256 and records it in `build-inputs.json` |

Why hybrid rather than one place:

- **In this repository only:** fastest to iterate, but Iron Sights' LICENSE
  covers code only and excludes game-derived material, so asset terms would be
  harder to read, and other projects would have to take the whole port.
- **Separate, fetched at build time:** clean licensing, but builds would need
  the network and could break when a release moves.
- **Hybrid:** clean MIT licensing and independent releases, while Iron Sights
  stays buildable offline. The vendored outputs are small (a few atlas images
  and JSON metrics, a few hundred KB).

The repository is `pRizz/iron-sights-assets` (decision 1 below): neutral and
free of game trademarks.

## Fonts

The game uses two fonts, each with 94 printable ASCII glyphs in the US version
(`struct font`: 13x13 kerning classes and per-glyph width, height and baseline).

| Role | Style | How we make it |
| --- | --- | --- |
| Body text (Zurich Bold's role) | Bold neo-grotesque sans | Drawn by hand as vector outlines in a font editor, stored as UFO (plain-text, diffable) |
| Titles and HUD (Bank Gothic's role) | Squared geometric capitals | Generated by a parametric script (stroke weight, corner radius, proportions), so any change regenerates every glyph |

Glyphs are designed on their own metrics. Iron Sights keeps the ROM's advances,
kerning and line spacing, so layout, wrapping and every menu stay exactly as
they are. It places each of our glyphs in the ROM glyph's cell by aligning
baselines and scaling to a common cap height, with the ink centred in the
cell's advance. A local fit check in Iron Sights (`scripts/port/check_open_fonts.py`,
needs the player's ROM, commits nothing derived from it) reports glyphs whose
ink would crowd their neighbours, so the designs can be adjusted.

Japanese text and characters outside the 94 keep the ROM's glyphs. PAL's extra
accented characters (135 per font) are out of scope until the fork supports a
PAL ROM.

## Renderer design

The game draws every glyph as its own texture rectangle (`textRenderGlyph`),
with outlines drawn as eight offset copies and list clipping done by cutting
the rectangle and its texture coordinates. The plan keeps that pipeline and
swaps only what the rectangle samples:

1. **Glyph registry.** When the ROM fonts load, a fork hook registers each
   glyph's pixel-data pointer with its font and character. The renderer looks
   up a texture load by address, which is exact and needs no fingerprints.
2. **Atlas substitution.** For a registered glyph, the renderer binds the font's
   multi-channel signed distance field (MSDF) atlas instead of the ROM bitmap
   and maps the rectangle's texture coordinates (in ROM texel units, already
   including any clipping) into the glyph's atlas cell. The quad grows by the
   glyph's ink overhang and the field's padding, as the glyph-border code does
   for filtered glyphs today.
3. **MSDF shader.** A new `SHADER_OPT_MSDF` bit in the existing shader
   generator decodes the field (median of three channels, edge width from
   screen-space derivatives) into coverage, multiplied by the combiner's colour
   as before. This works in desktop OpenGL and the browser's WebGL 2.
4. **Positions.** FORK-049's sub-pixel pen and precise-rect tag already give
   each scaled glyph exact corners; full-size glyphs keep whole-pixel positions,
   which the ROM metrics were designed for.
5. **Fallback.** Unregistered glyphs, Japanese text, a missing atlas or the
   setting turned off use today's path unchanged. The N64 build is untouched.

A new **Gameplay > Iron Sights > System** setting (working name Sharp Fonts,
`[display] sharp_fonts`) switches between our fonts and the ROM's in both the
Options folder and the watch. It defaults to off until both fonts exist and the
owner has play-tested them (decision 4).

MSDF is recommended over the alternatives:

- **Higher-resolution bitmaps** (the removed approach) still blur or alias at
  sizes they were not drawn for.
- **Rasterizing at the exact pixel size at run time** (`stb_truetype`, public
  domain) is slightly crisper at very small sizes, but needs a glyph cache keyed
  by size and more CPU work, and the game draws text at many sizes.
- **MSDF** keeps corners sharp at every size from one small atlas per font, with
  a cheap shader, and suits the squared title font especially well. Very small
  text (under about 12 screen pixels) is slightly softer; that can be tuned
  with the field range or a small-size bitmap tier later if captures show it.

### OPEN-4 implementation design (2026-11-05)

- **Embedding.** CMake turns the vendored `atlas.png` and `metrics.json` into a
  generated C file at configure time (pure CMake `file(READ ... HEX)`, so no
  tool is needed at build time, and a change re-runs configure). At start-up
  the port decodes the PNG with `puff.c` (Mark Adler, zlib license, vendored
  from `tools/extractor`; bounds-checked, no global state) plus a small
  unfilter step, and parses the metrics with the vendored picojson.
- **Registry.** `textrelated.c`'s font loader, in a fork block, registers each
  glyph's pixel pointer, width, height and baseline, and each font's reference
  capital (`H`). Bank Gothic maps to the title font; Zurich Bold stays on the
  ROM path until the body font exists.
- **Placement.** Our capitals match the ROM capital's ink as it reads on
  screen (edges where its interpolated row intensity crosses a quarter of
  its peak), sitting on that ink's foot with ink centred in the ROM glyph's
  width; glyphs that would overflow that width are squeezed horizontally.
  The ROM keeps the advance, kerning and line spacing.
- **Per-glyph textures.** Importing a registered glyph's texture with Sharp
  Fonts on uploads an RGBA image resampled from the atlas over the union of
  the ROM glyph box and our padded glyph box, at the atlas's own density, so
  the field's pixel range is unchanged. A cache-key flag keeps it apart from
  the ROM bitmap, so toggling the setting re-imports.
- **Quads.** The texture-rectangle path grows a registered glyph's quad to
  that union (sides always; top and bottom only where a clipped list did not
  cut the glyph) with sub-pixel corners, rewrites its UVs to the union, and
  skips the half-texel glyph border. Flipped (rotated) text and copy-mode
  rects keep the ROM path.
- **Shader.** `SHADER_OPT_MSDF` (bit 31) samples the texture directly (no
  3-point filter), takes the median of RGB, converts it to coverage over one
  screen pixel with `fwidth`, and feeds that as the texel to the unchanged
  combiner, so colours, outlines and fades work as before.
- **Setting.** Sharp Fonts (`[display] sharp_fonts`, default 0) on Iron Sights
  > System in the Options folder and the watch. FONT-1 of the
  [font styles plan](font_styles_plan.md) replaced it with Fonts (`[display]
  font_style`: Original, Enhanced, Iron Sights); `sharp_fonts=1` loads as
  Iron Sights.

## Use outside Iron Sights

The fonts are usable directly by anyone (TTF/OTF and atlases, MIT). An
emulator texture pack, matching the ROM's glyph textures by checksum for
GLideN64-style high-resolution packs, cannot ship ready-made without ROM-derived
data, so it would be built on the player's machine: an exporter reads their ROM
for glyph layout and checksums and renders our fonts into those textures.
Iron Sights keeps the better path (MSDF, sub-pixel layout, any resolution).
OPEN-6 is deferred (decision 5).

## Future assets

The assets repository is meant to grow beyond fonts: HUD icons, menu art and,
eventually, environment and weapon textures and models, all original and MIT,
each with a ROM-free generator or source files and a local exporter for other
ports where useful. No work on these is planned yet; this section records the
intent so the repository's layout, provenance rules and release process are
designed for more than fonts from the start.

## Milestones

| ID | Milestone | Done when |
| --- | --- | --- |
| OPEN-1 | Assets repository | MIT LICENSE, README with these rules and the future-assets intent, contribution and provenance guide, CI that rebuilds outputs deterministically and fails on drift, first tagged release (empty or test glyphs) |
| OPEN-2 | Title font v0.1 | Parametric generator produces all 94 glyphs; outputs TTF, MSDF atlas and metrics JSON; ROM-free tests check coverage, determinism and field validity |
| OPEN-3 | Vendoring | `update_open_assets.py` fetches a tag, verifies checksums, writes the outputs and records the version in `build-inputs.json`; packages ship the assets' MIT notice |
| OPEN-4 | Renderer | Glyph registry, atlas substitution, `SHADER_OPT_MSDF`, quad growth, setting and fallback; native and browser builds; captures of menus, HUD, debrief and watch at 720p, 1440p and 4K with outlines and clipped lists; full-size ROM text pixel-identical with the setting off |
| OPEN-5 | Body font v1.0 | All 94 glyphs drawn from scratch; fit check clean against the ROM metrics; owner review of the look in play |
| OPEN-6 | Emulator pack export (optional) | Local exporter builds a GLideN64-style pack from the player's ROM; verified in one emulator |

OPEN-2 and OPEN-4 can proceed together: the renderer can be developed against
the title font and a non-shipping placeholder body font.

## Decisions (accepted 2026-11-05)

1. **Assets repository:** `pRizz/iron-sights-assets`, public, MIT. The owner
   confirmed creating it, and it was created on 2026-11-05.
2. **Body-font placeholder:** Arimo (Apache-2.0) during development only, never
   shipped or vendored into a release.
3. **Renderer technique:** MSDF.
4. **Sharp Fonts default:** on once both fonts exist and the owner has
   play-tested them in the game. Until then it defaults to off. (The proposal
   offered this without a recommendation; this default follows the owner's
   practice of play-testing visual changes before they ship.)
5. **OPEN-6 (emulator export):** deferred.

## Milestone status

Update the relevant row with changes, verification evidence, blockers and the
next action before handing off.

| ID | Status | Notes |
| --- | --- | --- |
| OPEN-1 | Done | 2026-11-05: public [`pRizz/iron-sights-assets`](https://github.com/pRizz/iron-sights-assets) (MIT, detected by GitHub) with the provenance rules, roadmap and future-assets intent in its README, a CONTRIBUTING guide requiring a DCO sign-off and a provenance note, and Bright Builds Rules installed as in Iron Sights (auto-update disabled). CI runs the ROM-free tests, the Bright Builds checks and a drift check rebuilding `dist/` (bytes for TTF/JSON, pixels for PNGs) on every push; `v*` tags publish per-font archives, the license and SHA-256 checksums via `tools/package_release.py`. First release: [v0.1.0](https://github.com/pRizz/iron-sights-assets/releases/tag/v0.1.0), checksums verified after download, and a Windows repack matches it byte for byte (`c4038ca` fixed a case-insensitive sort that had made local packs differ) |
| OPEN-2 | Done | 2026-11-05: title font [v0.1.0](https://github.com/pRizz/iron-sights-assets/releases/tag/v0.1.0): a parametric generator (centerline skeletons stroked, unioned, clipped and softened with Clipper's integer geometry), all 94 glyphs with small-cap lowercase and equal-width figures, `IronSightsTitle-Regular.ttf`, an MTSDF atlas from our own numpy generator (48 px/em, 6 px range), `metrics.json` and a specimen drawn from the atlas. After the owner's review (`306c627`), A, M, W, Y and 7 use the classic diagonal structures, closer to the game lettering's general style without tracing or measuring it, and &, @ and ~ were redrawn. Ten ROM-free tests pass (coverage, heights, ink inside the advance, clean contours, the font file, field validity, corner colouring, determinism, reproducible packing). Follow-up [v0.2.0](https://github.com/pRizz/iron-sights-assets/releases/tag/v0.2.0) (released 2026-11-05; checksums verified, Windows repack identical): after comparing v0.1.0 with the game's glyphs locally, the owner asked for 17 changes (A, C, M, Q, S, W; 2, 3, 4, 5, 7, 9; %, #, $, &, ?), redrawn in our own skeletons and recorded as comparison-informed; the mean overlap share across all 94 glyphs rose from 34% to 35%, with $ at 66% and 4 at 54%. `f8f9ec1` shortened the hooks on C, S, 2, 3 and 7 and added the ampersand's down-right leg, and `a5018ee` gave the 9 a bigger loop, both at the owner's request (`1938cce` started the series). Iron Sights should vendor v0.2.0 in OPEN-3. [v0.3.0](https://github.com/pRizz/iron-sights-assets/releases/tag/v0.3.0) (released 2026-11-05, `84578c3`) raises the small capitals from 76% to 85% of the capital height after the owner's Sharp Fonts play-test, a proportion informed by locally comparing the game font's (about 84%) |
| OPEN-3 | Done | 2026-11-05: `scripts/port/update_open_assets.py --tag v0.2.0` downloaded the release's SHA256SUMS, LICENSE and `title-v0.2.0.tar.gz`, checked each against SHA256SUMS, and vendored the LICENSE plus `title/atlas.png` and `title/metrics.json` byte for byte into `port/fast3d/open_assets/`, with `manifest.json` recording the tag, archive checksum and each file's SHA-256 (`.gitattributes` keeps the folder byte-exact). `--verify` rechecks offline; `test_open_assets.py` (in the release workflow) also checks the MIT license, the 94 glyphs, the version against the tag and the atlas size against its PNG header. `build-inputs.json` records the release (`release_inputs.py`); Windows and Linux packages ship `Open-Assets-LICENSE.txt` (the package test requires it), the browser site lists it, and THIRD_PARTY_NOTICES describes it as vendored source used once the renderer lands. The atlas stays a PNG: the port has no PNG decoder and the distance field barely run-length compresses, so how it is embedded is decided in OPEN-4. During OPEN-4 the same script vendored v0.3.0 |
| OPEN-4 | In progress | 2026-11-05: implemented as FORK-051 per the design above (Sharp Fonts, default off): configure-time embedding, puff PNG decoding, the glyph registry, per-glyph field images, quad growth with sub-pixel corners and `SHADER_OPT_MSDF`. Windows build verified: setting off is pixel-identical to the previous build on the video menu; setting on renders the HUD and watch crisply at 720p and 900p, with outlines; `test_open_fonts.sh` passes. The browser build compiles and links. The owner's first play-test found letters too tall, too wide and overlapping; placement now matches the ROM capital's on-screen ink height and squeezes glyphs wider than their ROM space, so no letters overlap. The mission hints that seemed to confirm the height are small capitals, which were short (76% of the capital against the ROM's 84%); the owner asked for taller ones, and v0.3.0 (85%) brings the hints' ink height to 9.3 and 8.2 px against the ROM's 9.3 and 9.0. Remaining: owner play-test of the fix, the browser at run time, 1440p/4K captures, a clipped list and the debrief |
| OPEN-5 | Not started | |
| OPEN-6 | Deferred | |

## Technical references

- Glyph structures: `struct font` and `struct fontchar` in `src/bondtypes.h`.
- Text drawing: `textRender`, `textRenderGlyph`, `textRenderOutlined` and
  `textMeasure` in `src/game/textrelated.c`; the scaled-text pen (FORK-049).
- Glyph tagging (`G_NOOP` 0x47), the precise-rect tag (0x59) and texture
  rectangles: `port/fast3d/gfx_pc.cpp`.
- Shader option bits: `port/fast3d/gfx_cc.h`; shader generation in
  `port/fast3d/gfx_opengl.cpp`.
- Removal records: FORK-021, FORK-033 and FORK-047 in `FORK_CHANGES.md`.
