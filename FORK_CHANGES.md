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
and earned-status hints. It also highlights enhanced community fonts (FORK-021).
Contributor inventory-maintenance instructions sit under
Contributing, alongside the existing contribution links.

**Sources:** [README.md](README.md),
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

Added hands and forearms for the 13 firearms that lack them during ADS, with
per-weapon trigger/support grips. The current implementation supersedes the
initial octagonal procedural hands and estimated sleeve/cuff percentages: it
loads the original pause-watch forearm, cuff and watch meshes from the
player's ROM, preserving their clothing/watch material assignments and UVs. Those
meshes supply the actual wrist openings, rolled jungle sleeves, snow gear and
formal cuffs, rather than stretching jacket textures over the wrist/hand.
The current rifle/SMG/shotgun grip hand comes from the ROM's closed PP7 hand,
retaining its actual fingers, thumb, palm, textures and UVs. This replaces the
single curl previously applied to the open watch hand, which distorted fingers
and could fold skin around the wrist. Only main-bone skin triangles are used;
weapon metal and reload-only hands are excluded. The same measured native wrist
interface joins the hand and wardrobe forearm. The hand stays rigid, and only
the proximal arm stretches toward the elbow. Its body now smoothly increases
to 2.3 times the native width/depth behind the protected wrist/cuff region,
restoring rifle forearm volume without enlarging fingers, moving the wrist
join or distorting the watch. Hand scale, protected wrist length, bulk ramp
and body multiplier now have a
single named profile (`sAdsArmProfile`); pose tests lock the protected transforms,
body radius and continuity at the boundary independently.
The body multiplier increases the preceding 1.75 profile by approximately 31%
in width and depth. This is confined to the generated long-gun ADS arm transform;
native pistol/chopping arms and weapon icons do not use this shaping profile.
Hand scale, the protected 40-native-unit wrist band and 35-unit bulk ramp are
unchanged, preserving grip, joint and watch proportions.
Shaping writes frame-owned generated arm vertices; the shared wardrobe source
model stays immutable, so separate duplicated source models are unnecessary.
The original weapon poses, sight alignment and recoil anchors remain.
The original watch body, strap,
face and moving hands accompany every added left ADS arm, driven by the same
clock as the pause animation; the right arm has no watch.
Native pistol/chop arms use the same forearm source (FORK-017).

The pause model's bracelet has an open underside; a generated back completes
it for left long-gun grips, using the original metal texture setup and measured
attachment edges. Its inner/outer surfaces and rims follow a convex envelope
of the entire wardrobe-arm slab beneath the band, including intermediate
bulges, with clearance. Original case/face/clock geometry, UVs and natural grip
roll, arm/cuff placement and hand/seam are retained. Right arms, pistol/chopping
arms and icon transforms retain their existing fit. Unknown bracelet materials
retain the authored bracelet. The face can remain out of sight when turned
away; the band now has geometry on that visible side of the wrist.
ROM-free fixtures cover six wardrobes, full-slab band clearance, alpha,
identical fade-depth topology and source immutability. Attempts to expose the watch by changing wrist spacing
exposed shirt-cuff polygons through the blue jacket and were discarded; the
current fix adds the missing bracelet geometry without changing the arm fit.
Wardrobe/ADS regressions and Windows/Linux builds passed. The final Windows
audit captured all 20 guns and 220 ADS transition frames in the blue wardrobe,
plus Jungle and Snow Klobb raise/lower checks. Overview and sleeve/watch crops
were visually reviewed. All 72 gallery views matched repeated samples and an
independent process pass. Compared with the preceding thickness-only gallery,
the 12 long-gun ADS image differences are confined to the watch area; the other
60 views and all 35 icons are byte-identical. The version-6 icon cache also
matches its preceding bytes. The main executable and gallery were rebuilt.

The thickness-only refinement passed the wardrobe and ADS regressions and
Windows/Linux builds. A Runway gameplay audit captured all 20 guns and 219 ADS
transition frames, with the overview and AK47 raise/lower details visually
reviewed. Gallery regeneration matched all 72 views across successive samples
and independent processes. Exactly the 12 fork AK47/Klobb ADS views changed
across six wardrobes; the other 60 weapon views and all 35 icons remained
byte-identical. The native version-6 icon cache also reproduced its previous
bytes. The updated main Windows executable and gallery are available locally.

The rifle wrist roll now follows the weapon grip instead of aiming the watch
face at the camera. The support palm faces inward/up beneath the fore-end,
placing the watch on the outside/downward side of the wrist; its band can be
the only visible part. The trigger hand rolls outward around its grip as well.
Skin, sleeve, cuff and watch geometry rotate together, retaining their original
texture mapping. Measured native wrist/arm proportions now supersede the earlier
hand-heel bulk curve and separately bent watch-hand profile. The watch band,
case and clock hands refit together, keeping the face's proportions and
the grip-driven roll. Native arms use measured source/target wrist joints rather
than sizing their width/depth from arm length (FORK-017). This keeps the hand,
cuff and forearm connected without the narrow peg visible in the previous icons.
Wardrobe icons share the same correction (FORK-019).
The shared sleeve-rim closure (FORK-017) also keeps coat cuffs solid through
ADS transitions, using the same geometry in the color and depth-only passes.

Pistol and rifle wrist skin now samples UVs and vertex shade from the original
weapon forearm's triangles of the same skin material (1793/1796). Closest-surface
barycentric sampling transfers native mapping onto the established watch-model
geometry without changing positions, alpha, sleeves, cuffs, watch UVs or source
pixels. Shared skin/clothing vertices are excluded from remapping. Rifle grip
hands retain their native UV/RGB without the former additive skin lift; their
wrists use the same original skin source and scene brightness. The earlier warm
brightness correction remains only on native chopping/wardrobe icon forearms,
whose rendering is unchanged (FORK-017/FORK-019).
Skin-material regression fixtures cover mixed skin/clothing lists, watch
exclusion, full/dim scene light, alpha preservation and terminal state reset.
Filtered rifle-grip fixtures also check the selective correction and state
reset in color draws while keeping fade depth lists geometry-only. The pistol
lighting and rifle bulk refinement passed wardrobe/ADS regressions and
Windows/Linux builds. Windows review covered all 20 guns and 220 raise/lower
captures on Runway, Jungle AK47/PP7 and Surface PP7 transitions, and all seven dual
pistols. A fresh 35-image preparation matched the preceding version-6 cache
byte-for-byte, confirming native chopping/icon lighting stays intact.
Windows/Linux builds and wardrobe/ADS/HUD/distribution checks passed. Windows
captures covered 20 guns and 220 ADS transition frames on Runway, Jungle rifle
transitions and Surface chopping. Version-5 icon generation completed all 35
images with exact repeated pixels; all six wardrobe silhouettes and sampled
clothing regions matched the preceding cache, and firearm/gadget icons were
unchanged. The six updated wardrobe images were visually reviewed.


The live player cuff selector resolves blue, boiler, jungle, snow, tuxedo and
Connery outfits just as `bondviewSelectCuff` does, including Folder's selected
Bond and Adzy's override. A separate persistent ROM model buffer avoids borrowing
the pause animation's left-hand slot or retaining stage-owned pointers. Emitted
vertices and display lists belong to the frame; shared source meshes remain
immutable. The transition keeps its near depth range and geometry-only depth
prepass, so texture commands cannot change blending/depth state. The sniper
still finishes in its gun-free scope view. No new setting is required.

**Sources:** [hands and grips](port/src/ads_hands.c),
[shared authored mesh renderer](port/src/port_arm_mesh.c),
[closed hand source](port/src/port_grip_mesh.c),
[wardrobe selector](port/src/port_wardrobe.c), [ADS pose data](port/src/joy_port.c),
[gun rendering](src/game/gunfire.c), [visual audit](scripts/port/audit_ads.ps1),
[diagnostic environment inventory](port/src/port_console_env.h).
**History:** `2fcbb39`, `79d3892`, `599173d`; subsequent authored-mesh replacement.
**Checks:** ROM-free ADS and wardrobe regressions pass. Synthetic authored-mesh
fixtures check all six selectors, native UV/source preservation, left-only watch
parts, original pistol/chop bone matrices, geometry-only depth lists and invalid
vertex rejection, clock-hand transforms, opaque-list memory savings and signed
GBI opcode handling. Pose regressions cover both hands on all 13 grip pairs
through the raise/lower blend, checking orthonormal axes and an outward/downward
support wrist rather than a camera-facing watch. Rigid-grip checks preserve hand
distances and exact hand/forearm wrist transforms, and assert the thicker proximal
forearm radius independently of its centerline. Proportion checks cover independently fitted
native wrist width/depth, the tilted seam, elbow fit, triangle cross-section
interpolation and watch-face scale. The current joint correction passes the
ADS/wardrobe/HUD and distribution/cache tests and Windows/Linux video builds.
Native preparation passed all 35 repeated-capture checks, and the six corrected
icons were reviewed. The corrected build passed the 20-gun Runway audit with
220 raise/lower captures, dual PP7 wrist/watch review, and Surface chopping.
The source ROM measurements also confirmed valid native wrist/elbow rings for
the fist and all seven pistol variants. The grip-roll and thickness
refinement passed Windows/Linux video builds, ADS/wardrobe/HUD regressions and
distribution/cache validation. Windows review covered all 20 guns with 220
raise/lower captures on Runway, plus AK47 transitions on Jungle and 4:3 Surface
and PP7 transitions on Archives. Surface chopping also retained the native
hand/wrist seam in the sampled poses, but later review exposed wrist/cuff
openings. FORK-017's direct seam and rim closure supersedes that incomplete fit.
Runway frame-pool checks reported no overflow.
All six fuller wardrobe icons were reviewed after native regeneration; the
35-image cache passed exact repeated black/white pixel checks. Copied material commands cannot leak culling, combiner or
LOD settings into subsequent gun nodes. The ADS audit now uses the intro-skip harness under no-input;
the harness stops its A taps once gameplay owns input, preventing accidental
weapon cycling. Its `-Dual` fixture checks native left wrists in hip fire;
normal ADS retains its existing single-gun requirement. Windows visual review
checked all 20 supported guns and 220 raise/lower captures on Runway, with
additional AK47 transitions on Surface (4:3) and Jungle. Frame-pool audits on
Runway and Surface reported no overflow. Native dual PP7 and Surface chopping
were checked, and the original Runway pause arm was captured for comparison.
Windows and Linux video builds pass; Linux was compile-checked. The main Windows
executable was replaced without a backup. ROM-derived models, textures, caches
and audit captures remain local, except final rendered review PNGs in FORK-022.

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
ROM-derived output packs remain ignored. FORK-022 later adds a narrow exception
for final rendered review PNGs, not caches or extracted assets.

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
The first-person arm rule and [rendering contracts](scripts/port/arm_rendering.md)
record protected stock pistol data/rendering, independent rifle forearm shaping,
wrist/watch/material boundaries and the required synthetic/visual checks.
The versioned review gallery (FORK-022) is a narrow, user-authorized exception
to the generated-image exclusion: final reference HTML/PNG/manifest only,
without ROMs, extracted assets, caches or raw captures. Repository instructions
require fresh references alongside rendering changes and explicit image review.
Bright Builds adoption (FORK-023) preserves these rules and the former project
PR checklist while installing managed standards and extending the existing hook.

**Sources:** [AGENTS.md](AGENTS.md), [FORK_CHANGES.md](FORK_CHANGES.md),
[CONTRIBUTING.md](CONTRIBUTING.md), [README.md](README.md),
[PR template](.github/pull_request_template.md),
[arm rendering contracts](scripts/port/arm_rendering.md).
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
frontend/watch HUD pages, [README](README.md),
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
[README](README.md), [INI reference](tools/midas_ini_help.txt),
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
[setting/default](port/src/video.c), [README](README.md),
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
[save API](src/game/file2.h), [README](README.md).
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

Inherited pistol/chop forearms were uniformly bare, with empty wardrobe switches.
The native first-person karate-chop and all seven pistol variants now use the
pause-watch model's authored forearm geometry, materials and UV mapping. This
supersedes the initial implementation that clipped the native bare forearm at
estimated sleeve percentages and projected jacket textures onto it. The shared
watch cuff selector supplies actual blue/formal, boiler, rolled jungle, snow
and tuxedo clothing. Wrist width, depth and center now fit the original native
mesh's wrist ring independently of arm length, with the watch/palm axes mapped
to the native hand's actual width and back-of-hand directions. The shared
watch hand/forearm opening supplies source bounds; the native arm supplies target bounds.
A short fit across the tilted wrist seam joins the retained native hand. Wrist
bounds and seam membership now come from vertices shared by the authored hand
and forearm, rather than an approximate proximal-X band.
The native hand and watch forearm have different polygon outlines at the seam.
The forearm boundary now uses actual native wrist vertices, including their
authored tilt. Edge triangles split at the remaining native corners, retaining
the forearm's skin materials and interpolating its UVs/colors. Both meshes thus
share exactly the same outline without a separate patch, enlarged wrist or
changes to the hand. Split draws restore the original vertex-cache bindings;
allocations account for the added commands/vertices. This closes background
wedges through the chop animation and applies to both hands and all six wardrobe
icons. Pistol nodes contain palm surfaces as well as the forearm: replacing the
whole node previously removed those palm triangles, producing see-through hand
patches. The current implementation detects the long wrist-to-elbow strip from
the native mesh's separated longitudinal rings, preserves every distal triangle
with its original positions/materials/UVs/vertex shade, and replaces only the proximal
arm triangles. Their actual shared wrist boundary supplies the fit and stitch,
including its nonplanar tilt; there is no assumed pistol wrist plane at Z=0.
The same closed PP7 hand is reused by the rifle grip renderer (FORK-004), with
the source weapon and reload-bone geometry excluded. Generated rifle grip hands and
split wrist triangles also produce complete geometry-only ADS fade depth lists.
The shared renderer also closes the nearest substantial open clothing
rim with a fan using its existing boundary positions, UVs, colors and material.
This fills the annular opening around the wrist inside the cuff, which the
inherited watch pose normally hides but chopping and ADS angles can expose.
The cached topology selects clothing rather than skin or the elbow; generated
faces use frame-owned vertices and also enter the geometry-only ADS depth pass.
It applies across native arms, ADS arms and wardrobe icons without extending
clothing along the hand. Elbow bounds and native centerline are measured too; cached cross-sections of
the ROM triangle mesh normalize cuff/sleeve flare to the native arm, with a small
clothing allowance. First-person arms extend only their proximal end behind
the camera, keeping elbow caps out of view while preserving wrist/cuff/watch
spacing; static HUD exports keep their complete visible forearm. This replaces
uniform length-based scaling, which
made the chopping wrist especially shallow, and supersedes the subsequent
arm-body-only thickening. The watch strap fits the corrected wrist, with its
case/face/clock kept proportional in their plane. The native palm-width and
back-of-hand axes also align with the retained hand, preventing the watch from
being mounted on the wrong side of the wrist. Native distal hand/finger
meshes, bone matrices, grip positions, chop/reload/raise animations and sights remain intact. Both hands
are supported, including dual pistols; left-hand draws also carry the watch's
actual body, strap, face and hands. No new setting is required.

Native pistol hands now have a separate protected render path: a frame-owned
copy of the original display list suppresses only proximal forearm triangles
with degenerate TRI1/TRI4 slots, preserving every retained index and every
non-triangle command. It references the unchanged original vertex buffer, so
positions, UVs and RGBA cannot be enlarged, remapped or recolored by the generated
arm renderer. The hand draws last through the original model type/culling/fog/
environment/material setup in `model.c`, with native mip LOD restored. Its
dynamic filtered list bypasses the retained GPU part cache. This supersedes the
shared copied-hand renderer and wrist brightness lift, which preserved shape
data but changed the stock palm's shading and made it appear chunky.
The added pistol wrist now uses UVs and vertex shade sampled from the native
forearm's same-material triangles, retaining its fitted geometry. This replaces
the watch wrist's separate skin mapping and uniform brightness adjustments.
The added arm also uses the native two-cycle material and authored mip/texture
commands, matching the stock hand's sampling and shading. Clothing and watch
mapping stay authored; only exclusively skin-owned vertices can be remapped.
Geometry and fade alpha are unchanged. The retained stock hand is never
recolored or remapped to hide the seam. The arm fits its measured wrist.
Surface-mapping regressions check native barycentric UV/RGB sampling, scene-light
scaling, unknown-material fallback, and preservation of geometry/alpha. Mixed
material fixtures cover skin ownership, sleeve/watch UV preservation, shared
vertices and matching color/depth topology. Native-material fixtures also
require the source mip LOD and texture commands rather than a single-tile
override. The stock hand remains on its original renderer with immutable data.
The skin-mapping correction passed wardrobe/ADS regressions and Windows/Linux
builds. Windows review covered all 20 guns on Runway and Surface/Snow (220
transition captures each), all seven dual pistols, and PP7 samples in the other
four outfits. The wrist color seam is removed while retaining the sleeve depth
fix and existing forearm proportions. No new setting is required.
Native pistols now share the near weapon depth layer between the retained hand,
gun and added sleeve/watch, including hip fire, ADS transitions and dual wield.
The sleeve writes depth and the native hand compares against it; skin drawn
later can no longer cover a cuff that is physically in front. This supersedes
the previous painter ordering for these seven pistols. The layer retains the
scene's XY projection and restores scene projection/depth state after each draw.
Stock hand data and material colors remain untouched. Rifle grip shaping and
native chopping/icon rendering retain their existing paths.
The actual-model regression requires arm depth writes, native hand depth
comparisons and the geometry depth-enable bit while retaining the native
two-cycle material setup. The occlusion fix passed wardrobe/ADS regressions
and Windows/Linux builds. Windows audits covered all 20 guns on Runway (219
sampled transitions) and Surface/Snow (220), all seven dual pistols, and PP7
transitions in the other four outfits. Before/after review confirmed that skin
strips no longer overpaint the sleeve; stock hand vertices, UVs and colors are
unchanged. The guide and repository rule now protect cuff occlusion alongside
the stock hand data contract.

A scoped model hook runs only around first-person pistol/chop draws and offline
unarmed exports. It validates the proximal forearm's vertex/matrix references,
retargets the authored watch forearm into its original bone space and emits a
frame-owned copy. Unrecognized nodes retain their native draw. World/NPC/watch
models retain their normal render path. The six wardrobe-specific unarmed HUD
images render this same treatment; cache generator version 6 refreshes their
shared wrist fit and directly stitched chopping seam. Version 5 refreshed their
exposed-skin tone to match the retained hand. Version 4 invalidated the
thin-wrist proportions. Version 2 introduced authored forearms, and version 3
expanded their bodies; the measured-joint fit supersedes that profile (FORK-019). Source ROM assets stay immutable and
are never committed. Wardrobe selection follows player changes and restores.

**Sources:** [wardrobe mapping](port/src/port_wardrobe.c),
[API](port/include/port_wardrobe.h), [native arms](port/src/port_native_arms.c),
[closed grip source](port/src/port_grip_mesh.c),
[scoped draws](src/game/gunfire.c), [model hook](src/game/model.c),
[ADS arms](port/src/ads_hands.c), [icon selection](port/src/port_hud.c),
[exporter](port/src/port_weapon_export.c), [generator guide](scripts/port/weapon_icons.md).
**Checks:** [ROM-free wardrobe tests](scripts/port/test_wardrobe.sh) cover all
six authored outfits, Folder resolution, pistol/chop scope, original bone
preservation, left-only watch inclusion, UV/alpha preservation, malformed
vertex rejection and immutable source meshes. The wrist/rim regressions check
shared seam membership, a complete seven-to-twelve-sided synthetic join in both
directions, native-corner coverage/tilt, split vertex-cache restoration and
material/UV preservation. A hollow sleeve fixture checks the clothing rim cap,
excludes skin and elbow caps, and verifies its color/depth geometry and alpha.
Native-pistol preservation fixtures require unchanged vertex-buffer references,
all non-triangle commands, retained packed triangle indices, record metadata
and source bytes. They also check the neutral pistol-arm material policy cannot
inherit the generated-arm skin lift. These contracts are documented in
[the rendering guide](scripts/port/arm_rendering.md); the existing release CI runs
the wardrobe and ADS regressions without requiring a ROM.
The wardrobe test links the actual `model.c` renderer and verifies the retained
hand receives its native two-cycle material setup and bypasses immutable-part
caching, rather than testing a duplicate rendering wrapper.
The test compiles without builtins, rejects implicit function declarations and
links the game's actual `atan2f`, matching its runtime angle convention.
Additional fixtures preserve palm triangles from a mixed node, exclude source
weapon/reload-hand branches, check rigid hand placement and identical wrist
transforms, and compare complete color/depth geometry during ADS fading.
The ADS audit supports isolated wardrobe overrides for all six outfits and
per-gun hand crops of ADS/raising/lowering views; the override changes only the
RAM fixture while `GE_ADS_AUDIT` is active, not campaign wardrobe or settings.
`-Dual` without an item audits all seven dual-capable pistols, including both
native palms and their left watches. Runtime arm diagnostics report preserved
palm/replaced arm triangle counts and the measured interface for each pistol.
`-StockHands` supplies an isolated native-pistol reference for matched camera/
pose comparisons. An initial PP7 comparison matched all 9,589 skin pixels in a
distal-hand crop exactly after restoring the native render path; the wardrobe
arm below the wrist is intentionally different. This is a local reference,
not a portable cross-GPU pixel guarantee.
Optional audit environment values treat empty strings as disabled, preventing
a preceding dual/stock-reference/wardrobe run from affecting a later audit in
the same PowerShell session.
The protected native-hand refactor passed wardrobe/ADS and distribution/cache
regressions and Windows/Linux video builds. Windows review covered 20 guns with
220 transition captures, all seven dual pistols, matched stock/wardrobe Jungle
PP7 transitions and Surface PP7 transitions. All 35 regenerated icon images
again matched the preceding version-6 cache byte-for-byte. Reference captures
and generated game data remain ignored local artifacts, except the final
rendered review PNGs explicitly covered by FORK-022.
The palm/grip fixes passed the 20-gun audit in all six outfits (120 combinations,
1,319 transition captures), plus all seven dual pistols. A separate Surface/Snow
run passed all 20 guns and 220 transitions; dynamic frame-pool checks reported
no overflow, with vertex/auxiliary usage peaking at 76,352 of 102,400 bytes.
Enlarged hand crops were reviewed. Wardrobe/ADS/HUD/distribution checks and
Windows/Linux video builds passed. Native icon regeneration completed all 35
repeated-pixel checks and matched the preceding cache byte-for-byte, so the
version-6 icon cache remains compatible.
The correction passes wardrobe/ADS/HUD/distribution tests and Windows/Linux
video builds. Windows checks cover the 20-gun Runway audit with 213 sampled
raise/lower frames and denser Surface chopping captures, including palm/back
views that previously exposed the cuff opening. Native version-6 preparation
completed all 35 repeated-pixel checks; its six wardrobe icons were reviewed.
Current Windows gameplay checks
cover all seven pistol variants through the 20-gun ADS audit, dual PP7 (including
its left watch) and native chopping on Surface. The refreshed native cache
rendered all 35 images with exact repeated black/white pixels; all six wardrobe
icons were visually reviewed. ADS, HUD, wardrobe, distribution/cache codec and
suspend-state regressions pass. Real Windows save audits passed write, all four
manual slot round trips, undo-load and cross-process/cross-mission restore with
the new native-arm cache active.

The updated HUD audit accepts `-Chop` for repeated native melee captures; it
removes the sniper only in the isolated RAM fixture (`GE_HUD_AUDIT_CHOP`), since
upstream substitutes a rifle-butt attack whenever Bond owns that weapon,
including Surface's starting inventory. Normal attack selection is unchanged.
Windows and Linux builds passed; runtime captures ran on Windows. Native
caching supersedes the previous embedded wardrobe pack (FORK-019). Generated
ROM-derived output packs remain ignored, with the final review-PNG exception in
FORK-022. Temporary audit executables are removed.

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
hermetic compilation. The fork-specific windows-release.yml supersedes the
inherited main.yml workflow path; historical build 1 used main.yml. The ROM-free
C test job installs only gcc-multilib, without unused C++/CMake packages.
Windows now installs only the Win32 MinGW variant, preserves preinstalled
CMake/make, and uses official Ubuntu HTTPS archive/security repositories rather
than the slow Azure mirror. Fresh signed APT indexes resolve a per-profile
package cache: every restored .deb must match the signed index's size/SHA256
before installation. Stale/corrupt files are discarded. Only archive downloads
are cached, with plan/helper/profile keys and validated fallback reuse; no
installed package database or signed index is restored. Network and step
timeouts bound dependency setup.

SDL's archive has a separate version/SHA256 cache and is checked before each
extraction. A 256 MiB ccache reuses unchanged C/C++ compilations, with compiler
content checks, Ubuntu/architecture/toolchain identity and source/header/flag
invalidation. Unrelated runner-image revisions do not invalidate identical
tools; each release still records its exact runner revision and tool hashes.
No sloppiness is enabled. CMake uses available runner cores, fresh build
directories, fresh linking/metadata and fresh packages/attestations on every
run. Prior executables and ROM assets are never cached. Only main runs save
caches; PR tests use dependency restores without publication. Manual
clean_build disables object-cache restore/save and compilation reuse for an
independent source build. Cache statistics go into the run summary and
build-inputs.json alongside verified package fingerprints and reuse/mode data.

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

**Sources:** [workflow](.github/workflows/windows-release.yml),
[packager](scripts/port/package_windows.py), [input record](scripts/port/release_inputs.py),
[dependency cache](scripts/port/ci_dependencies.py),
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
and no event policy. Registering the fork-specific workflow restored automatic
push execution: [run 40019645555](https://github.com/pRizz/Project-Midas-ADS/actions/runs/40019645555)
passed tests, compilation and immutable publication. Its ZIP/EXE attestations
verified the expected workflow, main ref, exact commit and push trigger. The
maintainer guide retains manual dispatch as a recovery path.
The slow-run investigation measured 58m52s installing the compiler and 34s
compiling in run 40020492518, with package downloads averaging 36.6 kB/s.
Dependency-cache tests cover exact signed-metadata matching, SHA256/size and
filename checks, same-size corruption, stale-file rejection and missing files.
The toolchain-identity test checks reuse across hosted-image rollouts and
invalidation when compiler/frontend content or package versions change.
A fresh-container restore reused all 633,018 bytes of its needed archives with
zero package-download bytes, and installed successfully. Both cold and warm
MinGW builds completed locally; the warm build reused 254 of 257 cacheable
compilations (98.8%). Eight other compiler calls fell back to ordinary
compilation because inherited generated-code line directives name missing
virtual source paths. Hosted run 40035516033 completed all jobs in 2m12s;
Windows dependency installation took 14s and cold compilation took 42s.
Run 40035779322 restored all package and SDL downloads, but a concurrent
GitHub runner-image rollout invalidated the original image-version object key.
The revised key uses actual toolchain content to preserve reuse across those
image updates. Hosted run 40036110183 restored that revised cache, reused
254/257 cacheable compilations (98.8%), compiled/linked in 5s, and installed the
Windows toolchain in 9s with all 75,538,348 package bytes reused. The warm run
completed tests through publication in 1m22s, excluding its intentional queue
behind the cold run. Cold and warm release ZIP attestations verified; the warm
release recorded a fresh timestamp/run identity for the same source commit.

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
version. Version 2 adopted the watch model forearms (FORK-017), replacing cached
sleeve projections; version 3 expanded their forearm bodies. Version 4 replaces
the narrow wrist joints using measured native/source wrist bounds, a coordinated
palm/arm taper and a refitted watch. Version 5 adds the shared renderer's
skin-only brightness correction to blend exposed wrists/forearms with hands.
Version 6 measures the shared wrist seam and directly stitches the forearm edge
to the native chopping hand's polygon outline. It also closes the inherited
open clothing rim, preventing gaps seen inside the cuff (FORK-017).
Older versions are not reused. The version-6 cache rendered all 35 images
with exact repeated black/white pixels; its six corrected wrist/forearm
variants were visually reviewed. Headers, dimensions, complete slot coverage,
sizes and CRC are checked.
Generation is serialized per cache folder and published atomically. Invalid
caches regenerate; storage errors, generation failure or skipping preserve text
fallback. Wide Windows process/filesystem APIs and UTF-8 SDL paths support
Unicode. Scoped locks/process/window cleanup handles exceptions. About reports
readiness; last-preparation.log retains diagnostics. The HUD reads cached images,
including all six wardrobe icons and the selected-weapon caption. Generated
cache data stays out of Git. FORK-022 commits final rendered review PNGs under
its narrow exception. The optional Python PNG exporter remains for inspection.
The later first-person palm/grip fixes (FORK-004/FORK-017) retain cache version 6:
regenerating all 35 images after those fixes matched the preceding cache
byte-for-byte on the audited laptop, including the six unarmed wardrobe icons.
The exporter now explicitly enables geometry depth testing before drawing its
fixed model view, rather than inheriting the preceding gameplay gun's depth
state. This prevents the pistol depth-layer restoration from changing icon
occlusion. A fresh 35-image preparation after the wrist skin-mapping fix matched
the preceding version-6 cache byte-for-byte; no cache version change is needed.

**Sources:** [cache](port/fast3d/weapon_cache.cpp),
[format](port/include/port_icon_cache_format.h),
[capture](port/fast3d/gfx_sdl2.cpp), [renderer](port/fast3d/gfx_pc.cpp),
[startup](port/src/main.c), [HUD](port/src/port_hud.c),
[guide](scripts/port/weapon_icons.md).
**Validation:** Native generation rendered all 35 images with exact repeated
pixels in a Unicode cache folder. Reuse, corrupt-cache regeneration and
unwritable-folder fallback ran successfully on Windows. Regeneration matched
the prior cache byte-for-byte on this laptop; cross-GPU identity is not assumed.
The contact sheet was visually reviewed; an isolated gameplay audit of the
downloaded, attested ZIP also displayed the cached icons correctly. Codec/HUD tests cover malformed
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
and verified it remained responsive and resumed the normal game loop on close.
URL tests reject missing values, HTTP, lookalike domains and injected markup or
control characters. Corrected the existing malformed inventory heading to UTF-8.

### FORK-021 — Enhanced community font textures

**Status:** Implemented.

Added default-on **Display > Video > Enhanced Fonts** (`enhanced_fonts=1`) to
the frontend and watch, independently of 2D Filtering. Replaces matching Latin
font textures with 186 eight-times-resolution community Bank Gothic/Zurich
Bold masks from Carnivorous's CC0 GEPD texture pack, published by Graslu at
`ca06e063f5b985cf59b1ed7a01d03ba3cf86ec63`. These community-authored replacement
masks are embedded, with source fingerprints and licensed attribution; this
addition does not embed the player's original ROM font pixels. No runtime
download, external emulator or asset preparation is required. The Nintendo 64
preset disables the feature and Modern enables it; Classic Mode remains an
independent setting.

The inherited glyph renderer retains spacing, baselines, colours, clipping,
outlines and scaled HUD layout. Tagged I8 font draws match content fingerprints
after inherited padding cleanup. Unknown/modified fonts, Japanese text and
missing characters (both tildes) keep original rendering. Only matching fonts
receive high-resolution images and smooth sampling; images/logos and ammo
icons retain their existing behavior. Original and enhanced cache entries are
distinct, and live setting changes invalidate the renderer's current bindings.
The generator verifies a pinned source ZIP and records source-image hashes;
it adapts pack padding and uses lossless RLE storage. Regeneration requires
Pillow; ordinary builds do not. Release packages include CC0/credits and
build-inputs.json fingerprints the embedded community masks. No emulator
implementation code was copied.

**Known visual difference; decision deferred (2026-10-23):** enhanced Bank
Gothic digits are approximately 12% taller than sampled uppercase letters in
the source artwork. Original sampled digits/capitals have matching visible
heights and alignment; lowercase small capitals are a separate, shorter set.
The pack author's intent is undocumented in the sources reviewed. The
[font guide](scripts/port/enhanced_fonts.md#digit-height-decision-deferred)
records measurements, comparison method and source references. Keep current
proportions while deferring whether to normalize digits and the scope of any
adjustment; this documentation does not change rendering or assets.

**Sources:** [font replacement](port/fast3d/enhanced_fonts.cpp),
[community masks and provenance](port/fast3d/fonts/sources.json),
[renderer](port/fast3d/gfx_pc.cpp), [settings](port/src/video.c),
[frontend](src/game/front.c), [watch](port/src/port_watchpages.c),
[generator and regeneration guide](scripts/port/enhanced_fonts.md),
[generator](scripts/port/generate_enhanced_fonts.py),
[tests](scripts/port/test_enhanced_fonts.py),
[visual audit](scripts/port/audit_fonts.ps1),
[packaging](scripts/port/package_windows.py),
[credits](tools/THIRD_PARTY_NOTICES.txt), [license scope](LICENSE).
**Validation:** Windows MinGW and Linux video builds passed. ROM-free tests
check all 186 native mappings, both absent tildes, bounded decoding, malformed
RLE/size rejection and decoded antialiasing. Regeneration reproduced the output
byte-for-byte. Isolated 1920x1080 audits compared enhanced/original video menus,
mission difficulty/cheat hints and live objective/weapon HUD text. Runtime
uploads confirmed replacement in both font families, with zero replacement
uploads when disabled. Visual review corrected power-of-two padding adaptation
that initially made some letters too narrow. Package tests require font license
and attribution files. The new unit tests run in the release workflow. The main
Windows executable was rebuilt, and its isolated startup audit confirmed that
an existing configuration without the new key enables enhanced fonts by default.

### FORK-022 — Versioned hand and weapon review gallery

**Status:** Implemented.

Added a local HTML review tool and generator absent from upstream. The matrix
has 72 views: PP7, AK47/KF7 and Klobb; regular and settled ADS; original and fork
models; blue, boiler, jungle, snow, tuxedo and Connery wardrobes. It includes all
35 native generated weapon icons, grouping wardrobe-specific chopping arms.
Filters, enlarged inspection, stock/fork switching and pixel inspection support
reviews of hands, wrists, cuffs, watch, grip and sight alignment.

The stock control disables fork arm replacement and added rifle hands under the
same fixed fork camera/ADS pose. It is not an upstream ADS implementation or an
old executable. The environment-only `GE_HAND_GALLERY` audit fixes pose, wardrobe,
lighting and clock hands, with a neutral backdrop; normal gameplay is unaffected.
The generator rebuilds current sources, uses isolated settings and caches, then
requires exact repeated samples and a second independent 72-view process pass
before publishing. Icon preparation retains its own repeated-pixel check.
The input fingerprint also covers CMake presets, generated-header templates
under the source roots and the three always-linked asset C objects, including
the texture bank; Clang/WebGL work made those dependencies explicit. It still
excludes launcher documentation, local binaries and captures.
The manifest records recipe, normalized source-input fingerprint, ROM hash,
renderer identity and image/file hashes. Pixels are tested on the recorded
renderer; cross-GPU identity is not assumed. Output has no build timestamps or
absolute local paths. Python uses only its standard library.

The requested committed reference images are a narrow policy exception in
AGENTS.md/CONTRIBUTING.md and `.gitignore`. Only final HTML, manifest and PNGs in
`review/hand-gallery` qualify. ROMs, extracted models/textures, raw BMPs, binary
caches and copied executables remain excluded. Other audits remain local.

The opt-in installer configures this clone's `.githooks/pre-commit` and Python
interpreter without replacing a custom hook. Every commit verifies staged
references; unchanged input needs no ROM or render. Changed input regenerates
when necessary, then stops for review and explicit staging. Nothing is staged
automatically. Partially staged rendering inputs are rejected before capture.
The same hook also runs the Bright Builds starter checks (FORK-023).
CI performs ROM-free gallery integrity/freshness and workflow tests. The README
links to the gallery and setup guide. This supplements the hand/arm contracts
and geometry/material regressions (FORK-004/FORK-017), not automatic visual approval.

**Sources:** [generator](scripts/port/hand_gallery.py),
[template](scripts/port/hand_gallery_template.html),
[guide](scripts/port/hand_gallery.md), [tests](scripts/port/test_hand_gallery.py),
[hook](.githooks/pre-commit), [gallery](review/hand-gallery/index.html),
[capture driver](port/src/joy_port.c), [fixed gun pose](src/game/gunfire.c),
[stock arm control](port/src/port_native_arms.c),
[rifle control](port/src/ads_hands.c), [clock](port/src/port_arm_mesh.c),
[backdrop](port/fast3d/gfx_pc.cpp),
[CI](.github/workflows/windows-release.yml).
**Validation:** Seven ROM-free gallery tests passed, covering coverage/index
mapping, BMP/PNG pixels and checksums, repeated-capture rejection, normalized
source fingerprints, staged freshness, partial staging and custom-hook safety.
Windows MinGW and Linux video builds, wardrobe and ADS regressions passed.
All 72 real views matched their successive samples and the independent process
pass exactly. All 35 native icons regenerated successfully, reproducing the
preceding version-6 cache byte-for-byte. The rendered PP7/AK47/Klobb stock/fork
contact sheet and all six wardrobe hand icons were visually reviewed.
Working-tree and staged integrity/freshness passed. The HTML parsed successfully
and Node.js syntax validation of the embedded gallery script passed.
The browser prototype slice extended fingerprint coverage for CMake presets,
header templates and always-linked asset objects; all eight gallery tests and
both full capture passes passed with zero changed image cells.
Browser automation rejected the local file URL by policy, so interactive browser
validation was not completed; the gallery can be opened manually offline.

### FORK-023 — Bright Builds standards adoption and Bun checks

**Status:** Installed locally.

Adopted the managed Bright Builds Rules corpus from
`bright-builds-llc/bright-builds-rules`, requested ref `main`, resolved commit
`a1acfa84acf49d4fd6fbc0b00ad7dfa97e1e2c36`. This adds standards and verification
guidance absent from the inherited upstream baseline, without changing gameplay
or the comparison ref. The managed sidecar, contribution block, PR template,
local standards pages, Bun checker, README badges and read-only GitHub checks
workflow retain upstream ownership/markers and an audit trail. Upstream MIT
copyright and permission text are retained separately.

The existing AGENTS and CONTRIBUTING content is preserved outside managed
blocks. The inherited project PR template was backed up before replacement and
preserved as `scripts/port/project_pr_checklist.md`; local guidance requires it
alongside the managed template and fork inventory. `readme.md` is renamed to
`README.md` for case-sensitive managed updates, with inventory links corrected.
Managed badges include the owner-specific OpenLinks badge; local overrides keep
the game/fork identity primary and avoid unrelated in-game branding changes or
mandatory global skill installation. No optional Codex skill was installed.

The existing pre-commit hook now runs the managed Bun checker before staged
gallery verification; its Python configuration, freshness checks, review stop
and partial-staging protection remain. The starter checker runs in its own
GitHub workflow on pushes/PRs/manual dispatch, without adding a package manifest,
lockfile or application dependency. Bun is a development tool only. On the
audited Windows machine its existing npm-managed global installation was
upgraded from 1.1.16 to stable 1.4.4 and verified in PowerShell and Git Bash.

The initial checker reported 195 pre-existing file-length findings. The
user-owned TSV records 96 exact-file exceptions and five generated/vendor
directories, with reasons. New source files outside those directories remain
checked; this is a documented adoption baseline, not a whole-game refactor or
claim of complete standards compliance. The managed checker is unchanged.
The overrides preserve native interfaces and the scoped verification workflow.

Unattended rules updates are explicitly disabled: future updates must preserve
the fork inventory and review requirements. The manual status/update guide
records the pinned formatter stack, isolated Windows formatter environment,
managed-file ownership and local exception policy. Local installer backups are
ignored; the project checklist remains versioned. Rules/docs-only work does not
require a game rebuild or render regeneration.

**Sources:** [local guidance](AGENTS.md), [managed sidecar](AGENTS.bright-builds.md),
[standards index](standards/index.md), [overrides](standards-overrides.md),
[adoption audit](bright-builds-rules.audit.md),
[checker](scripts/bright-builds-check.ts),
[checks workflow](.github/workflows/bright-builds-checks.yml),
[hook](.githooks/pre-commit), [baseline](.bright-builds-rules-checks.tsv),
[project checklist](scripts/port/project_pr_checklist.md),
[maintenance guide](scripts/port/bright_builds_rules.md),
[upstream license](tools/BRIGHT_BUILDS_RULES_LICENSE.txt).
**Validation:** Installer status reports `installed` without managed drift,
auto-update disabled and checks CI enabled. The combined pre-commit hook passed:
Bun checked 704 source files with 101 reasoned exceptions and zero findings;
staged gallery freshness/integrity also passed. Existing project guidance,
README content, checklist and root license preservation were checked. New
user-owned rules docs passed the pinned GFM/frontmatter Markdown check; shell
syntax validation passed. No game rebuild or render regeneration was needed.

### FORK-025 — Immersive browser room

**Status:** Refined desktop room and cinematic are implemented, verified and
published on GitHub Pages. Safari remains experimental.

The requested follow-up on `codex/room-models-and-framing` adds licensed console/
controller replacements, background-drag cartridge pivoting, proportional game
presentation and a screen-fitted TV-flight endpoint. Local verification is
recorded below; final publication acceptance is tracked in WEB-010.

The user requested a canvas-only initial experience with realistic room props,
a CRT, rotating cartridge ROM selection and an animated insertion/power/TV
transition. Returning users have an inserted cartridge and click the power switch.
This replaces the initial visible launcher presentation introduced by FORK-024;
its storage/game interfaces and retained HTML controls remain authoritative.
F2 opens controls, Alt+Enter toggles fullscreen and P remains the native watch.
The [room plan](scripts/port/web_room_plan.md), WEB-007 through WEB-010 in
[progress](scripts/port/web_target_progress.md), AGENTS routing and the documented
chrome override persist scope for later sessions. Candidate models are tracked
with their licenses; the console download currently requires authentication.

`web/src/room/` now contains a pinned Three.js scene and pure transition module.
The full-window entrypoint shows an original CRT, cables and cartridge with
licensed console/controller models, a blue signal screen, wood floor, hover targets,
insertion, red power switch/LED and TV camera flight. F2 reveals the retained
Solid launcher without remounting the game canvas; Alt+Enter requests fullscreen
on the shared container. Errors/save failures reopen controls. Room rendering
pauses while hidden or covered and is disposed after game graphics begin.
Reduced motion suppresses idle movement/flicker and substitutes a short fade.
The initial `props.ts` box stand-ins were superseded by shaped prop modules.
The procedural console/controller are now replaced by Swayne's console and
Marshle's controller, both CC BY 4.0. `models.ts` loads only local, self-contained
GLBs; `console.ts` retains the physical switch/LED and original cables, while
`controller.ts` uniformly fits the controller. The CRT/cartridge remain original.
The Joseph Rohm candidate remains optional and is not claimed as shipped. Native
GLSL CRT effects include restrained tracking/noise, scanlines and a power-on
phosphor line. Reduced motion disables those temporal effects. `cinematic.ts`
aligns the cartridge over the slot before descent and eases the TV approach;
the room crossfades only after game graphics begin. Idle room drawing is capped
at 30 fps, static geometry is batched without absorbing interactive/LED parts,
and stationary shadows are reused. Tests cover transform preservation and the
keyframe endpoints. Context/chunk/image failures retain the standard launcher.
F2 can close an idle ROM/error modal to reach those controls, including when a
damaged remembered ROM restores before the first key gesture. Busy validation
and backup-confirmation dialogs remain protected. A direct browser regression
covers the modal-to-controls route rather than relying on cache timing.
Sustained slow room frames reduce its pixel ratio to 75%, then 50%, with smaller
shadows; isolated hitches and the game renderer/settings are unaffected.

`pivot.ts` adds primary-pointer background dragging only for the floating
cartridge. Direct cartridge selection remains clickable, six pixels distinguish
dragging from clicking, pitch is bounded to 60 degrees and yaw follows horizontal
movement. Pointer capture and cancellation handle leaving the canvas, lost focus,
controls/dialogs and transitions; a completed drag cannot activate a prop.
Manual tilt/yaw ease into slot alignment. Reduced motion permits direct dragging.
`framing.ts` and `cinematic.ts` fit the camera to the 2.35-by-1.7 screen and 42-degree
field of view, face it head-on and contain all corners at the end of the flight.
This replaces the fixed destination that approached too far into the screen.

`presentation.ts`, `player.tsx` and the player/room styles replace viewport
stretching with a centered canvas fitted to the actual framebuffer ratio.
Black bars fill the remaining space in viewport, controls and fullscreen; buffer
resolution is unchanged and mouse mapping uses the same visible element bounds.
Native resolution changes are observed. Absolute positioning and deferred,
deduplicated size writes avoid WebKit resize-observer feedback.

`tools/prepare-room-models.mjs` verifies pinned acquisition hashes and regenerates
the console-only GLB (203,896 bytes) and controller GLB (1,204,348 bytes).
`tools/model-geometry.mjs` preserves normalized vertex attributes when isolating
the console's existing switch; a regression prevents overbright packed colors.
The controller retains its source mesh, normals, UVs and unchanged PNG. Both props
are exact size/hash additions to `room-assets.ts`; `MODELS.md` records authors,
CC BY links, redistribution sources, changes and output hashes. Site assembly
includes that notice alongside the cartridge artwork notice. No external model
or texture download occurs at runtime. GLB texture ImageBitmaps are closed at
teardown, and model-load failures retain the standard launcher.

The user's later artwork request supersedes the original text-only cartridge
label. Their supplied AVIF is copied unchanged to
`web/public/room/cartridge-art.avif`; UV cropping and a material mask select the
printed label. [Artwork provenance](web/src/room/ARTWORK.md) records the source,
size, hash and explicit user request. It is not ROM-extracted data. Site assembly
includes Three.js/artwork notices, and `tools/room-assets.ts` records exact
size/hash checks for the approved images and model GLBs. A lossless PNG copy
with identical decoded pixels is also explicitly hashed/allowlisted for backends
without AVIF decoding (observed in Windows WebKit); capable browsers use AVIF.
A lazy scene chunk
keeps Three.js out of the initial launcher bundle; `tools/frontend-files.ts`
validates the Vite chunk graph, with tests for traversal/non-code/dangling paths.

New ROM-free scene and private actual-ROM audits cover viewport/controls,
selection, insertion/flight, cached-cartridge power startup and stable saves;
the previous launcher audits now explicitly enter F2 controls. The Pages workflow
also runs the room audit under the production project prefix. The first refined
preview passed TypeScript, 94 unit tests, formatting and assembly/allowlist.
Edge and Chrome passed the
ROM-free room checks; Edge passed actual-ROM insertion/handoff/audio/reload/power
startup. The retained launcher's full Dam audio/capture/focus/watch/campaign/profile/
settings/backup regression passed in Edge. Private captures stay in build/.
The refined scene also passes actual-ROM insertion/flight/audio/reload/power
startup in Chrome and isolated Linux Firefox, including verified room GPU-context
release. The initial renderer had 307 draws/177,510 triangles; batching/reduced
subdivision brought the tested scene to 86/52,710. An isolated Edge sample reached
28 fps at the intentional 30 fps idle cap, with about 1.2 ms average JS render
submission after warm-up; this is software-host evidence, not a hardware promise.
The first refined launcher was about 98 kB JS (35 kB gzip) plus a lazy 603 kB scene
(154 kB gzip), versus the original single 654 kB bundle. Vite's scene-chunk size
advisory remains visible. Native game/render source and ROM storage identity
are unchanged. WEB-007 through WEB-010 record the completed preview acceptance
and optional future hardware/Safari/model work.

The licensed-model/framing follow-up passes 104 unit tests, TypeScript, exact
artifact checks and repeatable model regeneration. Room/drag/framing/failure
audits pass in Edge, Chrome, Windows WebKit and Linux Firefox; the actual-ROM
handoff/reload/power audit passes Edge and Linux Firefox with native proportional
sizing in wide/portrait windows, controls and fullscreen. Edge additionally
verifies normal Dam movement/audio/P watch/focus/capture and the full viewport.
The retained launcher's backup/quota/tab-lock audit passes. An idle Edge sample
records 38 draws/30,500 triangles and about 28 fps at full scale. The current
launcher is about 100 kB JS (36 kB gzip), the lazy scene 668 kB (172 kB gzip), plus
1,408,244 bytes of model GLBs. These are local software-host measurements; the
scene chunk still has Vite's size advisory. `room-framing-check.mjs` renders the
actual production scene at the fitted endpoint in landscape and portrait;
`presentation-check.mjs` checks actual canvas bounds/ratio/background. Captures
and reports stay private in `build/web-room/framing-*`. No native render/gallery
input or storage identity changed.

PR #1 merged as `b38a21c`; the first main Pages run caught the remembered-ROM
modal/F2 race and withheld deployment. Repair `08a07d8` passed all main workflows:
Pages `40160374496`, Windows builds/provenance `40160374470` and Bright Builds
`40160374431`. The live runtime `faad72845829175b` identifies that clean source;
public Wasm size/hash/MIME and frontend/artwork files were verified. The actual
published room passes insertion/flight, normal Dam movement/audio/P watch/focus/
capture, durable reload, GPU teardown and physical returning power-switch startup.
A separate active session kept advancing across deployment, then preserved every
campaign/profile/settings backup byte after reload, restore and ROM removal.
Private evidence remains in `build/web-room/published-room`, `live-update-final`
and `public-artifact-report.json`. The plan, compatibility notes and stable
milestone blocks persist these final results for later sessions.

### FORK-024 — Browser target plan and portability groundwork

**Status:** Desktop browser preview is live on GitHub Pages. The accepted core,
launcher, persistence, deployment/update and measured compatibility gates pass.
Safari remains experimental by the user's explicit decision.

The root CMake project now has Emscripten core and SDL2/WebGL 2 configurations.
It keeps native flags/link processing separate, uses the verified high data
base as the initial correctness layout, and excludes native suspend snapshots
from browser builds. Wasm section syntax, explicit C declarations and local
alias spellings address the first survey failures. Browser arena allocation
uses linear-memory allocation and scheduler waits yield through Asyncify.
The optimized core reaches Dam and its explicit frame limit. A temporary local
browser harness renders Dam, measures keyboard/mouse control and sound, and
recovers focus without stuck movement. It is not the persistent launcher.

The playable prototype adds transparent-border GLSL
sampling because WebGL lacks native border-clamp sampling. Border flags reach
both ordinary and retained shader programs, nearest/linear sampling includes
correct edge coverage, supported mirror-clamp is enabled explicitly and mipmaps
are generated only after texture upload. Ten synthetic GPU sampling cases pass;
the corresponding real-ROM run emitted no sampler/mipmap warnings. Keyboard
walking exposed a collision callback accepting integers in place of three
floats; its contract is corrected and exercised with synthetic geometry under
native GCC and Wasm. Typed adapters preserve the model-joint enum boundary,
threshold context and debug-thread entrypoint. The optional strict callback
audit passed all 271 video translation units with function-pointer warnings
enabled/fatal. Browser focus recovery resets SDL keys/buttons, relative motion,
port pad caches and queued audio. The page follows menu pointer ownership,
distinguishes intentional menu releases from unexpected lock loss, and resets
input before requesting capture. Browser initial settings additionally bind P
to the watch (Return still confirms); native defaults are unchanged. Explicit
CSS aspect ratio avoids SDL's 1x1 sizing probe producing a square canvas.
Only a page gesture requests capture through a C entrypoint that updates SDL's
mouse focus/relative-mode bookkeeping. Automatic SDL deferred capture requests
are disabled on web: a pending automatic request could collide with Resume.
The page waits for confirmed capture and handles intentional menu releases.
The private direct-stage harness waits for normal menu initialization instead
of adopting uninitialized zero volumes at tick 1. Optimized Edge testing passed
movement, turning, nonzero Web Audio, focus pause/resume without a stuck key and
watch input recovery. Installed Chrome 155 passed the same optimized checks.
Normal Edge startup/menu navigation rendered 780 frames without a runtime error;
the file/mission menu screenshots were visually reviewed.
New runtime counters/coordinate getters and an isolated QA audio tap support
these measurements; the audio observer never enters the public runtime bundle.

The first full core link exposed cross-translation-unit signature mismatches
that trap under Wasm. A Clang AST recovery tool records real definition types
in generated browser declarations instead of relying on implicit C calls.
The platform also declares missing hosted-library interfaces and separates the
engine's `strtol`, ctype and fixed-point inverse-trig symbols from hosted SDK
functions. Corrected the vertex allocator
callback's declaration and pointer return, watch model transform wrappers'
floating-point returns, and unused extra/missing arguments in legacy ROM/debug
calls, including the positioned-joint callback. Texture-bank stage restoration
now snapshots each declared object with its real size instead of relying on
GCC's ordering of independent globals; Clang/Wasm exposed the prior pointer-as-ID
corruption. Added texture/DMA boundary checks and Wasm symbol-size contracts.
These are ABI/layout corrections; native hand geometry/material contracts and
the fork's comparison baseline remain unchanged. Native ADS, wardrobe, HUD,
state and the new image-bank regressions passed for the combined changes.
The full Windows rebuild and required native gallery regeneration passed: all
72 views/35 icons were repeatable with zero changed image cells; representative
pistol/rifle ADS references were visually reviewed and staged freshness verified.

New browser tooling supplies immutable SDK builds, optional private-ROM core
smokes, CMake presets, real-Clang declaration recovery and a local Playwright
runtime audit. No ROM is copied into the output directory or uploaded. A pinned
`web/` Bun subpackage adds strict source type checking and synthetic ROM/backup
boundary tests. The backup format allowlists campaign EEPROM, profiles, device
bindings, settings and custom controller layouts; it rejects ROMs, suspend
images, traversal, duplicates, invalid sizes and checksum damage. The launcher
now connects these adapters to actual game-file durability.
The launcher's central action changes from Choose ROM to Play as soon as a
selected or remembered ROM passes validation. Its copy confirms the ROM is
loaded, and Play uses the same engine-readiness gate and startup handler as the
lower launch bar. `web/src/player.tsx` owns this UI; the private
`web/tools/launcher-game-audit.mjs` covers first selection, central-button startup,
recall after refresh and returning to Choose ROM after removal. TypeScript,
production assembly/allowlist, launcher UX and actual-ROM checks validate this
follow-up; no game/render source changes are involved.
The separate IndexedDB ROM adapter snapshots original byte order in a Blob;
deleting it is independent of save files. A named Web Lock excludes a second
save writer, and a serialized synchronization queue retains dirty revisions
after errors, reports failure and supports retry. ROM-free browser integration
passed reload/original-byte preservation, aborted-write preservation, deletion
and cross-tab lock transfer; three queue tests passed. Actual campaign/profile/
settings reload now passes the private launcher audit.
The IDBFS adapter now mounts/restores before creating the runtime alias and
tracks writable closes, renames and deletes. Monotonic modification times keep
same-millisecond writes distinct. Backup replacement rolls back the prior memory
snapshot on failure and is refused after main starts, since the game caches its
EEPROM independently. A small ROM-free CMake fixture proves real C stdio writes
through the SDK filesystem, reload, writes during synchronization and backup
round trips. Injected synchronization/partial-write failures preserve the prior
files in memory and after reload. These are synthetic-data tests, not actual
campaign or host-quota-exhaustion validation. The video target exports IDBFS.
The inherited compiled logo payload required a ROM-backed replacement before
Pages publication.
The deployment slice now replaces that payload for web only: a ROM-free generator
retains typed display-list commands but creates 38 uninitialized texture/color/
vertex arrays. Startup copies their data from the player's ROM with validated
bounds and host endian conversion before the existing texel fixups. Native
builds still select the original asset source. All 38 arrays matched the private
stock ROM; five generator/binary-policy tests and native/Wasm converter checks
passed. Original signatures for all 32 substantial texture/vertex arrays are
absent in the final Wasm in both byte orders. The strict audit passes 272/272
sources, including build-tree generated files; ten audit/tooling tests pass.
Native ADS/wardrobe/HUD/state/image-bank/collision/logo regressions pass. Both
full native gallery passes reproduced every view/icon with zero changed image
cells; representative pistol/rifle ADS and freshness/integrity were reviewed.
Assembly now rejects original-logo source selection, compiled ROM fallback or
weapon-icon definitions and initialized generated logo arrays. A separate exact
public-file allowlist/digest verifier excludes local captures, ROMs and extra
build files. Runtime, renderer, font and frontend dependency notices are assembled
with the site. The pinned Pages workflow builds/tests PRs and deploys successful
main builds with write/OIDC permissions confined to its deployment job. Only
explicit public build-provenance environment fields enter the SDK container.
The final project-path artifact passes Edge's full real-ROM launcher/storage/
capture/audio/focus/watch audit and confirms the ROM-backed logo initialized.
Actions-based Pages configuration is enabled at
`https://prizz.github.io/Project-Midas-ADS/` with HTTPS enforced. First CI
publication succeeded in run `40145610255` at clean commit `2360bc8`; the Windows
and Bright Builds workflows also passed. Its live manifest carries truthful CI
provenance. A real update to `e4a81be` (Pages run `40146369375`) advanced the active
watch from frame 2,820 to 32,820, recovered gameplay capture and exactly preserved
campaign/profile/settings files across fresh initialization. Actual backup restore,
ROM recall/removal and save preservation passed without uploads/page errors.
An isolated Linux
Chromium run also passes the project-path launcher, ROM storage, C/IDBFS and ten
GPU sampler checks with container networking disabled.
Browser audit tools select Chromium/installed Chrome/Edge, Firefox or WebKit,
and an isolated local server serves only the assembled site. Initial WebKit
capability initialization passes. Firefox 157 initializes under Linux Xvfb/Mesa
llvmpipe after its Windows executable failed to launch and its Linux headless
run lacked WebGL. Neither is claimed as completed browser/gameplay compatibility
yet. The tracker records the isolated browser image digest and environment.
The next compatibility slice fixes Gecko's global report of an expected Web
Locks callback throw by rejecting the caller directly. Firefox's full launcher
UX passes under Linux Xvfb; actual Dam reaches gameplay but isolated audio is
still under investigation. WebKit's Windows IndexedDB backend rejects Blob/File
preparation, so remembered ROMs prefer Blob and retry clone/unsupported/unknown
preparation errors as an original-byte snapshot in the same schema-1 row.
Quota, permission and explicit abort failures do not trigger that fallback.
Existing Blob rows remain readable; byte rows reconstruct a Blob for normal ROM
validation. Browser storage tests preserve original bytes after mutation/reload,
and WebKit's complete ROM-free UX now passes. An opt-in private deployment audit
keeps an actual watch animating through a changed runtime version and then
requires capture recovery and identical reloaded save files. Its outcome is
still pending; it is not part of the public launcher.
The next compatibility checks identified a separate browser input failure:
SDL gamepad initialization could return before installing the keyboard/console
event watch. Web builds now continue keyboard/mouse assignment when the optional
Gamepad API is unavailable; native initialization behavior remains unchanged.
Private launcher QA can remove that API (`MIDAS_NO_GAMEPADS=1`) and requires the
fallback log plus working native profile/capture/watch behavior. The optimized
build passes all 272 strict callback checks and the full actual-ROM audit with
the Gamepad API deliberately removed. Firefox repeats its full actual-ROM audit
through the checked-in container/audio helpers. Native regressions pass, and
both complete gallery passes reproduce every view/icon with zero changed image
cells; freshness/integrity and eight gallery tests pass. WebKit's native profile
input is repaired, but that Windows runner lacks Web Audio and cannot complete
capture recovery; its gameplay remains unaccepted. A private mission sampler and whitelist-only
prototype stage query cover eight representative solo spawns, ADS, firing,
audio and rendering. The first sweep passed all eight in Edge, with local
software-renderer samples of roughly 49–60 rendered tasks per second and
730726400-byte linear capacity. These are short smoke checks, not mission
completion, resident-memory measurements or hardware performance promises.
A pinned optional browser-test image adds a local PulseAudio null sink; its
explicit Unix socket enables Firefox audio in isolation. A standalone oscillator
probe separates host-audio readiness from the game's mixer. A compatibility
runbook records the tested browser/mission matrix, measured limits and explicit
icon/offline/suspend/threading deferrals. Root README and About chrome link the
live preview and notes; Safari is visibly experimental. Final compatibility
commit `2c890b2` passed Pages `40148810530`, Windows builds/provenance `40148810523`
and Bright Builds `40148810562`. Its clean CI runtime `6521ef4a7a8c672b` passes
the full live-site actual-ROM audit with gamepads deliberately unavailable:
audio/capture/focus/watch, durable campaign/profile/settings, backups and ROM
recall/removal all pass without page errors or uploads. The accepted preview
milestones are complete; no private evidence is published.
The following launcher slice now builds locally with SolidJS and commit-pinned
MysticUI/Tailwind. It adds file/drop selection and ROM remembering, typed engine
and capture states, visible download/compile/save stages, backup controls with
replacement confirmation, storage protection and fullscreen. A static assembler
allowlists the runtime/config outputs, uses immutable content-versioned paths,
records the game's existing generated build metadata and verifies the downloaded
Wasm size/digest. Native profile filename rules now preserve valid dots,
punctuation and Unicode within the native byte limit. Manifest parsing rejects
malformed provenance and unsafe link schemes. Empty restores regain browser
defaults when Play starts. The launcher surfaces game-start failures and retains
save downloads. New sources use pinned Prettier 3.9.10 with a local configuration.
ROM-free UX acceptance passes invalid files, restore/cancel/download, denied
storage protection, fullscreen, cross-tab retry, remember preference reload and
damaged-ROM removal. An injected quota-style IDBFS abort preserves the prior
snapshot and exposes working Retry; actual host quota exhaustion is not claimed.
The private launcher audit has preserved real campaign/profile/settings bytes
across reload. Edge 156 and Chrome 155 pass normal-menu Dam startup, capture,
nonzero audio, focus pause/resume and watch release/recovery. Chrome additionally
round-trips the actual game backup through the restore UI. Real ROM removal
preserves save files. Launcher/game/watch/repeat-launch screenshots were reviewed;
private evidence stays local. No deployed site is claimed. Strict TypeScript,
62 unit tests, storage/IDBFS integrations, formatting and static assembly pass.
AGENTS now documents the root CMake versus browser subpackage commands.

Recorded the user-approved browser target design and six milestones so later
sessions can continue from repository-owned evidence. The planned target keeps
CMake, uses Emscripten/Wasm32 and WebGL 2, starts single-threaded, accepts a local
ROM, persists campaign/profile/settings data in browser storage, supports backup
download/restore and deploys successful `main` builds to GitHub Pages. Native
suspend points require separate browser work; optional icons/offline support
follow the playable/persistence/deployment gates. These are planned fork
additions, not inherited upstream capabilities or a working browser release.

The tracker uses stable milestone blocks with explicit completion criteria,
status, evidence and next actions. AGENTS links the plan and requires relevant
progress updates before handoff. A separate ROM-free CMake probe and Docker
entrypoint now investigate the Wasm ABI and address-layout constraints without
changing the native game target. The SDK is pinned to Emscripten 6.0.13 and its
image digest. A compile-only survey uses the real headless CMake source set,
records native-build errors, removes explicitly listed native-only flags for
the survey and records remaining failures per source. Three inherited C warning
classes are demoted for this diagnostic survey and reported in its summary;
the results do not establish link or gameplay compatibility. Runs disable
container networking and retain local logs under ignored `build/web-spike`.
Existing native and asset ownership contracts continue to apply.

**Sources:** [root CMake](CMakeLists.txt), [presets](CMakePresets.json),
[Emscripten platform](cmake/emscripten.cmake),
[browser adapters](port/web/runtime.c), [suspend gate](port/web/state.c),
[ABI declarations](port/generated/web_prototypes.h),
[declaration generator](scripts/port/web_prototypes.py),
[runtime builder](scripts/port/web_build.py),
[core smoke](scripts/port/web_core_smoke.mjs),
[runtime runbook](scripts/port/web_runtime.md),
[native symbol/segment adapters](port/src/segments.c),
[scheduler](port/src/pcsched.c), [startup](port/src/main.c),
[GL backend](port/fast3d/gfx_opengl.cpp),
[SDL backend](port/fast3d/gfx_sdl2.cpp),
[keyboard/controller startup](port/src/input_pc.c),
[bank reset](src/game/image_bank.c), [bank objects](assets/oddtextures.c),
[model ABI](src/game/model.c), [vertex callback](src/game/chr.c),
[watch transforms](src/game/bondinv.c),
[browser tooling](web/README.md), [ROM codec](web/src/rom.ts),
[backup codec](web/src/backup.ts),
[ROM storage](web/src/rom-storage.ts), [writer lock](web/src/user-lock.ts),
[sync queue](web/src/sync-queue.ts), [browser storage audit](web/tools/storage-audit.mjs),
[persistent files](web/src/persistent-files.ts), [runtime boundary](web/src/runtime-types.ts),
[filesystem fixture](web/tests/filesystem.c), [filesystem audit](web/tools/filesystem-audit.mjs),
[launcher](web/src/app.tsx), [engine](web/src/engine.ts), [capture controller](web/src/capture.ts),
[ROM lifecycle](web/src/rom-manager.ts), [site assembler](web/tools/build-site.ts),
[ROM-backed logo](port/web/logo.c), [logo generator](scripts/port/web_logo.py),
[logo regressions](scripts/port/test_web_logo.py),
[runtime policy](web/tools/runtime-policy.ts), [public artifact check](web/tools/verify-site.ts),
[Pages workflow](.github/workflows/web-pages.yml),
[compatibility evidence](scripts/port/web_compatibility.md),
[mission audit](web/tools/mission-audit.mjs), [audio probe](web/tools/audio-capability-audit.mjs),
[isolated audio helper](web/tools/with-browser-audio.sh),
[Linux game audit](web/tools/linux-game-audit.sh), [test-host image](web/tests/Dockerfile.browser-audio),
[sampler GLSL](port/web/sampler.glsl), [focus adapter](port/web/lifecycle.c),
[runtime audit](web/tools/runtime-audit.mjs), [menu audit](web/tools/menu-audit.mjs),
[collision regression](scripts/port/test_stan_callbacks.c),
[accepted plan](scripts/port/web_target_plan.md),
[milestone tracker](scripts/port/web_target_progress.md),
[spike runbook](scripts/port/web_target_spike.md),
[spike runner](scripts/port/web_spike.py),
[compile survey](scripts/port/web_spike_audit.py),
[SDK pin](scripts/port/web_toolchain.json),
[probe build](scripts/port/web_probe/CMakeLists.txt),
[ABI/address probe](scripts/port/web_probe/probe.c),
[tooling tests](scripts/port/test_web_spike.py),
[session guidance](AGENTS.md#repo-local-guidance).
**Validation:** Root Wasm core and video targets compile/link. Both final link
optimization levels 0 and 2 passed the private stock-ROM Dam smoke for 360 pumps
(62 and 59 graphics tasks respectively), with startup layout checks and
536870912-byte linear capacity. Linker signature warnings are fatal. The video
prototype ran in Windows Edge 156/SwiftShader past 720 pumps/713 graphics tasks,
sampled 57 fps/57 tps after startup and used 730726400-byte linear capacity;
these are local observations, not browser-matrix or resident-RAM claims. Native
ADS/wardrobe/HUD/state tests, native and Wasm image-bank reset tests, 15 Python
tooling tests and 33 browser codec tests passed. Private local ROM validation
passed all three byte orders with in-memory copies. Strict TypeScript checking
passed; dependency declaration checking is skipped for Bun/Vite's conflicting
ImportMeta declarations, as documented in `web/README.md`. The Windows native
build, repeated native gallery, seven gallery tests and gallery integrity/
freshness checks passed. A fresh AST scan reproduced the 205-record generated
header/report exactly. The web preset configured with the SDK toolchain; its
generator uses ordinary platform discovery because the pinned SDK image does
not include Ninja. Documentation formatting, local links and whitespace checks
passed. The standards checker passed with documented inherited size exceptions.
The current ROM-free survey passed its ordinary root build and 259/259 source
compilations, superseding the initial 246/257 compile-only result. Its build
result field is now `root_cmake_build_returncode`, replacing the misleading
native-prefixed name from before the root Emscripten branch existed.
The following prototype slice also passed all 271 strict callback-audit units,
17 Python tooling tests, eight gallery tests and ten GPU sampler cases. Native
ADS/wardrobe/HUD/state/image-bank/collision regressions and the Wasm collision
regression passed. Windows rebuilt and the 72-view/35-icon gallery repeated with
zero changed cells. Optimized Edge 156 measured 443 units of keyboard motion,
1.2 degrees of mouse turn, Web Audio peak 0.21, zero unfocused simulation steps,
31 resumed steps and 1.19 units idle drift; watch capture recovered. These are
local headless software-renderer observations, not a complete browser matrix.
After the capture-owner correction, three fast-link runs, two optimized Edge
runs and one optimized Chrome 155 run passed the complete audit with no rejected
capture requests. The final optimized measurements were about 435 units of
motion, a 1.2-degree turn, audio peak 0.22, zero unfocused steps, 31 resumed steps
and 0.70–1.32 units idle drift. The audit uses existing invisible/invulnerable
diagnostics and a fixed seed to isolate input from enemy impulses; normal
launches retain ordinary gameplay. All 36 codec/queue tests, browser ROM/lock
tests and the real IDBFS fixture passed. Native references were regenerated
again after the final C changes, with zero changed cells and fresh metadata.

The initial pinned Wasm ABI/address probes passed under Node. The
diagnostic headless-source survey compiled 246/257 units and recorded 11 failures;
the unmodified root build failed on native SSE flags. Those initial checks did not claim a
full Wasm link, browser gameplay or deployment. Seven ROM-free tooling tests
passed, including command preservation, immutable SDK validation and failed
survey reporting. Both probes reproduced their results on a repeated run.
Python compilation, pinned GFM/frontmatter formatting, relative links and Git
whitespace checks passed for that earlier tooling-only commit. It changed no
native game/render inputs; the current runtime/shared ABI work does and follows
the required native build/gallery workflow.
