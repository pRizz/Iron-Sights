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
and HUD/mission-cheat/ADS audits and the weapon-icon generator remove their
temporary executable on exit and retain diagnostic logs and captures. This supersedes briefly retaining the pre-toast executable for
older states.

**Sources:** [.gitignore](.gitignore), [artifact rules](AGENTS.md),
[state audit](scripts/port/audit_states.ps1),
[HUD audit](scripts/port/audit_hud.ps1),
[mission-cheat audit](scripts/port/audit_mission_cheats.ps1),
[ADS audit](scripts/port/audit_ads.ps1),
[icon generator](scripts/port/generate_weapon_icons.py),
[state storage guide](scripts/port/save_states.md). **History:** `29a9e6e`.
**Verification:** Git tracked-file changes in that commit. Local cleanup uses
executable/state build fingerprints and workspace-confined file paths; the
audit cleanup is checked with the isolated runtime audit. The native first-launch
icon helper also removes its temporary executable and DLL (FORK-019).

### FORK-002 — Fork identity and gameplay documentation

**Status:** Implemented.

Named this independent fork **Project Midas ADS**, retained its upstream link,
and documented the added controls, settings and gameplay behavior. Put the
latest gameplay video near the top of the README with a screenshot thumbnail
and YouTube play badge; the newer video supersedes the original demo link.
The README introduces the fork, puts the Windows ZIP download and three-step
setup before the demo (FORK-018), prominently links this full
inventory near the top, and summarizes ADS, sniper zoom, sprint, smoother
sights/combat feedback, objective alerts, the weapon strip, adaptive
health/armor, solo suspend points, and mission timer/selection cheat-target
and earned-status hints. Contributor inventory-maintenance instructions sit under
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

The added ADS arms now select their wardrobe from the current player's live
`bondtype`, the same mission cuff selector used by the watch. They load the original watch-arm fabric, cuff and skin textures from the
user's ROM through the native texture pipeline, replacing the initial uniform
flat olive sleeve and skin colors. Blue/formal, boiler, jungle, snow and tuxedo
variants follow the watch's selections; Folder resolves Connery vs other Bonds
in the same way as the watch. Jungle has rolled cuffs and exposed forearms;
long-sleeved outfits retain their trim, with snow sleeves reaching nearer the
wrist. The fuller proportions, grips, lighting and ADS poses remain. Material
lists contain geometry only so the transition's depth-only pass cannot be
overridden by texture setup. Texture binding retains the authored mip-chain
lengths so indexed-color palettes are read at their proper offsets, then samples
the base tile on these close arms. Wardrobe is resolved each draw without a cache,
following player changes, mission loads, Adzy's snow-cuff override and restores.
Native pistol forearms now use the same shared wardrobe mapping and sleeve/
cuff coverage (FORK-017). Their original distal hand/finger shapes, grip bones,
weapon poses and animations remain; the original models have no functioning
wardrobe switches. This also applies outside ADS, keeping the outfit consistent.

**Sources:** [hands and grips](port/src/ads_hands.c),
[ADS pose data](port/src/joy_port.c), [gun rendering](src/game/gunfire.c).
**History:** `2fcbb39`, `79d3892`, `599173d`.
**Checks:** ADS pose/hand-layer regression tests and visual audits.
Wardrobe tests cover every cuff, Folder's Bond variants, sleeve coverage,
authored texture/mip metadata and per-player/cuff changes. Geometry checks cover
both grips' list capacities, vertex UV/alpha bounds, and geometry-only opcodes
for depth-pass reuse. The ADS audit accepts a stage and Adzy switch to check
actual mission outfits and removes its copied executable after exit.
The revised ADS and existing HUD tests and Windows/Linux builds passed. Windows
visual review checked all 20 guns and a 40-frame transition overview from 220
raise/lower captures on Dam, plus the AK47 in Jungle (11 transition captures),
Surface, Archives (4:3), Egyptian and Dam with Adzy, and the AR33 on Cradle in
4:3. These cover boiler, rolled jungle, snow, blue and tuxedo wardrobes and the
snow override; pistols retain their native geometry and the sniper still
finishes in its gun-free scope view. The main Windows executable was replaced
without retaining a backup. Linux was compile-checked; gameplay captures ran
on Windows. Original ROM textures remain local and are not committed.

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
with Text as the default. Icon mode now uses 35 full-color, transparent
512 x 160 renders of actual weapon models, retaining original materials and
separate silenced/gold/silver appearances. Unarmed uses the hand/forearm model: six extra wardrobe variants match the
current mission cuff, resolving Folder's Bond and changing immediately with
player/stage changes or restores. The native cache has 38 indexed slots;
runtime preparation supersedes the older embedded/bootstrap packs (FORK-019).
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
and bounded RGBA packet packing. Exporter slots 32..37 explicitly render each
wardrobe without changing the mission's actual outfit; the native arm treatment
is shared with gameplay. The Python exporter's PNGs, preview/manifest and legacy
packet/header outputs remain developer inspection tools. Player builds now use
native runtime preparation and a per-user cache rather than embedding the pack;
no generation/rebuild cycle is needed. Native preparation shares the exporter
poses and uses premultiplied area downsampling instead of Python's Lanczos.
Icon mode falls back to names when its native cache is unavailable.
ROM-derived outputs remain ignored and are not committed.

**Sources:** [generator](scripts/port/generate_weapon_icons.py),
[export harness](port/src/port_weapon_export.c), [model drawing](src/game/gunfire.c),
[capture backend](port/fast3d/gfx_sdl2.cpp), [image renderer](port/fast3d/gfx_pc.cpp),
[packet decoder](port/include/port_icon_decode.h),
[embedded pack](port/src/port_weapon_images.c), [build wiring](CMakeLists.txt).
**Regeneration guide:** [weapon_icons.md](scripts/port/weapon_icons.md).
**Checks:** [generator tests](scripts/port/test_weapon_icons.py), HUD pack decoding
and coverage tests, solo/split-screen captures, Windows/Linux builds. Two full
generation runs of the earlier 29-image pack matched all 33 output files
byte-for-byte on the tested machine;
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

### FORK-015 — Mission timer cheat-target comparison

**Status:** Implemented.

Extended the inherited **Gameplay > HUD > Mission Timer** toggle rather than
adding a second timer. The inherited solo elapsed clock and fixed digit cells
remain; a saved best time for the live mission/difficulty appears beneath it
as `BEST 02:03`. No record and 007 mode omit that row (007's saved flag is not
a time). Campaign times have whole-second precision and a 1023-second ceiling,
shown as `BEST 17:03+` when capped. Records come from the selected campaign
folder, not a menu hover or a suspend point's older campaign contents. The
second row fits the existing bottom margin at 4:3 and wide aspect ratios.
An applicable timed cheat-unlock target appears beside elapsed time as
`00:42:50 / CHEAT 02:40`. The target is green within time, amber during the last
ten whole seconds, and red beyond the deadline. Lookup uses the live stage and
difficulty with upstream's mission-folder mapping and target-time table, so
only the matching difficulty shows a target. Other difficulties omit that
target; supported difficulties can still show a best record. Already unlocked
cheats still have a practice target.
The comparison follows the existing whole-second inclusive unlock deadline.
This changes presentation, not objective, cheat eligibility or unlock rules.
The inherited mission clock supplies pause and suspend-restore behavior.
The existing solo/HUD visibility and pickup-message spacing remain intact.
The setting remains off by default (`[display] mission_timer=0`); the INI guide
now matches the actual video default, correcting its inherited claim of 1.

**Sources:** [timer rendering](src/game/gunfire.c),
[target lookup/format/colors](port/src/port_hud.c), [HUD API](port/include/port_hud.h),
[campaign time API](src/game/file2.h),
[setting/default](port/src/video.c), [README](readme.md),
[INI guide](tools/midas_ini_help.txt).
**Checks:** [HUD tests](scripts/port/test_hud.c) cover stage/index resolution,
difficulty exclusions, saved-record stage/folder lookup and missing records,
formatting, deadline boundaries, warning colors and
restored-clock comparisons. [HUD audit](scripts/port/audit_hud.ps1) accepts a
mission, difficulty and timer switch plus an optional RAM-only `-BestSeconds`
fixture (`GE_HUD_AUDIT_BEST`) for targeted captures; temporary audit
executables are removed after exit (FORK-001). HUD, ADS and suspend-state
regression tests and Windows/Linux builds passed. Windows captures checked
the Dam Secret Agent 02:40 target at wide and 4:3 aspect ratios alongside text
and icon weapon strips, and elapsed-only output on Facility Agent. Warning
and expired colors are covered by unit tests; runtime captures cover the green
target. Linux was compile-checked. The main Windows executable was replaced
without retaining a previous build.

### FORK-016 — Mission-selection cheat status and difficulty hints

**Status:** Implemented.

Added an always-visible summary for the highlighted mission on the solo mission
grid and for the chosen mission on the difficulty screen. It shows the timed
cheat's required difficulty and target, and distinguishes amber **Cheat to
unlock**, green **Cheat unlocked** (earned in the selected campaign save), and
blue **Cheat available - Unlock All**. A required difficulty still locked by
progress is identified in the summary. The matching visible difficulty row
also has a target/unlocked/available badge and time; other rows, including 007,
receive no badge. Existing completion checkmarks and selection hit areas retain
their meaning. The hints are independent of the optional in-game mission timer.

Uses upstream's target table and the selected folder's saved earned bits. The
raw earned-bit reader now also bypasses compile-time Unlock All, matching its
existing handling of the runtime option, so convenience unlocks are not labeled
as earned. Mission access, cheat unlock requirements and campaign writes are
unchanged by these indicators.

**Sources:** [menu integration](src/game/front.c),
[status/requirements](port/src/port_mission_cheats.c),
[API](port/include/port_mission_cheats.h), [earned-bit reader](src/game/file2.c),
[save API](src/game/file2.h), [README](readme.md).
**Checks:** [ROM-free status tests](scripts/port/test_mission_cheats.sh) exercise
the actual save-bit reader with runtime and compile-time Unlock All, earned vs
available labels, selected-folder isolation, absent saves, target difficulty/time,
locked requirements and invalid entries. The isolated
[menu audit](scripts/port/audit_mission_cheats.ps1) uses RAM-only diagnostic save
fixtures via `GE_MISSION_CHEAT_AUDIT=mission,earned,completed` and
`GE_MENU=imissions|jdifficulty`; it removes its copied executable after exit.
The diagnostic knob is listed by the console's harness command
([environment inventory](port/src/port_console_env.h)). The status tests passed
in both override configurations, along with HUD and ADS regression tests and
Windows/Linux builds. Six Windows captures checked earned and pending mission
summaries, badges on Secret Agent and 00 Agent, an Agent target while hovering
007, blue Unlock All availability, and a locked required difficulty. Wide and
4:3 layouts were visually reviewed, including the final Egyptian grid tile.
All copied audit executables were removed after exit. The main Windows
executable was rebuilt and replaced without retaining its previous build.
Linux was compile-checked; runtime visual review ran on Windows.


### FORK-017 — Mission wardrobe on native arms and unarmed icons

**Status:** Implemented.

The native first-person karate-chop and all seven pistol variants now dress
Bond's original forearm geometry using the same live cuff selector, original
watch-arm textures and sleeve coverage as the added ADS arms (FORK-004).
This replaces the upstream models' uniformly bare forearms; their cuff-switch
entries are empty. Native hand/finger meshes, bone matrices, grip positions,
chop/reload/raise animations and weapon sights remain intact. Jungle exposes
the forearm with a rolled sleeve boundary; other outfits have long sleeves
and their own cuff fabric. Both hands are supported, including dual pistols.
The six wardrobe-specific unarmed HUD images (FORK-011) are deterministic
renders of this same native model treatment, rather than invented silhouettes.
No new setting is required.

A scoped model-render hook runs only around native first-person pistol/chop
draws and offline unarmed exports. It identifies the native proximal forearm,
validates its vertex references, preserves its authored bone matrix and clips
triangles at the wrist/cuff/sleeve boundaries, interpolating lighting and UVs.
Generated vertices belong to the current frame; shared ROM assets are never
modified or retained in a model cache. Unrecognized lists fall back to the
original draw. World/NPC/watch models retain their normal render path.
The shared wardrobe mapping prevents rifles, native arms and icons diverging.

**Sources:** [wardrobe mapping](port/src/port_wardrobe.c),
[API](port/include/port_wardrobe.h), [native arms](port/src/port_native_arms.c),
[scoped draws](src/game/gunfire.c), [model hook](src/game/model.c),
[ADS arms](port/src/ads_hands.c), [icon selection](port/src/port_hud.c),
[exporter](port/src/port_weapon_export.c), [generator guide](scripts/port/weapon_icons.md).
**Checks:** [ROM-free wardrobe tests](scripts/port/test_wardrobe.sh) exercise all
six outfits, Folder resolution, pistol/chop scope, clipping bounds, native bone
preservation, invalid vertex rejection and immutable source vertices. HUD tests
check all six icon indices and decode every generated image. Generator tests
check the slot contract alongside matte validation and packet round trips.

The updated HUD audit accepts `-Chop` for repeated native melee captures; it
removes the sniper only in the isolated RAM fixture (`GE_HUD_AUDIT_CHOP`), since
upstream substitutes a rifle-butt attack whenever Bond owns that weapon,
including Surface's starting inventory. Normal attack selection is unchanged.
The regenerated 35-image pack passed exact repeated black/white pixel checks
for every image. Windows gameplay review checked all 20 ADS guns with 217
transition captures, DD44 Jungle and Magnum Surface transitions, plus native
chops on Jungle, Archives and Surface (rolled, blue and snow sleeves). Best-time captures checked a 02:03 record at 4:3,
a capped 17:03+ record with a cheat target at widescreen, and missing-record
omission on Surface. HUD, ADS, wardrobe, mission-cheat (both Unlock All modes),
suspend-state and generator tests passed. Windows and Linux builds passed;
Linux was compile-checked and runtime captures ran on Windows. The main Windows
executable included the generated wardrobe pack and replaced its prior build;
runtime caching now supersedes embedding that pack (FORK-019).
ROM-derived outputs remain ignored and copied audit/export executables are
removed after exit.

### FORK-018 — Windows player releases and recorded provenance

**Status:** Implemented; publication is performed by the checked-in workflow.

Replaced the inherited Linux/Windows EXE-only, three-day artifact workflow with
ROM-free regression tests, a Windows x86 player ZIP and a versioned development
GitHub Release on each successful main push or manual main dispatch. PRs run
tests with read-only permissions. Full-SHA-pinned actions, checksum-pinned SDL2,
exact checkout checks and recorded compiler/runner inputs identify the process.
The publish job alone receives release/OIDC/attestation permissions. ZIP and EXE
receive attestations; the bundle, checksums, identity and input record persist
as release assets. Actions artifacts last 30 days. Unique immutable releases
provide durable downloads; repository release immutability was enabled during
setup. Maintainer setup also documents explicit workflow enablement for an
existing fork and the canonical origin URL; the local origin was updated from
the renamed repository redirect. This is recorded provenance, not a claim of
hermetic compilation.

The normal GE_ROMDATA_FALLBACK=OFF build now compiles without a ROM, extraction
or private ROM secret. The three formerly retained inline menu/logo models
(legal page, Nintendo and GoldenEye logos) load from the user's ROM. Their
inline texture words match the host representation before texel fixup; fresh
ROM buffers bypass the compiled-pointer texture cache. Fallback-only assembler
blobs are no longer unconditional build dependencies. Inherited checked-in
scaffolding, tables, fonts and port resources remain; this does not claim
removal of every inherited source resource.

Packaging explicitly allows the program, SDL runtime/license, controller
defaults, shader list, help, licenses and instructions/metadata. ROMs, icons,
settings, saves, profiles and old executables are excluded. The README puts a
latest Windows ZIP link and three-step setup near the top, covering extraction,
the user's USA ROM in Data, local asset preparation, About and updating. The
inherited native missing/invalid-ROM explanation remains in use. Instructions
distinguish signed provenance from Windows Authenticode signing.

**Sources:** [workflow](.github/workflows/main.yml),
[packager](scripts/port/package_windows.py), [input record](scripts/port/release_inputs.py),
[player instructions](scripts/port/release/START-HERE.txt),
[verification](scripts/port/release/VERIFY.txt), [guide](scripts/port/releases.md),
[build](CMakeLists.txt), [ROM loader](port/src/romdata.c),
[conversion](port/src/romfile_model.c), [texture fixup](src/game/objecthandler_2.c).
**Validation:** A source-only snapshot compiled and linked Windows without a
ROM or extracted model/blob files and launched with runtime ROM data. Windows
and Linux builds and gameplay/HUD/wardrobe/mission-cheat/suspend regressions
passed. Five distribution tests cover truthful build identity, allow-listed
deterministic ZIPs, bounded cache decoding and inline texture depths, shared
references and invalid offsets/depths. Actionlint passed. The first hosted
[run](https://github.com/pRizz/Project-Midas-ADS/actions/runs/40016371907)
passed tests, compilation and publication. Its immutable release ZIP and EXE
passed attestation verification against this main workflow and source commit;
packaged metadata and all file checksums matched. Verification instructions
accept both configured push builds and deliberate manual main dispatch/retries.
Initial SSH push probes did not create Actions runs despite enabled settings
and no event policy; the maintainer guide documents the manual fallback.

### FORK-019 — Native first-launch weapon image cache

**Status:** Implemented on Windows; Linux reads caches and otherwise uses names.

Replaced rebuild-time embedding (FORK-011) with local preparation by an isolated
copy of the current game's renderer. First launch with a validated ROM renders
all 35 weapon/wardrobe images at 2048x1536, checks exact repeated black/white
pixels, recovers transparency and downsamples with premultiplied area sampling
to 512x160 RGBA. Players need no Python, compiler, admin rights or downloads.
A progress window allows Esc/close to skip and continue with names. Scoped
intentional-wait tracking prevents the Windows hang watchdog from killing a
healthy preparation that exceeds its normal startup grace. The helper
uses neutral settings and no player campaign files, references the ROM without
copying it, and removes temporary program files after exit.

A per-user SDL application-data cache is keyed by ROM fingerprint and generator
version. Headers, dimensions, complete slot coverage, sizes and CRC are checked.
Generation is serialized per cache folder and published atomically. Invalid
caches regenerate; storage errors, generation failure or skipping preserve text
fallback. Wide Windows process/filesystem APIs and UTF-8 SDL paths support
Unicode. Scoped locks/process/window cleanup handles exceptions. About reports
readiness; last-preparation.log retains diagnostics. The HUD reads cached images,
including all six wardrobe icons and the selected-weapon caption. Generated
data stays out of Git. The optional Python PNG exporter remains for inspection.

**Sources:** [cache](port/fast3d/weapon_cache.cpp),
[format](port/include/port_icon_cache_format.h),
[capture](port/fast3d/gfx_sdl2.cpp), [renderer](port/fast3d/gfx_pc.cpp),
[startup](port/src/main.c), [HUD](port/src/port_hud.c),
[guide](scripts/port/weapon_icons.md).
**Validation:** Native generation rendered all 35 images with exact repeated
pixels in a Unicode cache folder. Reuse, corrupt-cache regeneration and
unwritable-folder fallback ran successfully on Windows. Regeneration matched
the prior cache byte-for-byte on this laptop; cross-GPU identity is not assumed.
The contact sheet was visually reviewed. Codec/HUD tests cover malformed
cache cases and text layout without a cache. Python exporter tests passed.

### FORK-020 — Embedded build identity and About links

**Status:** Implemented.

Frontend and watch Port Options pages offer **About Project Midas ADS**.
The executable embeds UTC time, build number/date, full Git commit, branch,
dirty status and available source, Actions run and release/download URLs, with
matching packaged JSON. CI labeling requires an exact commit and clean tree.
Local/modified/no-Git builds do not invent an attested run. Controlled builds
can fix timestamps with MIDAS_BUILD_TIME_UTC or SOURCE_DATE_EPOCH.

Windows uses native blue underlined hyperlinks and keyboard navigation; baked
HTTPS GitHub links open through SDL in the default browser. A manifest enables
common controls and UTF-8 paths without elevation. Text is escaped separately
from link markup, and the game window owns the dialog. Closing resets activation
input. Linux uses native buttons for the same links. About shows cache status.
Display metadata is informational; attestations verify downloaded bytes.

**Sources:** [stamp generator](scripts/port/build_info.py),
[CMake wrapper](scripts/port/buildinfo.cmake), [dialog](port/fast3d/about.cpp),
[URL validation](port/src/port_about.c), [modal tracking](port/src/port_modal.c),
[watchdog](port/src/main.c), [error dialogs](port/src/video.c),
[manifest](port/win/ProjectMidas.manifest),
[frontend](src/game/front.c), [watch](port/src/port_watchpages.c).
**Validation:** Identity tests use actual temporary commits and cover modified/
untracked files, missing Git and fixed timestamps. Windows/Linux builds passed.
The Windows dialog was visually inspected with the correct full commit,
dirty/local label, timestamp, cache status and hyperlink controls. Desktop
automation initially failed during the browser click check; a screenshot-based
retry confirmed that Download releases opens the correct default-browser tab.
About and the inherited missing-ROM error window now suspend hang detection
while open, with counters reset before normal monitoring resumes. This fixes
the watchdog treating a healthy, long-lived native dialog as a stalled game.
A Windows audit kept About open beyond the 60-second startup grace (70 seconds)
and verified it remained responsive.
URL tests reject missing values, HTTP, lookalike domains and injected markup or
control characters. Corrected the existing malformed inventory heading to UTF-8.
