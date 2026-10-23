# Project Midas ADS: fork change inventory

This is the maintained inventory of additions and modifications made by this
fork. Update it whenever repository work changes an entry or introduces a new
area, following [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Comparison baseline and scope

- **Upstream project:** [murkantor/Project-Midas](https://github.com/murkantor/Project-Midas).
- **Inherited baseline:** Project Midas 1.0.1,
  [`6be5920beed453b6a81fdf9935fb9851928b718d`](https://github.com/murkantor/Project-Midas/commit/6be5920beed453b6a81fdf9935fb9851928b718d).
- **Initial inventory:** 2026-10-22, covering the fork commits after that baseline
  and the working tree at initialization. At that point, several HUD/rendering/
  icon changes were uncommitted; inclusion here does not imply a release, commit,
  or push.

This describes our differences from the inherited baseline, rather than a live
comparison with upstream's latest branch. The original PC port, its existing
menus, multiplayer features, ROM loaders and other inherited functionality are
not claimed as additions by this fork.

Keep this as a current feature map: update an entry when its implementation
changes, retain useful replacement/history notes, and add separate entries for
new areas. Status describes implementation state, not Git publication state.

## Inventory

### FORK-001 — Repository binary hygiene

**Status:** Implemented.

Removed the tracked `tools/GELauncher.exe`, `tools/SGIImageViewer.exe` and a Python
bytecode cache. Added an ignored `quarantine/` location for pre-existing binaries
moved out of tools. The repository continues to exclude local executables,
build output and ROM-derived assets from version control.

Local rebuilds replace executables without retaining old-build backups. The
user accepts losing compatibility with old suspend points. Obsolete game
executables and suspend images matched to those builds may be purged without
archiving or another confirmation. Keep the current main executable and its
states, unknown images, normal campaign saves, profiles and settings. The state
audit removes its temporary executable on exit and retains diagnostic logs
and captures. This supersedes briefly retaining the pre-toast executable for
older states.

**Sources:** [.gitignore](.gitignore), [artifact rules](AGENTS.md),
[state audit](scripts/port/audit_states.ps1),
[state storage guide](scripts/port/save_states.md). **History:** `29a9e6e`.
**Verification:** Git tracked-file changes in that commit. Local cleanup uses
executable/state build fingerprints and workspace-confined file paths; the
audit cleanup is checked with the isolated runtime audit.

### FORK-002 — Fork identity and gameplay documentation

**Status:** Implemented.

Named this independent fork **Project Midas ADS**, retained its upstream link,
and documented the added controls, settings and gameplay behavior. Put the
latest gameplay video near the top of the README with a screenshot thumbnail
and YouTube play badge; the newer video supersedes the original demo link.
The README introduces the fork before the demo, prominently links this full
inventory near the top, and summarizes ADS, sniper zoom, sprint, smoother
sights/combat feedback, objective alerts, the weapon strip, adaptive
health/armor and solo suspend points. Contributor inventory-maintenance instructions sit under
Contributing, alongside the existing contribution links.

**Sources:** [readme.md](readme.md),
[gameplay thumbnail](.github/images/ads-gameplay-thumbnail.png),
[settings reference](tools/midas_ini_help.txt).
**History:** `9e46d0d`, `aefb55a`, `74ce634`, `5edcf1a`, `4e37864`.
**Verification:** README video/image targets and documented settings.

### FORK-003 — ADS controls, movement and weapon alignment

**Status:** Implemented.

Added aim-down-sights for 20 supported firearms with individual weapon poses
and sight alignment. Right mouse defaults to ADS; **Gameplay > General Settings
> Aim Mode** can restore the red reticle. The existing hold/toggle aim setting
applies to either mode. Middle mouse remains a separate, rebindable hold-to-ADS
action. Players can walk and strafe at half speed while aiming. Per-player ADS
state and simulation-time transitions support different tick rates. Refined the
view to reveal front sights with a slight perspective along the top of each gun.
Recalibrated the AR33/M4 square sight aperture and RC-P90 sight tip vertically
against the centered shot ray after the perspective adjustment left them low.
The correction changes only those ADS poses. The AR33 also uses a little more
muzzle-up tilt to keep the receiver clear of its square opening; eye distances
and the RC-P90's pitch are retained. A subsequent long-gun audit also raised
the Klobb, ZMG, both D5K variants and regular shotgun to correct smaller
vertical sight offsets. These use individual height corrections; the other
audited long-gun poses are retained.
The silenced D5K explicitly compensates for its longer muzzle node so its
shared sight mesh receives the same pose as the unsilenced D5K.

**Sources:** [ADS interface](port/include/port_ads.h),
[input/state and poses](port/src/joy_port.c), [mouse input](port/src/input_pc.c),
[camera and movement](src/game/bondview2.c), [gun rendering](src/game/gunfire.c),
the shared port settings table and frontend/watch settings pages.
**History:** `d017318`, `79d3892`.
**Checks:** [ADS tests](scripts/port/test_ads.sh),
[ADS visual audit](scripts/port/audit_ads.ps1).
The audit skips the intro and hides persistent HUD elements for clear sight
inspection; its contact sheet marks the screen center for calibration.
Targeted captures checked both corrected sights at wide FOV 90/110, the AR33
in 4:3, and raise/lower transitions for both weapons. ADS regression tests and
Windows/Linux builds passed with the corrected poses.
The [long-gun alignment audit](scripts/port/ads_alignment.md) records the
remaining weapon review and per-weapon corrections. The audit tool also emits
enlarged `sight-details.png` sheets. A ROM-free regression test checks matching
D5K sight poses across the two different muzzle references.
The final audit captured all 20 ADS weapons in wide/4:3 at 60 Hz and the five
changed guns at wide FOV 110/120 Hz, with 11 transition captures each. Visual
review, ADS regression tests, and Windows/Linux builds passed.

### FORK-004 — ADS hands, forearms and transition occlusion

**Status:** Implemented.

Added basic hands and forearms for the 13 firearms that lack them during ADS,
with per-weapon trigger/support grips. Increased their thickness and proportions
through subsequent refinements. Pistols retain their original hands. Corrected
the rendering order/depth treatment so the original hand and added forearm
occlude correctly while ADS animates in, including during partial blends.

**Sources:** [hands and grips](port/src/ads_hands.c),
[ADS pose data](port/src/joy_port.c), [gun rendering](src/game/gunfire.c).
**History:** `2fcbb39`, `79d3892`, `599173d`.
**Checks:** ADS pose/hand-layer regression tests and visual audits.

### FORK-005 — Sniper scope presentation and contextual wheel zoom

**Status:** Implemented.

Sniper ADS animates into a zoomed scope view, hiding the gun and hands while
showing centered red crosshairs. Mouse movement turns the whole view. Wheel
up/down adjusts magnification only while ADS with the sniper; elsewhere, normal
wheel bindings apply. Existing zoom controls remain available.

**Sources:** [ADS and wheel handling](port/src/joy_port.c),
[mouse wheel dispatch](port/src/input_pc.c), [scope rendering](src/game/gunfire.c).
**History:** `79d3892`, `45e6439`.
**Checks:** ADS scope/wheel tests; [sight audit](scripts/port/audit_sights.ps1).

### FORK-006 — Configurable Shift sprint

**Status:** Implemented.

Added **Gameplay > General Settings > Sprint**, on by default. Holding the
rebindable **Walk / Sprint** action (Left Shift by default) boosts movement to
1.5 times normal speed. Aiming and crouching prevent the sprint boost. Turning
the setting off restores the original half-speed Shift-to-walk behavior; aim
and walk slowdowns do not stack.

**Sources:** [movement/input](port/src/joy_port.c),
[option definitions](port/src/port_options.c), frontend/watch settings pages,
[settings reference](tools/midas_ini_help.txt).
**History:** `939b400`. **Checks:** ADS test suite movement/settings cases.

### FORK-007 — Optional combat feedback

**Status:** Implemented.

Added default-on **Hit Markers** and **Damage Direction** HUD settings. Confirmed
character damage produces a thin white hit marker; headshots use gold.
Directional red damage arcs point toward the source and track the player's view
as it turns. Fixed the initial left/right reversal and replaced blocky lines
and segmented arcs with smooth strokes/curves. Feedback state is per player,
uses simulation time, and hides when gameplay controls are unavailable.

**Sources:** [HUD state/rendering](port/src/port_hud.c),
[character damage](src/game/chraction.c), [explosions](src/game/explosion.c),
[player damage](src/game/bondwalk2.c), [PC overlay renderer](port/fast3d/gfx_pc.cpp).
**Checks:** [HUD tests](scripts/port/test_hud.sh) cover source direction across
headings, player isolation and timing; [HUD audit](scripts/port/audit_hud.ps1)
covers white/gold feedback, solo and split-screen views.

### FORK-008 — Named objective notifications

**Status:** Implemented.

Added default-on **Objective Alerts** for solo missions. Queued notifications
show the objective letter, name and updated status, with completion/failure
colors. Disabling the setting restores the original messages. Notifications
reset with mission state and do not run as multiplayer objective alerts.

**Sources:** [objective updates](src/game/objective_status.c),
[mission reset](src/game/objective.c), [notification queue](port/src/port_hud.c).
**Checks:** HUD queue/filter/timing tests and completed/failed objective captures.

### FORK-009 — Weapon inventory HUD strip

**Status:** Implemented.

Added default-on **Weapon Strip** above the ammo counters. It lists owned
weapons, filters out gadgets and duplicate weapon entries, highlights the
selected weapon, and scrolls larger inventories with `+N` overflow counts.
The layout supports solo and split-screen viewports. The strip sits closer to
the bottom while leaving room for ammo counters. Cards, icons, text, padding
and selection marks are scaled to 70% of their initial size, keeping the lower
edge in place; smaller viewports can fit more cards. After three seconds without
a weapon change, the entire strip (text, model images, image rims and selection)
smoothly fades over one second to 20% opacity. Switching weapons restores full
emphasis. Pickup messages account for the strip's position. Menus, cutscenes
and death hide the strip.

**Sources:** [inventory strip](port/src/port_hud.c),
[inventory reset](src/game/bondinv.c), [HUD integration](src/game/bondview2.c),
[message positioning](port/src/joy_port.c).
**Checks:** HUD inventory filtering/selection/scrolling and idle timing tests;
visual captures checked active, quiet and weapon-switch states, text/icon
styles, and ammo/player-name spacing in solo, two- and four-player layouts.

### FORK-010 — Smooth aiming reticles and cursors

**Status:** Implemented.

Replaced the PC red target's pixel/sprite construction with thin, smoothed
lines and a circle. The same renderer supports sniper and always-on crosshairs,
menu/watch target cursors, hit markers and damage arcs. Preserved custom
crosshair colors, fractional aim positions, off-center/wide aiming and
split-screen positioning. The N64 sight path remains separate.

**Sources:** [overlay commands](port/fast3d/gbiex_compat.h),
[PC renderer](port/fast3d/gfx_pc.cpp), [sights](src/game/gunfire.c),
[menu/watch cursor wrapper](src/game/bondwalk2.c), [shared HUD API](port/include/port_hud.h).
**Checks:** Sight captures with multisampling off, custom colors, off-center
aiming, menu cursors and split-screen layouts.

### FORK-011 — Weapon icon mode and model-image generation

**Status:** Implemented.

Added **Gameplay > HUD > Weapon Strip Style: Text / Icons** (`weapon_hud_icons`),
with Text as the default. Icon mode now uses 29 full-color, transparent
512 x 160 renders of actual weapon models, retaining original materials and
separate silenced/gold/silver appearances. Unarmed uses the hand model.
These images replace the initial hand-drawn vector silhouettes. Cached textures
and a subtle edge keep the icons readable without extra model rendering in
normal gameplay; the selection highlight and strip scrolling remain intact.
The selected icon also shows its weapon name centered underneath, with a
shadow for contrast and viewport-edge clamping. The caption stays opaque for
three seconds after selection, then fades to zero over one second; the icon
strip retains its separate 20% idle opacity. Icon mode reserves a small label
row above the ammo counters, including after the name disappears, and pickup
messages account for that row. Text fallback builds keep the text-only layout.
HUD timing tests cover complete caption disappearance and restoration on
weapon changes at 30/60/120/240 Hz. Solo wide/4:3 and four-player captures
checked placement, idle disappearance and selection refresh; Windows/Linux
builds and ADS/settings regression tests passed.

The generation script runs an isolated exporter with fixed camera, part poses
and metallic reflection vectors. Repeated black/white captures must match
exactly before transparency recovery, downsampling, preview/manifest generation
and bounded RGBA packet packing. The local generated pack is embedded when the
game is rebuilt. Fresh builds can run the exporter without generated images;
icon mode falls back to names until generation and rebuild are complete.
ROM-derived outputs remain ignored and are not committed.

**Sources:** [generator](scripts/port/generate_weapon_icons.py),
[export harness](port/src/port_weapon_export.c), [model drawing](src/game/gunfire.c),
[capture backend](port/fast3d/gfx_sdl2.cpp), [image renderer](port/fast3d/gfx_pc.cpp),
[packet decoder](port/include/port_icon_decode.h),
[embedded pack](port/src/port_weapon_images.c), [build wiring](CMakeLists.txt).
**Regeneration guide:** [weapon_icons.md](scripts/port/weapon_icons.md).
**Checks:** [generator tests](scripts/port/test_weapon_icons.py), HUD pack decoding
and coverage tests, solo/split-screen captures, Windows/Linux builds. Two full
generation runs matched all 33 output files byte-for-byte on the tested machine;
cross-GPU byte identity is not assumed.

### FORK-012 — Fork inventory maintenance rule

**Status:** Implemented.

Added this inventory and a repository-wide instruction to maintain it alongside
every addition, modification, removal or replacement. Backfilled the existing
fork work from Git history and the current implementation. Linked the inventory
from the README and contributor guide, and added a PR checklist item. Future
documentation and repository-rule changes update this entry or add a distinct
entry when appropriate. AGENTS.md also records the user's standing permission
to replace executables and purge obsolete builds and their matched suspend
images, with normal campaign/profile/settings data excluded (FORK-001).

**Sources:** [AGENTS.md](AGENTS.md), [FORK_CHANGES.md](FORK_CHANGES.md),
[CONTRIBUTING.md](CONTRIBUTING.md), [readme.md](readme.md),
[PR template](.github/pull_request_template.md).
**Verification:** Local Markdown paths and Git whitespace checks; no runtime
behavior changed by the inventory/rule addition.

### FORK-013 — Persistent adaptive health and armor gauges

**Status:** Implemented.

Added **Gameplay > HUD > Health Display: Automatic / Always**, defaulting to
**Always** (`[display] always_health=1`). Always keeps the native curved gauges
visible during live gameplay, showing actual health and armor at 75% of the
original thickness when emphasized. After three quiet seconds, they smoothly
fade to 25% opacity and 20% of the original thickness over one
second, preserving their placement and arc radius. Accepted damage and health
or armor changes restore full emphasis, independently of damage indicators.
State and timing are per player and pause with gameplay. The setting works
independently of the weapon strip; menus, cutscenes and death do not gain
persistent gauges. Automatic retains the original temporary gauge behavior,
and watch-menu gauges retain their original presentation. Damage/immunity
timers are unchanged.

**Sources:** [HUD state and gauge styling](port/src/port_hud.c),
[gauge rendering](src/game/bondview2.c), [settings](port/src/port_options.c),
frontend/watch HUD pages, [README](readme.md),
[INI reference](tools/midas_ini_help.txt).
**Checks:** [HUD tests](scripts/port/test_hud.sh) cover idle timing at
30/60/120/240 Hz, pause/player isolation, damage refresh with indicators off,
visibility gates and gauge geometry/alpha. [ADS tests](scripts/port/test_ads.sh)
cover menu availability and setting persistence. The HUD audit supports
`-AutomaticHealth` and captures active/idle health with a refreshed weapon strip.
Windows and Linux builds passed. Visual captures checked the HUD settings page,
full/quiet gauges in solo and split-screen, and Automatic mode without
persistent gauges.

### FORK-014 — Solo suspend points and restore/undo controls

**Status:** Implemented.

Added four manual slots per campaign folder, a separate quicksave and one
undo-load image. F6 opens Suspend Points, F5 quicksaves, F9 quickloads and F8
undoes a load; all actions are rebindable for keyboard/controllers. The watch
Gameplay page also opens the menu. Rows show mission, difficulty, save date/time
and elapsed mission time. Manual overwrite requires confirmation. Loads first
preserve the running mission as an undo point, then resume gameplay immediately.
Compact three-second toasts identify saved/loaded manual slots, quicksave or
undo-load and their mission, fading over the last half-second. A new action
replaces the previous toast; the automatic pre-load undo capture is silent.
This replaces the initial full-screen restored-state confirmation/pause.
The fully opened watch can be saved; resuming a saved watch closes it through
the existing animation. Saving is solo-only, alive, outside cutscenes/demo
playback. Loading after death or from the title screen is supported. Campaign
saves, profiles and settings are not rolled back.

This is an exact-executable/ROM native memory-image format, with a generated
registry of owned engine sections, fixed game/asset arenas and converted live
ROM data, including its background conversion registry so later stage changes
remain coherent. Native host state, renderer objects, stacks, diagnostic heap
pointers, settings and campaign data are excluded. Link-map registration is embedded in
Windows/Linux executables at build time. ROM directory storage is stable across
processes, the original ROM fingerprint is retained before conversion, and the
Linux main arena requests the same fixed address as Windows. Loads validate all
headers, descriptors, sizes, build/ROM fingerprints and payload CRCs before
applying. Writes sync a temporary file, preserve a backup and atomically replace
the prior slot. Rendering is synchronized before replacing the world; graphics
caches, queued PCM and input/HUD presentation reset afterwards.
Menu keys are held out of ordinary gameplay bindings until released; closing
the slot menu waits for a fresh input sample, preventing Enter/confirm from also pausing or
switching weapons. No rewind, automatic checkpoint, thumbnails or cross-build
migration in this version.
Local images are ignored by Git.

**Sources:** [save engine/menu](port/src/port_state.c),
[API](port/include/port_state.h), [registry generator](scripts/port/embed_state_layout.py),
[CMake](CMakeLists.txt), [frame integration](src/boss.c),
[arena handling](port/src/pcsched.c), [ROM loader](port/src/romdata.c),
[input actions](port/src/joy_port.c), [native input](port/src/input_pc.c),
[watch pages](port/src/port_watchpages.c), [render barrier](port/src/video.c),
[native audio](port/src/audio_pc.c), shared HUD panel/text helpers and aim gates,
[README](readme.md), [INI reference](tools/midas_ini_help.txt),
[format/validation guide](scripts/port/save_states.md), [.gitignore](.gitignore).
**Checks:** ROM-free [codec/compatibility and input tests](scripts/port/test_states.sh),
[layout tests](scripts/port/test_state_layout.py), and existing ADS/HUD tests
passed. Windows and Linux builds passed; the main Windows executable was
rebuilt. The isolated [runtime audit](scripts/port/audit_states.ps1) passed with
synchronous and threaded rendering: four manual slot restores, quickload,
undo-load, death recovery, watch-menu saves, fresh-process Dam/Facility and
title-screen restores, movement after restore, returning to a previously loaded
stage and restoring again, and damaged-file rejection without world changes.
Probes compare player/character records, mission time, equipped weapon and RNG
seeds at restore. Captures checked the slot menu and resumed gameplay. The toast
revision adds unit coverage for immediate resumption, labels, replacement and
real-time fade duration, and updates the runtime audit to resume without any
confirmation input and capture save/load notifications. Revision unit tests,
ADS/HUD tests, both builds and full Windows audits with synchronous/threaded rendering
passed; captures checked manual-save, quicksave and loaded-slot toasts with
gameplay visible. Linux was compile-checked; gameplay audits ran on Windows.
