# Font styles plan

Iron Sights will offer three text styles: the ROM's **Original** fonts, the
community **Enhanced** fonts, and our own **Iron Sights** font. The Enhanced
fonts are never bundled: when the player chooses them, the game asks before
downloading them from their original repository, verifies each file and
installs it where a manual install would go. The owner accepted this plan
and its recommended decisions on 2026-11-05. Acceptance is not evidence that
any milestone works; the [milestone status](#milestone-status) records what
has been built and verified.

## Why

- The fork embedded Carnivorous's enhanced Bank Gothic and Zurich Bold masks
  until 2026-11-05 (FORK-021). They were removed because they are redrawn
  versions of Rare's glyphs, so the pack author's CC0 dedication could not clear
  the underlying rights, and because the GoldenEye 007 HD maintainer had
  asked that texture packs stay external mods that players add themselves
  ([GhostlyDark/GoldenEye-007-HD#3](https://github.com/GhostlyDark/GoldenEye-007-HD/issues/3)).
  Nothing about their license has changed since.
- Players liked them, and the open fonts (FORK-050, FORK-051) cover only Bank
  Gothic so far.
- A player-installed mod that the game can fetch on request keeps the fonts
  external: Iron Sights ships a loader, a file list and checksums, never the art.

## Ground rules

- **Never bundled or hosted.** No target, package, release or Git commit
  contains the Enhanced font images or masks derived from them. The package
  test that rejects `Enhanced-Fonts-*` files stays, and gains a check that no
  file from the pack's list ships.
- **Downloaded only on the player's request.** The game contacts the network
  only after the player picks Download in the consent prompt. A `midas.ini`
  that selects Enhanced without the files installed falls back to Original
  and does not download.
- **From the original repository.** Files come from
  [Graslu/GEPDTexturePacks](https://github.com/Graslu/GEPDTexturePacks)
  (CC0-1.0, archived) at the commit the fork used before,
  `ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63`, which is also that repository's
  last. Each file is fetched from `raw.githubusercontent.com` at that commit:
  186 PNGs totalling 247,988 bytes (checked 2026-11-05).
- **Verified.** The source code pins each file's path, size and SHA-256. A
  file that does not match is discarded; a partial download is never used.
- **What we commit.** Code, the file list (paths, sizes, checksums) and the
  pack's source, commit and license. The pack's file names contain each
  texture's Rice CRC, which the game computes at run time from the player's
  own ROM glyphs to match files to glyphs; no ROM-derived data or pack art is
  committed.
- **Caveat.** Helping players fetch Rare-derived art is the same position as
  any port that supports player-installed texture packs; it is not a licence
  to the art. If the pack's author, its host or a rights holder objects, the
  download is removed and Enhanced becomes manual-install only, or is removed.

## The setting

**Gameplay > Iron Sights > System > Fonts**, in the Options folder and the
watch, replaces the Sharp Fonts row (FORK-051):

| Choice | Bank Gothic (HUD, watch, titles) | Zurich Bold (briefings, folders) |
| --- | --- | --- |
| Original | ROM | ROM |
| Enhanced | GEPD masks | GEPD masks |
| Iron Sights | Open title font (MSDF) | ROM, until the open body font (OPEN-5) |

- Key `[display] font_style`: 0 Original, 1 Enhanced, 2 Iron Sights; default 0.
  An existing `sharp_fonts=1` with no `font_style` line loads as 2, and saving
  writes only `font_style`. The pack has 93 of each font's 94 glyphs; the
  one it lacks keeps the ROM glyph.
- The value column shows `Iron Sights`; the help text names it the Iron Sights
  custom font. With the pack missing, the Enhanced value reads
  `Enhanced (get)` and choosing it opens the prompt instead of applying.
- A **Remove Enhanced Fonts** action on the same page deletes the installed
  files (asking once, like the watch's armed Reset/Delete rows) and falls back
  to Original if Enhanced was selected.

## The consent prompt and download

1. Choosing Enhanced without the files opens a prompt in the menu that was
   used (Options folder or watch): "Enhanced fonts are a community redraw by
   Carnivorous (CC0), not included with Iron Sights. Download 186 files (about
   250 KB) from github.com/Graslu/GEPDTexturePacks?" with **Download** and
   **Cancel**, starting on Cancel.
2. Download runs on a worker thread with a progress line (files done of 186)
   and Cancel. The game keeps running and drawing.
3. Each file is checked against its pinned size and SHA-256, written to a
   temporary name and renamed into `Data/FontPacks/gepd/` when complete. The
   last step writes `SOURCE.txt` naming the repository, commit, author and
   CC0 license.
4. On success the setting applies Enhanced. On failure (offline, HTTP error,
   checksum mismatch, disk full) the prompt says what failed and offers Retry
   and Cancel; already verified files are kept, and the setting stays on
   Original.
5. A manual install (copying the same files into the folder) is detected the
   same way, for offline machines and Steam Deck Game Mode.

Every input confirms, cancels and backs out of the prompt and progress
screen: keyboard (Enter/Escape), mouse (pressing a choice, as the folder
erase dialog does), gamepad (A/B) and browser touch (tap). Prompts name the
control the current input has.

### Transports

| Target | Transport |
| --- | --- |
| Windows | WinHTTP (part of Windows; no new dependency) |
| Linux, Steam Deck | libcurl loaded at run time (`libcurl.so.4`); without it the prompt explains the manual install. FONT-3 checks whether the Linux crash reporter already links it |
| Browser | `fetch()` (the files are served with `Access-Control-Allow-Origin: *`), stored in the persistent browser file system with the saves |

## Renderer

The Enhanced path returns from FORK-021's history (`378bee0^`), rebuilt on
FORK-051's glyph registry so both styles share one hook:

- **Loading.** On the first Enhanced glyph (and after a download) the port
  reads the folder, checks each file's size and SHA-256 against the list and
  decodes the PNG with the vendored puff and unfilter step. The list maps each
  file to its font and character. (The design first matched files by the
  Rice CRC in their names; the game clears glyph padding before registering
  glyphs, which changes that CRC, so FONT-2 uses the pinned mapping.)
- **Masks.** The old generator's preparation moves to run time: take the
  image's luminance as an 8x mask, keep authored X coordinates with at most one
  native pixel of overhang, fit only glyphs taller or wider than allowed
  (area filtering instead of Lanczos), and trim fringe that would touch a
  tightly kerned neighbour, comparing the whole font first so results do not
  depend on order.
- **Drawing.** The texture import uploads the mask under a cache-key flag of
  its own, the texture rectangle grows the quad to the mask's ink, and the
  mask is smooth-sampled whatever 2D Filtering says, as before. Advances,
  kerning and line spacing stay the ROM's.

## Milestones

| ID | Milestone | Done when |
| --- | --- | --- |
| FONT-1 | Setting | `font_style` replaces Sharp Fonts in both menus with help, ini help and settings-placement entries; `sharp_fonts` migrates; Enhanced without files falls back to Original; Original and Iron Sights render as today |
| FONT-2 | Enhanced renderer and manual install | Pinned file list, folder scan with checksum checks, PNG decode, run-time mask preparation, registry and quad hooks; ROM-free tests with synthetic pack files (bad checksum, missing glyph, kerning trim); captures of both fonts with a local copy of the pack |
| FONT-3 | Native download | Consent prompt and progress in the Options folder and watch for every input; WinHTTP and libcurl workers with cancel, retry, atomic writes and `SOURCE.txt`; remove action; tests against a local HTTP fixture for success, cancel, mismatch and offline |
| FONT-4 | Browser | `fetch()` download into persistent storage, the prompt with touch controls and audits; or Enhanced recorded as unsupported in the browser with its reason |
| FONT-5 | Release | Owner play-test of all three styles; README, THIRD_PARTY_NOTICES and release notes describe the download; package checks confirm no pack file ships |

## Decisions (accepted 2026-11-05)

1. **Setting:** one Fonts row with Original / Enhanced / Iron Sights, key
   `font_style`, default Original, replacing Sharp Fonts.
2. **Source:** single files from `raw.githubusercontent.com` at commit
   `ca06e063`, pinned by checksum, rather than the repository's 32 MB
   archive.
3. **Install folder:** `Data/FontPacks/gepd/`, files under their pack names.
4. **Linux:** libcurl loaded at run time, manual install otherwise.
5. **Browser:** include the download in FONT-4 rather than leave Enhanced
   native-only; it needs no new transport.
6. **Order:** FONT-1, then FONT-2 (useful on its own through manual
   install), then FONT-3 and FONT-4.

## Milestone status

Update the relevant row with changes, verification evidence, blockers and the
next action before handing off.

| ID | Status | Notes |
| --- | --- | --- |
| FONT-1 | Done | 2026-11-05: `[display] font_style` (0 Original, 1 Enhanced, 2 Iron Sights; default 0) replaces Sharp Fonts as **Fonts** on Iron Sights > System in the Options folder and the watch, with help, ini help, README and notices updated. `sharp_fonts=1` loads as Iron Sights unless `font_style` is present; saving writes only `font_style`. Until FONT-2, Enhanced draws the ROM fonts and its help says it is not yet available; the value reads `Enhanced`, and the `Enhanced (get)` label and prompt come with FONT-3. Verified: Windows build; settings placement and standards checks; 720p HUD captures draw the ROM font for 0 and 1 and the open font for 2 and for a file with only `sharp_fonts=1`; the watch page shows Fonts reading Iron Sights. `audit_hud.ps1 -FontStyle` and `audit_sights.ps1 -WatchPage` support these captures. Next: FONT-2 |
| FONT-2 | Done | 2026-11-05: `enhanced_fonts_list.inc` pins the 186 files; `enhanced_fonts.cpp` reads `Data/FontPacks/gepd/`, verifies each file, decodes it (`png_decode.cpp`, now shared with the open fonts) and prepares 8x masks at run time (`enhanced_font_masks.cpp`), which the renderer uploads, smooth-samples and draws with FORK-021's quad growth. Deviation from the design above: files map to glyphs by character from the pinned list, not by computing Rice CRCs, because the game clears glyph padding before registering glyphs. The kerning trim needs each font's kerning table, so `portEnhancedFontsGlyph` registers it. Verified: all 186 run-time masks byte-identical to the FORK-021 generator's (local harness with the downloaded files); `test_enhanced_fonts.sh` (ROM-free, no pack files, in CI); Windows build; GCC 11 compile; 720p HUD and 960p difficulty-screen captures with and without the files. `enhanced_fonts_pack.py install` is the manual install. Next: FONT-3 |
| FONT-3 | Done | 2026-11-05: an **Enhanced Fonts** page under System in the folder and on the watch carries the consent text, a status line, Download / Cancel / Retry / Use (or Check Folder Again) and Remove (asks first); `enhanced_fonts_download.cpp` fetches, verifies, writes `.part` and renames, then writes `SOURCE.txt`, on a worker thread through WinHTTP or run-time-loaded libcurl. Deviations from the design above: choosing Enhanced on the Fonts row does not open the prompt (stepping through values would jump pages); its value reads `Enhanced (Get)` instead and the page is the next row; the watch page sits before the Control tab's pages, which moves their `GE_WATCHPAGE` numbers up by one. Verified: engine tests with a fake transport; a Linux libcurl run against a local server; Windows captures of download, real GitHub download, cancel, failure, remove, the System row and the watch page. Not covered: the watch's armed Remove at run time, Linux and browser builds at run time. Next: FONT-4 |
| FONT-4 | Not started | |
| FONT-5 | Not started | |

## Technical references

- FORK-021 (removed Enhanced Fonts) and its last tree, `378bee0^`:
  `port/fast3d/enhanced_fonts.*`, `scripts/port/generate_enhanced_fonts.py`
  and `port/fast3d/fonts/sources.json` (the 186 file paths and checksums).
- FORK-051 and the [open assets plan](open_assets_plan.md): the glyph
  registry, PNG decoding and texture-rectangle hooks this plan reuses.
- The folder erase dialog in `src/game/front.c` and the watch's armed rows in
  `port/src/port_watchpages.c`: existing confirm patterns for every input.
