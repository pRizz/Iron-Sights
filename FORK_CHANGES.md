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

**Sources:** [.gitignore](.gitignore). **History:** `29a9e6e`.
**Verification:** Git tracked-file changes in that commit.

### FORK-002 — Fork identity and gameplay documentation

**Status:** Implemented.

Named this independent fork **Project Midas ADS**, retained its upstream link,
and documented the added controls, settings and gameplay behavior. Put the
latest gameplay video near the top of the README with a screenshot thumbnail
and YouTube play badge; the newer video supersedes the original demo link.

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

**Sources:** [ADS interface](port/include/port_ads.h),
[input/state and poses](port/src/joy_port.c), [mouse input](port/src/input_pc.c),
[camera and movement](src/game/bondview2.c), [gun rendering](src/game/gunfire.c),
the shared port settings table and frontend/watch settings pages.
**History:** `d017318`, `79d3892`.
**Checks:** [ADS tests](scripts/port/test_ads.sh),
[ADS visual audit](scripts/port/audit_ads.ps1).

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
The layout supports solo and split-screen viewports. Pickup messages account
for the strip's position. Menus, cutscenes and death hide the strip.

**Sources:** [inventory strip](port/src/port_hud.c),
[inventory reset](src/game/bondinv.c), [HUD integration](src/game/bondview2.c),
[message positioning](port/src/joy_port.c).
**Checks:** HUD inventory filtering/selection/scrolling tests and visual audits.

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
entry when appropriate.

**Sources:** [AGENTS.md](AGENTS.md), [FORK_CHANGES.md](FORK_CHANGES.md),
[CONTRIBUTING.md](CONTRIBUTING.md), [readme.md](readme.md),
[PR template](.github/pull_request_template.md).
**Verification:** Local Markdown paths and Git whitespace checks; no runtime
behavior changed by the inventory/rule addition.
