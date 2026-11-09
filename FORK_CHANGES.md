# Iron Sights: fork change inventory

This is the maintained inventory of additions and modifications made by this
fork. Update it whenever repository work changes an entry or introduces a new
area, following [AGENTS.md](AGENTS.md) and [CONTRIBUTING.md](CONTRIBUTING.md).

## Comparison baseline and scope

- **Upstream project:** [murkantor/Project-Midas](https://github.com/murkantor/Project-Midas).
- **Current inherited baseline:** Project Midas 1.0.2b,
  [`9062f9f120b7f64d39e5db59500ed50e8e7a6686`](https://github.com/murkantor/Project-Midas/commit/9062f9f120b7f64d39e5db59500ed50e8e7a6686).
- **Previous baseline:** Project Midas 1.0.1,
  [`6be5920beed453b6a81fdf9935fb9851928b718d`](https://github.com/murkantor/Project-Midas/commit/6be5920beed453b6a81fdf9935fb9851928b718d).
- **Initial inventory:** 2026-10-22, covering the fork commits after that baseline
  and the working tree at initialization. At that point, several HUD/rendering/
  icon changes were uncommitted; inclusion here does not imply a release, commit,
  or push.

The baseline advances deliberately from 1.0.1 to 1.0.2b with FORK-026 on
2026-10-25. Earlier entries retain their implementation history; imported 1.0.2b
work belongs to upstream. FORK-026 records overlaps and fork-specific resolutions.
This describes our differences from the inherited baseline, rather than a live
comparison with upstream's latest branch. The original PC port, its existing
menus, multiplayer features, ROM loaders and other inherited functionality are
not claimed as additions by this fork.

Keep this as a current feature map: update an entry when its implementation
changes, retain useful replacement/history notes, and add separate entries for
new areas. Status describes implementation state, not Git publication state.

## Inventory

### FORK-064 — Taser and watch laser ADS

**Status:** Implemented on 2026-11-08 at the owner's request and built into
the main Windows executable; the owner reviewed it and approved it on
2026-11-09.
Linux and browser builds not run locally.

Inherited behaviour (and FORK-003 until now): ADS covered the items from the
PP7 to the Moonraker laser, the grenade and rocket launchers and the throwing
knife. The watch laser (item 23, just past that range) and the taser (31)
fell back to the original aim mode with its red reticle, even with Aim Mode
set to ADS. Upstream draws both at the hip with a fixed rotation in
`gunUpdateAndFire` (`D_80035C70`, shared with the detonator, and
`D_80035C7C`, which turns the taser about 52 degrees so its face shows).

Both now use the normal ADS controls, the 150 ms raise and lower, half-speed
movement, ADS zoom (75% FOV), ADS Spread/Recoil and the existing single-item,
tank, switch and reload gates. Neither has sights, so each pose is a sight
picture of its own, calibrated with ADS Crosshair's aim point:

- **Taser:** turned a quarter turn to face the camera, its prongs tipped
  0.5 radian towards the target. The two prongs straddle the aim point with
  their tips level with it (measured within 2 px at 960x540).
- **Watch laser:** the arm rolls 0.35 radian and the watch face sits below
  the aim point, where the beam leaves it, so the hand stays below and right
  of the aim point instead of covering it.

`portAdsBlendRotation` (`joy_port.c`) eases each item's native hip rotation to
its ADS orientation with the blend, each angle the shorter way round (the watch
laser's native angles sit near a full turn, which would otherwise spin), and
leaves hip-fire angles untouched. The taser model has no muzzle node
(`Switches[3]`), so `gunfire.c` now poses an item without one from its model
origin; every earlier supported weapon has that node and is unchanged. Both get
pose-table entries but no generated grips: their native hands and the watch
laser's native arm move rigidly, without changing vertices, UVs, materials or
render setup, and the detonator keeps its pose. Weapon Kick and tracers already
leave both native.

**Shared hooks:** two `portAdsBlendRotation` calls in `gunUpdateAndFire`'s
taser and detonator/watch-laser rotation branches, and the muzzle-node fallback
in its ADS block (`src/game/gunfire.c`), all outside `TARGET_N64`.

**Sources:** [poses, rotation and gates](port/src/joy_port.c),
[interface](port/include/port_ads.h), [gun pose hooks](src/game/gunfire.c),
[capture driver](scripts/port/audit_ads.ps1) (now 23 items, with labels),
[alignment guide](scripts/port/ads_alignment.md),
[arm contracts note](scripts/port/arm_rendering.md).

**Checks:** `test_ads.c` covers both items' poses, gates and the rotation blend
(hip angles kept bit for bit, the halfway and settled taser turn, and the watch
laser easing the short way). `test_ads.sh`, `test_wardrobe.sh`,
`test_upstream_merge.sh`, `test_hud.sh`, `test_states.sh` and
`bun scripts/bright-builds-check.ts all` pass. Native ADS audits
(`audit_ads.ps1 -Item 31` and `-Item 23 -Transitions`, Runway, 960x540) were
reviewed settled and through 11 raise/lower frames each: no flips or spins.
Firing in ADS was not captured; the play-test covers it. The review gallery
was regenerated from the isolated build (both passes, 72 views): zero changed
image cells, so only its manifest and index record the new rendering inputs.

### FORK-063 — Shotgun Tracers: a tracer per pellet, and Decorative Pellets

**Status:** Implemented on 2026-11-08 and built for Windows; a scripted Dam
capture showed the player's pellet tracers. The owner then asked for other
players' pellets and guards' shotguns too; those were added the same day and
smoke-tested (a solo shotgun run and a two-player Temple run both exit
cleanly) but have not been seen in a capture. Decorative Pellets followed at
the owner's request. After play-testing, the owner made both settings on by
default (they were off at first), then asked for enemy pellet tracers to be
toned down with fixed values, without a new setting.
Linux and browser builds not run locally.

Inherited behaviour: a Shotgun or Automatic Shotgun blast fires
`NUMBER_SHOTGUN_BULLETS` (5) pellets through `chraiDefaultWeaponFireHandler`
in `chraiCheckUseHeldItem` (`chrprop.c`). Each pellet's hit calls
`gunSetTracerTarget`, but tracers are only created for the weapons listed in
`gunUpdateAndFire`, which leave out both shotguns, and each hand has one
`weapon_beam`, so shotguns draw no tracer. Guards fire a shotgun as a single
shot (`chrlvFireWeaponRelated` in `chraction.c`), and its tracer list leaves
shotguns out too.

**Gameplay > Iron Sights > Combat Feedback > Shotgun Tracers**
(`[display] shotgun_tracers`: 0 Off, 1 On; default 1), after Tracer Strength
in the Options folder and on the watch. When on, each pellet of a player's
blast draws its own tracer from the drawn muzzle to that pellet's hit. In
multiplayer each pellet also gets a beam from the shooter's third-person gun
(`field_2A18`) for the other players' views, under the same 5-degree
agreement check `gunCreateBeamForHand` applies to a bullet
(`portPelletSameDirection`). A guard's shotgun shot draws one beam from its
muzzle to the shot's end (the guard fires one shot, so there are no pellets
to draw).

**Decorative Pellets** (`[display] decorative_pellets`: 0 Off, 1 On; default
1), after Shotgun Tracers in both menus, takes effect only while Shotgun
Tracers is on (`portDecorativePelletsEnabled`). A guard's shotgun shot then
also draws four fake pellet tracers whose directions lie uniformly in a
3-degree cone around the real shot (`portPelletDecorativeEnd`,
`PORT_DECORATIVE_PELLET_CONE_TAN`), each as long as the real shot. They are
not tested against walls, so one can pass slightly into the surface the real
shot hit. Its help line says why: enemies really fire a single shot, which
alone can hit. The label is shorter than the requested "Decorative Enemy
Pellet Tracers" because that would be ten characters longer than any existing
row; the help line names enemies instead.

- **Hooks.** The pellet loop calls `portPelletTracerRecord(hand, i)` after
  each pellet, copying the tracer target it just set. `gunUpdateAndFire`'s
  tracer switch gains a shotgun case that places the muzzle with
  `gunWeaponFovPlaceWorld` and calls `portPelletTracersCreate`, which skips a
  hit nearer than the muzzle as `gunCreateBeamForHand` does. The player
  branch of the beam-timer tick in `chrprop.c` calls
  `portPelletTracersAdvance`, and `gunRenderFirstPersonGunModels` draws them
  with `portPelletTracersDraw` right after the hand's own tracer. The other
  players' beams are drawn by `portPelletTracersDrawOther` beside the native
  multiplayer character beams in `propobj.c`. `chrlvFireWeaponRelated` calls
  `portChrShotgunTracer` when a guard's shotgun fires a traced shot (`sp264`,
  every shot for single-shot weapons), before its weapon list would discard
  it; the NPC branch of the beam tick advances it
  (`portChrShotgunTracersAdvance`) and the character branch of the beam draw
  draws it (`portChrShotgunTracersDraw`).
- **Same look, own timing.** Pellet beams are drawn by the native tracer
  draw, so Tracer Strength styles them, at `PORT_PELLET_TRACER_SCALE` (60%)
  of a bullet tracer's width and glow alpha (60% of the native width even at
  100%). `portPelletBeamInit` and `portPelletBeamAdvance` reproduce the native
  setup and timer numbers (10000-unit path cap, a fifth of the path per field
  with a 500-unit floor, the 3000-unit streak cap, a start up to one step
  either side) but take their random start from a private xorshift generator
  and always advance by `g_GlobalTimerDelta`. The native setup and timer draw
  `randomGetNext`, so reusing them would shift the game RNG; these never do,
  so hits, AI and demos are unchanged.
- **State.** Five first-person beams, five third-person beams and five
  recorded hits per player and hand in `port_pellet_tracers.c`, started spent.
  A new blast replaces the hand's previous pellet beams. Guards' beams live in
  16 slots keyed by character; a slot is free, and never drawn, once
  60 ticks old, so a character that leaves play (or a reused record) cannot
  show a stale beam, and when all are busy the oldest in turn is reused.
  Each guard hand holds five beams: the real shot first, then the decorative
  ones, drawn from the same private generator.
- **Enemy look.** Guard beams record their kind in the spare `unk02` byte of
  `BeamRecord` (`PORT_TRACER_KIND_ENEMY`, `PORT_TRACER_KIND_DECORATIVE`; the
  player's and other players' pellet beams stay `PORT_TRACER_KIND_PLAYER`),
  and the draw hook asks `portTracerLookKind` for that kind; the byte is only
  read for shotgun beams, which only this fork creates. An enemy's beams are
  40% of a bullet tracer's width and minimum on-screen width (the player's
  pellets keep 60% width and the full minimum) and 30% of its glow alpha;
  decorative ones also draw at half opacity (core vertex alpha 128, which the
  glow strip inherits), so the real shot stands out. At Tracer Strength 200%
  that is about 4.8 world units wide with a 0.6 px minimum. The draw hook
  scales the template vertex alpha by the look's `coreAlpha`, which is 255,
  and so unchanged, for every other tracer.
- **Input.** An ordinary toggle row, so keyboard, mouse, gamepad and touch
  change it like any other.

`port_tracers.h` forward-declares `struct BeamRecord` and `struct coord3d`
because including `bondtypes.h` there clashed with the platform headers in
`video.c`.

**Sources:** [pellet hooks](port/src/port_pellet_tracers.c), pellet beam
math, option and look in [port_tracers.c](port/src/port_tracers.c) and its
[header](port/include/port_tracers.h); hooks in [chrprop.c](src/game/chrprop.c),
[gunfire.c](src/game/gunfire.c), [chraction.c](src/game/chraction.c) and
[propobj.c](src/game/propobj.c); rows in
[port_options.c](port/src/port_options.c), [front.c](src/game/front.c) and
[port_watchpages.c](port/src/port_watchpages.c), [help](port/src/port_option_help.c),
[ini template](port/src/video.c), [ini reference](tools/midas_ini_help.txt) and
[README](README.md).
**Checks:** [tracer tests](scripts/port/test_tracers.c), run by
[test_ads.sh](scripts/port/test_ads.sh), cover the default and menu bridge,
the pellet look at 200% and 100%, the native setup numbers, the path cap,
pellets that start past their hit, zero-length paths, the five-field
crossing at a quarter-field tick, the 5-degree multiplayer agreement, and
Decorative Pellets' default, its dependence on Shotgun Tracers, fan ends
that keep the shot's length and sit on the 3-degree rim for level and
straight-up shots, and the player, enemy and decorative looks (including a
non-shotgun beam ignoring the kind byte); the test links no `randomGetNext`, so a
call from this code would fail to build. They pass (Docker
`midas-linux-release`), as do `settings_placement.py` and
`bright-builds-check.ts all`. The Windows build compiles; the regenerated
hand gallery changed 0 image cells. A scripted capture (Dam, All Guns cheat,
`GE_AUTOWHEEL=20,770` to the Shotgun, `GE_AUTOFIRE`, `GE_SHOT=1`, isolated
copies in `build_win/tracer-audit-*`) showed orange pellet tracers at each
impact in the frames after a blast with the option on and none with it off.
The muzzle flash covers the firing frame, and the runs were not
frame-identical, so the comparison is visual rather than a pixel count. The
capture frames were deleted afterwards; the logs remain. After the
multiplayer and guard additions, `test_upstream_merge.sh` also passes, and
a solo shotgun run and a two-player Temple run (`GE_MP=temple`,
`GE_MPPLAYERS=2`, `GE_MPDRIVE=1`, option on) exit cleanly; neither shows a
multiplayer shotgun or a shotgun guard on screen.

### FORK-062 — Tracer Strength: wider, longer, glowing bullet tracers

**Status:** Implemented on 2026-11-08 and built for Windows; a scripted Dam
capture confirmed the look at 100/200/400%. Awaits the owner's play-test.
Linux and browser builds not run locally.

Inherited behaviour: every bullet tracer (the player's, guards',
multiplayer characters' and autoguns') is one camera-facing strip drawn by
`sub_GAME_7F061E18` in `gunfire.c` with the orange `flareimage3` texture: 6
world units wide (radius 30 at the 0.1 model scale), a streak 20% of the
shot's path (`unk24`, capped at 3000), crossing the whole path in about five
fields. Its width is fixed in the world, so long shots thin to under a pixel,
and short shots that hit the floor ahead show for only one to three frames.

**Gameplay > Iron Sights > Combat Feedback > Tracer Strength**
(`[display] tracer_strength`: 100 to 400 percent in steps of 50; default 200),
after Debrief Hit Map in the Options folder and on the watch. 100% draws the
native strip untouched. Above it, with s = percent / 100, `portTracerLookFor`
gives width × s, streak length × (1 + (s − 1) / 2) up to 2 (and never past
the native 3000-unit cap, which keeps vertices in s16 range), a minimum full
width of (s − 1) × 1.5 px of a 1080-line screen (scaled with the current
field of view and per strip end, so distant ends widen and near ends keep
their width), and a glow strip 2.5 times the core's width at vertex alpha
90 × (s − 1), capped at 160. The glow is drawn first with the same texture and
render mode, then the core over it.

- **Hook.** `sub_GAME_7F061E18` scales `radius` and `dist` after the per-item
  radius, rewrites the four strip vertices with `portTracerShapeStrip` (the
  native layout times per-end scales, truncated to s16 as natively) and emits
  the glow strip before the core. The Moonraker laser and watch laser return
  no look and keep their beams.
- **Drawing only.** `CapBeamLengthAndDecideIfRendered` and
  `gunAdvanceBeamTimer` are untouched, so tracer timing, its draws from
  `randomGetNext`, hits and demos are unchanged. The lifetime is therefore the
  native one; a visual afterglow that outlives the native timer would be a
  separate addition.
- **Not covered.** Shotguns draw no tracers natively; FORK-063 adds a tracer
  per pellet, on by default.
- **Input.** An ordinary percentage row, so keyboard, mouse, gamepad and touch
  adjust it like any other.

**Sources:** [tracer module](port/src/port_tracers.c) and its
[header](port/include/port_tracers.h); draw hook in [gunfire.c](src/game/gunfire.c);
option bridge in [joy_port.c](port/src/joy_port.c), rows in
[port_options.c](port/src/port_options.c), [front.c](src/game/front.c) and
[port_watchpages.c](port/src/port_watchpages.c), [help](port/src/port_option_help.c),
[ini template](port/src/video.c), [ini reference](tools/midas_ini_help.txt) and
[README](README.md).
**Checks:** [tracer tests](scripts/port/test_tracers.c), run by
[test_ads.sh](scripts/port/test_ads.sh), cover the default, clamping and menu
bridge, the 100/200/400% looks and their caps, view depth and field of view,
the minimum-width scale near and far, the native strip layout and per-end
widening, the glow strip, and the laser and 100% exemptions; they pass
(Docker `midas-linux-release`), as do `settings_placement.py` and
`bright-builds-check.ts all`. The Windows build compiles; the regenerated hand
gallery changed 0 image cells. A scripted capture (Dam, PP7, `GE_AUTOFIRE`,
`GE_SHOT=1`, isolated copies in `build_win/tracer-audit-*`) showed the firing
frame's tracer adding about 1.9 times the native brightness at 200% and 2.4
times at 400% in the tracer region (which also holds part of the muzzle
flash), with a visibly wider streak and glow. The capture frames were deleted
afterwards; the logs remain.

### FORK-061 — Weapon Kick: a per-shot recoil spring

**Status:** Implemented on 2026-11-08 and built for Windows. The owner
play-tested Enhanced and asked for heavier shotguns and launchers, a rocket
launcher kick, a Ludicrous mode at 3x and a Cartoon mode at 5.5x, keeping
Enhanced as the default; that revision awaits its play-test. Linux
and browser builds not run locally.

Inherited behaviour: the whole-gun recoil animation (`GUN_ANIM_STATE_RECOIL1`
and `RECOIL2` in `gunTickHandState`, driven by `RecoilSpeed`, `RecoilBack` and
`RecoilUp`) starts only when the hand leaves `GUN_ANIM_STATE_FIRE`. Automatic
weapons stay in that state while the trigger is held, so a held burst showed
only the native ±0.3-unit jitter (`WEAPONSTATBITFLAG_00000020`/`40` in
`gunUpdateAndFire`), and one kick played after the trigger was let go. Pistols
and shotguns entered `RECOIL1` after each shot, but as a soft sine-eased rise.
ADS used the same animation, scaled by FORK-045's ADS Recoil.

**Gameplay > Iron Sights > Aiming and Movement > Weapon Kick**
(`[display] weapon_kick`: 0 Original, 1 Enhanced, 2 Ludicrous, 3 Cartoon;
default 1), after ADS Recoil
in the Options folder and on the watch. Enhanced gives every round fired a kick
on a per-player, per-hand spring: back towards the camera, a little up, muzzle
pitch, and a small random sideways and roll movement. A stiff, slightly bouncy
spring returns the gun in roughly 60 to 150 ms, so held automatic fire shows a
jolt per round. Sustained fire also builds a climb, which approaches a per-weapon
ceiling without passing it and drains about a quarter of a second after the last
shot. Ludicrous is Enhanced with three times the pitch, sideways kick, roll and
climb (step and ceiling), 1.5 times the push back and rise, and a spring 20%
softer so the bigger kick reads as heavier. Cartoon uses 5.5 times the
rotation and climb, twice the travel and a spring 30% softer, which flips the
magnum and shotgun past 60 degrees. Travel grows less than rotation in both
so close guns such as the rocket launcher stay clear of the camera.
`portGunKickExaggerate` holds both scalings. Original
keeps the inherited animation exactly.

- **Weapons.** A table in `port_gun_kick.c` gives each class its own peaks and
  spring: pistols (the silenced PP7 and the DD44 separately), the magnum and
  Golden Gun, SMGs (the silenced D5K and the RCP90 lighter), rifles, both
  shotguns, the sniper rifle, the grenade launcher and the rocket launcher.
  After the first play-test the shotguns (back 3.0, pitch 12°) and grenade
  launcher (back 2.6, pitch 11°) were raised above every pistol, and the
  rocket launcher, which has no native kick, gained a slow heavy shove (back
  1.8, rise 0.8, pitch 7°). The native stats were tuned for one animation per
  burst and are too small per round, so the table does not derive from them.
  The knife, fists, taser, lasers, throwables and gadgets have no entry and
  keep the native behaviour.
- **Native kick.** While any mode but Original is on, `portGunKickNativeScale` zeroes the
  native recoil amplitudes for weapons in the table, multiplied into FORK-045's
  existing ADS Recoil hook. The `RECOIL1`/`RECOIL2` timing, and with it the fire
  rate, is unchanged. The native per-shot jitter still plays and still draws
  `randomGetNext`, and pistol slide movement (`BoltRecoilBack`) is untouched.
- **Shot and pose hooks.** `gunTickHandState` calls `portGunKickFire` where a
  round takes ammo, which happens once per shot on field ticks. `gunUpdateAndFire`
  calls `portGunKickAdvance` every tick with `g_GlobalTimerDelta`, then adds the
  translation to `gunofs` and multiplies the rotation into `rotmtx`, after the
  native recoil matrix. In ADS the same translation is added to the pose the
  ADS blend moves towards, so the kick shows once at every blend. The spring
  update is the exact damped-oscillator solution, so the motion is the same at
  every tick rate, and a paused game holds it.
- **ADS.** The drawn kick is scaled by `portAdsRecoilScale`, so ADS Recoil (50%
  by default) halves it once the sights settle. Both shotguns keep their full
  kick in ADS, as FORK-045 already does for them.
- **Visual only.** The shot ray, spread, fire rate, ammo and game RNG are
  unchanged. The kick uses its own xorshift generator, so demos replay as
  recorded. The hand gallery's review pose discards recoil, so its images
  are unchanged.
- **Input.** An ordinary enum row, so keyboard, mouse, gamepad and touch adjust
  it like any other.

**Sources:** [kick module](port/src/port_gun_kick.c) and its
[header](port/include/port_gun_kick.h); hooks in [gunfire.c](src/game/gunfire.c);
option bridge in [joy_port.c](port/src/joy_port.c), rows in
[port_options.c](port/src/port_options.c), [front.c](src/game/front.c) and
[port_watchpages.c](port/src/port_watchpages.c), [help](port/src/port_option_help.c),
[ini template](port/src/video.c), [ini reference](tools/midas_ini_help.txt) and
[README](README.md).
**Checks:** [kick tests](scripts/port/test_gun_kick.c), run by
[test_ads.sh](scripts/port/test_ads.sh), cover the default and option bridge,
which weapons have a profile, the heavy weapons out-kicking pistols, the
single-shot peak, matching motion at 60 and 240 Hz, the return to rest, the
climb ceiling and settling, the Ludicrous and Cartoon scaling and Ludicrous's larger drawn shot,
the native-kick ownership, ADS Recoil scaling, resetting on weapon change and
Original. They
pass (Docker `midas-linux-release`), as do `settings_placement.py` and
`bright-builds-check.ts all`. The Windows build compiles and links. The
regenerated hand gallery changed 0 image cells. No in-game capture of the
kick has been taken; the owner's play-test judges the strength and feel.

### FORK-060 — ADS Crosshair: the always-on crosshair while aiming

**Status:** Implemented in all builds (2026-11-08); Windows built and tested,
Linux and browser not built locally. Requested by the owner as a debugging aid
and play-tested; it led to the crosshair calibration of FORK-003's ADS poses.

Inherited behaviour: Always Crosshair (`[display] always_crosshair`, upstream)
draws a small cross at the gun's aim point while not aiming, and the fork's
ADS (`port/src/port_sight.c`) hides every target while the gun is raised to
the sights. **Gameplay > Iron Sights > HUD Additions > ADS Crosshair**
(`[display] ads_crosshair`: 0 Off, 1 On; default 0), after Always Crosshair
Size in the Options folder and on the watch, keeps that cross, at the aim
point and at Always Crosshair Size, in ADS, so the sights can be checked
against where shots go. It does not need Always Crosshair on. The scoped
Sniper Rifle keeps its own red scope mark, and the multiplayer menu and death
still hide it. An ordinary toggle row, so keyboard, mouse, gamepad and touch
adjust it like any other.

- **State and hook.** `port_crosshair.c` holds the value
  (`portCrosshairAds`), `port_hud.c`'s option get/set reads and writes it (so
  midas.ini persists it like Always Crosshair Size), and `port_sight.c`'s ADS
  early return draws the always-on target when it is on. `video.c` writes
  `ads_crosshair=0` into a new midas.ini.

**Sources:** [state](port/src/port_crosshair.c) and its
[header](port/include/port_crosshair.h), [option hook](port/src/port_hud.c),
[draw](port/src/port_sight.c), [option row](port/src/port_options.c),
[frontend row](src/game/front.c), [watch row](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [ini template](port/src/video.c),
[ini reference](tools/midas_ini_help.txt), and the
[HUD](scripts/port/test_hud.c) and [sight](scripts/port/test_upstream_merge.c)
tests.
**Checks:** `test_hud.sh` (default off, set and read back) and
`test_upstream_merge.sh` (drawn in ADS at the aim point, hidden when dead)
pass, with `test_wardrobe.sh`, `test_ads.sh` and `settings_placement.py`. The
all-guns ADS audit on Facility with the setting off and on changed 104 pixels
per gun (the cross; it sits in the AR33's rear aperture) and none on the
scoped Sniper Rifle, and the HUD Additions sheet was captured with the row.
Not captured: the watch page.

### FORK-059 — Iron Sights application icons from one generator

**Status:** Implemented on 2026-11-08; not yet seen in a real Windows Explorer
or taskbar, Linux desktop, or Steam Deck menu.

Every application icon now shows the Iron Sights three-dot sight picture. They
replace the inherited Project Midas satellite-dish icon that upstream's
`port/include/icon.png` supplied to three places. `scripts/port/make_brand_art.py`
(renamed from FORK-046's `make_steam_art.py`) is now the only source of icons. It
draws one text-free mark in three versions. **Tab** is a simplified version on a
transparent rounded tile for 48 px and below. **Desktop** adds the detailed
shading (serrations, edge, glows) on the same tile. **Square** is the detailed
version on an opaque full square for iOS. The generator writes:

- `port/win/ProjectMidas.ico`: 16/24/32/48 px tab and 64/128/256 px desktop
  frames, compiled into `IronSights.exe` (Explorer and a pinned taskbar entry);
- `port/include/icon_rgba.h`: the 64 px desktop frame for `SDL_SetWindowIcon`
  (title bar and running taskbar entry), in the old layout and 1035 lines;
- `port/include/icon.png`: a 1024 px desktop master, which `package_linux.py`
  still ships as `iron-sights.png` for the Linux packages and the Steam Deck
  application menu (the icon in Steam's "Add a Non-Steam Game" list);
- FORK-024's web icons and FORK-046's Steam art, which stayed byte-identical;
- since FORK-024's search metadata, the web app manifest's 192/512 px desktop
  icons and opaque maskable 512 px icon in `web/public/icons/`, and the
  1200x630 link-preview image `web/public/social-card.png`.

The file names and paths are unchanged, so the `.rc` file, CMake, `gfx_sdl2.cpp`
and the packager change only in comments that named the old script. Upstream's
`scripts/port/make_icon.py` is removed. It resized one PNG to every size, so it
could not use the simplified art for small frames. If upstream changes its icon,
resolve the binary conflict by rerunning our generator.

To keep future icons aligned, the generator also records each output's SHA-256
in `scripts/port/brand-art.sha256`. `test_steam_art.py` (standard library only,
in CI) requires that record to cover every generated icon and the Steam art, and
each file to match it. It also checks the ICO frame sizes, the 64 px header, and
the master's size and transparent corner, and that Linux packaging ships the
master. `AGENTS.md` adds the rule that a new place showing an icon gets a new
generator output, never a separately drawn image. The inherited, unreferenced
Project Midas press image (`port/include/presskit/`) is deliberately left alone.

**Sources:** [generator](scripts/port/make_brand_art.py),
[checksums](scripts/port/brand-art.sha256), [executable icon](port/win/ProjectMidas.ico),
[window icon](port/include/icon_rgba.h), [master icon](port/include/icon.png).
**Checks:** `test_steam_art.py` passes its 29 tests, and rerunning the generator
reproduces identical files. The main Windows build was rebuilt. The 32 px icon
that Windows extracts from `IronSights.exe` matches the generated frame pixel
for pixel. `icon_rgba.h` is a render input, so the hand gallery was regenerated:
both passes verified all 72 cases and 38 icons with 0 changed image cells, and
only its source fingerprint moved. The standards and settings placement checks
pass.

### FORK-058 — Richer Reflections and Gun Glint: shinier weapon metal

**Status:** Implemented in all builds (2026-11-07, extended twice 2026-11-08:
scene reflection and Gun Glint, then the level reflection and the glint fix);
Windows built and tested, Linux and browser not built locally. The owner
play-tested each part: a gentle contrast curve suits the speckled gold guns
and the silver guns look best without it (2026-11-07); the scene reflection
no longer flickered after smoothing, and the glint "looks nice" and went to
the other ordinary guns (2026-11-08). The setting was named Shiny Gold Guns
until 2026-11-08, when it took the scene reflection and became Richer
Reflections ("Better Reflective Guns" overran the Options folder's value
column).

Upstream draws the Cougar Magnum (`ITEM_RUGER`), Golden Gun, Silver PP7 and
Gold PP7 with N64 sphere mapping: `gunfire.c` sets `g_WeaponEnvmapLight`
(ambient 150, one white directional light) and a camera reflection lookat, and
fast3d computes reflection texture coordinates per vertex (`G_TEXTURE_GEN`).
The draw multiplies an I4 64x64 (Magnum, Silver PP7) or CI8 32x32 (Golden Gun,
Gold PP7) reflection by the lighting, through a two-mip trilinear blend.

**Gameplay > Iron Sights > System > Richer Reflections** (`[display]
shiny_weapons`: 0 Off, 1 On; default 1) and **Gun Glint** (`[display]
gun_glint`, default 1), after Text Antialiasing in the Options folder and the
watch. With Richer Reflections on, the Golden Gun's and Gold PP7's reflection
texture gets a luminance S-curve (contrast 0.5) and a highlight lift (gain
0.4), scaling colour by a luminance ratio so gold stays gold and fading towards
white past white, and all four shiny guns take the scene reflection below
(the silver guns without the curve, unless `silver = 1`). Gun Glint adds the
moving highlight below to the ordinary guns. Off, those guns draw exactly as
upstream. Both are ordinary toggle rows, so keyboard, mouse, gamepad and touch
adjust them like any other.

- **Scene reflection** (shiny reflections candidate 3a,
  `port/fast3d/gfx_screen_env.inl`). The shiny gun draws after the world and
  before the HUD, so when its span opens the backend copies the target (the
  scene so far, resolving MSAA) into a mipmapped texture. At the span's first
  shiny batch a warp pass builds a 64x64 sphere map in the game's own
  `G_TEXTURE_GEN` layout: each texel's reflection of the view ray, mirrored
  forward when it points behind the camera (shade faded across the seam to
  `screen_back` 0.85), samples the copy at a blurred mip (`screen_blur` 8);
  the authored texture times that scene relative to the screen's average
  brightness (its 1x1 mip) is mixed in by `screen` (0.5), so the metal keeps
  its colour and level-to-level brightness. Each new map is blended into the
  last (`screen_smooth` 0.2, reset on a weapon change). Shiny batches sample
  it on both texture units; the game's lookup, lighting and floor shading are
  unchanged. First tries rejected: the raw scene replaced the gold and went
  dark; unsmoothed and less blurred, it flickered as the player moved
  (frame-to-frame change of the reflection while walking in Facility:
  median 19.5 / 255 before, about 6-9 after). It can only show what is on
  screen, so walking straight changes it little; the level reflection below
  replaces it by default.
- **Level reflection** (candidate 4, `port/fast3d/gfx_envcube.inl`; owner
  play-tested 2026-11-08: "decently better"). At each shiny span the backend
  renders the level around the camera into a cube (six camera-aligned 128x128
  faces in a 3x2 atlas, cleared to the level's fog/sky colour sent with G_NOOP
  `0x60`), and the warp pass samples it instead of the screen copy, so the
  guns reflect walls passing by and what is behind the player. The cube draws
  the room cache's retained GPU buffers (`gfx_roomcache.inl`), skipping
  translucent and lit draws; props, characters and rooms not yet seen are
  missing. GoldenEye loads the camera's rotation and position with the
  perspective into the projection matrix (`bondview2.c`) and gives each room
  only its offset as modelview, so `roc_env_note` splits the camera out of the
  projection (which also yields the game's near and far planes) and keeps each
  room's placement in one reference space, rebuilt with the current camera.
  With `room_cache = 0` (the PC default), Richer Reflections puts the cache in
  an internal capture-only mode 3 (`roc_hooks.c` `portRoomCacheSetMode`, also
  called at boot and when the setting changes in `video.c`; a
  `headless_stubs.c` answer for builds without a renderer): rooms are recorded
  once and never replayed, model parts do not register, and the frame draws as
  before (outside the gun, the hip view matched the screen-copy run pixel for
  pixel; replay mode 2 changed hundreds of thousands of level pixels). The
  render-side switch is `roc_env_only` in `gfx_pc.cpp`'s cache dispatch.
  Debug build cost: CPU 0.27-0.35 ms per cube on Facility and 0.58-0.90 ms on
  Dam, GPU 0.06-0.18 ms (`GE_ENVCUBE_TIME=1`; `GE_ENVCUBE_DUMP=n` writes the
  n-th atlas). Nothing limits the room count yet, so the cost grows as a
  level is explored. `map = 0` returns to the screen copy.
- **Reflection colour** (both sources). The first play-test found the blue
  sky on the gold guns uncanny: the scene multiplied the metal per channel.
  The scene now contributes only brightness (`screen_hue` 0; 1 restores its
  colours), and half of it (`screen_mirror` 0.5) shows as the scene itself in
  the metal's average colour (a 4x4-tap mean of the authored image) rather
  than as light and dark on the pattern, which near-white silver images had
  clipped and their stripes hid. On Dam, turning the scene on changes the
  Silver PP7 about as much as the Golden Gun (mean 31-32 vs 33-44 per changed
  pixel).
- **Gun Glint** (`port/fast3d/gfx_glint.inl`). The ordinary guns, PP7 to
  sniper rifle (`ITEM_WPPK`..`ITEM_SNIPERRIFLE`, less `glint_skip`), are
  prelit (no vertex normals) and their metal is nearly black, and the game
  draws texture x shade, so a highlight in the shade could not show (first
  try: 0-208 pixels changed). Instead, `port_shiny.c` loads the camera's
  reflection axes (`gSPLookAt`) at the span, vertices record their camera-space
  positions, and after each metal batch its triangles are drawn again
  additively (ONE, ONE; depth tested, not written) with a per-pixel
  Blinn-Phong highlight from the face normal and a world-fixed overhead light
  rotated into camera space, dimmed by the floor shade (strength `glint`
  0.25, `glint_power` 64). Metal means an image whose mean saturation is at
  most `glint_sat` 12 (the PP7's metal measured 0; skin and wood 28-119), and
  draws bracketed by G_NOOP `0x61` never take it: the generated sleeve
  (`port_native_arms.c`) and the native hand pass (`model.c`, two calls in the
  existing `#ifndef TARGET_N64` code beside `portPartCacheEmit`). A value per
  face washed whole faces grey and was replaced by the per-pixel pass.
  Fixed 2026-11-08 after the owner saw a layer over the AR33's sight: the pass
  drew whole triangles, so the rear sight's transparent hole brightened by
  about 14/255, and in ADS a sloped face towards the eye nearly doubled in
  brightness over the sight picture. Images with any see-through texel
  (alpha below 128) no longer count as metal, blended and cutout draws
  (`use_alpha`) take no glint. An audit of every gun (Facility, hip, mid-raise
  and ADS, glint on vs off, before and after) found the AR33 sight the only
  see-through overlap; the hip glint is unchanged on every other gun except the
  DD44, whose 71-pixel glint went with its image's transparent texels. The fix
  first also faded the glint out in ADS; the owner asked to keep it there, so
  `glint_ads` (shiny_weapons.ini, default 1) sets the share kept at full ADS,
  scaled linearly with the ADS blend (G_NOOP `0x62` carries `portAdsBlend()`
  from `port_shiny.c`); 0 restores the fade. With the default, every gun's ADS
  glint matches its pre-fix pixel count, except the AR33's excluded rear sight
  (11,897 → 10,397 changed pixels, the sight post and hole untouched). The
  owner play-tested it. The regenerated hand gallery's images are again
  identical to those before the fade.

- **Span.** `portShinyWeaponTag` (`port/src/port_shiny.c`) brackets the
  first-person weapon draw with G_NOOP `0x5E` (item in the low byte, 0 =
  closed; `0x5D` is FORK-056's outline tag) for the four shiny items and the
  ordinary guns; knives keep the stock look. A gun taking neither the tuned
  look nor the scene reflection drops the span; the ordinary guns take only
  the glint. Inside it, only reflection-mapped triangles change: the
  native hands and other triangles without texture generation keep the stock
  lighting, UVs and textures (`scripts/port/arm_rendering.md`).
- **Advanced tuning.** An optional `shiny_weapons.ini` beside midas.ini (the
  game never writes it; re-read twice a second, so edits apply in play)
  overrides the shipped values: `gold` and `silver` (which guns), `contrast`,
  `gain`, `levels` (stretch the image's range), `ambient` (lighting floor, -1
  keeps 150), `diffuse` (directional light multiplier), `room` (share of the
  floor-tile darkening kept, below), `coverage` (reflection coordinate scale),
  `sweep` (shift with camera yaw and pitch), `drift` (constant shift, like the
  intro logo's spin), `sharp` (no mip chain on the reflection), `bloom` (0 off,
  1 subtle, 2 ludicrous, with `bloom_threshold` and `bloom_strength`), the
  scene reflection's `screen*` keys, `map`, `screen_hue`, `screen_mirror`, the
  glint's `glint*` keys and `log`
  (or `GE_SHINY_LOG`: draw state on stderr). Coverage, sweep and drift fold
  coordinates with a triangle wave instead of clamping. Documented in
  `tools/midas_ini_help.txt`. These keys are not midas.ini options, so they
  need no `settings_placement.py` entry; being file-only, they need no input
  handling.
- **Bloom** (off by default). Upstream has none. The backend mirrors every
  draw in the span into its own buffer (target size, own depth buffer, cleared
  at the span's start) with the same program and depth state: batches whose
  bound texture is the shiny reflection in colour, the rest (hands, matte
  parts) as black through a ZERO/ZERO blend, so it holds only the reflective
  pixels left visible, in either draw order. At the span's end a
  soft-threshold bright pass (smoothstep over 0.25 of luma) downsamples it to
  half size, a 9-tap separable Gaussian runs `passes` times with a growing
  step, and the result is added onto the target before the HUD draws; every GL
  state touched is saved and restored. Subtle: threshold 0.55, strength 0.6,
  radius 1.5, 2 passes. Ludicrous: threshold 0.25, strength 5, radius 2, 4
  passes. Retained (room cache) draws are not mirrored, and a batch drawn
  untextured right after a shiny one would count as shiny (the bound texture
  is the test); neither occurs in the four guns as far as the captures show.
  OpenGL only (the only backend); WebGL2 not tested.
- **Renderer hooks.** `port/fast3d/gfx_shiny.cpp` owns the tuning and tone
  curve; `gfx_shiny_weapons` (`gfx_api.h`, set by `video.c`) switches it.
  `gfx_pc.cpp`: the `0x5E` tag and its frame-start reset, the tuned
  ambient/diffuse, coordinate remap and scaled fog alpha in `gfx_sp_vertex`
  for texgen vertices, the draw-state log, the `shiny` texture-cache key field
  (`gfx_pc.h`; a variant id changes with texture-affecting settings), an upload
  wrapper that tone-curves every decoder's output for that key, `sharp` in the
  sampler latch, and the bloom span (open and close at the tag, a frame-start
  safety close, `bloom_mark` in `gfx_flush`). The backend gains three optional
  `GfxRenderingAPI` entries (`bloom_begin`, `bloom_mark`, `bloom_end`,
  `gfx_rendering_api.h`), implemented in `gfx_bloom.inl`, which
  `gfx_opengl.cpp` includes before its API table; its draw path calls
  `gl_bloom_mirror` after the stream draw. `gunfire.c` gains the two tag calls
  inside the existing `#ifndef TARGET_N64` blocks around the hand draw.
- **Prototype removed before shipping.** An F4 hotkey cycled nine presets
  (Original, Custom, Contrast, Punchy, Punchy + Lit, Punchy + Bloom, Ludicrous
  Bloom, Punchy + Motion, Logo Texture) with a HUD message; it was keyboard
  only, so it went with the presets. A `logo` knob put the intro Rare logo's
  reflection images on the guns; read as the renderer reads the intro's draw
  they decoded dark blue-grey (luma 4-20), byte-swapped green and orange, not
  the gold the intro shows, so it was dropped unverified. The game no longer
  writes a commented `shiny_weapons.ini` template on first run.

Measurements (ADS audit on Runway, 960x540, pixels that differ from Off;
luminance 0-255): Off, deviation 24-36. The shipped look changes 4,192-5,213
Golden Gun/Gold PP7 pixels with a deviation of 35-52, and none on the silver
guns. Stronger tuning ("Punchy": contrast 0.8, gain 0.8, ambient 90, diffuse
1.4, sharp) reached 39-56, with shadows at 7-26.

**The 192 ceiling is Rare's floor shading, not a port clamp.** Every
first-person weapon (PropType 4) draws with `G_RM_FOG_PRIM_A` in cycle 1
(`model.c` render modes), blending towards the fog colour register by its
alpha. `gunfire.c` fills that register from `tileColor`, which
`set_color_shading_from_tile` (`propobj.c`) derives from the floor tile under
the player: alpha = (255 - tile luminance) x 0.75, RGB = the tile's tint with
its grey removed, eased each tick by `update_color_shading`. On Runway the
register is (0, 0, 0, 63), so the gun is mixed 25% towards black and peaks at
192 (189 for the CI8 guns); a blown-out test (ambient 255, diffuse 2, contrast
1, gain 6) still stopped at exactly 192. The RDP applies the same blend, so
this matches the console, and it darkens the hands too. `room` scales that
alpha on the reflective vertices only (`d->fog` for texgen vertices in the
span): with `room = 0` the blown-out test reaches 255; Punchy with `room =
0.4` gives a deviation of 47-67 with highlights at 188-230. Low values also
brighten the metal in dark rooms, where the game darkens it up to 75%.
Bloom over that: subtle changes about twice the gun's pixels and lifts the
99th percentile to 215-255; ludicrous changes 34,000-53,000 pixels and
saturates the silver guns into white glare (an earlier ludicrous setting,
threshold 0.3, strength 2.2, radius 2.5, 5 passes, was a faint haze).
`levels` changes little (the reflection images already span 0-241 and 0-255);
`sharp` changed no pixel with mipmaps off. The game builds the reflection
lookat from the camera's look direction (`bondview2.c` `guLookAtReflect`), so
turning should move the reflection; the audit never turns the camera.

**Candidate work.** Upstream's sphere mapping ignores the player's position, so
the reflection stays still while walking. Strategies for reflections that
follow the player (position sway, sky and floor-light tint, screen-space
reflections from the last frame, live environment capture, baked probes) are
recorded with their trade-offs in the
[shiny reflections candidates](scripts/port/shiny_reflections_plan.md)
(2026-11-07). The position sway and the sky/floor-light tint were built and
play-tested on 2026-11-08; the owner rejected the sway as unrealistic at every
speed tried, and both were archived unmerged on the `archive/shiny-walk-sway`
branch (`d973a2c`). The scene reflection (candidate 3a) shipped instead, then
candidate 4, the level reflection above, after a feasibility spike and a
play-tested prototype (findings in the candidates plan).

**Sources:** [tag](port/src/port_shiny.c) with its
[header](port/include/port_shiny.h), [draw hook](src/game/gunfire.c),
[tuning](port/fast3d/gfx_shiny.cpp) and its [header](port/fast3d/gfx_shiny.h),
[renderer](port/fast3d/gfx_pc.cpp) and [API globals](port/fast3d/gfx_api.h),
[cache key](port/fast3d/gfx_pc.h), [bloom](port/fast3d/gfx_bloom.inl),
[scene reflection](port/fast3d/gfx_screen_env.inl), [glint pass](port/fast3d/gfx_glint.inl),
[level cube](port/fast3d/gfx_envcube.inl) with its [room cache hook](port/fast3d/gfx_roomcache.inl),
[capture mode](port/src/roc_hooks.c) and [headless stub](port/src/headless_stubs.c),
[backend](port/fast3d/gfx_opengl.cpp), [backend API](port/fast3d/gfx_rendering_api.h),
[sleeve bracket](port/src/port_native_arms.c), [hand bracket](src/game/model.c),
the [wardrobe test stub](scripts/port/test_wardrobe.c),
[setting](port/src/video.c), [options](port/src/port_options.c),
[frontend row](src/game/front.c), [watch row](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [ini reference](tools/midas_ini_help.txt),
the [review gallery](review/hand-gallery/index.html) and the [build](CMakeLists.txt).
**Checks:** The Windows build compiles and links with no new warnings. A local
capture script (not in the repository) ran the ADS audit (`GE_ADS_AUDIT=18`
to `21`, with transitions) for each gun in isolated folders and measured the
changed pixels; captures were reviewed side by side, and the shipped build
with no tuning file matches the play-tested look pixel for pixel. The owner
play-tested the prototype presets in the Windows build. `test_wardrobe.sh`,
`test_ads.sh`, `test_hud.sh` and `test_upstream_merge.sh` pass;
`settings_placement.py` and `bright-builds-check.ts all` pass; the hand
gallery regenerated with zero changed image cells and verifies (2026-11-07).
2026-11-08: scene reflection captured on Runway, Facility and Bunker (gold
guns' brightness within about 10% of before) and its flicker measured walking
through Facility (deterministic runs, frame-to-frame change of the reflection's
contribution). Gun Glint: classification logged per image; changed-pixel masks
for both PP7s on Dam and Facility and for the DD44, Klobb, KF7, RC-P90,
Shotgun and Sniper Rifle on Dam showed every changed pixel on gun metal; the
regenerated hand gallery changed 60 of 72 cells (PP7, AK47 aimed, Klobb), all
168,072 changed pixels previously grey or black, with identical counts across
all six wardrobes per gun and pose (so none on a sleeve) and no icon changes.
The Options folder's System sheet was captured with both rows and Gun Glint's
help line. The regressions above, settings placement and the standards check
pass again. Not run: the watch page captured (the level fly-in needs input),
the Linux and browser builds, every level, and a long play session.
Level reflection and glint fix (2026-11-08): cube atlases dumped and reviewed
on Facility and Dam; hip and ADS captures of the Golden Gun, Silver PP7 and
Cougar Magnum compared against the screen copy and the earlier colour mix; the
all-guns glint audit above; the owner play-tested the level reflection and
colour mix in the Windows build (the glint fix was not play-tested before
shipping). `test_wardrobe.sh`, `test_ads.sh`, `test_hud.sh`,
`test_upstream_merge.sh`, `settings_placement.py` and `bright-builds-check.ts
all` pass; the regenerated gallery changed only the ADS images of the PP7,
AK47 and Klobb (36 images), every changed pixel darker and the counts equal to
the earlier ADS glint (1,956, 6,432 and 3,286) in all six wardrobes, and it
verifies.
Linux build fix (2026-11-08): from `2516632` the Linux CI job failed compiling
`gfx_opengl.cpp`, because `gfx_screen_env.inl` included `<cmath>` partway
through the file. On Linux, `<string>` reaches `pthread.h`, which finds the
game's `src/sched.h` on the include path, then `ultra64.h` and `PR/gu.h`. Its
`sinf`/`sqrtf` declarations, without a linkage specification, then preceded the
C library's `extern "C"` ones, which C++ rejects. MinGW and Emscripten were
unaffected. `gfx_opengl.cpp` now includes `<cmath>` before `<string>` (the
order `gfx_pc.cpp` already uses), and the `.inl` relies on it. The CI Linux
build, in the `midas-linux-release` container with `linux_build.py`, compiles
and links again. The Windows build and an unchanged hand gallery confirm the
other compilers.

### FORK-057 — Shader prewarm list recorded over the fork's screens

**Status:** Implemented 2026-11-07 on Windows. The list is shared by every
target and ships to the browser too.

The shader prewarm list (`tools/shaders_base.txt`, shipped as
`Data/shaders_base.txt`) is compiled behind the front end, so a colour combiner
program's first use does not stall gameplay. On mobile web each such stall costs
hundreds of ms. The inherited recorder, `scripts/port/record_shaders.sh`,
drives every solo stage, the front end, a watch open, 2P/4P multiplayer and the
attract demos. That list dates from the 2026-10-18 import, and the recorder
never reached the fork's own screens. Scripted runs also skip preparing the icon
cache, so it never saw icons: that is how the weapon strip's two programs were
missed (FORK-019, added in `403cedd`).

The recorder now also runs:
- **Icons:** the front-end run prepares the icon cache in the foreground
  (`GE_ICON_CACHE_FORCE`), so every later mission shows the strip with icons.
- **Sights:** the ADS audit raises every gun, single and dual, to its sights
  (`GE_ADS_AUDIT`, with the all-guns cheat), covering scopes, crosshairs and
  the strip on each weapon.
- **Debrief:** the hit-map probe aborts into the debrief and its closer look
  (`GE_HITMAP_PROBE=40,zoom`).
- **HD pack:** with `HD_PACK=<zip>`, Dam and Facility run with that pack.

It also copies every DLL beside the executable, now that the build ships
Sentry's. Shader ids do not change with the texture filter (a filter change
recompiles the same list), so one recording covers every filter setting.

**Result:** a full recording with the 2026-11-07 build (`36ded5a` plus this
change) and the GLideN64 HD pack ran all 55 scenarios with exit 0. It compiled
71 distinct programs. Exactly one was missing from the list:
`0000000001080108 10000100`, a fogless world-geometry program with a
horizontally clamped texture, first used in the ADS run (all guns, intro
skipped). It is added (89 to 90 lines). The icons, hit-map debrief, HD pack and
multiplayer runs needed nothing new, and the strip's two programs were
recorded in the Facility run as expected. Not covered: the hidden Iron Sights
font (MSDF programs; players cannot select it), the downloaded Enhanced font,
and non-default display modes such as N64 AA or Classic Mode. A player's own
`Data/shaders.txt` still learns anything beyond the list after its first use.

**Sources:** [recorder](scripts/port/record_shaders.sh),
[list](tools/shaders_base.txt), [prewarm](port/fast3d/gfx_pc.cpp).

### FORK-056 — Outlined text keeps clean outlines at high resolution

**Status:** Implemented in the Windows build (2026-11-07) after the owner
reported jagged gray outlines on HUD ammo digits such as 6 and 7. The owner
play-tested it and approved it the same day.

Outlined text is the white text with a gray ring around it: the ammo counters,
the bottom-left HUD messages and the countdown timers (`textRenderOutlined`).
Upstream draws the ring as eight copies of each glyph, one native pixel away
in each direction, under the glyph itself. At 320x240 that is the glyph grown
by a one-pixel square. Upscaled, the copies sit several screen pixels apart (4.5
at 1080p), so where a diagonal stroke ends, a copy's flat end shows as a step
in the ring. The 7's stem showed it most. The Original font's blur mostly hid
it, but the sharp Enhanced and Iron Sights fonts showed it clearly.

The PC build now draws the outline glyph once, tagged, and the renderer
repeats it around the square one native pixel out, with copies at most one
framebuffer pixel apart (40 at 1080p, 8 at native resolution, where they are
upstream's offsets). The ring is therefore the glyph grown by that square at
screen resolution: straight bands along diagonals and square corners as
before. Copies are offset from the clipped glyph quad, rather than each being
clipped again like upstream's. The half-size HUD halves the square with the
text. All font styles change. With Original, the ring is also a more even gray
band instead of the soft overlap of eight blurred copies. The N64 build keeps
upstream's loop. There is no setting, and input is unchanged.

**Upstream hooks:** `textRenderGlyphOutlined` in `textrelated.c` replaces its
eight-copy loop on PC with one quad between `PORT_TEXT_OUTLINE` tags (`G_NOOP`
`0x5D`). In `gfx_pc.cpp`, the display-list parser sets `s_GeTextOutline`,
frame start clears it, and `gfx_draw_rectangle` emits the ring of copies for
a tagged glyph.

**Sources:** [glyph outline](src/game/textrelated.c), [renderer](port/fast3d/gfx_pc.cpp).

**Checks:** the Windows build compiles and links. With all three font styles,
1920x1080 HUD audits (`audit_hud.ps1 -Resolution 1920x1080 -FontStyle ...`)
before and after the change show the step in the 7's outline is gone with
Enhanced and Iron Sights. Square digits such as 8 and 0 keep their outline
shape. A 3840x2160 request was clamped to a 1600x900 window on this display,
so 4K was not captured. `test_hud.sh`, `test_open_fonts.sh` and
`test_enhanced_fonts.sh` pass. The hand gallery regenerated with zero changed
image cells (source fingerprint only).
### FORK-055 — Growth soak audit and a bounded texture-name pool

**Status:** Implemented 2026-11-07. Measured on Windows (plain and with an HD
pack) and in desktop Chrome; Linux builds but no Linux soak was run.

`GE_SOAK=<stages|all>[:<frames>[:<laps>]]` plays solo stages over and over in
one process, each for a number of frames (Agent; the stage's normal start), and
returns through the title between them as an attract demo does. It logs
`ge: memstats:` at each stage's end and again in the menus after it: process
memory and the renderer's resource counts. On Windows that is private bytes and
working set. In the browser it is the Wasm memory size and dlmalloc's in-use
heap. On Linux it is `/proc/self/statm` and `mallinfo2`. The renderer counts
are texture cache entries, the free texture-name pool, live GL textures,
framebuffers, shader programs and HD-pack bytes. `GE_MEMSTATS=1` alone logs one
line about a second into each stage.
`scripts/port/soak.py` runs it on an isolated executable copy and settings (GE_DRIVE,
invulnerability, fixed seed) and reports per-counter growth from lap 1 (lap 0 is
warm-up) to the last lap. `--analyze` reads a browser log. `bun run audit:soak
<rom> <out> <origin> [spec]` (`web/tools/soak-audit.mjs`, private, actual ROM)
sets `GE_SOAK` in `Module.ENV` before Play; the game reads it live there.
These are test hooks only, with no settings or input changes.

The soak found one unbounded resource. `gfx_pc.cpp` keeps evicted texture names
for reuse instead of deleting them (upstream behavior), and that pool only grew.
On Windows, live GL textures rose 309, 322, 344, 344 over four laps of all 20
stages. The pool is now capped at 128 names (`GE_FREE_TEXTURE_IDS_MAX`): beyond
that the oldest are deleted. HD-pack textures, which were already deleted
rather than pooled, are unchanged.

**Upstream hooks:** `front.c` gains `portSoakLaunchTick` beside the existing
direct launchers (the same folder/difficulty/briefing recipe) and lets
`MENU_RUN_STAGE` route a soak stage's end to the legal screen, as it does for
the icon stage. `lv.c` calls `portSoakTick` next to the other per-frame test
hooks. `gfx_opengl.cpp` counts live textures in `new_texture`/`delete_texture`
and publishes framebuffer and program counts at frame end (SDL atomics; `<atomic>`
there breaks the Linux build's header order). `gfx_pc.cpp` publishes its counts
at frame start, and its existing `gfx_release_cached_texture` now trims the pool.
`hd_texture_import.inl` publishes the HD budget it already sums.

**Sources:** [soak harness](port/src/port_soak.c), [driver](scripts/port/soak.py),
[browser soak](web/tools/soak-audit.mjs), [renderer](port/fast3d/gfx_pc.cpp),
[GL backend](port/fast3d/gfx_opengl.cpp), [HD budget](port/fast3d/hd_texture_import.inl),
[menu hook](src/game/front.c), [frame hook](src/game/lv.c).

**Checks:** Windows, all stages, 600 frames, 4 laps (about 9.5 min). Before the
cap, private bytes were flat at about 263 MB, and shader programs plateaued at
48 with no framebuffer growth, but the pool reached 298 names. With the cap,
the pool peaks at 82, live GL textures stay at 297-304 on every lap and private
bytes stay at about 263 MB. Every run completed and exited cleanly. Windows with
the HD pack (before the cap): private bytes flat at about 279 MB, the HD budget held,
textures flat after warm-up. Chrome, all stages, 300 frames, 4 laps (before
the cap): in-use heap flat at about 177.5 MB and the Wasm memory unchanged at
624 MB; live textures 251 to 298, the same pool growth. Windows, Linux and Wasm
builds pass, and the hand gallery regenerated with zero changed image cells.

### FORK-054 — Intro logos always draw with at least 4x MSAA

**Status:** Implemented in the Windows build (2026-11-07); the owner asked for
it after reviewing captures of both logos with MSAA off, 2x, 4x and 8x and N64 AA.
Not yet play-tested by the owner.

The spinning 3D Nintendo and Rareware logos at startup (`MENU_NINTENDO_LOGO`,
`MENU_RAREWARE_LOGO`) render with at least 4x MSAA whatever the player's
`[display] msaa` is. Upstream draws them at the player's level like every other
frame, which at the 2x default (FORK-029) or with MSAA off leaves visible stair
steps on the logos' slow-turning outer edges. A player's 8x stays 8x. N64 AA
and Classic Mode keep the player's choice, because the N64 AA resolve visibly blurs
the logos at window resolution. Every other screen, including the legal
screen, the black switch between logos and the eye intro, keeps the player's level. There is no
setting; the `msaa` help line in both menus and `tools/midas_ini_help.txt` say
the intro logos always use at least 4x. No input changes.

**Upstream hooks:** `gfx_pc.cpp` already raises the frame's MSAA to at least
4x for the inherited Line Mode cheat in `gfx_start_frame`; the fork adds a
second request (`gfx_set_intro_logo_aa`, an atomic the render thread reads at
frame start, like Line Mode's) that joins the same `std::max` floor, guarded by
framebuffer support. `front.c`'s per-tick menu interface handler sets it inside
its existing `GE_VIDEO` block. With the render thread on, the floor can lag the
menu by one frame at each change; both edges fall on black switch screens.

**Sources:** [renderer](port/fast3d/gfx_pc.cpp), [menu hook](src/game/front.c),
[help](port/src/port_option_help.c), [ini help](tools/midas_ini_help.txt).

**Checks:** the Windows build compiles and links. 1080p intro captures (every
30 frames to frame 1080) with the new build at `msaa=1` and `msaa=2` are pixel
identical to each other on both logos and match a previous-build `msaa=4` run
wherever the animation is in step; zoomed edges at `msaa=1` show 4x
coverage shades. `msaa=8` differs from the 4x frames (its 8x level is kept).
The eye intro at `msaa=1` stays identical to the previous build's `msaa=1`.
The hand gallery regenerated with zero changed image cells (source fingerprint
only).

### FORK-053 — HD texture packs: downloaded or copied in by the player

**Status:** Implemented in the native Windows and Linux builds (2026-11-06);
not in the browser or Xbox builds. The owner reviewed it in play and approved
it on 2026-11-06; custom packs (added the same day) await the owner's review.

Iron Sights again draws HD campaign environment textures from intermissionfb's
GoldenEye 007 HD pack, but never bundles it. The pack's maintainer disallowed
integrating its textures in a port and asked that packs stay "an external mod
added by the user and kept updated by its original maintainers"
([GhostlyDark/GoldenEye-007-HD#3](https://github.com/GhostlyDark/GoldenEye-007-HD/issues/3),
2026-11-05). The owner reads that as allowing a download the player starts,
from the makers' own GitHub, as long as no Iron Sights package carries the art
(owner's decision, 2026-11-06; the maintainer was not asked about a download).
So the pack arrives only when the player chooses Download, or when they copy it
in; releases still carry nothing under `Data/TexturePacks` (the packaging
guards from FORK-033 stay), and Iron Sights does not convert or modify it.
Since 2026-11-06 the same folder also takes **custom packs**: anything else a
player puts there, such as the artist repository's own ZIP (asked for in
[issue #6](https://github.com/pRizz/Iron-Sights/issues/6#issuecomment-6671173971))
or a pack of their own being made.

**Gameplay > Iron Sights > System > Texture Pack** (`[display] texture_pack`:
0 Original, 1 HD; default 1; there is no 4K choice), in the Options folder and
the watch, below the Enhanced Fonts row, followed by an **HD Textures** page. With HD, the game looks in `Data/TexturePacks` under the
root (beside midas.ini, or the AppImage's writable data folder) for a
GLideN64 PNG pack exactly as published: any ZIP, such as
`ge007-hd-v2025.12.30-gliden64-png-hd.zip`, and any PNGs from an extracted
copy, in any subfolder. With no usable texture found the value reads `HD (Get)`
(`HD (not found)` where this build cannot download) and the originals stay.
FORK-033's ini values `hd`, `4k` and `2` read as HD.

- **Download page.** HD Textures (Options folder and watch), and **GET HD
  TEXTURES** as a row of the Options folder's top level (after GET ENHANCED
  FONTS) while the files can be downloaded and are not all in place, work as
  the Enhanced Fonts page does (FORK-052): a description naming the pack, its
  author, its 150 MB size and its source, a status line, then Download /
  Cancel Download / Retry Download / Use HD Textures / Use Original Textures
  and Remove HD Textures (the folder asks Yes/No, the watch asks twice).
  Picking Download is the player's consent. A finished download sets Texture
  Pack to HD; Remove leaves the setting alone (HD without a pack draws the
  originals). Status lines count megabytes (`Downloading: 59 of 150 MB`), a
  busy server's wait, or a failure's reason.
- **What it fetches.** Pinned to release v2025-12-30: the PNG release's ZIP
  (135,256,079 bytes, SHA-256 `a72d082e...`, which matches GitHub's own digest
  for the asset) from the release, and the three Egyptian textures missing from
  that ZIP from the repository at the revision the scope came from (their SHA-1
  blob ids were checked against the repository's). Files go to
  `Data/TexturePacks` and `Data/TexturePacks/ge007-hd-egyptian`, then
  `SOURCE.txt` records the source, the author and that the art is not part of
  Iron Sights. `hd_download_engine.cpp` streams each file to a `.part` name
  while hashing it, checks size and SHA-256, and renames it into place; a busy
  server (429, 502-504) is asked again after its Retry-After or 2, 4, 8 and
  16 s, five tries a file. The shared HTTPS layer gained a streaming form for
  this (`platform_stream_transport`: no size cap, no overall time limit, curl
  gives up after 30 s under a byte a second), so 150 MB never sits in memory.
  `GE_HDPACK_BASE_URL=http://127.0.0.1:port` points a test at a local fixture.

- **Index.** `hd_texture_pack.cpp` reads each ZIP's central directory (stored
  and deflated entries, ZIP64 included, with 64-bit offsets, so the
  repository's 2.4 GB ZIP reads on the 32-bit Windows build; encrypted
  entries, other methods and images over 80 MB are skipped) and lists PNGs
  with Rice names (`GOLDENEYE#tttttttt#f#s[#pppppppp|#$]_all.png` /
  `_ciByRGBA.png`). Custom packs come first, then the download; within each,
  extracted PNGs win over ZIP copies, and the order is sorted by path, so the
  same files always give the same choice. The log line counts both
  (`hd: N textures in ... (C from custom packs, D from the HD download)`).
- **Downloaded or custom.** The files the HD Textures page fetches, at their
  exact paths (the release ZIP's name at the top of the folder and the three
  PNGs in `ge007-hd-egyptian`), are the download and keep the reviewed scope
  below. Every other ZIP or PNG under `Data/TexturePacks` is a custom pack: it
  is read for every texture it names (the scope's aliases still apply) and
  wins over the download, so a player can add the repository's extra
  textures (effects, smoke, explosions, crosshairs, gun barrel, portraits,
  watch art, world weapon pickups) or override any of them with their own.
  The Hacks folder's textures are for ROM hacks and do not match this game's
  textures. A copied-in release ZIP under another name, or an extracted
  release, is a custom pack too.
- **Custom images load in the background, at the HD tier.** The repository
  holds the artist's masters (mostly 1024 to 4096 px; the release's copies are
  mostly at most 256 px), which take about 100 ms each to inflate at 2048 px
  (270 ms at 4096 px, measured at -O2 on Linux). Read on the render thread,
  they froze Dam's first frames until the stall watchdog exited the game, and
  a handful would have filled the 128 MB budget. So a custom pack's image is
  read on one of two worker threads while the original draws, reduced to at
  most 8x native per side (`kMaxScale`; the tent filter widens with the
  ratio, and it now premultiplies as it reads, so a 4096 px master needs a
  16 MB buffer rather than a 128 MB copy), and swapped into the waiting cache
  node at the next frame start (`hd_request`; a node evicted meanwhile is
  skipped). An image that cannot be read, or an allocation the 32-bit
  Windows build's address space refuses, keeps the original. Re-indexing
  discards outstanding work. The download's images keep the immediate read,
  so its reviewed look is unchanged. This matches the Original/HD choice:
  there is still no 4K tier.
- **Custom Texture Packs page (2026-11-06).** Gameplay > Iron Sights >
  System > **Custom Texture Packs**, after HD Textures in the Options folder
  and on the watch (native builds only, like HD Textures), explains in seven
  short lines where a player's own pack goes, what counts as one, that it wins
  over the download and updates within seconds, and that the gun and hands
  stay original; a status line then reads `Custom packs: N textures`, `No
  custom pack found.`, `Looking for packs...` (before the first search) or
  `Texture Pack is Original.`, from the index's custom count
  (`portTexturePackCustomCount`). **Open Folder** creates `Data/TexturePacks`
  if needed (`portTexturePackFolder`) and opens it with `portOpenFolder` in
  `port/fast3d/about.cpp`, which hands `SDL_OpenURL` a `file://` URL
  (backslashes turned to slashes, spaces and other awkward bytes escaped), as
  the About page opens links. The opener lives in C++ beside the About code
  because `SDL.h` cannot follow `ultra64.h` on glibc: `PR/os.h` declares
  `bcopy`, `bzero` and `bcmp` with `int` lengths, and the Linux CI build of the
  first version failed on those conflicts. The page is a `PortDownloadPage`
  without a Remove (action 1 is NULL, so the folder skips that button); the
  watch's rows are `sIsCustomTex` (`PWHINT_CUSTOMTEX` 3000, `PWACT_CUSTOMTEX`
  114), and its page sits before the Control tab's, so their numbers do not
  move. `GE_MENU=zcustom` opens the folder page. Input: the description and
  status are text rows, and Open Folder is an action row that every input
  confirms as on the HD Textures page; Back leaves as on every page.
- **Tracing.** `GE_HD_TRACE=1` logs each texture once: `match` (the download),
  `custom` (requested from a custom pack), `miss` (no pack has it), and `kept`
  for every first-person weapon or hand texture, naming the pack file that
  has it, so a pack author can see why weapon art does not apply in the hand.
- **Protected from every pack.** The first-person weapon and hands keep the
  game's textures, so the hand and arm contracts (`arm_rendering.md`) hold
  whatever a pack contains: `gunfire.c` brackets the span from the ADS hand
  layer to its end with `portHudViewmodelTag` (a `0x5C` G_NOOP), and while it
  is open the renderer neither looks a texture up in the pack nor reuses a
  pack image the world drew (`hd_native` in the cache key: a pickup on the
  floor may use the same texture). The flag also clears at frame start. Text
  glyphs stay on their own paths (FORK-050, FORK-052), and the ammo icons take
  the pack's art by slot.
- **Scope.** The download draws campaign environment textures: our inventory of the
  pack's area folders (`port/fast3d/texture_packs/hd_level_sources.json`,
  641 names) plus our reviewed native-palette aliases
  (`hd_level_native_aliases.json`, 162, and `hd_dam_native_aliases.json`,
  15; plus `hd_level_close_aliases.json`, 17, under Close HD Matches below),
  restored from FORK-033 and embedded at configure time
  (`hd_texture_scope_embed.cmake`). They hold names and fingerprints, no
  artwork. Since 2026-11-07 it also covers the gunbarrel intro, sky clouds,
  impact effects, explosions and smoke (below). Weapons, fonts and the rest of
  the HUD are outside it.
  With the v2025.12.30 PNG pack 466 of
  the 641 are present (640 keys with aliases, 610 before the coverage
  audit below); three Egyptian textures exist
  only in the artist repository and are used if the player adds those PNGs.
- **Images.** A texture is read when a level first draws it: inflated with
  the vendored puff and decoded by `png_decode.cpp` (8-bit RGB/RGBA,
  non-interlaced; every image in the v2025.12.30 pack is one of these). An
  image must keep the native aspect ratio and be at least the native size,
  otherwise the original stays.
- **Renderer.** Restored from FORK-033 (`hd_texture_import.inl`): the Rice
  fingerprint of each native tile (`hd_texture_format.cpp`), the upload at the
  authored scale with native UVs and masks, the `hd_bytes` cache field, deleting
  rather than recycling HD texture ids, and a 128 MB budget evicted at frame
  start. `hd_textures.cpp` re-indexes when the setting changes and when any
  file under the pack folder is added, removed, replaced or edited (a
  recursive listing of paths, sizes and times, checked about every 5 s), so a
  pack being made shows each saved PNG within seconds.
- **Coverage audit (2026-11-07).** A local audit ran each campaign level (an
  isolated copy, walking and firing rockets while turning) with the pack
  treated as custom, so every key could apply, and compared the traced
  textures with the scope. Two gaps in the intended coverage were closed:
  - *Row order.* GE swaps odd rows only for textures it builds a set number of
    images for (`texSwapAltRowBytes` in `src/game/image.c`), so GLideN64
    hashed the others unswapped. `Pack::find(tile, key)` tries the swapped key
    first, then the natural row order (`hd_texture_pack.cpp`); `lookup` and
    the `kept` trace use it. Mostly effects outside the scope matched this
    way, plus about one Dam environment texture.
  - *Palette aliases.* 47 in-scope textures were drawn with a native palette
    that no alias named. Native/HD contact sheets were reviewed as for the
    first 132: 30 were added to `hd_level_native_aliases.json` (signs and
    decals, brick, windows and posters in Streets, stone in Statue, foliage
    and panels in Jungle, and others in Depot, Caverns, Frigate, Runway,
    Archives and Bunker). The other 17 have colours that only roughly match:
    noise-like native colours (perhaps a palette read in another format),
    opaque corners or transparent bands that the HD art fills differently,
    or painted detail this palette variant hides. The private evidence is in
    `build_win/hd-alias/review`.
  - *Close HD Matches.* Those 17 are in `hd_level_close_aliases.json` and
    apply while **Gameplay > Iron Sights > System > Close HD Matches**
    (`[display] close_hd_matches`, default 1, after Texture Pack in the
    Options folder and on the watch; native builds only, like Texture Pack)
    is on, so they show by default and a player can keep those originals.
    `hd_textures.cpp` keeps two scopes, with and without them, and
    re-indexes when the setting changes; custom packs take the same
    aliases. Input: an ordinary toggle row, adjusted as every other one by
    keyboard, mouse, gamepad and the watch's arrows.
  The audit scripts and ROM-derived images stay local. Checks: `test_hd_textures.sh` gained a fixture PNG named
  for a tile's natural row order, found second for the swapped tile and not
  tried for an unswapped one. With the normal download in isolated copies of
  the Windows build, Depot, Caverns, Streets, Statue, Jungle, Frigate, Runway,
  Archives and Bunker indexed 653 keys (623 before) and the trace showed all
  30 new aliases as `match` and none of the 17 close ones (before they had a
  setting). `test_hd_textures.sh` also checks that the close list parses,
  names in-scope textures and shares no palette with the reviewed lists.
  With the setting on, nine levels indexed 670 keys and traced all 17 close
  aliases as `match`; with it off, Runway, Jungle and Depot indexed 653 and
  drew the 8 they reached as originals. A 1280x720 capture shows the row
  and its help line on the Options folder's System page. Not checked: the
  live re-index when the row changes in play, and the watch's row.
  Not run: Linux, a long play session, and side-by-side captures of each
  new alias.

- **Intro, sky and effects (2026-11-07).** The download's scope grew past the
  environments, following the coverage audit:
  - `hd_effect_sources.json` (308 names, matched by key): the gunbarrel intro's
    299 strips (`UI/Gunbarrel`, 440x1 I8 drawn as 1760x4), the sky clouds
    (64x64 IA8 as 512x512, in 12 levels) and eight impact textures (debris,
    five spark sprites, glass shards, the bullet hole). `parse_scope` now takes
    several texture lists.
  - `hd_effect_remaps.json` (28 remaps): explosions and their smoke. Each of
    the 15 frames in `g_ExplosionDisplayLists` draws, in two cycles, a 56x56 IA8
    smoke texture under a 64x64 wrap mask times a 16x14 RGBA16 fire frame
    (`IMAGE_SMOKE_n`, `IMAGE_FIRE_n`). GLideN64 hashed the fire frame as 16
    rows and the smoke as 64 rows of 64 bytes at its 56-byte stride, both
    running past the data into the next pool record: an 8-byte header (the
    texture number and 6 stale bytes, `texLoad`) and the next texture. With
    those stale bytes zero, 8 fire and 9 smoke textures reproduce a pack key
    exactly; the others take the remaining distinct art in the bright-to-dark
    order between them. A remap names a native key and a pack key (which joins
    the scope) plus `keep_rows`/`of_rows` (the fire frames keep 14 of 16 rows;
    `Pack::read` crops) or `masked` (the smoke image covers the whole 64x64
    mask, exactly what `ge_upload_masked` rebuilds the tile as).
    `hd_texture_import.inl` now looks mask-wrapped tiles up with
    `maskWrapped`, which only masked entries satisfy, and uploads them at the
    mask's size; Rare's RGBA16 water stays excluded. Frame 0 is fully
    transparent on both layers and keeps its original: paired with the pack's
    dark smoke it painted black puffs in the middle of every blast. Frames 13
    and 14 are transparent too, but their exact matches carry the artist's
    fading smoke and stay.
  Not done: the death effects, monitors and crosshairs never appeared in the
  audit, and letters stay on the font paths. A custom pack's copies of these
  keys are remapped the same way. Checks: `test_hd_textures.sh` covers several
  texture lists, a remap's row crop, a masked remap, a refused crop and a
  document with neither list. Captures with the download (isolated copies of
  the Windows build) traced the 299 intro strips, the clouds in Dam, Runway and
  Surface, seven impact textures in Facility with a KF7 (the bullet hole did
  not appear there) and all 28 explosion and smoke remaps in Dam as `match`;
  the intro, clouds, sparks and debris, and the explosions frame by frame,
  were compared with the original side by side. Not checked: Linux, the
  bullet hole in play, and other levels' explosions.

The removed `.istex` format, the CI preparer and its download, the bundled
packs and the AppImage bundle root do not return.

- **HUD ammo icons (2026-11-06).** With Texture Pack on HD, the HUD's ammo
  icons draw the pack's own `UI/Ammo` art, the feature FORK-047 had embedded
  and then removed: 13 of the 14 slots (the plain mine has no art; the tank
  shell's six palette copies are one image). The pack's keys never match the
  game's 32-bit icons, so, as in `5a3b7c0`, the ammo draw tags its slot
  (`port_hud_ammo.c`, a `0x5B` G_NOOP from `gunfire.c`) and the renderer reads
  that slot's PNG from the pack, resizes it to the icon's visible size x8
  (`hd_textures::resample`: a tent filter in premultiplied alpha; the art has
  other proportions than the icons) and lays it over a canvas the size of the
  padded native tile x8, keeping native UVs, placement and colours, sampled
  smoothly. The art is read from the installed pack, never embedded; there is
  no separate setting.
- **Edge smear fixed (issue #6).** The renderer's in-shader clamp for a tile
  wider than its visible image stopped half a native texel short of the
  edge, so on an enlarged image the last half native texel (four texels of 8x
  art) repeated one column: the smear reported on the 5- and 7-texel ammo
  icons. Each cached texture now records its enlargement (`hd_scale_s/t`), and
  the clamp stops half a texel of the uploaded image short instead. It applies
  to every HD image, the pack's environment textures included.

**Unsupported, by decision:** the browser and Xbox builds have neither the
Texture Pack row nor the HD Textures page; 150 MB in browser storage would not
suit the browser version. The 4K and HTS versions of the pack are not read or
offered (7z archives in a different format). Linux downloads need libcurl,
like the fonts; without it the page reads `Copy to Data/TexturePacks`.

**Input:** both pages use the shared download-page code, so keyboard, mouse
and gamepad confirm, back out and scroll as on Enhanced Fonts; there is no
browser touch context because the browser build has no such page.

**Earlier rendering fixes, kept:** the restored adapter keeps FORK-033's
wrap-mask cropping at the authored scale and leaves special composite and
water addressing on the original path, and images keep the PNG's own row
order (the prototype's vertical flip of asymmetric braces and labels was fixed
by that in FORK-033).

**Upstream hooks:** in `gfx_pc.cpp` the texture release helper at the cache's
five free sites, the import call before the format decoders, the frame-start
call and the include of the adapter; `hd_bytes` in `gfx_pc.h`; the ammo slot tag (`0x5B`) and its smooth sampling, and the clamp's
HD scale in the vertex setup, in `gfx_pc.cpp`; two `portHudAmmoIconTag` calls
around the ammo icon draw in `gunfire.c`; the first-person tag (`0x5C`), its
frame-start reset, the `hd_native` cache-key field and the skipped import in
`gfx_pc.cpp`/`gfx_pc.h`, the `hd_request` cache-value field, and two
`portHudViewmodelTag` calls in `gunfire.c` (inside the existing
`#ifndef TARGET_N64` blocks around the hand layer).

**Sources:** [pack index](port/fast3d/hd_texture_pack.cpp) and [header](port/fast3d/hd_texture_pack.h),
[ammo slot tag](port/src/port_hud_ammo.c) with its [declarations](port/include/port_hud.h) and
[draw hook](src/game/gunfire.c), [HUD audit](scripts/port/audit_hud.ps1),
[download engine](port/fast3d/hd_download_engine.cpp), [download API](port/fast3d/hd_textures_download.cpp)
and its [C header](port/include/port_hd_download.h), [menu actions](port/src/port_hd_download.c),
[shared page table](port/include/port_download_page.h), [streaming transport](port/fast3d/enhanced_fonts_http.cpp),
[runtime](port/fast3d/hd_textures.cpp), [identity](port/fast3d/hd_texture_format.cpp),
[adapter](port/fast3d/hd_texture_import.inl), [scope data](port/fast3d/texture_packs/hd_level_sources.json),
[embedding](port/fast3d/hd_texture_scope_embed.cmake), [C API](port/include/port_hd_textures.h),
[renderer](port/fast3d/gfx_pc.cpp), [build](CMakeLists.txt), [setting](port/src/video.c),
[options](port/src/port_options.c), [frontend row](src/game/front.c),
[watch row](port/src/port_watchpages.c), [help](port/src/port_option_help.c),
[watch page list](port/include/port_watchpages.h),
[headless fallback](port/src/libultra_shim.c), [ini reference](tools/midas_ini_help.txt),
[notices](tools/THIRD_PARTY_NOTICES.txt), player guides ([Windows](scripts/port/release/START-HERE.txt),
[Linux](scripts/port/linux/START-HERE.txt), [Data](scripts/port/release/DATA-README.txt)),
[release notes](scripts/port/releases.md), [README](README.md),
[tests](scripts/port/test_hd_textures.sh) with the [pack fixture](scripts/port/make_hd_texture_fixture.py)
and the [download test](scripts/port/test_hd_download.cpp), and [CI](.github/workflows/windows-release.yml).
**Checks:** `test_hd_textures.sh` (Docker) checks the restored fingerprint
vectors, Rice names, the scope, and the index over a generated pack (deflated
and stored ZIP entries, an extracted PNG, a palette alias, an out-of-scope name
and a corrupt image); it runs in the release workflow. The Windows build
compiles and links. With the local v2025.12.30 PNG pack in an isolated folder,
Dam found 610 textures from the ZIP and the same from the extracted folder, and
1600x900 captures show its rocks, mountains, road and stripes in HD while the
weapon's and hand's surfaces stay pixel-identical (edges differ by sub-pixel
pose shifts, which also occur between two HD runs). The download engine's
test (fake transport) covers success, a checksum mismatch, a longer body than
pinned (which caught a bug: a stopped transfer had been accepted), busy retries
and giving up, cancel and removal sparing the player's own files. In the game,
Download on the HD Textures page against a local fixture serving the same four
files showed `Downloading: 59 of 150 MB`, then `Installed.` and Use Original
Textures; the files verified and the pack loaded 616 textures (the Egyptian
three and their aliases added). Captures show GET HD TEXTURES in the Options
folder, the watch's System page reading `HD (Get)` and its HD Textures page.
`test_enhanced_fonts.sh` and `test_ads.sh` still pass. Ammo icons: the
resampler test (no colour bleeding from transparent texels, uniform images stay
uniform), `test_hud.sh`, and 1600x900 HUD captures (`audit_hud.ps1`, which
gained `-TexturePack`) of the 9 mm, rifle, magnum and grenade icons in HD with
clean right edges, against the originals. Not run: a download from GitHub
itself, the Linux build locally, other levels, and a long play session.
Custom packs: the fixture gained `player/`, a hand-written ZIP64 archive (every
size, offset and count in its 64-bit fields; Python's `zipfile` reads it
back) and a loose PNG, and the tests check that custom copies win over the
download, read keys outside its scope, keep aliases, and that with no
download every file is custom. In the 32-bit Windows build, with GitHub's
"Download ZIP" of the repository (2,387,276,857 bytes, 2,412 entries, 1,091
of them past 2 GiB) in an isolated folder, Dam indexed 2,384 textures, read 56
custom ones in the background (the longest frame gap was 270 ms, against
201 ms with Original; before the background reads the game stalled and the
watchdog exited, and the first background version ran out of address space
in the resampler) and drew the repository's scenery and its rifle ammo art,
whose entry starts at byte 2,352,547,137. A synthetic pack of solid magenta
images for the KF7 view's 13 textures (`kept`, each found in the pack) and
the 56 scenery textures drew magenta scenery around an unchanged KF7 and
hand. With the release ZIP alone the index found 623 keys from the download
(the 610 environment keys and 13 ammo slots); with both, 2,384 custom and 10
from the download. `test_wardrobe.sh`, `test_ads.sh`, `test_hud.sh`,
`test_enhanced_fonts.sh` and `test_upstream_merge.sh` pass, and the hand gallery
regenerated with zero changed image cells. Not run for custom
packs: effects, explosions and menus in play, other levels, Linux, and a long
session. Custom Texture Packs page: Windows build; captures of the folder page
(reading `Custom packs: 69 textures` with a test pack) and the watch page
(`No custom pack found.` without one); the first folder capture caught a crash
from a missing sheet title, fixed by adding it to `port_sheet_title`;
`test_hd_textures.sh`, `test_enhanced_fonts.sh` and `test_web_menu_pointer.sh`
pass, and the hand gallery regenerated with zero changed image cells. Not run:
pressing Open Folder (it would open a file manager on the desktop) and Linux.

### FORK-052 — Font styles plan

**Status:** Plan accepted with its recommended decisions on 2026-11-05.
FONT-1 (the setting), FONT-2 (the Enhanced renderer with manual install),
FONT-3 (the in-game download), FONT-4 (the browser download) and FONT-5
(release) are done; the owner signed off on the play-test on 2026-11-06.

**Shared with FORK-053 (2026-11-06).** The Enhanced Fonts pages now run on the
download-page table (`port_download_page.h`) that the HD Textures pages also
use, in both menus; the Options folder's extra rows come from
`portOptionsExtraRows`. Labels, actions, prompts and help keys are unchanged.
The HTTPS layer gained `platform_stream_transport` beside the fonts' capped
in-memory transport, which now wraps it; `test_enhanced_fonts.sh` passes.

**Text Antialiasing (2026-11-06).** Small Enhanced text, such as the 0.5x and
0.6x cheat lines on the difficulty screen, showed strokes of uneven weight.
Each mask is 8x its glyph, so text at native size or smaller shrinks it
several times (about 3.5x for 0.5x text at 1080p), and glyph texture
rectangles had no mip chain unless the 3D filter was linear with anisotropy
on: 3-point filtering forced `GL_NEAREST` plus the shader's three taps at full
mask resolution, and linear with anisotropy off sampled plain bilinear. Each
pixel then read a few of the dozen or so texels under it, so stems snapped to
whole pixels. Captures at 1080p measured it: in the footer text only 21% of
inked pixels were partial-coverage edges with 3-point or with anisotropy off,
against 64% with linear and anisotropy.

**Gameplay > Iron Sights > System > Text Antialiasing** (`[display]
text_antialiasing`, default 1, applies live; in the Options folder and the
watch, after the Enhanced Fonts row) now makes an Enhanced mask minify through
a trilinear mip chain whatever the texture filter; magnification keeps the
chosen filter. The sampler latch treats such a draw as a mip draw
(`ge_mip_active` 2, so toggling re-applies it per texture) and
`gfx_opengl_set_sampler_parameters` builds the chain and sets
`GL_LINEAR_MIPMAP_LINEAR`. 0 samples the masks as before. Original glyphs are
magnified, not shrunk, and the open fonts' distance fields compute their own
coverage, so neither changes. With it on, the same footer measured 67%
partial edge pixels under 3-point and 69% with anisotropy off; with it off the
3-point capture matched the earlier one exactly, and only text pixels change.
Input: an ordinary toggle row, so every supported input changes it as it does
the rows around it. Upstream hooks: the `text_aa` term and the `ge_mip_active`
value in `gfx_pc.cpp`'s sampler latch, and the new first branch in
`gfx_opengl.cpp`'s sampler setup. Sources: [renderer](port/fast3d/gfx_pc.cpp),
[GL sampler](port/fast3d/gfx_opengl.cpp), [global](port/fast3d/gfx_api.h),
[setting](port/src/video.c), [options](port/src/port_options.c),
[frontend row](src/game/front.c), [watch row](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [ini reference](tools/midas_ini_help.txt).
Checks: Windows build; difficulty-screen captures at 1080p with 3-point,
linear and linear without anisotropy, on and off; the System sheet capture;
settings placement and standards checks; `test_enhanced_fonts.sh`,
`test_open_fonts.sh`, `test_hud.sh`, `test_web_menu_pointer.sh` and
`test_upstream_merge.sh`; the hand gallery regenerated with zero changed image
cells. Not run: the browser and Linux
builds at run time, and other screens' Enhanced text side by side.

[`scripts/port/font_styles_plan.md`](scripts/port/font_styles_plan.md)
proposes one **Gameplay > Iron Sights > System > Fonts** setting choosing
Original (ROM), Enhanced (the community GEPD masks removed in FORK-021) or
Iron Sights (the open title font, and the open body font since OPEN-5,
FORK-051), replacing the Sharp Fonts row.
The Enhanced fonts would stay out of every package and commit: choosing them
asks the player before downloading the pack's 186 font PNGs from
Graslu/GEPDTexturePacks at its pinned final commit, checks each file against a
pinned SHA-256 and installs it in `Data/FontPacks/gepd/`, where a manual
install also works. The plan covers the setting and `sharp_fonts` migration,
the consent prompt for every input, WinHTTP, libcurl and browser `fetch()`
transports, the Enhanced renderer rebuilt from FORK-021's history on the
FORK-051 glyph registry, milestones FONT-1 to FONT-5 and the accepted
decisions.
`AGENTS.md` points to it.

**FONT-1 (2026-11-05).** **Gameplay > Iron Sights > System > Fonts**
(`[display] font_style`: 0 Original, 1 Enhanced, 2 Iron Sights; default 0)
replaces the Sharp Fonts toggle in the Options folder and the watch. Iron
Sights draws Bank Gothic text with the open title font (FORK-051); Original
draws the ROM fonts, and so did Enhanced until FONT-2. An older `sharp_fonts=1` loads as
Iron Sights unless a `font_style` line is present; saving writes only
`font_style`. Sources: [setting](port/src/video.c),
[options](port/src/port_options.c), [frontend row](src/game/front.c),
[watch row](port/src/port_watchpages.c), [help](port/src/port_option_help.c),
[ini reference](tools/midas_ini_help.txt), [notices](tools/THIRD_PARTY_NOTICES.txt),
[README](README.md), a `-FontStyle` choice (Original, Enhanced, IronSights,
LegacySharpFonts) on the [HUD audit](scripts/port/audit_hud.ps1) and a
`-WatchPage` parameter on the [sight/watch audit](scripts/port/audit_sights.ps1);
the audits' `-SharpFonts` switches now write `font_style=2`.

**Checks:** the plan's download size (247,988 bytes) and file list were
checked against the pinned commit through the GitHub API. For FONT-1, the
Windows build compiles and links and settings placement and the standards
check pass. 1280x720 HUD captures with `font_style` 0 and 1 draw the ROM font
(maximum vertical edge step 74 on the mission hints), while `font_style=2`
and a file with only `sharp_fonts=1` draw the open font (200 and 203); a
1600x900 capture of the watch's Iron Sights > System page shows the Fonts row
reading Iron Sights. Saving a migrated file was not exercised at run time.

**FONT-2 (2026-11-05).** With Fonts on Enhanced, both fonts' glyphs are drawn
from Carnivorous's CC0 GEPD font images, which the player installs in
`Data/FontPacks/gepd/` under their pack file names; Iron Sights ships none of
them. `port/fast3d/enhanced_fonts_list.inc` pins the 186 files (93 per font;
the pack has no tilde) by font, character, path in the pack, size and SHA-256,
taken from FORK-021's removed manifest with sizes from the GitHub tree at
commit `ca06e063`. Files map to glyphs by character rather than by the pack's
texture CRCs, because the game clears each glyph's padding before registering
it. On the first Enhanced glyph the port reads the folder, skips any file
whose size or checksum differs (logging it), decodes the PNGs and prepares 8x
masks with the removed generator's rules, now at run time
(`enhanced_font_masks.cpp`): the red channel, authored X coordinates with at
most one native texel of overhang, oversize images fitted with a port of
Pillow's Lanczos filter, and fringe that would touch a tightly kerned
neighbour trimmed using the font's kerning table. The texture import uploads
the mask under its own cache-key flag (`enhanced_glyph`), it is always
smooth-sampled, and the texture rectangle grows a whole glyph's quad to show
ink past its width, as FORK-021 did. Glyphs without a valid file keep the ROM
bitmap. Advances, kerning and line spacing stay the ROM's.

The font loader's fork block in `textrelated.c` also registers each glyph
with its kerning index and its font's kerning table
(`portEnhancedFontsGlyph`, with a weak no-op in `libultra_shim.c`). PNG
decoding moved from `open_fonts.cpp` into `png_decode.cpp`, which also reads
8-bit RGB. The font sources build in the `ge_open_fonts` object library.
`scripts/port/enhanced_fonts_pack.py install|check --dest DIR` downloads the
pinned files from the pack's repository at that commit and checks them, for
manual installs and local tests; `audit_hud.ps1 -FontPack DIR` and
`audit_fonts.ps1 -FontPack DIR` copy a folder into the audit's isolated
`Data/FontPacks/gepd`. Sources: [masks](port/fast3d/enhanced_font_masks.cpp),
[loader](port/fast3d/enhanced_fonts.cpp), [file list](port/fast3d/enhanced_fonts_list.inc),
[PNG decoding](port/fast3d/png_decode.cpp), [renderer](port/fast3d/gfx_pc.cpp),
[cache key](port/fast3d/gfx_pc.h), [font registry hook](src/game/textrelated.c),
[weak fallback](port/src/libultra_shim.c), [setting](port/src/video.c),
[build](CMakeLists.txt), [pack tool](scripts/port/enhanced_fonts_pack.py),
[tests](scripts/port/test_enhanced_fonts.sh).

**FONT-2 checks:** `enhanced_fonts_pack.py install` fetched all 186 files and
each matched its pinned size and SHA-256. With them, a local harness (not
committed; it used the removed manifest's glyph metrics and the extracted
fonts' kerning tables) found all 186 run-time masks byte-identical to the
FORK-021 generator's recorded mask checksums, including the 15 fitted with
Lanczos and the 2 trimmed. `test_enhanced_fonts.sh` (added to the release
workflow) checks the pinned list, verification, RGB PNG decoding with row
filters, mask preparation and fitting, Lanczos resizing, the kerning trim,
quad growth and that the loader refuses a file with the wrong checksum, with
no pack files; `test_open_fonts.sh` passes with the shared decoder. The
Windows build compiles and links, and the new sources compile under the CI
Linux image's GCC 11. In 1280x720 HUD captures with the files installed,
Enhanced draws the timer, ammo, mission hints and weapon strip sharply in the
original letter shapes and places; without them it matches Original. A
1280x960 difficulty-screen capture shows Zurich Bold text replaced. Not yet
run: the Linux and browser builds, and captures beyond 720p and 960p.

**FONT-3 (2026-11-05).** **Gameplay > Iron Sights > System > Enhanced Fonts**
opens a page in the Options folder and the watch. It says who made the fonts,
their license, that Iron Sights does not include them, and the download's
size and source (186 files, 250 KB, GitHub Graslu/GEPDTexturePacks), then
a status line (Not installed, Incomplete: N of 186, Downloading: N of 186,
Failed: reason, Installed) and two actions. The first reads **Download**,
**Cancel Download**, **Retry Download**, **Use Original Fonts** /
**Use Enhanced Fonts** (installed: switches Fonts; see FONT-4) or, where the
build cannot download, **Check Folder Again**; the second is **Remove Enhanced Fonts**, which asks first (the
folder's Yes/No dialog, starting on No; on the watch a second press on
**Confirm Remove**, as Reset to Default does) and sets Fonts back to Original
if it was Enhanced. Picking Download on this page is the player's consent; the
game never contacts the network otherwise. While the files are missing, the
Fonts row's Enhanced value reads **Enhanced (Get)**; choosing it applies, and
draws the ROM fonts, rather than jumping to the page, because stepping through
the values with arrows or the stick passes over it. When a download the player
started finishes with every file installed, Fonts switches to Enhanced on the
next frame (`videoStartFrame`).

The download (`enhanced_fonts_download.cpp`) runs on a detached worker
thread: for each pinned file not already valid it fetches
`raw.githubusercontent.com/Graslu/GEPDTexturePacks/ca06e063.../GoldenEye
Texture Pack/<path>` (percent-encoded), checks the size and SHA-256, writes
`<name>.part` and renames it into `Data/FontPacks/gepd/`, then writes
`SOURCE.txt` (source, commit, author, CC0). Cancel stops between files and
inside a transfer; quitting mid-download abandons it, and no partial file is
ever used. Transports (`enhanced_fonts_http.cpp`): WinHTTP on Windows (linked
as `winhttp`) and, on Linux, libcurl loaded at run time (`libcurl.so.4`, then
`libcurl-gnutls.so.4`); each caps a file at 1 MiB and gives short reasons (No
connection, Timed out, Secure link failed, Server error N, File too large, A
file did not match, Folder not writable). A Linux system without libcurl gets
**Check Folder Again** and the manual-install hint. Xbox has no transport;
the browser fetches through its own pump (FONT-4). `GE_FONTPACK_BASE_URL` points the download at a local
fixture for tests and is honoured only for `http://127.0.0.1:` addresses.
`port/src/port_enhanced_fonts.c` holds the actions both menus share; the
menus' C API is `port/include/port_enhanced_fonts.h`, with weak fallbacks in
`libultra_shim.c`. Upstream-file hooks: the folder's `SHROW_INFO` (a text
line) and `SHROW_FONTACT` row kinds, the `PORT_SHEET_IS_FONTS` sheet and a
`GE_MENU=zenhanced` harness in `front.c`; the watch's `PORTWATCH_IS_FONTS` page
with hint and action rows in `port_watchpages.c`, placed before the Control
tab's pages because `options.c` tells those apart by number, so the Control
tab's `GE_WATCHPAGE` sub-page numbers move up by one (the new page is 14);
`port_options.c`'s Enhanced value text; and `videoStartFrame`. Every input
reaches the page and its actions through the existing row handling: keyboard,
mouse and gamepad in the folder and on the watch.

**FONT-3 checks:** `test_enhanced_fonts.sh` also builds
`test_enhanced_fonts_download.cpp`, which drives the engine with a fake
transport: fetching and installing, keeping valid files without fetching,
refusing a mismatched file (nothing written), cancel, a network failure,
removal and URL encoding. A 64-bit Linux build of the engine with the libcurl
transport installed all 186 files from a local server (through
`libcurl-gnutls.so.4`) and reported No connection for a closed port. The new
sources compile under the CI Linux image's GCC 11 (32-bit), and the Windows
build compiles and links. Windows captures at 1280x960, with scripted gamepad
A presses on the folder page: a download from a local fixture installed 186
files and `SOURCE.txt` and switched the text to the Enhanced fonts; a real
download from GitHub over HTTPS installed all 186; a slowed fixture showed
**Downloading: 15 of 186** with **Cancel Download**, and cancelling left
**Incomplete: 18 of 186 files** with no `.part` files; a closed port showed
**Failed: No connection** with **Retry Download**; Remove showed its dialog,
and Yes deleted every file and set Fonts back. The System page shows **Fonts:
Enhanced (Get)** and the new row, and a 1600x900 capture shows the watch page
(seven description lines, both actions, Back) fitting the face. Not covered:
the watch's armed Remove and the Linux and browser builds at run time; harness
runs never write `midas.ini`, so saving the switched setting was not seen. `scripts/port/test_ads.c`
builds `port_options.c` on its own, so it stubs `portEnhancedFontsValueText`;
FONT-3's first push failed CI's tests job at link time without it.

**Get Enhanced Fonts shortcut (2026-11-06).** At the owner's request, so
players find the download, the Options folder's top level gains a fourth row,
**4. GET ENHANCED FONTS**, under Gameplay, Control and Audio Visual. It opens
the same Enhanced Fonts page (whose text and Download action remain the
consent), and Back returns onto it. The row shows only while the build can
download and the files are not all installed: it disappears once the pack is
installed (Back then lands on the last row) and is absent where the build
cannot download (Xbox, Linux without libcurl), where System >
Enhanced Fonts still explains a manual copy. This is a deliberate exception to
keeping fork rows on the Iron Sights pages. The watch has no shortcut: its face
holds exactly nine rows (Rare's seven plus Gameplay and Audio Visual) and a
tenth would make it scroll, so the watch keeps the page under System only.
Source: `portOptionsFontsRow`, `portOptionsRowCount`/`portOptionsRowKind` and
the folder's row drawing and Back in `src/game/front.c`. Checks: 1280x960
captures of the folder showing four rows without the pack and three with it
installed, a scripted A on the row opening the page, and B returning to the
folder with the cursor on the row.

**Enhanced Fonts buttons stand out (2026-11-06).** The owner found Download
and Remove hard to spot among the description, and chose dimmed text with
boxed buttons. In the folder the six description lines draw in a lighter ink
(`PORT_SHEET_COL_NOTE`, black at 0xA0), the status line stays full ink, a blank
line follows, and the two actions draw in capitals as rounded buttons
(`portSheetDrawRoundBox`): a one-pixel ink outline 12 pixels left and right and
6 above and below the label (native 440x330 units), its left edge on the text
column with the label moved in by the padding, corner radius 6, filled
with the highlight band's ink in the same rounded shape on hover. A second
blank line separates the two buttons so their padded boxes do not touch. On the watch the description draws in a dimmer green
(`PORTWATCH_COL_NOTE`, through the new `portWatchRowSubdued`); its actions
already sit on button plates and its font is already capitals, and the face
has no free row for the gap. Source: `src/game/front.c` (the sheet builder,
`portSheetLabel`, the sheet's row drawing), `src/game/options.c` (the watch's
row drawing), `port/src/port_watchpages.c` and `port_watchpages.h`. Checks:
1280x960 folder captures of the page plain, with Download hovered (the help
box still names it), after a scripted A (Retry Download boxed) and installed
(Use Enhanced Fonts boxed), then of the rounded, padded buttons plain, hovered
and with A on Remove opening its dialog, and a 1600x900 watch capture with the dimmed
description, a full-strength status line and the three button plates.

**Busy-server backoff (2026-11-06).** GitHub publishes no limit for
`raw.githubusercontent.com`, and since 2025 players have reported 429 answers
there, mostly from shared addresses; 186 requests in a row could meet one.
When a file's answer is 429, 502, 503 or 504, `install` now waits and asks for
that file again instead of failing: for the Retry-After the server named, or
else 2, 4, 8 and 16 seconds, five tries in all. A fifth busy answer, or a
wait over a minute, fails with **Failed: GitHub busy** and **Retry Download**
(files already installed are kept). While it waits the status line reads
**GitHub busy: retry in Ns**, counting down, and **Cancel Download** stops
the wait at once. Transports return a `Failure` (reason plus `retry_after`)
instead of a string: WinHTTP reads Retry-After as text (its numeric query
refused the header in testing), libcurl through `CURLINFO_RETRY_AFTER` (7.66
and later; older ones read as no wait). `install` takes a `Progress` (files
done, seconds left waiting) and a `Wait`, which tests replace. Source:
`port/fast3d/enhanced_fonts_download.{h,cpp}`, `enhanced_fonts_http.cpp`.
Checks: `test_enhanced_fonts_download.cpp` covers the backoff sequence, a named
wait, giving up after five tries, refusing a two-minute wait, cancelling
during a wait and the real wait's countdown and cancel. Against a local server
answering 429: the Windows game showed **GitHub busy: retry in 5s** two
seconds into a 7-second Retry-After, finished all 186 files after two 429s,
and with every answer 429 stopped after five requests on **Failed: GitHub
busy**; a 64-bit Linux build through `libcurl-gnutls.so.4` installed 186
files in 6.3 s after one 429 naming 5 seconds and gave up after five
requests and 30 s when every answer was 429.

**FONT-4 (2026-11-06).** The browser downloads the Enhanced fonts too: the
Enhanced Fonts page under System, its watch page and the Options folder's
**Get Enhanced Fonts** row now appear in the browser as on Windows and Linux.
The browser has no threads, so the download became steps:
`enhanced_fonts_download::Installer` decides what to fetch, verifies and
writes each file and applies the busy-server backoff, and `install()` is now
the blocking loop over it that the native worker runs. In the browser
`portEnhancedFontsPump`, called every frame from `portEnhancedFontsFrame`,
answers its steps: each file is fetched with `fetch()` beside the game
(`EM_JS` helpers keep the answers in `Module.midasFontFetches` until the pump
takes them), busy waits are timed with `emscripten_get_now`, and Cancel aborts
the request in flight. raw.githubusercontent.com answers with
`Access-Control-Allow-Origin: *`; a fetch that fails while the browser is
online is retried like a busy answer, since a 429 without CORS headers reaches
the page only as a failed fetch. The files go to `/user/Data/FontPacks/gepd`,
inside the browser's saved storage: `web/src/font-pack-paths.ts` lets their
writes, renames and deletions schedule the storage sync
(`persistent-files.ts`) without adding them to save backups, which stay
limited to the player's own files. Everywhere, the folder is now set from the
game's root folder at start (`portEnhancedFontsSetDirectory(portRootPath(...))`
in `video.c`) instead of the launch folder, so a native game started from
another folder finds and installs its fonts beside `midas.ini`.

The page's first button, once the files are installed, reads **Use Original
Fonts** while Fonts is Enhanced and **Use Enhanced Fonts** otherwise, and
switches between them. A finished download already set Fonts to Enhanced; the
old label made it look as if a further press were needed (owner feedback).
Help: "Downloads the files named above from GitHub and uses them, or cancels,
or switches fonts."

Input: the page needs only taps on its buttons (browser touch sends cursor
motion and A in menus, as on every folder row) and the Remove dialog's
choices, answered by tapping a choice like the folder's other Yes/No dialogs;
it needs no native touch context. Not run: the touch audits, the full launcher
in a browser, and Remove in the browser game (the scripted D-pad presses did
not reach the Remove button in the probe page).

**FONT-4 checks:** the engine and download tests (unchanged, through the new
`Installer`) and the ADS test pass; the Windows game installed 186 files from a
local server after one 429 and showed **Use Original Fonts**. `bun test
src/font-pack-paths.test.ts` checks which paths persist without being backed
up. `bun run test:filesystem` (Edge, real IDBFS) gains a case: a font file
written as `.part` and renamed survives a reload, is absent from the backup
snapshot, and its deletion survives a reload; with the tracking change
removed, the same case fails. The browser game (`web_build.py video`, the
probe page with a scripted A press, Edge headless) downloaded from a local
server through one 429 with CORS (countdown, progress, then Installed with
**Use Original Fonts** and the Enhanced text), through two 429s sent without
CORS headers (retried, then installed), and from GitHub itself: each time 186
files and `SOURCE.txt` with no `.part` left and "186 of 186 files installed"
in the log, and no page errors.

**FONT-5 preparation (2026-11-06).** Release checks: the Windows package test
plants Enhanced font files (`Data/FontPacks/gepd`) in the build folder and
asserts none ships, and the Linux package checks (`test_sentry_linux.py`,
tar.gz, AppImage and Steam Deck ZIP) reject any `Data/FontPacks` path, as they
already did texture packs. The release notes written by `windows-release.yml`
and the packaged `Data/README.txt` describe the Fonts setting and the optional
download (source, size, CC0, Remove). `audit_hitmap.ps1` takes `-FontStyle`
(0 to 2) for debrief captures. The owner play-test checklist lives in the font
styles plan. Checks: `test_distribution.py` passed locally (two tests that
need GCC skipped; CI runs them); the Linux checks run in CI. Not run: Remove
inside the browser game and the full launcher (FONT-4). The owner then
play-tested the checklist and signed off the same day; FONT-5 is Done.

### FORK-051 — Iron Sights font: open-font MSDF text

**Status:** Implemented in the Windows and browser builds, but **hidden from
players since 2026-11-06**: the owner chose not to offer it for now because the
font still has deficiencies. The browser build was run at 1440p and 4K, and
the owner signed off on the play-test on 2026-11-06. Part of the open assets
plan (OPEN-4 and OPEN-5, Done; FORK-050). Since OPEN-5 (2026-11-06) the open
body font draws Zurich Bold text too.

**Hidden (2026-11-06).** Fonts offers only Original and Enhanced in the
Options folder and the watch (`port_options.c`). A saved `font_style=2`, or
the older `sharp_fonts=1`, loads as Original (`videoFontStyle` in `video.c`),
unless `GE_IRONSIGHTS_FONTS=1` allows it for development; the font, HUD,
sight and hit map capture scripts set it, so their Iron Sights switches keep
working (the Fonts row then shows the raw value `2`). The help line, ini
reference, README, release notes, notices and both plans say why. The
renderer, vendored fonts and tests are unchanged. Checks: the Windows build;
a 1600x900 System page with `font_style=2` reads Original, and with the
override draws the open fonts.

With **Gameplay > Iron Sights > System > Fonts** set to **Iron Sights**
(`[display] font_style=2`; Original by default) in the Options folder or the
watch, every Bank Gothic glyph (the HUD, watch, mission hints, timers, weapon strip,
titles) is drawn from the open title font and every Zurich Bold glyph
(briefings, folder menus, help lines) from the open body font
(pRizz/iron-sights-assets, MIT, vendored at v0.4.0), each as a multi-channel
signed distance field, sharp at any resolution. Fonts defaults to Original
(font styles plan decision 1). Open assets plan decision 4 foresaw Iron
Sights as the default once both fonts were play-tested; with both done, the
owner kept Original as the default (2026-11-06).

The game keeps its own text layout: advances, kerning, line spacing,
clipping, colours and outlines are unchanged. Each glyph's ROM texture is
replaced:

- **Embedding and decoding.** `port/fast3d/open_fonts_embed.cmake` turns the
  vendored `atlas.png` and `metrics.json` into a generated C file at
  configure time in plain CMake (re-run when they change). `open_fonts.cpp`
  decodes the PNG with vendored, unmodified puff 2.3 (zlib license, from
  zlib v1.3.1) plus PNG unfiltering (in `png_decode.cpp` since FONT-2,
  FORK-052), and parses the metrics with picojson.
- **Registry.** `textrelated.c`'s font loader registers each glyph's pixel
  pointer, width, height and baseline (a weak no-op covers builds without the
  renderer). Bank Gothic maps to the title font and Zurich Bold to the body
  font; each font's atlas loads from its own embedded files
  (`open_fonts_embed.cmake` embeds both), and each places from its own ROM
  `H`.
- **Placement.** Our capitals match the ROM `H` as it reads on screen: its
  ink edges are where its rows' peak intensity, interpolated between row
  centres, crosses a quarter of its peak (6.7 of its 8 rows). Our glyph sits
  on that ink's foot, with its ink centred in the ROM glyph's width; a glyph
  that would still be wider than that width plus 0.4 texel is squeezed
  horizontally to fit, so letters never overlap. The first build scaled to all
  8 rows; in play the letters came out too tall, too wide and overlapping, and
  the owner asked for them shorter and less wide (2026-11-05). Captures at
  720p measured the mission-hint line at 9 px for the ROM, 11 px for that
  build, 8 px when scaled to the rows' summed ink, and 9 px with the
  quarter-peak edges. Those hints are drawn in small capitals, so the 9 px
  match came partly from our small capitals being too short (76% of our
  capital against the ROM's 84%); title font v0.3.0, vendored at the owner's
  request after play-testing the draft, raises them to 85%, and
  the hint line's ink-mass height is then 9.3 and 8.2 px against the ROM's
  9.3 and 9.0 (v0.2.0: 8.6 and 7.6). Rows are then counted 2 px taller than
  the ROM's, because sharp edges cross the count's half-intensity threshold
  where the ROM's blurred ones do not.
- **Textures.** Importing a registered glyph uploads an RGBA field image
  resampled from the atlas over the union of the ROM glyph box and our padded
  glyph box, at the atlas's own density. A `msdf_glyph` cache-key flag keeps
  it apart from the ROM bitmap, and turning the setting on or off rebinds.
- **Quads.** `gfx_dp_texture_rectangle` grows the glyph's quad to that box
  (sides always; top and bottom only where a clipped list did not cut the
  glyph) with sub-pixel corners, maps its UVs onto the box, skips the
  half-texel glyph border, and draws without the tile UV clamp. Rotated text
  and copy-mode rects keep the ROM path.
- **Shader.** `SHADER_OPT_MSDF` (bit 31) samples the field directly (no
  3-point filter), takes the median of RGB and turns it into coverage over one
  screen pixel with `fwidth`; the combiner uses that as the glyph's intensity.

With Fonts on Original, rendering is unchanged. The setting began as a
Sharp Fonts toggle (`[display] sharp_fonts`); FONT-1 of the font styles plan
(FORK-052) replaced it with Fonts, and `sharp_fonts=1` still loads as Iron
Sights.

The C++ registry builds as its own CMake object library (`ge_open_fonts`)
with none of the game's include directories. Built with them, the first
pushed version failed the Linux CI build: under GCC 11, `<algorithm>` reaches
`<pthread.h>`, whose `<sched.h>` resolved to the game's `src/sched.h`, and the
game's declarations then clashed with the C library's. Windows (GCC 13 MinGW)
and the browser were unaffected. The CI Linux image reproduced the errors on
the old include path, and its `linux_build.py` build compiles and links with
the object library.

**Upstream hooks:** a fork block in `textrelated.c`'s font loader; in
`gfx_pc.cpp` the I8 import, cache key, 2D sampler choice, UV-clamp decision,
shader option, texture-rectangle path and frame-start rebind; the shader
generator in `gfx_opengl.cpp`; `gfx_cc.*` and `gfx_pc.h`.

**Sources:** [open fonts](port/fast3d/open_fonts.cpp) and [header](port/fast3d/open_fonts.h),
[embedding](port/fast3d/open_fonts_embed.cmake), [puff](port/fast3d/third_party/puff/README.md),
[renderer](port/fast3d/gfx_pc.cpp), [shader](port/fast3d/gfx_opengl.cpp),
[shader options](port/fast3d/gfx_cc.h), [cache key](port/fast3d/gfx_pc.h),
[font registry hook](src/game/textrelated.c), [weak fallback](port/src/libultra_shim.c),
[setting](port/src/video.c), [options](port/src/port_options.c),
[frontend row](src/game/front.c), [watch row](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [ini reference](tools/midas_ini_help.txt),
[build](CMakeLists.txt), [notices](tools/THIRD_PARTY_NOTICES.txt),
[tests](scripts/port/test_open_fonts.sh), [fit check](scripts/port/check_open_fonts.py), [README](README.md), and
`-SharpFonts` switches (now `font_style=2`) on the [HUD](scripts/port/audit_hud.ps1),
[sight/watch](scripts/port/audit_sights.ps1) and [text](scripts/port/audit_fonts.ps1) audits.
**Checks:** the Windows build compiles and links; the browser video build
(`web_build.py video`) compiles and links and its wasm audit passes, but was
not run. `test_open_fonts.sh` (added to the release workflow) decodes the
vendored atlas, registers a ROM-like font and checks a glyph's box, field
inside its stem, outside its corner and on the ROM baseline. `test_hud.sh`,
`test_hitmap.sh`, the open-asset verification, settings placement and the
standards check pass; the regenerated gallery has zero changed image cells.
With the setting off (now Original), 1280x960 video-menu captures are
pixel-identical to the build before this change, with point-sampled and
smoothed glyphs. With it on,
1280x720 HUD captures show the outlined timer and ammo digits, the mission
hints and the weapon strip crisp where the ROM font is blurry, at the ROM's
capital height with no overlapping letters, and a 1600x900 watch page shows
every Bank Gothic row replaced with capitals and small capitals in their ROM
places. The 1440p, 4K, debrief and browser captures followed (below); a
glyph cut by a clipped scrolling list is still not captured.

**Body font (OPEN-5, 2026-11-06).** `open_fonts.cpp` keeps one atlas per
ROM font and registers Zurich Bold as well as Bank Gothic; the placement is
unchanged. `test_open_fonts.cpp` registers a ROM-like Zurich Bold and checks a
body glyph's field image. `update_open_assets.py` vendors both fonts from
v0.4.0 and `test_open_assets.py` checks each. The new local
`scripts/port/check_open_fonts.py` fits an open font into the game font's
glyph widths as the renderer does and lists any glyph squeezed below a
threshold (it reads the extracted font sources and prints numbers only);
`compare_open_font.py` now also reads Zurich Bold, whose kerning table is
declared with an explicit size. The first in-game draft showed specks and
nicks: the assets' MSDF generator now stores the true distance more than
1.5 px from an edge (a false near-edge median in, for example, the foot of
the n's counter showed as a dot in small text, where the shader spreads
coverage over few pixels), the body atlas uses 96 px/em, curves are
simplified more finely, and several glyphs were redrawn. Checks:
`test_open_fonts.sh` (Docker), `test_open_assets.py`, the Windows build, the
standards and settings placement checks, the regenerated gallery (zero
changed image cells; it renders the Original fonts), the fit check (no glyph
squeezed) and 1600x900 captures of the Video, Iron
Sights, Files and mission pages in both styles, with no stray specks found by
a scan of the Video page. The owner reviewed the result in play and approved
the release. Not run: the browser build with the body font, and native
captures above 1600x900.

**Captures (2026-11-06).** The browser build drew the Iron Sights font at run
time at a true 2560x1440 and 3840x2160 (headless browser canvas). Native
Windows captures on this machine stop at 1600x900, since SDL shrinks larger
windows to fit the 1920x1080 desktop; at that size the HUD, Credits,
Difficulty, the scrolled watch Video page, the watch inventory list and the
debrief render cleanly in both fonts. A glyph actually cut by a clipped list
(a watch inventory item's scrolling description) was not captured; it was
part of the owner's play-test, which passed.

### FORK-050 — Open assets and sharp text plan

**Status:** Plan, repository rule and vendored assets; the owner accepted the plan
with its recommended decisions on 2026-11-05. The assets repository
[`pRizz/iron-sights-assets`](https://github.com/pRizz/iron-sights-assets) was
created the same day with its license, rules, roadmap, CI and Bright Builds
Rules (OPEN-1), and
released the parametric title font as v0.1.0 after the owner's review (OPEN-2),
then v0.2.0 with comparison-informed refinements and v0.3.0 with taller small
capitals after the owner's Sharp Fonts play-test (OPEN-4). v0.4.0
(2026-11-06, OPEN-5) adds the body font, drawn in code as variable-width
strokes and approved by the owner after play-tests, and a generator fix that
also rebuilds the title font's atlas. Iron Sights first vendored v0.2.0
(OPEN-3), then v0.3.0, and now vendors v0.4.0:
`scripts/port/update_open_assets.py` fetches a tag, checks every download
against the release's SHA256SUMS and copies the LICENSE and each font's
`atlas.png` and `metrics.json` (title and body) byte for byte into
`port/fast3d/open_assets/` with a `manifest.json` of checksums (`--verify`
rechecks offline; `.gitattributes` keeps the folder byte-exact).
`test_open_assets.py` runs in the release workflow, `build-inputs.json` records
the release, Windows and Linux packages ship `Open-Assets-LICENSE.txt`, the
browser site lists it, and THIRD_PARTY_NOTICES describes it. The renderer that
draws it is FORK-051 (OPEN-4), the Fonts setting's Iron Sights choice
(Original by default).

[open_assets_plan.md](scripts/port/open_assets_plan.md) plans original,
MIT-licensed fonts to replace the ROM's glyphs, and a renderer that draws them
sharply at any resolution. It follows the removal of every bundled third-party
art set (FORK-021, FORK-033, FORK-047). The plan covers provenance rules
(drawn from scratch, never traced from the ROM or any pack or copied from their
outlines, coordinates, pixels or metrics, no ROM-derived data in the assets;
local comparison with the game's glyphs allowed since 2026-11-05), a hybrid layout (a separate MIT assets repository as the source
of truth, its built outputs vendored here at a pinned, checksummed version), a
parametric title font and a body font (planned as hand-drawn; OPEN-5 drew it
in code as variable-width strokes), and a renderer that keeps the
game's glyph pipeline but swaps ROM bitmaps for MSDF atlas cells through a glyph
registry and a new `SHADER_OPT_MSDF` shader bit, behind a Sharp Fonts setting
with the ROM fonts as fallback. It also records the owner's intent to grow the
repository into other original game assets later, and to let other projects
use them, including a deferred exporter that builds an emulator texture pack
from the player's own ROM. Milestones OPEN-1 to OPEN-6 are listed with a
status table.

Accepted decisions: the assets repository is `pRizz/iron-sights-assets`
(public, MIT), Arimo (Apache-2.0) is a
development-only body-font placeholder that never ships, the renderer uses MSDF,
Sharp Fonts (now the Fonts setting's Iron Sights choice) defaults to off
until both fonts exist and the owner has play-tested them (on 2026-11-06,
with both done, the owner kept Original as the default instead), and the emulator exporter (OPEN-6) is deferred. `AGENTS.md`
now sends work on fonts, text rendering and original assets to the plan and its
provenance rules, as it does for the web target.

`scripts/port/compare_open_font.py` (local review only) reads the game's glyph
bitmaps from the decompilation's `assets/font/font*.c` (kerning table, 94
six-field character entries with signed baselines, 8-bit glyph rows padded to
multiples of 8 bytes with one extra row) and an open TrueType font, scales the
open glyphs to the game's cap height and baseline, and writes a sheet of four
panels per character (game, ours, overlay with overlap in red, difference in
red) with each glyph's overlap share. It writes only to the ignored
`build_win/font-compare/`. A few game character slots hold special symbols
rather than their ASCII characters (`@` and `\` in Bank Gothic), so those rows
do not compare like for like. The first sheet, Bank Gothic against title font
v0.1.0, gave a mean overlap of 34%; at 8-pixel capitals a one-pixel offset
moves the share a lot, so it is a rough guide.

**Sources:** [plan](scripts/port/open_assets_plan.md), [repository rules](AGENTS.md),
[comparison tool](scripts/port/compare_open_font.py),
[vendored assets](port/fast3d/open_assets/manifest.json),
[update script](scripts/port/update_open_assets.py), [tests](scripts/port/test_open_assets.py),
[release inputs](scripts/port/release_inputs.py), [Windows](scripts/port/package_windows.py)
and [Linux](scripts/port/package_linux.py) packaging, [browser notices](web/tools/build-site.ts),
[notices](tools/THIRD_PARTY_NOTICES.txt), [release workflow](.github/workflows/windows-release.yml).
**Checks (OPEN-3):** the vendored files verify against their manifest; the open
asset tests (4), the distribution test (now requiring the license) and the web
package's 209 tests pass, and the standards check reports no findings.
**Checks:** documentation only; no build or render regeneration needed.

### FORK-049 — Sub-pixel scaled text

**Status:** Implemented in the Windows build; the owner play-tested it and
approved it (2026-11-05). The browser target was not built or tested with it.

Upstream's `portTextScale` (inherited, `textrelated.c`) draws text smaller by
shrinking each glyph's rect. It kept the pen on whole native pixels: each
glyph's advance was rounded up and each gap rounded to the nearest pixel, and
glyph sizes were truncated to quarter pixels. One native pixel is 2.2 screen
pixels at 720p and 4.4 at 1440p, so scaled text such as the hit map legend
(55%) showed uneven letter spacing ("Ki ll sh ot") and uneven stroke weight at
every resolution. The fork found this while reviewing FORK-048 captures.

The pen now keeps its fractional position (`port_text_frac`) across a text
run: gaps, advances and spaces move it by their exact scaled widths, and only
the whole part goes into `*x`, so the game's clipping and layout still see
integers. Each scaled glyph sends its exact corners through upstream's
precise-rect tag (`portRectPreciseTag`, `G_NOOP` tag `0x59`, 1/256 native pixel)
before its texture rectangle, and its size is no longer truncated. The texel
step is rounded rather than truncated. In `gfx_dp_texture_rectangle`, a precise
glyph rect also extends its texture span by the corners' fractions, so a glyph
drawn at a fractional width is not stretched; other precise rects (the aim
sight) keep their old UVs. `textMeasure` steps the same exact moves, so measured
widths match what is drawn. Scaled words come out slightly narrower than before,
since advances are no longer rounded up. Layout that measures them, such as the
hit map's counts column, follows.

At scale 1 every move is a whole pixel and the fraction stays 0, so the game's
own full-size text is unchanged. This is the first step of the planned text
renderer work (see FORK-021): it fixes layout precision with the ROM's glyphs;
original MIT fonts drawn at any resolution would follow.

**Upstream hooks:** fork blocks inside upstream-owned `textrelated.c`
(`portTextScale` helpers, `textRenderGlyph`'s scaled branch, `textRender`'s
reset per call and line, `textMeasure`), and one block in `gfx_pc.cpp`'s texture
rectangle path. The N64 build is untouched.

**Sources:** [text](src/game/textrelated.c), [renderer](port/fast3d/gfx_pc.cpp),
[precise tag](src/game/bondwalk2.c).
**Checks:** the Windows build compiles and links. Video-menu captures at
1280x960 are pixel-identical before and after with point-sampled and smoothed
glyphs (`audit_fonts.ps1 -Mode Video`), so full-size text is unchanged. The hit
map closer look's 55% legend and the debrief's 80% counts at 960x720 show even
spacing and stroke weight; the debrief layout still fits. A 1280x720 HUD capture
(`audit_hud.ps1 -MissionTimer -Dialogue`) shows the scaled mission hints and
weapon strip names intact. `test_hud.sh` and `test_hitmap.sh` pass, and the
regenerated gallery has zero changed image cells.

### FORK-048 — Debrief hit map

**Status:** Implemented and captured in the Windows build (scripted hits on
Archives and Frigate). The owner play-tested the Windows build and approved it
(2026-11-05); the browser target was not built or tested with it. The closer
look's PARTS tab (2026-11-08) is built and captured in the Windows build with
scripted hits on Dam; it has not been play-tested yet.

Adds default-on **Gameplay > Iron Sights > Combat Feedback > Debrief Hit Map**
(`[display] debrief_hit_map=1`) to the Options folder and the watch. Upstream's
solo debrief (`constructor_menu0D_missioncomplete`) only counts head, body,
limb and other hits. The fork now also records where each of the player's
bullets struck a character and draws front and back body figures with a dot per
hit. The owner chose a 2D front/back view, dots coloured by outcome, the figures
on the statistics page plus a closer look, solo only, and a tally for hits with
no position (2026-11-05).

**Recording.** `chrHandleBulletHit` (`chr.c`) reports each player bullet before
its damage. The bullet's view-space ray is moved into the frame of the hit
box's joint (the node matrix rows, divided by their squared length, as
`modelTestRayIntersectsTransformedBBox` does), and a slab test finds where it
entered the box. The hit stores the box and the entry point as fractions across
the box's local x and y. A box keeps that frame in every animation pose, so the
point lands on the same spot of the reference figure. A hit is a back hit when
it entered the box's rear half (local -z); characters face +z, as the feet and
head boxes show. Hits on gun and hat boxes (upstream's "Others") are not
placed. The fork's damage hook in `chraction.c`, which also raises hit markers,
marks a hit as the kill shot when its damage takes the character from below to
at least `maxdamage`. The same hook stores each hit's damage, in thousandths
of the victim's full health (`maxdamage`): all of it, and for a kill shot the
part past the health left (`portHitMapDamageUnits`; each clamps at 65535). A
hit that does no damage, such as one on a character already dying, records
0. Only solo play records (`getPlayerCount() == 1`). Up to
1024 hits are kept per mission; `init_player_BONDdata_stats`
(`initBondDATA.c`) clears them where upstream clears the shot counters.

**Figure.** The first character hit in a mission supplies the figure: its body
model's hit boxes, at joint positions summed from the group origins. GE's
unanimated skeleton is a starfish, with every limb bone running out sideways
along its local x. `portHitMapPose` therefore swings each leg chain straight
down about its thigh joint and each arm chain 65 degrees below horizontal about
its shoulder; the torso, pelvis and head keep their place. Other characters'
hits map onto the figure by part and box order.

Each part is drawn as the oval inscribed in its box: a heat-tinted wireframe in
dossier ink. An opaque oval in the paper's average colour (sampled 202,194,156),
grown by the stroke width, backs each part first, so the red CLASSIFIED stamp
does not show through the translucent shading. Parts are filled crimson
(`#c81e23`) at 6% opacity plus up to 79% more by their share of the busiest
part's hits (front and back share one scale), then outlined in ink. The owner
chose this deeper, stronger ramp for contrast over the first version's
`#e24b4a` at up to 61% (2026-11-05). Hits are ink dots on a paper-coloured
rim; head hits are gold, ringed with one pixel of ink so they read on the
paper (owner's request, 2026-11-05). A kill shot's rim is red, edged with a one-pixel paper
ring (the overlay's thinnest stroke) so it stands apart from crimson parts,
and kill shots are drawn after the other dots so none cover them. A hit's fractions across its box are taken onto
the oval with the elliptical grid mapping, so a side hit lands on the rim and a
corner hit just inside it, never off the part. Dot size and stroke width grow
with the figure. Hits in a box's back half go on the back figure, which is
mirrored so the character's left is on the screen's left, as seen from behind.
The owner chose this style from mock-ups (2026-11-05). It replaces the first
version's solid slate rounded boxes with white dots.

**Debrief.** With the map shown, the debrief rearranges its hit counts:
`portHitMapDebriefCounts` prints Shot total and Kill total with Head hits,
Body hits, Limb hits and Others stacked under them, at 80% size. The values sit
just past the widest label, so all six rows fit above the page's foot. Upstream
prints the last four in a column beside the totals, and still does when the map
is off. The overview sits in the lower-right corner on the page's foot, with a
**HIT MAP** caption under the two figures, as close to the text as its shape
allows. `front.c` reports the right edges of the rows near it: the Time value,
New Cheat notice, Target and Best Time values, Accuracy value and weapon of
choice. The page is split into bands: the statistics rows, the weapon row and
the counts column. Each part's oval, and the caption, must keep clear of the
text in any band it overlaps. Placement prefers figures right beside the
counts column, as tall as fit there; on a typical debrief that keeps the
figures' shoulders under the weapon row, so they stand about as tall as the
counts. If nothing fits there, the tallest figures that fit anywhere are used,
as far left as they can go. The owner asked for the counts under the totals, the
map snug in the lower-right quarter and less space between it and the counts
(2026-11-05). This replaces three earlier layouts. The first kept the stock
counts and fitted the figures right of the statistics above them. The second
also hid the folder's crest (showing the `SW_BLANK` sheet instead of
`SW_PAPER`) and raised the figures beside the stage name; the crest is back now
that the map has left its corner. The third placed the tallest figures that fit
flush against the tabs, which left a wide gap after the counts. The whole bottom
area opens a closer look in the statistics' place: the counts strip across the
page, plus the map's column where it rises above that strip. The closer look
hides the folder's CLASSIFIED stamp (`SW_CLASSIFIED`), which ran across the
figures. It shows larger **FRONT** and **BACK** figures, a legend (hit, head,
kill shot and the hits-per-part shading), a tally and **CLOSE**. Each dot
entry gives its count, as **Hit: N**, **Head: N** and **Kill shot: N**: Hit
and Head count the ink and gold dots, which add up to Hits shown, and kill
shots are counted within them (owner's request, 2026-11-05). The tally
reads **Hits shown: N**, the dots on the figures. Only when the debrief counts
more head, body and limb hits than that, a second line reads **Not recorded:
M**, the difference. Fists, the knife and thrown knives never reach those
counts (their weapons lack `WEAPONSTATBITFLAG_PLAYER_STAT_HIT`, so
`inc_curplayer_hitcount_with_weapon` skips them; checked at runtime), so the
difference is bullet hits the map could not record: past its 1024 cap, or a hit
box it could not find. The owner asked for wording a first-time viewer
understands (2026-11-05). It replaces **Placed** and **Not placed**, whose
description wrongly blamed melee and thrown knives. A Melee/thrown line was
tried and dropped once those weapons proved never to count. A mission whose
hits were all unrecorded, or with the setting off, shows the stock page. Explosions never count as hits upstream, so they appear nowhere.

**Parts view.** The closer look has **FIGURES** and **PARTS** tabs between its
title and CLOSE; it opens on FIGURES, the shown tab is underlined, and the
debrief's init returns it to FIGURES. PARTS replaces the figures, legend and
tally with a table of Head, Chest, Pelvis, Upper arms, Forearms, Hands,
Thighs, Shins and Feet: left and right sides are counted together, and GE's
"shoulder" box is the upper arm, "arm" the forearm and "leg" the shin. Each
row gives hits, kill shots, and bars with percentages for its share of the hits
and of the damage, both bars on one scale (full at the largest share on the
page). Beside the table: Hits shown, Not recorded (only when non-zero, as on
FIGURES), Kill shots, Hits per kill, Damage dealt, Overkill, Gun hits and Hat
hits, with notes that 100 damage is one full health bar and that gun and hat
hits do no damage. Damage dealt is the recorded damage in hundredths of a full
health; Overkill is the share of it kill shots spent past the health left. The
rows count only the map's recorded hits, so they add up to Hits shown. Gun and
hat hits are never placed on the figures, so they come from the debrief's own
counters (`SHOT_REGISTER_GUN`/`HAT`), which upstream adds together as Others.
GE has no neck or abdomen hit box, so the table has neither; the owner
preferred the game's real parts to splits invented inside a box. The owner
chose the separate PARTS view, combined sides, the gun and hat rows and the
damage shares from mock-ups (2026-11-08) over a table squeezed beside the
figures.

**Input.** Every input points at the bottom area with the menu cursor and
confirms there, as it does for the tabs: mouse click, gamepad A/Start/Z, the
keyboard's bound confirm, or a touch tap. While the closer look is open, a
confirm anywhere off the tabs, B or a right-click (the frontend's mouse B)
closes it, so a tap off the tabs closes it on touch. The NEXT and PREVIOUS tabs
keep working on both views. A confirm on FIGURES or PARTS switches the closer
look's view instead of closing it, with the same pointing as CLOSE (mouse
click, gamepad A/Start/Z, keyboard confirm, touch tap). The bottom area, CLOSE
and the view tabs use the tabs' hover highlight; the bottom area's is L-shaped,
covering the strip and the map's column. No input is left unsupported.

**Rendering.** The HUD vector overlay (`gfx_ge_vector_overlay`) gains shape 7,
an ellipse turned by an angle, filled or outlined at a given width, with
feathered edges; `portHudEllipse` exposes it. The screen angle comes from the
stored pose angle, because the game's `atan2f` is built on a 16-bit `acosf`
table and visibly skewed near-vertical limbs. The first version's shape 7 was a
filled rounded box (`portHudBox`); the ellipse replaced it.

**Upstream hooks:** one call each in `chrHandleBulletHit` (`chr.c`),
`init_player_BONDdata_stats` (`initBondDATA.c`), the existing fork block of
`handles_shot_actors` (`chraction.c`), and the debrief's init, interface and
constructor (`front.c`), all under `#ifndef TARGET_N64`. The constructor also
reports seven value rows through `PORT_HITMAP_TEXT`, which is empty on N64. Its
stock hit-count block is wrapped in braces as the `else` of a
`portHitMapDebriefDrawn()` branch that calls `portHitMapDebriefCounts`. The
interface shows `SW_CLASSIFIED` only while the closer look is closed. The
closer look returns before the statistics.

**Audit:** `scripts/port/audit_hitmap.ps1` runs an isolated copy on a solo stage
with invulnerability.
`GE_HITMAP_PROBE=<hits>[,zoom][,parts][,live][,best=<s>][,cheat][,cap=<n>]` fires bullets
through the game's own `chrTestHit` and `chrHandleBulletHit` at random points
inside on-screen characters' body boxes, at the point where Bond's own shots
resolve (after `chraiCheckUseHeldItems`, `lv.c`). It then aborts as the watch
does, moves from the status page to the debrief (`front.c` harness), opens the
closer look if asked (`-Parts` on its PARTS tab), captures a frame and exits. `-Cursor x,y` parks the menu
cursor; `-Off` turns the setting off. `-Difficulty`, `-Best` (a RAM-only best
time) and `-Cheat` (the New Cheat notice) bring up the debrief's longest rows.
`-Cap n` records at most n hits (`portHitMapLimitSet`, harness only), so the
rest show as Not recorded without needing 1024 targets. `GE_HITMAP_LOG=1`
prints the figure boxes,
each hit, its damage and kill, and the game's own front test (the melee bearing from
`get_distance_actor_to_position`). Captures stay in the ignored
`build_win/hitmap-audit-*` directories.

**Sources:** [header](port/include/port_hitmap.h), [core](port/src/port_hitmap.c),
[recorder](port/src/port_hitmap_game.c), [debrief](port/src/port_hitmap_draw.c),
[Parts view](port/src/port_hitmap_parts.c), [drawing header](port/src/port_hitmap_draw.h),
[probe](port/src/port_hitmap_probe.c), hooks in [chr.c](src/game/chr.c),
[chraction.c](src/game/chraction.c), [initBondDATA.c](src/game/initBondDATA.c),
[lv.c](src/game/lv.c) and [front.c](src/game/front.c); the ellipse shape in
[gfx_pc.cpp](port/fast3d/gfx_pc.cpp), [gbiex_compat.h](port/fast3d/gbiex_compat.h)
and [port_hud.c](port/src/port_hud.c); option bridge in
[joy_port.c](port/src/joy_port.c), rows in [port_options.c](port/src/port_options.c),
[front.c](src/game/front.c) and [port_watchpages.c](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [default ini](port/src/video.c),
[settings reference](tools/midas_ini_help.txt), [env list](port/src/port_console_env.h)
and [audit](scripts/port/audit_hitmap.ps1).

**Checks:** [hit map tests](scripts/port/test_hitmap.sh) cover the ray's move
into a scaled, turned joint frame, the entry surface (including a ray starting
inside, and a miss), box fractions and the front/back half, part and ordinal
lookup, the leg and arm pose, fitting and facing in front and back views, hits
following the posed box, and corner and side hits landing on the oval. They
also cover the Parts view's core: left and right parts sharing a row, damage
and overkill in health units (an ordinary hit, a kill shot, damage past the
health, no damage, no health and the clamp), per-row and total counts that
skip non-body boxes, and a placed hit starting without damage. The
settings placement check, HUD and hit sound tests and the standards checks pass.
`test_ads.c` includes `joy_port.c`, which now calls the hit map's option hooks,
so it stubs `portHitMapOptionGet`/`Set` beside its video and input stubs; without
them `test_ads.sh` failed to link, which failed CI's ROM-free test step on the
hit map commit. `test_ads.sh` passes with the stubs.
The regenerated hand gallery has no changed image cells (only its source
fingerprint) and verifies. Audit captures at 960x720 show the rearranged overview (also hovered over the
counts), the closer look without the stamp, the cheat-difficulty page with the
New Cheat notice, the Best Time page and the setting off, with the crest back
on each. In each run, kill rings matched the debrief's Kill total and hits shown
matched head, body and limb hits. A closer look with `-Cap 100` after 120 shots
read Hits shown: 100 and Not recorded: 18 (two shots struck a gun or hat). On live targets (`-Live`), all ten hits the
game's bearing called clearly front or back (eight front, two back, on pelvis
and limbs) got the same side.

### FORK-047 — HD HUD ammo icons (removed)

**Status:** Removed on 2026-11-05, the day it was added. No target carries the
art or its setting. Since 2026-11-06 FORK-053 draws the same `UI/Ammo` art from
the pack the player downloads or copies in, reusing this entry's slot tag, and
fixes the right-edge smear described below.

Commit `5a3b7c0` embedded intermissionfb's HD ammo art from the GoldenEye 007 HD
release (v2025.12.30, `UI/Ammo`) in every target. It replaced the HUD's
ammo-type icons beside the ammo counters, switched by a default-on **Gameplay >
Iron Sights > System > HD HUD Icons** setting (`[display] hd_hud_icons`),
requested in issue #6. The source repository has no license. Asked to add one
([GhostlyDark/GoldenEye-007-HD#3](https://github.com/GhostlyDark/GoldenEye-007-HD/issues/3)),
its maintainer replied on 2026-11-05 that integrating the textures directly in a
port like Iron Sights is explicitly disallowed: texture packs should stay
external mods added by the user and kept updated by their maintainers. The
owner then removed all of the pack's art, along with FORK-033.

The removal reverts that commit: the embedded art, decoder/canvas, generator,
provenance and notice files (`port/fast3d/hud_icons/`, `hd_ammo_icons.*`,
`generate_hd_ammo_icons.py`), the `0x5B` slot tag in `gunfire.c`
(`port_hud_ammo.c`), the renderer cache key and smooth-sampling exception in
`gfx_pc.cpp`, the setting, rows, preset and help, the About credit, the
Windows/Linux `HD-Ammo-Icons-NOTICE.txt`, the browser site's notice entry, the
`build-inputs.json` record, `audit_hud.ps1 -OriginalAmmoIcons` and the icon
tests. The HUD draws the original ammo icons again on every target. An existing
`hd_hud_icons=` line in midas.ini is ignored as an unknown key (logged once at
startup). Releases published before the removal still contain the art; they
were left in place for now.

A right-edge smear reported on issue #6 came from this feature: the in-shader
clamp works in native texels, so on the 8x art it skipped the last half native
texel of icons with padded TMEM rows (5- and 7-texel widths). It went away with
the feature and needed no separate fix.

**Sources:** the revert of `5a3b7c0` across [renderer](port/fast3d/gfx_pc.cpp),
[draw hook](src/game/gunfire.c), [settings bridge](port/src/video.c),
[option row](port/src/port_options.c), [frontend row](src/game/front.c),
[watch rows and preset](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [About](port/fast3d/about.cpp),
[Windows](scripts/port/package_windows.py)/[Linux](scripts/port/package_linux.py)/[browser](web/tools/build-site.ts)
packaging, [release inputs](scripts/port/release_inputs.py), [CI](.github/workflows/windows-release.yml),
[HUD tests](scripts/port/test_hud_weapons.c) and [HUD audit](scripts/port/audit_hud.ps1).
**Checks:** The Windows build (Docker `goldeneye`, `build_win`) compiles and links
without the art. `test_hud.sh` passes without the slot-tag cases; wardrobe, state,
buffer and ADS regressions pass (ADS after the separate FORK-048 test-link fix),
as do the distribution, settings placement and its tests, CI dependency, Steam
art, enhanced font and gallery tests, the web package's `bun test` and the
standards check. The regenerated gallery verifies 72 views and 38 icons with zero
changed image cells. The original icons were not captured in play, and the
browser build was left to CI.

### FORK-046 — Steam library artwork for the Steam Deck ZIP

**Status:** Implemented; not yet tried in a real Steam client or on a Steam Deck.

A Non-Steam Game shortcut to the Steam Deck ZIP's `run.sh` previously appeared in
Steam as a plain text tile named `run.sh`, with no icon. Steam reads non-Steam
library artwork from image files in `userdata/<user>/config/grid/`, named after the
shortcut's 32-bit app ID, rather than from the executable, so nothing could be
compiled in. The ZIP now ships original artwork in `steam-art/`: a 600x900 cover,
a 920x430 banner, a 1920x620 hero background and a transparent title logo. It
also ships `set-steam-art.py`, a standard-library Python helper the player runs
in Desktop Mode. The helper makes `run.sh` executable and installs an "Iron
Sights" application-menu entry for that folder, with the bundled `iron-sights.png`
icon. Steam's "Add a Non-Steam Game" list then offers the game by name and icon.
On a second run, it parses each Steam user's binary `shortcuts.vdf`, matches
shortcuts whose executable or start folder is this folder, and copies the
artwork as `<id>p.png`, `<id>.png`, `<id>_hero.png` and `<id>_logo.png`. It
never writes `shortcuts.vdf`, and it keeps existing grid images for the shortcut
unless `--force` is given. Shortcuts without a stored `appid` use Steam's legacy
CRC32-of-exe-plus-name ID. The other Linux packages and the Windows ZIP are
unchanged.

The artwork is generated by `make_brand_art.py` (named `make_steam_art.py` until
FORK-059), using Pillow, and the PNGs are committed, so release packaging needs
no Pillow.
It draws a three-dot iron-sight picture from polygons and sets the title in
Black Ops One (SIL Open Font License 1.1), downloaded from a pinned Google Fonts
commit with a SHA-256 check and cached under `build/steam-art/`. Rendered text is
not covered by the font licence; no font file is committed or shipped. The
artwork uses no game assets or GoldenEye/007 marks. Since 2026-11-08, the same
generator also renders the browser launcher's text-free sight icons into
`web/icons/`; `--icons-only` skips the Steam art and its font download. FORK-024
describes those icons. Since FORK-059, `port/include/icon.png`, which ships as the
Steam Deck menu icon `iron-sights.png`, is the generated sight icon, no longer
the inherited Project Midas dish.

**Sources:** [artwork generator](scripts/port/make_brand_art.py),
[artwork](scripts/port/linux/steam-art/), [web icons](web/icons/),
[helper](scripts/port/linux/set-steam-art.py),
[packager](scripts/port/package_linux.py),
[Steam Deck instructions](scripts/port/linux/STEAM-DECK.txt),
[player instructions](scripts/port/linux/START-HERE.txt),
release notes in [the workflow](.github/workflows/windows-release.yml) and
[README](README.md).
**Checks:** [test_steam_art.py](scripts/port/test_steam_art.py) runs in CI. It
checks the image sizes, VDF parsing, shortcut matching, app-ID conversion, grid
names, preservation of existing artwork, menu-entry quoting, and an end-to-end
run against a fake Steam folder that confirms `shortcuts.vdf` is unchanged. It
passes on Windows and under WSL Linux. A packaging check built a Steam Deck ZIP
from a placeholder AppDir and listed the new files and their checksums. The
packaged smoke test ([test_sentry_linux.py](scripts/port/test_sentry_linux.py))
now also verifies their checksums. The workflow edit refreshed the hand
gallery's source fingerprint. Both render passes verified all 72 views, and no
image cells changed. Rerunning the generator reproduces byte-identical PNGs.
Steam's file names come from community
documentation, not a Valve specification. Behaviour in a real Steam client,
including whether its menu-entry import keeps `run.sh` as the executable,
remains unverified.

### FORK-045 — ADS Spread and ADS Recoil

**Status:** Implemented; the user's Windows play-test confirmed the 50% defaults.

Upstream fires each bullet at the crosshair plus a random offset scaled by the
weapon's `Inaccuracy` stat (`bullet_path_from_screen_center` in `gunfire.c`;
weapons flagged for first-shot accuracy quarter it on a volley's first shot).
That spread is divided by the field of view, so it is the same angle at any
zoom: ADS zoom made it look larger on screen without changing it. Upstream's
`RecoilBack`/`RecoilUp` only kick the gun model (the hand's recoil matrix); the
shot ray never uses them, so in ADS the sights jumped off a target the shots
still hit.

**Gameplay > Iron Sights > Aiming and Movement** now has **ADS Spread** and
**ADS Recoil**, each the percentage of the native value kept while aiming down
sights, 0 to 100 in steps of 10, both defaulting to 50 (`[display] ads_spread`
and `ads_recoil`; 100 is the original). The multiplier eases with the ADS blend,
so it reaches the setting as the sights settle and returns to native at the
hip. ADS Spread multiplies after the first-shot bonus, so a first AK47 shot in
ADS keeps 12.5% of its normal spread at the default. Both shotguns keep their
full pellet spread and kick, and demo playback keeps the native values so
recorded shots replay as recorded. The settings apply to every player in
multiplayer alike, through each player's own ADS state. The scoped sniper's
spread is already 0. Since FORK-061, ADS Recoil also scales Weapon Kick's
per-shot spring, and while Weapon Kick is Enhanced the native kick it scales is
zeroed for every weapon Weapon Kick covers.

**Sources:** [policy](port/src/port_ads_accuracy.c) and
[header](port/include/port_ads_accuracy.h); spread hooks in both
`bullet_path_from_screen_center` copies and the recoil hook in
[gunfire.c](src/game/gunfire.c); option bridge in [joy_port.c](port/src/joy_port.c),
rows in [port_options.c](port/src/port_options.c), [front.c](src/game/front.c)
and [port_watchpages.c](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [default ini](port/src/video.c),
[settings reference](tools/midas_ini_help.txt) and [README](README.md).
**Checks:** [ADS accuracy tests](scripts/port/test_ads_accuracy.c), run by
[test_ads.sh](scripts/port/test_ads.sh), cover the defaults, option clamping
and menu bridge, the blend easing, the shotgun exemption, per-player state and
the demo gate. The Windows build compiles and the settings placement check
passes. No in-game capture measures the spread; the user's play-test judges
the feel.

### FORK-044 — Iron Sights settings page and placement rule

**Status:** Implemented; part of the Windows build the user play-tested for FORK-043.

Upstream's settings pages held fork settings mixed in with upstream's. Every
setting this fork adds now lives under **Gameplay > Iron Sights**, in four
subsections, in both the Options folder and the watch:

| Subsection | Settings |
| --- | --- |
| Aiming and Movement | Aim Mode, Knife ADS, ADS Spread, ADS Recoil (FORK-045), Weapon Kick (FORK-061), Sprint, Ludicrous Sprint |
| Combat Feedback | Hit Markers, Hit Sounds, Damage Direction, Tracer Strength (FORK-062), Shotgun Tracers and Decorative Pellets (FORK-063) |
| HUD Additions | Always Crosshair Size, Weapon Strip, Weapon Strip Style, Health Display, Objective Alerts, Mission Start Hints (desktop) |
| System | About / Build Info, Fonts (FORK-051, FORK-052), Crash Reporting (Windows/Linux) |

The watch's Iron Sights page also holds the Suspend Points action, which moved
there from the watch's Gameplay page. Rows left General Settings, HUD, Port
Options and Video, so those pages are back to upstream's rows plus upstream-owned
port settings. Enhanced Fonts then applied immediately, as it already did on the
watch, rather than waiting for the Video sheet's Apply Changes (the setting was
removed with FORK-021). Crash Reporting
is new on the watch. midas.ini keys, sections and defaults are unchanged.
Fork changes to upstream settings, such as Mission Timer's cheat targets,
stay on upstream's pages.

The folder uses its existing page-link rows (`SHROW_GOSHEET`): new sheet kinds
appended after Classic, titles, help lines and a `GE_MENU=zironsights` /
`zaim` / `zfeedback` / `zhud` / `zsystem` harness (`i` was already the mission
grid). The watch adds five pages before `PORTWATCH_CONTROLLER`, which moves the
Control tab's `GE_WATCHPAGE` numbers up by five; Game Options pages keep theirs.
The HUD, sight and browser crosshair-size audits now open the new pages (as
did the HD texture audit until FORK-033 was removed); the watch sight audit uses `GE_WATCHPAGE=3,12,0,0`.

AGENTS.md now requires new fork settings to go on these pages. The ROM-free
[placement check](scripts/port/settings_placement.py) treats every option key
missing from [the baseline key list](scripts/port/settings_upstream_keys.txt)
(generated from `9062f9f`, the 1.0.2b baseline) as a fork setting. It fails when
a fork setting is missing from either menu's Iron Sights pages or appears on an
upstream page, or when an upstream setting appears on an Iron Sights page.
Documented ini-only or single-menu exceptions live in the script. The
pre-commit hook runs it before gallery verification, and the Windows workflow's
ROM-free test step runs it and its tests.

**Sources:** [folder sheets](src/game/front.c),
[watch pages](port/src/port_watchpages.c) and [enum](port/include/port_watchpages.h),
[help](port/src/port_option_help.c), [AGENTS.md](AGENTS.md),
[standards overrides](standards-overrides.md), [hook](.githooks/pre-commit),
[audits](scripts/port/audit_sights.ps1), [HUD audit](scripts/port/audit_hud.ps1),
[browser audit](web/tools/crosshair-size-audit.mjs), [README](README.md),
[settings reference](tools/midas_ini_help.txt) and the feature guides that name
menu paths.
**Checks:** [placement tests](scripts/port/test_settings_placement.py) cover
option parsing, a clean layout, a setting missing from one menu, a fork setting
on an upstream page, an upstream setting on an Iron Sights page, and the
repository itself; the committed pre-change layout produced 55 errors.

### FORK-043 — Distance-independent hit sounds

**Status:** Implemented; the user's Windows play-test on Dam confirmed distant hits sound as they do up close. The web target was not built or tested with it.

Added **Gameplay > Iron Sights > Combat Feedback > Hit Sounds**, on by default, next to Hit Markers. Upstream
plays `HIT_BULLET_FLESH_SFX` at a struck guard's position (`gunfire.c`), with the
shared distance fade from `sub_GAME_7F0539E4`: full volume within 200 units,
fading to silent at 6000, so distant hits are barely heard. A player's bullet
plays that impact just before applying its damage (`chr.c`). `gunfire.c` now
reports each bullet flesh impact it starts, and when the same tick's damage to
that character is confirmed by the hook that raises the hit marker in
`chraction.c`, the impact's volume is set to full before it starts. The native
voice, sample and pan are kept, so a confirmed hit sounds exactly as it does up
close. Unconfirmed impacts, such as on a character that takes no damage, keep
their native fade. Each shotgun pellet raises its own impact, as up close.
Fists and knives play other native sounds, and mines, gadgets and the watch
laser play no impact, so none of them change. Thrown knives keep the native
fade, because their damage lands before their impact sound starts. The boost is
solo only, because split-screen players share speakers. Headshots use the same
sound; the gold marker remains their cue. The setting is a `[display]
hit_sounds` midas.ini key with folder/watch rows and help text; it does not
change the visual Hit Markers setting (FORK-007). Its rows are on the Iron
Sights pages (FORK-044).

This replaced the first prototype, which played a second copy at the volume the
native copy lost to distance. The synth's loudness grows with roughly the square
of a voice's volume, so the two partial voices peaked 3 to 6 dB below a
point-blank hit at 5 to 50 m, and play-testing still heard distant hits as
distant. The squaring is native (`src/libultrare/audio/env.c` maps each voice's
volume to its square "to give something close to loudness"), and AGENTS.md now
tells later work to record audible changes rather than reason from volume
arithmetic. A review of the fork's other volume code found nothing else that adds
or splits volumes: the Music/FX percent sliders map linearly to the native raw
volume as upstream's sliders do, so the mixer's squaring gives them its intended
loudness taper, and Stereo Pan posts a native pan value that the mixer's
equal-power table applies.

**Audit:** `scripts/port/audit_hit_sound.ps1` runs an isolated Dam copy with
music muted. `GE_HITSOUND_PROBE=<metres,...>` replays a hit at each distance from
a stand-in character above Bond, with Hit Sounds off and then on, through the
native impact and the confirmation hook. It does not fire a gun or apply damage.
`GE_AUDIO_CAPTURE=<path.wav>` records the mixed 22050 Hz output before the
unfocused-window mute, and the probe log gives each hit's frame.
`hit_sound_capture.py` measures each hit's peak stereo power against the nearest
unraised hit. It fails a raised hit more than 10% from it or an unraised far hit
that no longer fades, and writes `hits_compare.wav` with each distance off then
on. Captures stay in the ignored `build_win/hitsound-audit-*` directories.

**Sources:** [hit sound policy](port/src/port_hit_sound.c),
[header](port/include/port_hit_sound.h), impact hook in
[gunfire.c](src/game/gunfire.c), damage hook in
[chraction.c](src/game/chraction.c), option bridge in
[joy_port.c](port/src/joy_port.c), rows in [port_options.c](port/src/port_options.c),
[front.c](src/game/front.c) and [port_watchpages.c](port/src/port_watchpages.c),
[help](port/src/port_option_help.c), [default ini](port/src/video.c),
[settings reference](tools/midas_ini_help.txt); audit harness in
[probe](port/src/port_hit_sound_probe.c) (ticked from [lv.c](src/game/lv.c)),
[capture](port/src/port_audio_capture.c) (fed from
[audio_pc.c](port/src/audio_pc.c)), [env list](port/src/port_console_env.h),
[audit](scripts/port/audit_hit_sound.ps1) and
[measurement](scripts/port/hit_sound_capture.py); audio rule in
[AGENTS.md](AGENTS.md#repo-local-guidance).
**Checks:** [hit sound tests](scripts/port/test_hit_sound.sh) cover raising the
matching impact once, per-pellet impacts, ignoring other characters' and older
impacts, and split-screen and setting gating.
[Measurement tests](scripts/port/test_hit_sound_capture.py) cover log parsing and
the pass/fail rules. The audit on Dam measured raised hits at 5 to 58 m within
0.1 dB of point blank, while unraised hits faded from -3.2 dB at 5 m to
-20.5 dB at 50 m; the same measurement fails a capture of the two-copy
prototype. The 58 m unraised hit is masked by an unrelated Dam sound in its
window, and the gunfire-to-damage path itself was not scripted. The Windows
workflow's ROM-free test step runs the hit sound and measurement tests.
[test_ads.c](scripts/port/test_ads.c), which compiles `joy_port.c` with its
option modules, includes the hit sound module for its option bridge; the first
push of this feature failed CI at that link because the test lacked it.

### FORK-042 — Stray casing polygons in the browser

**Status:** Implemented; real-ROM acceptance recorded below.

The browser build drew large flat polygons, yellow or dark, for single frames
after Bond fired a pistol. Upstream's decompiled `casing->vel.z = frac * 0.0f`
multiplies an unset local by zero. GCC yields 0, as the N64 effectively did, so
native is unaffected. Clang folds it to a literal NaN. The casing's position then
stays NaN, so it never passes the `pos.y < floor` landing test and keeps its slot
until all 20 are stuck. Wasm converts the NaN translation to fixed-point 0, which
puts each casing on the camera, where it spins; at some angles its triangles cross
the camera plane and fill the screen. x86 converts NaN to 0x80000000, which throws
the casing out of view. The casing velocity is now an explicit 0.0 in both variants
of the ejection code, so native behavior is unchanged and browser casings now eject
and fall visibly. The hazard could return through an upstream merge, so a ROM-free
source test rejects the old expression.

`render-flash-audit.mjs` reproduces this by firing at the reported Dam position and
fails on a one-frame flash of 100 or more grid cells. `MIDAS_FLASH_MANUAL=1` keeps
the last 180 frames when a player presses F8, which is how the glitch was first
captured; `MIDAS_FLASH_TRACE_DRAWS=1` adds vertex data for draws reaching the camera
plane. Frames stay in ignored local directories.

This fix did not need depth clamp or mipmap changes: experiments with both left the
polygons.

A Clang `-Wuninitialized -Wsometimes-uninitialized` scan of the 287 web source files
reports 21 further warnings in decompiled game code. Each was reviewed for the same
hazard, and none needs a change. `gunfire.c` uses `#line`, so its warning is listed
by variable; line numbers elsewhere are as Clang reports them:

| Site | Variable | Verdict |
| --- | --- | --- |
| `chrprop.c:980` | `new_var` | Matching no-op; assigned 300 before its first real use |
| `front.c:1474` | `image` | Unreachable; the option icon is only ever 0, 1 or 2 |
| `front.c:17952` | `text` | Unreachable; three bit tests always count 0 to 3 |
| `gunfire.c` | `sp74` | Unreachable; inner branches cover the outer guard's four states |
| `image.c:420` | `indicesperbyte` | Paletted path only; see below |
| `model.c:1642` | `matrixPtr` | Matching no-op, `if (matrixPtr);` |
| `objecthandler.c:816` | `next` | Matching no-op, `if (next);` |
| `options.c:4461` | `sp60`, `sp5C` | Dead; `unused_draw_watch_inventory_page` has no callers |
| `options.c:5212` | `dpadtext` | Unreachable; the guard requires one of the four tested buttons |
| `options.c:5293` | `ctext` | Unreachable; the guard requires exactly one tested button |
| `othermodemicrocode.c:414` | `depth2`, `lrs`, `sp138`, `line` | Depth is a 2-bit GBI size; see below |
| `othermodemicrocode.c:681` | `line` | As above |
| `propobj.c:4824` | `previousOpenPosition` | Dead; both sides of the guard are set to 0, documented in source |
| `propobj.c:5175` | `sp63C` | Overwritten, or left unread with `hitGround` at 0, documented in source |
| `propobj.c:7606` | `tmp1` | No effect; only copied to `tmp2`, which is never read |
| `stan.c:1675` | `uninitialized` | Not a web defect; see below |
| `stan.c:2139` | `polygon` | Matching no-op, `if (polygon);` |

`texInflateZlib` reads a palette for every image, so `texAlignIndices` should only
see paletted formats, and Clang compiles it as 1 index per byte for CI8 and 2
otherwise. `tex->depth` is a 2-bit field, while `tconfig->depth` is a byte from game
data that is also passed to the RDP as a 2-bit size. `stan.c` is the one read that
can execute: when a walk fails before saving an edge, `stanSavedColl_pointI` gets
the unset value. Native reads an unwritten stack slot, which is garbage. Wasm reads
a zero-initialized local, which is a valid index, so the web build is the safer one
here. Temporary counters on an instrumented web build logged no non-paletted format,
out-of-range depth or unset `stan.c` read during walk-and-fire routes in Dam, Facility
and Statue. That is a sample rather than proof for all missions. The native `stan.c`
read is left as inherited behavior, with no observed failure to justify changing it.

**Sources:** [casing ejection](src/game/gunfire.c),
[source regression](web/tools/casing-velocity.test.mjs),
[flash audit](web/tools/render-flash-audit.mjs).
**Validation:** In headless installed Chrome on one host, the firing scenario gave
6 flashes of at least 100 cells in 11,887 frames before the fix, worst 1,776 cells.
Two runs after it gave none in 14,623 frames, worst 70. Native gave none in 8,992
frames, worst 98, so the remaining flashes are ordinary muzzle effects. A web capture
after the fix shows a brass casing ejecting beside the PP7. The player's own F8
captures showed the same polygons before the fix; a by-ear-and-eye check on the
deployed build has not been done. Edge and Firefox were not rerun for this fix.
Web unit tests, TypeScript, native buffer, upstream-merge and HUD checks pass, and
two-pass gallery regeneration changes zero image cells.

### FORK-041 — Browser audio output on the render thread

**Status:** Implemented; real-ROM browser acceptance recorded below.

Browser audio no longer plays through SDL's ScriptProcessor, which runs on the
game's main thread. Its queue cannot play while that thread is busy, so any long
frame or level load made the browser replay a stale 1024-sample buffer: the
buzz heard at screen transitions and Dam's start. A larger C-side queue cannot
fix this, because that queue is only read on the blocked thread.

`port/web/audio_output.js` (linked with `--pre-js`) now plays mixer frames from
an AudioWorklet queue at 22,050 Hz, with the browser resampling to the device.
The queue depth is how long the game thread can stall without an audible fault:
80 ms target in gameplay and 200 ms in menus and cutscenes, where level loads
happen. Trimming back to the gameplay target renders fewer frames, so no sound is
cut. A stall longer than the queue fades out over 5.8 ms, then fades back in after
30 ms is queued, instead of looping or clicking. Upstream pacing in
`port/src/audio_pc.c` stays in charge; `port/web/audio_config.h` redirects its
queued-audio SDL calls to `port/web/audio_output.c`, with small `#ifndef` and
`__EMSCRIPTEN__` hooks for the web target, queue cap and floor. Native audio is
unchanged, including its 256-sample period and 41 ms target. The AudioContext
stays at `Module.SDL2.audioContext`, which touch input resumes. Before the page
may play, frames drain on the wall clock as SDL's silence timer did.

Without AudioWorklet (for example a non-secure origin), or if its module fails to
load, the output falls back to SDL's ScriptProcessor at the FORK-026 512-sample
period; that path keeps the old stall behavior. Gameplay trades about 10–15 ms
more latency for this: the median queue is 101 ms against 67 ms plus a 21 ms
ScriptProcessor buffer before.

The transition audit records the rendered output from the menus through Dam's
start and adds deliberate 60, 150 and 600 ms stalls. The QA audio observer and
hitch probe now tap whichever node feeds the speakers, since no ScriptProcessor
exists on the normal path. `linux-game-audit.sh` gains `audio` and `transitions`
modes. WAVs and reports stay in ignored local directories.

**Sources:** [render-thread output](port/web/audio_output.js),
[device calls](port/web/audio_output.c), [queue settings](port/web/audio_config.h),
[pacing hooks](port/src/audio_pc.c), [target and metrics](port/web/audio_bridge.inl),
[link options](cmake/emscripten.cmake), [unit tests](web/tools/audio-output.test.mjs),
[transition audit](web/tools/audio-transition-audit.mjs),
[fault metrics](web/tools/audio-quality.mjs),
[QA observer](web/tools/audio-observer.mjs).
**Validation:** On the same Windows host, installed Chrome on the previous build
replayed stale audio for 80–91 ms during Dam's 97–99 ms load task and 37–40 ms
during a 52 ms menu task. Forced stalls gave 43 ms (60 ms stall), 128 ms plus a
hard 66 ms gap (150 ms) and 587 ms (600 ms) of replay. With this change, Chrome
and Edge have zero replays and zero hard gaps across the menus and Dam's start;
the 60 and 150 ms stalls produce no fault and the 600 ms stall a faded 372–398 ms
gap. Three Chrome runs and one Edge run of the quality audit have zero silent gaps
and sample jumps, with exactly silent pause. Linux Firefox on software rendering
has zero replays and hard gaps, plus soft gaps during its slower startup (worst
stall 4.6 s). Linux WebKit's touch audit passes with a running 22,050 Hz context
and nonzero output; Linux WebKit's desktop launcher flow stopped before audio with
Play disabled, in launcher code this change does not touch. Chrome launcher, room
and touch game audits pass. Chrome and Edge `room-exit-audit` runs measure a 0.56
output peak before exit and exactly zero in the returned room, including with
reduced motion in Chrome. A forced-fallback Chrome run confirms the
SDL path still plays, with its old replay faults. Native buffer tests pass, and
two-pass gallery regeneration changes zero image cells. These runs are not
physical listening tests, and physical Safari and iOS remain experimental.

**Background recovery (2026-11-06):** On iPhone and iPad, after the browser or
Home Screen web app sat in the background, all game audio played 1–2 s behind
every action until the browser was quit. A reused iOS context can come back with
that much delay, and a reset of the queue cannot remove it, so the output now
marks its context stale when the page is hidden, or when WebKit reports an
`interrupted` context. The next keydown, mousedown, pointerup or touchend then
disconnects the node, closes the context, and builds a new 22,050 Hz context and
processor inside that gesture, which may start it at once. A processor load
still pending for the old context is dropped. Until a gesture arrives, the old
context keeps playing or draining on the wall clock as before. Each rebuild logs
the old context's state, base and output latency, and its queued frames to the
browser console, so a device trace can show where the delay was. The same path
runs on desktop browsers, where the bug has not been seen; the rebuild adds a
single short gap after the first input on return.
**Background recovery validation:** Unit tests cover a rebuild after hiding, a
plain gesture that keeps the context, a single rebuild after an interruption,
and a processor load that the rebuild supersedes. A scratch real-ROM Chrome check
of a local build hid the page in Dam for 3 s and then pressed Resume game. That
click closed the old context and started a new running one; output peaked at
0.24 and 0.18 afterward (0.19 before), with no underruns. The check simulated
hiding in-page; it could not reproduce the iOS delay itself, so the fix awaits a
physical iPhone/iPad play-test.

### FORK-040 — Local mission music WAV export

**Status:** Implemented for desktop builds; generated audio remains local.

An opt-in offline mode renders a named ROM music sequence through the PC port's
existing N64 instrument bank and audio interpreter directly to 22,050 Hz,
16-bit stereo PCM WAV. Set `GE_MUSIC_EXPORT=dam|facility|damx|facilityx|wind`
and `GE_MUSIC_EXPORT_WAV=<path>` when launching an isolated Windows build with
the user's ROM. Rendering starts after the audio bank is initialized and before
the game loop, so menu and mission effects are absent. The first synchronized
repeat in at least three quarters of the sequence's channels ends an export;
this accommodates Dam's one nonrepeating channel. A naturally ending sequence
also stops when all channels end. The 10-minute cap fails rather than writing
an unbounded loop. Normal launches, settings and audio-device output are
unchanged. Upstream has ROM sequence extraction but no standalone mission WAV
export. Regenerate privately with the two environment variables; WAVs, logs
and the isolated build under `build_win/music-export*` are ignored and must not
be committed.

**Sources:** [music initialization hook](src/music.c),
[offline audio frame](port/src/audio_pc.c),
[WAV exporter](port/src/port_music_export.c),
[stage music mapping](src/game/music_0D2720.c).
**Validation:** Isolated Windows desktop build succeeded. Stock USA ROM runs
rendered Dam (145.76 s) and Facility (120.05 s) through their first shared
repeat. `ffprobe` confirms 22,050 Hz stereo 16-bit PCM and expected sizes;
`ffmpeg` volume checks find nonzero audio with peaks below clipping. No ROM or
rendered audio was staged. The isolated two-pass hand gallery regeneration
changed zero image cells; gallery freshness and workflow tests passed.

### FORK-039 — Archived upstream PR #104 gameplay capture harness

**Status:** Removed from fork `main`; no game behavior or default changes.

The PR-specific Windows capture runner and instructions introduced in
`e4a1e07` and refined in `d433559` were removed from the maintained fork after
the upstream ambient-light test. The former
`scripts/port/audit_upstream_pr104.ps1` and `scripts/port/pr104_gameplay.md`
are preserved in a local `PR-104-ambient-light` archive outside Git, together
with ignored videos, telemetry, logs and a checksum manifest. The separate PR
worktree remains available. Upstream 1.0.2b has no equivalent audit runner;
fork `main` no longer carries this contribution-only tooling.

**Validation:** Before removal, the isolated PR head `05a8d83` build recorded
180-second manual Dam and Facility runs with 178 process samples each. Both
exited cleanly without a crash log, and the user reported no lighting or
geometry issue. The archive retained 15 run folders and 199 evidence files;
the tooling files and the two completed gameplay videos have SHA-256 checksums.

### FORK-038 — In-game browser hitch notice

**Status:** Implemented for active browser gameplay; the notice is a timing
signal, not a diagnosis of memory pressure or graphics hardware.

The browser player uses its existing animation-frame monitor to detect one
gap of at least one second or two gaps of at least 500 ms within a minute
after five seconds of active play. It resets the baseline while the game is
paused, loading, hidden or unfocused, and warns once per page session. The
non-interactive notice disappears after 12 seconds and suggests closing extra
browser tabs or apps if hitches continue. No system resource probe, saved
setting, network request or additional frame loop is added. This is a fork
browser UI addition over the inherited game and complements the private
diagnostics in FORK-037.

**Sources:** [detector](web/src/hitch-notice.ts),
[capture hook](web/src/capture.ts), [player notice](web/src/player.tsx),
[styling](web/src/player.css), [tests](web/src/hitch-notice.test.ts),
[WEB-006](scripts/port/web_target_progress.md).
**Validation:** Web tests and typecheck, production site assembly, exact public
artifact verification, Prettier and repository standards checks passed. Unit
and capture tests cover thresholds, inactive reset and one notice per session.

### FORK-037 — Private browser hitch diagnostics

**Status:** Scripted and manual local diagnostics with synchronized cross-app
reproduction and a lower Chrome-load mitigation lead; root cause not yet
isolated and no backend workaround validated.
This adds no shipped runtime behavior, settings or asset changes.

The browser tooling now offers a repeatable menu/Dam timing audit with movement,
ADS and firing. It records bounded frame gaps, browser long tasks, selected
WebGL call times, game/audio counters, native logs, save synchronization and
aggregate audio output into ignored local JSON. Durations, Chrome/Edge selection,
an optional mission invulnerability control and disabled-probe comparison are
runner-only options. It does not write ROM bytes, raw sound or screenshots and
is not packaged into the public site. The investigation record explains why
these diagnostics are needed and distinguishes measured intervals at SDL swap
from an unproven GPU/driver cause. This extends the existing browser audit
surface in FORK-024 and audio continuity work in FORK-026 without changing them.
An optional browser-level CDP controller now keeps rolling main/compositor/GPU
traces after each stable phase, saves gzip streams and per-incident reports,
and retries data-losing phases with a larger buffer. Timing marks align frame,
native, audio, focus and save events. The reports record build/browser/renderer,
canvas, focus, trace loss, CPU samples and nearby thread activity, and reject
software rendering as hardware evidence. This remains private tooling only.
An audit-only `MIDAS_HITCH_SWAP_YIELD=0` option disables SDL's post-swap
Asyncify yield to compare pacing on the same build. Reports include that option
and the browser time origin for correlation with host counters. Headed gameplay
capture retries a trusted pointer-lock gesture and logs failures; the focus
probe also records pointer-lock errors. No shipped game setting changes.
The private runner can select Chrome's `gl` or `d3d11` ANGLE backend in its
temporary browser and records the requested backend alongside the actual WebGL
renderer. It can vary scripted pointer-move event counts from zero to 128 per
gameplay pass, with timing marks around each move. These are diagnostic options,
not site or saved game settings.
An optional headed manual phase allows physical mouse play after scripted Dam
entry. A bounded 100 ms count ring records the frequency of locked pointer
events without coordinates or raw inputs, alongside the existing frame timing.
This observer is armed only for the private manual audit and does not change
ordinary game sessions.

**Sources:** [runner](web/tools/hitch-audit.mjs),
[audit browser launcher](web/tools/browser-choice.mjs),
[browser probe](web/tools/hitch-observer.mjs),
[CDP controller](web/tools/hitch-cdp-trace.mjs),
[ROM-free CDP smoke](web/tools/hitch-cdp-trace.test.mjs),
[audio probe](web/tools/hitch-audio-worklet.js),
[script entrypoint](web/package.json), [run instructions](web/README.md),
[investigation](scripts/port/web_hitch_investigation.md),
[WEB-006](scripts/port/web_target_progress.md).
**Validation:** Optimized isolated web build, assembled site and exact public
artifact verification passed. Private actual-ROM Chrome, Edge and minimal-probe
Chrome audits reached normal menus and Dam. Multi-second frame gaps and audio
queue underruns recurred; memory did not grow and no save sync overlapped a
captured gameplay hitch. A later short audit passed on the combined main source
after its Files/context-menu change. The private traces remain in ignored
`build/web-hitches/`.
The ROM-free CDP stream/compression/mark test passes. Headless and headed Chrome
156 lossless menu and Dam traces on the same local runtime reproduce 1–6 s gaps;
first Dam traces with data loss are superseded by larger-buffer retries. The
headed Dam retry overlaps a long browser-main task and native swap interval,
without an identified long GPU task. The root cause remains unresolved.
A final short actual-ROM menu/Dam smoke passes the completed no-incident path
without trace loss; its 10-second windows do not change the longer findings.
Matched 10-second-menu/roughly-120-second-Dam headed Chrome comparisons on one
optimized NVIDIA/ANGLE build reproduced multi-second gaps with SDL's swap yield
enabled and disabled. The toggle is diagnostic, not a verified mitigation.
Concurrent private Windows counters found example gaps without sustained CPU,
disk, paging, DPC or interrupt saturation. The first Windows Performance
Recorder attempt failed under this session's profiling policy; the user later
captured a synchronized elevated ETW trace. A lossless headed Chrome/Dam trace
recorded a 2.0355-second gap while the user observed a Codex Desktop spinner
freeze. Streaming ETW analysis found reduced Chrome, Codex and DWM CPU activity
and a sharp drop in DWM-Core events during the gap, while other system work
continued. The shared graphics presentation cause remains to be isolated;
this evidence does not establish a driver fault. The ETW trace and analysis
stay private under ignored `build/web-hitches/`. Web tests, ROM-free CDP smoke,
typecheck, exact site verification and repository standards checks passed for
the earlier audit implementation; this documentation update has its own checks.
Installed headed Chrome/RTX 3070 runs on one optimized build confirmed the
requested hardware ANGLE backends. Two roughly one-minute OpenGL Dam runs had
worst gaps of 0.93 and 0.25 seconds; interleaved D3D11 runs had 7.19 and
5.04 seconds. The first D3D11 run lost pointer lock late, while the repeated
run remained focused. A longer OpenGL run had a 1.28-second gap and audio
queue-dry reports, so it did not eliminate the symptom. Scripted samples
initially suggested a backend mitigation; a matched longer D3D11 run had
five gaps of at least 500 ms, worst 2.45 seconds, without focus loss or
sustained host CPU/paging pressure. A D3D11 run without scripted pointer
motion still had four gaps of at least 500 ms, so those hitches do not require
pointer movement; a 32-step sample did not show a dose response. Streaming ETW
provider counts identified DWM drawing and
present submission falling sharply while vertical sync events continued. A
read-only mouse path review found fixed per-tick scalars and a bounded browser
diagnostics ring, not a growing game-side mouse queue.
A ROM-free synthetic-event check confirmed that the manual probe emits counts
and timing only. A five-second actual-ROM headed manual-mode smoke reached Dam,
recorded its phase/build/backend metadata and completed without input or focus
errors. The user then played two manual 120-second sessions and reported
Codex Desktop freezes in both. D3D11 recorded 8,995 locked pointer events,
four Dam gaps of at least 500 ms and a worst gap of 7.05 seconds. OpenGL
recorded 8,025 events, four such gaps and a worst gap of 5.93 seconds.
Both backends had severe freezes without sustained CPU/disk/paging saturation;
OpenGL is not a reliable workaround in manual play. Mouse-event rates before
the largest stops ranged from below to above the run median, without a clear
threshold. The manual traces remain local and ignored.
WPA 11.8 was installed locally with user approval for trace inspection. Its
exporter exhausted practical free memory on the roughly 1 GB synchronized
ETL, so that export was stopped; the unchanged trace remains available for
streaming analysis. A shorter 441 MiB WPR capture overlapped a 543 ms OpenGL
game gap, with a 524 ms Chrome renderer wait and browser long task while DWM
and Dxg present events continued. The user noticed a Codex freeze but could
not map it to that gap. Host paging was elevated, unlike the earlier
multi-second shared freeze. WPAExporter also strained memory on the smaller
ETL, while low-memory ETW CPU/scheduler readers completed. A short WPR capture
of a multi-second shared freeze remains useful if the symptom recurs.
After the user closed regular Chrome windows, a five-minute no-game Codex
interval had no reported freeze. Matched two-minute manual Dam runs on both
D3D11 and OpenGL then recorded about 8,000 locked mouse events each, no Dam
gap of at least 100 ms and no perceived game/Codex freeze. Earlier runs with
48 Chrome processes had four gaps of at least 500 ms per backend; the new
runs had 23–25 Chrome processes and about 1 GB more median available RAM.
Closing extra Chrome windows is a reversible mitigation lead, not a validated
root-cause fix; longer play and, if needed, a synchronized recurrence are
still required. The new private evidence remains ignored by Git.

### FORK-036 — Buffer and memory-safety audit

**Status:** Repository-wide audit completed with targeted fixes and explicit
remaining risks; this is not a whole-program memory-safety guarantee.

The repository-wide [audit record](scripts/port/buffer_audit.md) separates
confirmed defects, defensive gaps and tool/runtime coverage. The reproducible
[inventory](scripts/port/audit_buffers.py) classifies tracked engine, platform,
renderer, audio, web, tooling, generated and vendor sources and records sites
for unbounded text, memory operations, allocation, binary I/O and fixed arrays.
Reports are ignored local artifacts; lexical matches do not establish defects.
Cppcheck used actual Linux/Windows video compilation databases (287/288 units).
Parser gaps and untriaged legacy candidates remain documented rather than
being reported as clean. The audit preserves upstream/N64 layout and the
hand/arm contracts; it adds no new settings or geometry changes.

Native bank allocation and resize now use checked remaining capacity, validate
bank indices and fail diagnostically rather than wrap or spin. Both high-arena
alignment paths reject wraparound. Model conversion validates complete node
metadata before reading fields, and audio preflights DMEM spans, short-list
diagnostics, predictor indices and derived resampler/history accesses.
Capacity-preserving shader helpers replace unbounded appends. The renderer
checks vertex/triangle, matrix and light limits in release builds and copies
only the eight bytes actually owned by ambient lights. Native token buffers,
character exhaustion, visible-room insertion/diagnostics and multiplayer
ammo-slot indices are bounded; a missing Files controller return is supplied.

The fresh-process arm failure found during issue #4 was a separate ownership
gap: restored high-arena bytes did not restore their model-header/cache owners.
Arm/grip state and explicitly selected arena-backed texture/crosshair symbols
now travel with the world; host heap copies, settings, files and GPU state stay
process-owned (FORK-014/FORK-017). The headless adapter supplies the missing
no-device stubs needed to link and run the sanitizer target.

**Sources:** [bank guards](port/src/port_memp.c), [bank API](port/include/port_memp.h),
[engine hooks](src/memp.c), [high mappings](port/src/pcsched.c),
[model conversion](port/src/romfile_model.c), [audio spans](port/include/port_audio_bounds.h),
[audio interpreter](port/src/audio_pc.c), [shader text](port/fast3d/shader_text.h),
[shader builder](port/fast3d/gfx_opengl.cpp), [graphics spans/light copy](port/fast3d/gfx_memory.h),
[renderer hooks](port/fast3d/gfx_pc.cpp), [tokens](src/token.c),
[characters](src/game/chr.c), [ammo](src/game/prop.c), [visibility](src/game/bg.c),
[Files controller](src/game/front.c), [snapshot ownership](scripts/port/embed_state_layout.py),
[headless defaults](port/src/headless_stubs.c), [weak adapters](port/src/libultra_shim.c).

**Validation:** [ROM-free regression runner](scripts/port/test_buffers.sh)
executes real implementations under optional 32-bit ASan/UBSan, including
10,000 malformed model inputs, overflow/one-past/canary cases and
[audit-tool tests](scripts/port/test_audit_buffers.py). The
[Windows workflow](.github/workflows/windows-release.yml) runs it with sanitizers.
The report records successful all-20-mission headless startup samples and the
sanitizer exclusions. Existing ADS, wardrobe, HUD, suspend, merge, campaign
backup and HD-texture checks passed; backup/HD codecs also ran in both native
word sizes under ASan/UBSan. Native Aztec ADS/quickload effect checks passed all
24 captures and source-byte checks; representative fire/smoke was inspected.
Fresh-process Dam restoration from Facility passed with threaded rendering.
Windows/Linux video and optimized Wasm builds passed, along with 176 browser
unit tests. The reference gallery regenerated all 72 views and 35 icons with
repeated samples and an independent second pass: zero changed image cells.
The final executable passed full synchronous suspend checks (all slots,
quick/undo/death, watch save, cross-mission/title restore and truncated-image
rejection) and threaded fresh-process Dam/Facility restoration.
The final 120 Hz threaded/MSAA-2 Aztec effect audit and a fresh Facility-process
restore of that Aztec snapshot each passed all 24 captures and texture-byte
checks; a restored fire/smoke capture was visually reviewed.

**Leak detection (2026-11-07, on `91640bc`):** the original samples ran with
`detect_leaks=0`. `audit_buffers.py --mission-smoke --leaks` now runs them with
LeakSanitizer and the checked-in [suppressions](scripts/port/lsan_suppressions.txt),
and fails a mission on a leak report as well as ASan/UBSan output. A
[renderer run](scripts/port/leak_audit_video.sh) covers a `GE_VIDEO=ON` plus
`GE_SANITIZE=ON` i686 build under Xvfb and Mesa softpipe in
`midas-linux-release`; that configuration links without `headless_stubs.c`.
The headless sanitizer target had stopped linking because the FORK-048 hit-map
probe sets the renderer's `ge_shot_force`; `headless_stubs.c` now defines it.
No run reported a leak: all 20 headless missions, a 14,000-frame title with two
attract-demo stage changes, multiplayer, settings writes, a console command and
the hit-map debrief headless; title, Dam (render thread off and on), Facility,
Aztec, Frigate, Cradle, two-player Temple, the hit-map debrief and the
in-process icon stage with the renderer; and 17 ROM-free suites built with
ASan/UBSan/LSan wrappers. The suppressions file therefore has no active entry;
it records why the deliberate never-freed singletons (they stay reachable)
need none and how a future third-party entry must be scoped. LSan sees only
orphaned heap blocks at exit, not reachable caches that grow, GL objects or the
mmap'd arenas, and MinGW has no LSan. The [audit record](scripts/port/buffer_audit.md#leak-detection)
lists the method, rerun commands, runs and gaps. The gallery fingerprint was
regenerated for the stub change with zero changed image cells. The audit-tool
tests cover the leak option and verdict.

### FORK-035 — Desktop mission-start control hints

Desktop solo missions show compact gadget and aim-down-sights hints, plus a
conditional sprint hint, for eight seconds of controllable gameplay, fading
smoothly during the last two seconds.
The inherited upstream HUD has no such onboarding. The timer waits for Bond's
actual control-unlock predicate, pauses while the watch, menus, console or
cutscenes take control, and hides with the HUD or on death. Mission starts and
restarts rearm it; generic HUD/inventory resets do not. Suspend-point restores
dismiss it. Multiplayer, browser, Xbox and headless targets omit the overlay.
**Gameplay > Iron Sights > HUD Additions > Mission Start Hints** is available in the desktop folder
and watch menus. It defaults on (`[display] mission_hints=1`) with a saved
completed-display count (`mission_hints_shown=0`). After three complete windows
that actually rendered a hint, the setting automatically turns off. Shortened
missions, hidden/unbound hints and suspend restores do not consume a display.
Explicitly re-enabling resets the count for the next mission. Counts/settings
stay outside suspend images, so loading an older image cannot reset the quota.

The first two rows describe tapping the live `next_inventory` binding and aiming
down sights through the current `r` (Aim) binding in ADS mode. They respect the
player's Hold/Toggle setting; a separately mapped `aim_down_sights` action is
the fallback when normal Aim uses the red reticle or is unbound. They use the
existing watch binding readback,
including mouse-button names and remapped keys. Keyboard alternatives show the
first mapped key; unbound actions disappear. Labels follow player one's most
recently used device. A button, trigger or decisive stick movement on player one's
pad selects its friendly button labels; a key press or mouse click selects key
labels. Before any input, the intro's player-one claim decides. With
`p1_hybrid=0` and a pad owning player one, pad labels always show. Previously a
keyboard/mouse intro claim kept key labels even while the player used a pad. N64-mode pads
omit these direct-action hints because their controls do not consume those
actions. A third row says to hold the live Walk / Sprint binding to sprint only
while the Sprint gameplay setting is on. It disappears when Sprint is off or
the action is unbound, and follows remapped keyboard/controller labels.
Bare text, with a one-pixel shadow and no rectangular background, is
vertically centered and starts slightly inside the left health arc. Its anchor
follows health centering/edge transitions independently of the ammo HUD, at wide
and 4:3 aspect ratios. Friendly mouse names include **Right Mouse Button**;
Mouse X1/X2 remain distinct as Mouse Button 4/5. Display aliases do not change
saved binding tokens. Text uses uniform small capitals at 0.60 scale (reduced
from 0.65 for a further, roughly eight-percent size reduction) and a
six-framebuffer-pixel rightward inset, including a matching shadow offset.

**Sources:** [overlay and timing](port/src/port_mission_hints.c),
[interface](port/include/port_mission_hints.h), small lifecycle/draw hooks in
`src/game/objective.c`, `src/game/bondview2.c` and `port/src/port_state.c`.
`port_options.c`, `port_watchpages.c`, `front.c`, `joy_port.c` and `video.c`
connect the menu setting, runtime defaults and persisted fields.
The renderer reuses `portHudLabel`; input labels come from the
existing `video.c`/`input_pc.c` binding APIs rather than a second key table.
**Validation:** The last-used-device rule (`port/include/port_input_prompt.h`,
tracked in `port/src/input_pc.c`) has ROM-free decision checks. It was built for
Windows but not exercised with a physical controller in this change.
[ROM-free checks](scripts/port/test_mission_hints.sh) pass
control-lock timing, pause/resume without rearming, eight-second duration at
30/60/120/240 Hz, smooth final fade, live custom keyboard/mouse/controller
labels, conditional sprint visibility/remaps, health anchoring, uniform
typography, quota persistence and stage/HUD/death/multiplayer/restore gates. Existing
HUD, ADS and suspend-state regressions pass. The native release workflow runs
the new hint checks. [Isolated Windows captures](scripts/port/audit_mission_hints.ps1)
cover default/remapped/unbound hints. An isolated four-process mission audit
verifies saved counts 1/2/3, automatic Off and continued Off after relaunch,
alongside the one-time ADS mouse migration retaining a custom alternative.
Windows/Linux and optimized Wasm builds pass. Windows wide/4:3 captures verify
the final inset, small-cap text, friendly/default/remapped bindings, fade and
disappearance; the folder HUD menu shows the new toggle. Unbound captures omit
the overlay. Browser artifact integrity, touch-bridge and menu-pointer checks
pass. The gallery fixture explicitly disables onboarding with its other HUD
overlays; its nine workflow tests pass. Both independent gallery passes reproduce
all 72 views and 35 icons with zero changed image cells, and integrity verification passes.
The additional 0.60 font-size refinement passes the hint regression and Windows
build, with legibility/alignment reviewed in wide and 4:3 mission captures.
Its gallery refresh again matches both independent passes with zero changed
image cells.
The three-row update passes the ROM-free hint checks and Windows rebuild.
Isolated wide/default, wide/Sprint-off and 4:3/remapped mission captures were
visually reviewed for text, conditional visibility and health-arc spacing.
The refreshed gallery passed two independent 72-view captures and 35 icons with
zero changed image cells.

### FORK-034 — Portable desktop save import and export

Windows and Linux add **Import** and **Export** directly to the
four-campaign Files screen, sharing the web target's two-row/three-column
action layout. Columns now read Import/Export, Copy/Options, Erase/Exit Game
from left to right. This replaces the original rightmost transfer column and
shortens its visible labels from Import Save / Export Save, saving horizontal
space on desktop and web. Compact column spacing keeps the leftmost icons and
labels clear of the SELECT FILE title. The title is now left-aligned at menu
x=40, approximately ten menu pixels inside the leftmost folder's edge. This
replaces the inherited desktop centre anchor and browser-specific centre
offset, preserving the existing title sizes and vertical placement. Browser
accessibility names retain the save context, and measured hit targets follow
the new positions and label widths.
The inherited upstream desktop menu has no portable JSON save
transfer. Windows uses native Open/Save dialogs; Linux launches `zenity`, falling
back to `kdialog` when unavailable, with direct argv calls and no shell. Xbox,
N64, headless and web builds do not compile this desktop adapter. No setting or
fresh-install default is added.

Exports on Windows, Linux (both zenity and kdialog) and web now suggest
`iron-sights-saves-YYYY-MM-DDThh-mm-ss.json`, using local date/time with
zero-padded month, day, hour, minute and second fields. This replaces the fixed
native filename and web's date-only filename. Desktop Save dialogs still allow
editing the suggested name. JSON `createdAt` remains ISO UTC; the web filename
and metadata describe the same export instant. The web formatter is isolated in
`web/src/backup-filename.ts`, shared by Files and legacy storage-panel exports.

The existing browser version-1 `project-midas-saves` envelope transfers exact
EEPROM, profile, device-binding, INI and custom-controller bytes both ways.
Exports flush pending profile/settings writes. ROMs, texture packs, logs,
statistics and executable-specific suspend images remain excluded. Import
validates the allowlisted paths, 512-byte EEPROM, file/total limits, canonical
base64 and per-file SHA-256, rejects conflicting Windows filename aliases, and
requires explicit full-replacement/restart confirmation. Cancellation and
invalid input leave the live data intact.

Confirmed imports are staged privately, outside the export allowlist. Restart
occurs after exit handlers have saved the old session; the next process applies
the marker before settings/profiles/EEPROM initialization. A pre-image and
journal support interrupted-restore recovery and retry; uncommitted marker
temporaries are discarded at startup. Collection/application refuse linked save
directories. An unresolved restore failure stops startup and keeps recovery
data. Desktop instances sharing one save folder must be closed before import.

Focused sources: [codec](port/fast3d/save_backup.cpp),
[storage/recovery](port/fast3d/save_restore.cpp),
[dialogs/restart](port/fast3d/save_transfer.cpp),
[C interface](port/include/port_save_transfer.h),
[startup](port/src/main.c), [menu hooks](src/game/front.c) and [CMake](CMakeLists.txt).
The hosted C++ object target avoids inherited SDK header collisions. The pinned,
unmodified picojson/PicoSHA2 headers and retained notices live under
`port/fast3d/third_party`; `.gitignore` permits that focused vendor directory,
and the file-size checker excludes the upstream picojson header. Both native
packages carry `Save-Transfer-LICENSE.txt`; packaged start guides describe
replacement semantics and Linux picker prerequisites.

[Usage and verification](scripts/port/save_transfer.md) document the public flow.
[ROM-free storage checks](scripts/port/test_save_transfer.py) and
[actual web/native interoperability](web/tools/native-backup-audit.ts) use
synthetic temporary files. Native-release CI runs host/i686 storage checks;
Pages CI additionally runs the actual codec comparison. Windows and Linux video
builds, host/i686 storage checks, the Windows driver and real browser codec
round trips pass locally. Actual-ROM Windows UI acceptance passes native-cursor
and mouse activation, desktop export parsed by the web codec, picker/confirmation
cancellation, invalid-input preservation, and confirmed web import with automatic
process restart, exact EEPROM/profile/INI bytes and marker cleanup. Widescreen
and 4:3 Files layouts were reviewed. Linux interactive picker acceptance remains
unverified on this Windows host. Windows-specific regressions also pass
non-ASCII filename-alias rejection and recovery after a partial replacement
blocked by a read-only file. All 36 web backup boundary tests, packaging checks,
type/scoped-format checks, nine gallery workflow tests and the standards checker
pass. The refreshed 72-view/35-icon gallery passes two independent render runs
with zero changed image cells; its source fingerprint is current.
The column/label refinement also passes widescreen/4:3 native capture review,
optimized native/Wasm builds, actual-game Edge native/direct and touch-mode
transfer checks, responsive hit targets, production artifact verification and
the ROM-free menu-pointer regression. Its gallery refresh again reproduces all
107 references across both independent runs without changed image cells.
The heading's left alignment was reviewed in native widescreen/4:3 and browser
desktop/phone captures. Native/Wasm builds, browser Files transfer acceptance,
artifact verification and pointer checks pass; its refreshed gallery again
matches both independent passes with zero changed image cells.
Timestamped filenames pass Windows/Linux builds, Windows Save-dialog review,
web type/format/build/artifact checks and all 36 backup boundary tests. An actual
Edge download verifies the local timestamp against its unchanged UTC metadata;
manual formatter checks cover zero padding and a year boundary.
Its gallery refresh matches all 72 views and 35 icons across two independent
passes with zero changed image cells.

### FORK-033 — Native campaign HD texture packs (removed)

**Status:** Removed on 2026-11-05. No target prepares or ships third-party
texture packs. Since 2026-11-06, FORK-053 reads a pack the player installs
themselves, reusing this entry's fingerprints, renderer adapter and alias
data; the bundling, `.istex` format and CI preparer described below stay
removed.

From 2026-10-29 the native Windows/Linux OpenGL builds could replace campaign
environment textures from a compressed `Data/TexturePacks/hd.istex` (or `4k.istex`)
sidecar, selected by **Gameplay > Iron Sights > System > Texture Pack**
(`[display] texture_pack`, HD by default). The trusted-main `hd-artwork` CI job
converted a pinned subset of intermissionfb's GoldenEye 007 HD v2025.12.30
release (plus three textures from a pinned artist revision) into that format,
and every Windows, Linux tarball, AppImage and Steam Deck release bundled it with
attribution notices. The source repository has no license; the earlier text
here recorded that as the owner's choice. Its maintainer has since stated
([GhostlyDark/GoldenEye-007-HD#3](https://github.com/GhostlyDark/GoldenEye-007-HD/issues/3),
2026-11-05) that direct integration in a port like Iron Sights is explicitly
disallowed and that texture packs should remain external mods added by the user
and kept updated by their maintainers. The owner chose to remove the whole
feature rather than keep a converter for player-supplied copies, so nothing in
Iron Sights handles that art.

Removed: the `.istex` loader, format/identity code and renderer adapter
(`hd_textures.*`, `hd_texture_format.*`, `hd_texture_import.inl`,
`port_hd_textures.h`), the vendored puff decoder and its file-length exception,
the `ge_hd_textures` CMake object target, the renderer hooks (`gfx_pc.cpp`
texture release, import and frame-start calls, now as before `00ec557`, and the
`hd_bytes` cache field), the setting in the Options folder, watch, help, ini
writer and `midas_ini_help.txt`, the bundled-root lookup in `video.c` and
`GE_BUNDLED_TEXTURE_ROOT` in AppRun, the About credits, the preparer, source and
alias manifests, supplement verifier, artwork allowlist, CI job and downloads,
packaging, provenance (`build-inputs.json` `hd_textures`), player-guide sections,
README bullet, `hd_textures.md`, and the loader, preparation, distribution and
capture-audit scripts. `test_linux_hd_launcher.py` became
`test_linux_launcher.py` without its texture-root checks; it still covers the
Steam Deck launcher and AppRun's writable-root rules and now runs in CI by that
name. The inherited upstream BMP pool-byte texture rewrites are unaffected.

Packaging now guards the other way: `test_distribution.py` puts a stray
`Data/TexturePacks/hd.istex` in the build folder and requires that nothing under
`Data/TexturePacks/` ships, and the Linux smoke test requires the tarball,
Steam Deck ZIP and extracted AppImage to contain no `Data/TexturePacks` entry.
An existing `texture_pack=` line in midas.ini is ignored as an unknown key
(logged once at startup), and an old `Data/TexturePacks` folder is no longer
read; the Windows player guide says it can be deleted. Releases published before
the removal still contain the pack; they were left in place for now. FORK-047,
the HD ammo icons from the same release, was removed at the same time.

**Sources:** [renderer](port/fast3d/gfx_pc.cpp), [cache metadata](port/fast3d/gfx_pc.h),
[settings bridge](port/src/video.c), [options](port/src/port_options.c),
[watch](port/src/port_watchpages.c), [frontend](src/game/front.c),
[help](port/src/port_option_help.c), [About](port/fast3d/about.cpp),
[CMake](CMakeLists.txt), [CI](.github/workflows/windows-release.yml),
[Windows](scripts/port/package_windows.py)/[Linux](scripts/port/package_linux.py) packaging,
[release inputs](scripts/port/release_inputs.py),
[package tests](scripts/port/test_distribution.py),
[Linux smoke test](scripts/port/test_sentry_linux.py), [AppRun](scripts/port/linux/AppRun)
and its [launcher tests](scripts/port/test_linux_launcher.py),
player guides ([Windows](scripts/port/release/START-HERE.txt),
[Linux](scripts/port/linux/START-HERE.txt), [Steam Deck](scripts/port/linux/STEAM-DECK.txt)),
[release notes](scripts/port/releases.md), [buffer audit](scripts/port/buffer_audit.md),
[INI guide](tools/midas_ini_help.txt), [README](README.md).
**Checks:** Same run as FORK-047: the Windows build compiles and links without the
loader, `test_distribution.py` passes with a stray pack in the build folder and
`test_linux_launcher.py` passes; settings placement, the remaining ROM-free
regressions, web tests, standards check and gallery (zero changed cells) pass.
The Linux packages and their smoke test need the Linux release image and run in
CI; they were not run locally. No in-game capture was taken.

### FORK-032 — Best-time column on mission difficulty pages

**Status:** Implemented.

The selected mission's difficulty page now shows an always-visible **Best Time**
column beside the inherited Agent, Secret Agent, 00 Agent and optional 007 rows.
The heading and centered times use the difficulty labels' native Zurich Bold
font, size, black ink and menu text renderer directly on the dossier paper, with
no added background panel or rule. Each label shares a column center halfway
between the red completion checkmark's right edge and the dossier paper's right
edge, giving equal space on either side. They fit beside the existing completion
checkmarks and cheat-target badges. Only
available difficulty rows appear. This adds a fork presentation of the upstream
campaign's packed time records; the upstream briefing already displayed a best
time after choosing a difficulty, but the difficulty list had no record column.

Measured records use `MM:SS` from the selected campaign and mission. Unplayed
records, absent saves and the inherited 1023-second completion/overflow sentinel
show `--:--`. Custom 007 also shows `--:--` because the inherited save
does not store a measured 007 time. Unlock All never supplies a synthetic record.
The column needs no setting and does not write saves or change selection areas.
The shared native/browser port uses one focused drawing hook in `front.c`;
the original N64 path is unchanged.

**Sources:** [record lookup and column](port/src/port_mission_times.c),
[API](port/include/port_mission_times.h), [menu hook](src/game/front.c),
[inherited packed save codec](src/game/file2.c),
[release regression gate](.github/workflows/windows-release.yml).
**Checks:** [ROM-free regressions](scripts/port/test_mission_times.sh) pass with
runtime and compile-time Unlock All, exercising the actual save codec for all
20 missions and three standard difficulties, campaign isolation, missing saves,
zero/sentinel records, 007, invalid inputs and bounded buffers. Existing mission
cheat regressions also pass in both variants. The isolated
[menu audit](scripts/port/audit_mission_cheats.ps1) accepts three `-BestTimes`
values in Agent/Secret Agent/00 Agent order, with 1023 meaning no measured record.
Its optional fixture fields extend `GE_MISSION_CHEAT_AUDIT` in RAM only and keep
the existing three-field fixture compatible. Captures/logs remain ignored and
copied audit executables are removed after exit. Windows captures at 960x540 and
800x600 with measured and missing records across all four rows verify the
background-free native menu text, common column center and equal spacing between
the checkmark and paper edge.

Windows compilation, the existing HUD regressions and release-workflow
`actionlint` validation pass. Four final isolated Windows captures at 960x540
and 800x600 verify measured records through 17:02, minute-boundary formatting,
mixed and entirely absent records, 007, Unlock All, cheat-badge/checkmark spacing,
and an Aztec page with only Agent unlocked. The background-free native text
supersedes the initial dark panel, gold rule and light-colored record text.
Both independent gallery passes verified all 72 views and 35 icons with zero
changed image cells; only source-fingerprint metadata was refreshed. All eight
gallery workflow tests and the standards checks pass.

### FORK-031 — Linux i686 portable releases and AppImage

**Status:** Implemented.

Adds trusted-main Linux i686 compilation, a portable tar.gz, a type-2 AppImage
and a Steam Deck ZIP to the existing native release workflow. The ZIP reuses
the same i686 game and bundled libraries, adds a launcher that works from any
Steam working directory, and keeps writable player data beside the executable.
Device-verified SteamOS support remains pending. All game/SDL/Sentry ELF files
are built in a digest-pinned Ubuntu 22.04 container. SDL source and the AppImage
runtime are SHA256 pinned; the Sentry source remains commit pinned. The tarball
preserves executable permissions. Allow-listed packaging excludes ROMs, saves,
INI files, crash queues, extracted assets and debug companions. Non-system
runtime dependencies and license notices are bundled; the 32-bit loader, glibc
2.35+ and OpenGL driver stack remain distribution requirements. AppImage does
not remove these requirements or claim verified Steam Deck support.

The AppImage launcher seeds missing packaged defaults into
`${XDG_DATA_HOME:-$HOME/.local/share}/iron-sights`. A small Linux-only root hook
accepts an absolute `GE_DATA_ROOT`; tarball launches retain executable-relative
data by default. The SDK/helper still load relative to the executable, separate
from this writable root. This is fork distribution work, replacing the prior
local-only status for Linux; the historical PowerShell three-platform packager
remains a manual legacy path. Native publication waits for Windows and Linux
smoke tests, uploads exact ELF symbols/source context, attests all three Linux
packages and publishes one commit-tagged release. Compiler-object caching is
separate from packages; a clean-build dispatch bypasses it.

FORK-033 once added the same verified HD artwork artifact to all three Linux
packages, with a separate read-only bundled texture root set by AppRun and a
runner-side artwork download placed before Docker created build output. That
artwork, the AppRun variable and the download step were removed on 2026-11-05;
the smoke test now requires each Linux format to contain no `Data/TexturePacks`.

A Linux-only ELF export rule keeps the game's N64 libc/math replacements
private, so shared graphics/audio/reporting libraries use standard libc calls.
It retains the private symbol table and DWARF companions for source resolution.

The requested native x86_64 port was assessed and deferred as substantial engine
work: an ABI probe changes pointer size from 4 to 8 and `Gfx` from 8 to 16 bytes;
texture paths explicitly reject 64-bit host pointers. No unverified x86_64
binary is published. ROM layouts, address conversions, graphics parsing and
state serialization require a coordinated audit before changing that ABI.

**Sources:** [baseline container](scripts/port/linux-release.Dockerfile),
[build](scripts/port/linux_build.py), [packager](scripts/port/package_linux.py),
[AppImage launcher](scripts/port/linux/AppRun),
[Steam Deck launcher](scripts/port/linux/steamdeck-run.sh),
[Steam Deck instructions](scripts/port/linux/STEAM-DECK.txt),
[ELF export boundary](port/crash/linux_game_symbols.map),
[player instructions](scripts/port/linux/START-HERE.txt),
[workflow](.github/workflows/windows-release.yml), [root hook](port/src/main.c),
[release guide](scripts/port/releases.md),
[packaged tests](scripts/port/test_sentry_linux.py),
[launcher tests](scripts/port/test_linux_launcher.py),
[reporting](#fork-030--native-sentry-crash-reporting-and-symbol-uploads).

**Validation:** The 32/64-bit ABI probe reproduced the sizes above. Linux
reporter syntax, Sentry tooling, ADS/wardrobe/HUD and distribution/launcher tests
pass. The earlier nine packaged reporting scenarios passed, including a real AppImage
fault and its writable-root behavior. The Windows 72-view/35-icon gallery was
regenerated with zero changed cells. A private ROM-loaded softpipe smoke reached
6000 frame pumps/13 submitted graphics tasks and exited normally with Sentry
active. Local llvmpipe/LLVM 15 also failed with reporting disabled; this is not
a hardware-driver or Steam Deck certification. Final rebuilt tarball/AppImage
packages repeated all nine reporting scenarios successfully. Hosted CI and
dashboard verification follow the same exact-artifact release workflow. The
first hosted run passed Linux compilation, symbol uploads and the nine original tests,
then exposed an artifact-uploader permission mismatch with Docker's private
temporary directories. Tests now preserve logs/event JSON in a readable ZIP and
remove their isolated runtime copies before the host uploads diagnostics.
The Steam Deck packager produced a valid local ZIP from the previously released
Linux AppDir: its game is ELF32, its two new files match their SHA-256 entries,
and no ROM entry is present. Both ROM-free POSIX launcher tests passed in Docker.
The full-size tarball, AppImage and Steam Deck ZIP passed all ten packaged
crash-reporting scenarios locally using the previously released Linux binary.
The workflow fingerprint refresh completed two gallery passes with zero changed
image cells across all 72 views and 35 icons.
Physical Steam Deck validation remains pending.

### FORK-030 — Native Sentry crash reporting and symbol uploads

**Status:** Implemented.

Adds Windows and Linux reporting, default ON, with a persistent Iron Sights > System toggle
and early section-aware `[diagnostics] crash_reporting=0` opt-out. OFF stops the
SDK and clears its dedicated queue; `GE_CRASH_REPORTING=0` suppresses a process.
Render/audit runs suppress reporting. The inherited Windows crash log remains;
its stack walker now receives a context copy before forwarding the original
exception to Sentry. Persistent watchdog stalls produce an explicit error.

The GCC x86 game optionally loads a matching, unmodified MSVC x86 Sentry DLL and
native crash helper. Missing reporter files leave gameplay available. GNU PE
build IDs make embedded DWARF matchable. Trusted main CI builds the pinned SDK,
records runtime hashes/license, uploads exact game/SDK symbols and source context
with a CI-only secret, and tests the packaged executable with a loopback crash
receiver before publishing. This is new fork behavior, not inherited upstream
telemetry. Existing main-build provenance and packaging remain the delivery path.

Linux uses a focused POSIX adapter with executable-relative optional SDK loading,
early section-aware INI parsing and descriptor-relative no-follow queue cleanup.
It shares the menu toggle and project, uses `linux-release` for CI builds, and
restores the prior local signal handlers on shutdown. AppImage queues follow its
writable root. FORK-031 records the Linux release build and package formats.

Symbol-upload tooling pins both supported Sentry CLI 3.8.2 asset SHA256 values
in `sentry_config.json` and downloads from the versioned release URLs directly.
Every cached/downloaded binary is checked before use; a valid cached binary
needs no network access. This removes the unauthenticated GitHub release-metadata
API lookup that hit a hosted-runner 403 rate limit and blocked Windows release
publication. Pins were verified against the upstream release asset digests;
missing or malformed pins and mismatched downloads fail closed.

Only structured events are enabled; no minidumps, ROMs, saves, screenshots or raw
logs are attached. Default native events contain module paths, a generated
installation ID and technical system/CPU state; see the payload disclosure.
Offline retry uses a dedicated queue with startup pruning of ten events, 5 MiB
and seven days. Opt-out deletes pending data without traversing reparse points.

**Sources:** [Windows adapter](port/crash/reporting_win.c),
[Linux adapter](port/crash/reporting_linux.c),
[public API](port/include/port_crash.h), [startup/logger](port/src/main.c),
[menu table](port/src/port_options.c), [menu row](src/game/front.c),
[help](port/src/port_option_help.c), [build integration](scripts/port/sentry.cmake),
[pins/public DSN](scripts/port/sentry_config.json),
[SDK/symbol tooling](scripts/port/sentry_tools.py),
[workflow](.github/workflows/windows-release.yml),
[packager](scripts/port/package_windows.py),
[manual packager](scripts/port/package_release.ps1),
[input metadata](scripts/port/release_inputs.py),
[reporting guide](scripts/port/crash_reporting.md).

**Validation:** Tooling/distribution tests, ADS, wardrobe, HUD, Windows compilation
and all nine packaged Windows reporting scenarios pass locally. These cover
access violations, early and live opt-out (including a read-only INI), missing-DLL
fallback, process suppression, healthy shutdown, explicit watchdog error delivery
and exactly-once retry after a connection failure. Watchdog reporting is bounded
so an unavailable SDK cannot prevent its existing forced exit. Standards report
zero findings. Cloud source-line verification uses the exact CI executable and
its debug ID, following the reporting guide; receipt logs remain local.
The CLI download refinement passes eight ROM-free tooling tests, including
offline cache reuse, direct repair of corrupt caches and rejected mismatched
downloads. Real Linux/Windows downloads match the pins and report version 3.8.2;
the native release workflow passes actionlint.
Both independent gallery passes retain all 72 views and 35 icons with zero
changed image cells after the CI ordering fix; nine gallery workflow tests pass.

### FORK-029 — Shared presentation defaults and usable Video pagination

**Status:** Implemented.

New/default settings use Wide for solo HUD ammo/messages, health/armor arcs and
multiplayer HUD layout (`hud_wide`, `health_wide`, `mp_hud_wide` = 1). MSAA and
anisotropy start at 2x (`msaa=2`, `anisotropy=2`), and 2D filtering starts enabled
(`filter_2d=1`) on every platform. Explicit saved INI values still override
these defaults; existing hardware capability fallbacks and Classic Mode
overrides remain authoritative. This differs from inherited centered HUDs,
disabled AA/anisotropy and point-filtered 2D. The browser bootstrap no longer
overrides common 2x MSAA with its former off setting.

Video's middle tab now shows Next while another settings page exists. Classic
is a sibling of Video and Audio in Options > Audio Visual, with matching label
and help. This supersedes the brief last-Video-page Classic tab. Previously
Classic occupied the tab unconditionally,
leaving Video 2/2 (Max FPS) unreachable after the fork added Enhanced Fonts.
Previous still returns to earlier pages and pending values survive page turns;
Apply Changes remains available on both pages. Classic retains its existing
leave-sheet/apply semantics. About discovery and desktop Start behavior are
covered by FORK-020 and FORK-024 below.

**Sources:** [shared defaults/input hook](port/src/video.c),
[browser bootstrap](web/src/browser-settings.ts),
[frontend labels/navigation](src/game/front.c),
[INI reference](tools/midas_ini_help.txt),
[native Start regression](scripts/port/test_start_input.c) and
[SDL2 test runner](scripts/port/test_start_input.sh). The runner needs the native
32-bit SDL2 development build; set `SDL2_CONFIG` to its `sdl2-config` path.

**Validation:** Windows and optimized Wasm builds pass. An isolated native
load/save audit verifies all six new defaults and retention of explicit older
values. Windows UI review reaches Video 2/2, Classic and both solo Wide HUD rows;
About opens the existing build dialog and its helper text fits. The native Start
regression verifies the compiled Return binding, removal of Return-only watch
suppression, tick reset, custom rebinding and blanked input. The desktop input
automation could not reliably trigger gameplay keys (including the old pause
key), so it is not claimed as an end-to-end keyboard pass. Edge's actual-ROM
Start audit passes Enter/P watch interoperability, held-key behavior, Alt+Enter
isolation and HTML Resume. Type checking, 146 browser tests, site allowlist,
HUD/ADS/upstream-integration and distribution checks pass. Gallery regeneration
passes both independent captures of 72 views and 35 icons. All 107 images reflect
the new anisotropy default and were compared visually; private diagnostics are
under `build/ui-defaults` and `build_win/options-audit-20261026`.

### FORK-028 — Browser touch controls

**Status:** Implemented; browser acceptance recorded in WEB-011.

Adds keyboard-free browser input requested for iPhone, iPad and Android. Touch
mode defaults on for coarse touchscreens or touch devices without Pointer Lock;
an on-screen checkbox saves an explicit override independently of game files.
The saved **Touch layout** selector defaults to **N64 controller** for new or
unset preferences. Its right-hand cluster has a circular blue A, circular green
B, circular red Start and gray rectangular Z. Their colored fills use 50% alpha
beneath the existing highlight/shadow treatment so the game remains visible
through them, while white labels retain full opacity. This replaces the original
opaque fills, preserving controller colors and shapes. Z is now a much larger
upright rectangle at the right edge: 80 CSS pixels wide and 184 high at the base
size, or 72 wide on narrow phones. Start, B and A share a
vertical centerline at every size, with Start on top and A at the bottom. A gray
rounded rectangular **R** spans both columns above Start and Z, replacing the
former 64×48 placement beside Start (or above only Start on narrow phones).
It retains the same
ADS toggle action and pressed state, with a highlighted border while aiming.
Modern actions retains its Aim label. R shares the translucent gray controller
finish used by Z. All touch buttons, the gadget icon, gaps, labels and stick grow
with the shorter viewport dimension above 600 CSS pixels, capped at 150% of the
phone button sizes. A 1024×768 tablet uses 128% button sizing; phone-sized
landscape viewports retain their base targets. R spans 152 pixels at the base
size, or 144 on narrow phones, before tablet scaling.
Initial R-only presentation validation: TypeScript, scoped formatting, production assembly/
allowlist and Chromium/Windows WebKit touch fixtures pass; portrait/landscape
captures and existing toggle/reset checks were reviewed. Native render inputs
were unchanged in that presentation-only update. Evidence is
`build_win/mobile-r-*.log` and private touch captures.
This replaces the former 104×48 horizontal Z; the stick, safe areas and remaining
button hit targets stay separate. Existing press/release and drag-to-fire/look
behavior is retained. On the Files screen the N64 cluster and Modern Select/Back
buttons are hidden, superseding the earlier upper docking that made the N64
buttons jump. Direct cursor/tap navigation and Import/Export remain available.
All controller buttons and the stick now stay hidden throughout startup,
Files/mission selection, briefings and cutscenes in both layouts. The full cursor
surface handles menu taps and permitted intro skipping. A browser-only
`portWebTouchContext` query uses existing stage/watch/live-input predicates;
gameplay controls fade in over 240 ms when native input becomes live, including
after later cinematics. Reduced motion disables the animation. The fully open
in-mission watch retains directional and action buttons for navigation/resume.
This supersedes controller arrows/A/Start on premission screens. Context or Files
changes release held actions/Aim and cancel old menu gestures, preventing a
release from selecting or skipping in a new context. Cinematics suppress hints.
The touch overlay never renders browser Pause or Options in either layout or
menu mode; Start/Watch opens the native pause menu, while browser focus-loss
pause and Resume remain. This supersedes the original top Pause/Options pair.
Shared safe-area-aware insets now leave at least 28/36/40 CSS pixels on the
left/right/bottom. These replace the earlier extra 10-pixel right/bottom rule
and also inset the stick, gadget button and menu arrows. On 320-pixel phones,
a 96-pixel stick, tighter N64 column gap and higher menu arrows keep targets
separate. The Modern Watch button is inset at the upper left.
The gesture background now fills the viewport in gameplay and menus. Transparent
button-group containers and helper/status text pass touches through; only the
actual buttons and movement stick claim their visible control areas. This
supersedes the restricted right-side look rectangle, which excluded the top and
bottom and left dead space between controls. Owned movement is routed by pointer
ID at document capture phase, so walking, look/fire dragging and menu panning
remain independent when native pointer capture fails or the finger crosses a
button. Existing release, cancellation and focus-reset rules remain.

Instruction text sits eight CSS pixels below the top safe area and stays visible
for five seconds on pre-mission menus/Files, replacing the three-second menu
deadline. Missions, cutscenes and the in-mission watch have no instructional
helper text; this removes the former movement/watch banner, Drag to look label
and MOVE stick label. Timers follow
semantic context changes, preserving their deadline across ordinary playing
state updates; pause/resume and mode changes reset them. Generic mobile startup
instructions use this overlay instead of a duplicate app banner. Status/error
messages retain their message lifecycle but also pass input through.
Timed menu helper text and app status hints use the shared `HelperText` component
to fade out over 350 ms before removal. Reduced
motion shortens the opacity fade to 80 ms; hidden hints stop announcing at the
start of the fade. A new hint cancels pending removal.
Watch arrows now issue one digital direction edge per contact, with no repeated
hold or duplicate full-strength stick input in the watch. Fire/other holds and
analog movement retain their ownership. The Wasm bridge regression covers all
four directions held for 120 polls, duplicate downs and a subsequent short tap.
Options folder sliders (ranged rows such as Music Volume) are adjustable by
touch. Tapping one opens the native locked edit as before; the edit previously
needed D-pad/C left-right, the stick or the mouse wheel, none of which the menu
cursor surface sends, so a touch user could open it but never change the value.
While `port_sheet_edit_row` is open, `portSheetValueEditOpen` (src/game/front.c)
makes `portWebTouchContext` report a new value-edit context (4). The overlay then
shows − / + buttons at the lower left and Done (A) / Cancel (B, which restores
the opening value) at the lower right, and a sideways drag on the cursor surface
steps the value once per 18 CSS pixels. A tap still keeps the edit. Steps queue
through `portWebTouchStep` (bounded to ±40) and the open edit takes them each
tick via `portWebTouchTakeSteps`, a web-only hook in the edit loop. Unlike the
wrapping D-pad press, touch steps stop at either end of the scale, so a drag
past Music 100% stays at 100%; on the deferred Video sheet they pend like any
other step. Queued steps are dropped once the edit closes.
Cancel/B now also restores a deferred Video row's pending pick (Field of View,
Weapon FOV), which the edit previously left pending and Apply Changes would
apply; this fixes desktop and controller B as well as touch Cancel.
Sources: `port/web/touch.c`/`.h`, `src/game/front.c`, `touch-context.ts`,
`touch-input.ts`, `touch-menu.tsx`, `touch-controls.tsx`, `touch-hints.ts`,
`touch.css`, runtime types, the context/bridge/unit tests and the touch fixture,
plus the private real-ROM `web/tools/touch-slider-audit.mjs`. Validation: the Wasm
bridge regression, 156 browser unit tests, TypeScript, the Edge touch fixture
(−/+, drag, tap-to-keep, Done/Cancel) and Edge's real-ROM audit (Music 50 → 55
via −/+ and Done, a long drag stopping at 100, Cancel restoring 100, a leftward
drag stopping at 0) pass; the gallery regenerated with zero changed images.
The other Options folder states a menu tap could not answer now have touch
controls too, each reported as its own native context ahead of the value edit:
an open pick list (5, now `portMenuPickListOpen`) shows ▲ / ▼ and Cancel, and the
list takes the step queue as rows to scroll (three per tap, clamped like the wheel;
the next tap picks by hover even where the finger last rested). A bind row's
capture (6) previously soft-locked touch, since only B, Delete or a three-second
hold ended it; it now shows Cancel (B) and Unbind, which `portWebTouchUnbind`
latches and the sheet reads as its Delete key. Input Device's wait (7) shows a
Cancel that `portWebTouchTakeBack` delivers to the wait, which listens to real
devices only, instead of its ten-second timeout. Both waits hide the cursor surface.
Latched steps, Unbind and Back are dropped once their screen closes. On the Files
screen the Erase prompt (upstream: defaults to cancel, switched only by left/right)
now follows the cursor and a pointer press on cancel or confirm answers with that
word, on desktop as well as touch; a press elsewhere keeps the current choice.
Its hit boxes (`port_erase_box`) are recorded where the words draw. In the watch,
a touch Left/Right held past about 0.3 s (`portWebTouchHeldSide`, read only by
`watch_adjust_volume_slider` in `src/game/options.c`) moves a volume bar like a
held D-pad; other watch screens keep one step per contact. Sources add
`src/game/options.c` and `web/tools/touch-options-audit.mjs`. Validation: the Wasm
bridge regression (contexts, list steps, latches, held sides), 201 browser unit
tests, TypeScript, the Edge touch fixture and Edge's real-ROM Options audit (Max
FPS list scrolls to its end and closes, a rebind row cancels, re-arms and
unbinds, Input Device's wait cancels at once, Erase taps answer Cancel, Confirm
and Cancel) pass, and the gallery regenerated with zero changed images. The
watch volume hold and gamepad captures have no real-ROM run.
A follow-up covered the menus outside the Options folder, and AGENTS.md now has
an Input idioms rule: every screen must work with keyboard, mouse, gamepad and
browser touch, prompts name the control the current input has, and an input left
unsupported is recorded here. 007 mode's sliders (Rare's idiom: hold A and move
the cursor) report a hold context (8, `portMenuHoldDrag`); there the cursor
surface holds A while a finger rests, starting 80 ms after touch-down so the
screen picks the touched bar first, and releases it on lift, so dragging along a
bar sets it (before, only a tap set it, and dragging did nothing). Multiplayer
setup's drop-down (weapon slots, Load Set, Load Settings, limit values) now joins
the pick-list context through `portMenuPickListOpen` (which replaces
`portSheetPickListOpen`), scrolling by touch steps and closing with Cancel.
Screens only a pad drives - multiplayer Player Options, Health, Teams and the
character picker, which have no cursor hit-testing - report a pad context (9,
`portMenuPadOnly`): the overlay hides the cursor surface and shows a D-pad with
A, B and Start, and an arrow there is a D-pad press only, without the one-frame
stick step menus otherwise get, which those screens would count twice. Before,
Player Options soft-locked a touch player on its first row. Player Options'
seating wait skips a seat on touch Cancel (`portWebTouchTakeBack`), as player 1's
key or click does, instead of its ten-second timeout. The rebind prompt reads "Tap
Unbind or Cancel" while touch controls are on (`portWebTouchEnabled`). The Erase
prompt's boxes are padded 10 native pixels and split halfway where cancel and
confirm would meet. `GE_MENU=k007` opens the 007 sliders for Dam (harness only).
Validation: the Wasm bridge regression (hold and pad contexts, D-pad-only arrows,
the enabled flag), 201 browser unit tests, TypeScript, the Edge touch fixture
(hold timing, pad buttons) and the Edge real-ROM audit (a drag sets Enemy Health
to 1000% and then about 250%, a tap still sets a bar, the weapon list scrolls and
closes, Player Options skips its seat on Cancel and Down moves player 1's row,
the Erase boxes do not overlap) pass; the gallery regenerated with zero changed
images. Not changed: leaving the Health screen still needs every player to
confirm (or L/R); a tap outside a multiplayer drop-down still picks its highlight,
as a mouse click does.
The name keyboards (Save Config / Layout As, Save Set As, Save Settings As) now
also take the phone's own keyboard. Their TYPE cell only starts SDL text input,
which never brings up a phone keyboard. While one is open, `portOskWebOpen` makes
the touch context report text (10); the overlay keeps the letter grid tappable and
adds a Phone keyboard button. Its tap focuses a field (`web/src/touch-text.tsx`)
that docks to the top of the visual viewport, above the phone keyboard, and shows
the keyboard's title, the name, Done and Cancel; the game canvas is never resized.
The field sits outside the overlay's gesture-suppressing surface so its taps and
focus behave normally, and it stops key events so typing never reaches the game's
bindings. Each edit rewrites the whole name (`portWebTextClear`/`Append`, one
character code at a time, so the runtime needs no string helpers), which keeps
autocorrect and swipe typing working; `portOskWebAppend` applies the typed
keyboard's rules (printable ASCII but '|', '<' and a leading '#', 21 characters),
mirrored by `sanitizeName` in `touch-text-bridge.ts`. Done/Enter and Cancel set a
request (`portOskWebFinish`) that the next `portOskTick` applies with the grid's
OK rule and sounds; hiding the keyboard keeps the text and returns to the grid.
The field follows the native title and name while not focused, so a "NAME IN
USE" retitle shows. Player Options' profile keyboard is a separate per-panel
implementation and still uses the on-screen D-pad. Validation: the Wasm bridge
regression (text context, character reads, rewrite, finish, closed keyboard), 206
browser unit tests (including the name rules and bridge), TypeScript, the Edge
touch fixture (launch, focus, title, '|' dropped, whole-name rewrite, no key events
reaching the game, Done, Cancel, Enter) and Edge's real-ROM audit (a New Set's Save
As typed "Touch Set" through the field and saved `Data/WeaponSets/Touch Set.gewset`)
pass. How the phone keyboard covers the screen is unverified: emulation shows no
soft keyboard, so iOS Safari and Android Chrome need a device check.
Gameplay buttons distinguish taps from looking: A/B/R/Start, Gadget, More and
discrete Modern actions activate only on tap-up within a 10 CSS-pixel slop.
Moving farther latches a look gesture, so returning to the starting point cannot
activate the button. Drags continue across other controls by contact ID. Fire,
Sprint and Zoom retain immediate held behavior; right Fire retains drag-look.
Watch-menu buttons keep their existing digital navigation rather than looking.
Cancellation, lifecycle changes, disappearing gadgets and removed More buttons
cannot activate stale taps. Focused sources are `touch-button-gestures.ts`, its
unit tests, `touch-controls.tsx` and `touch-button-audit.mjs`.

Dragging the move stick well past its ring sprints, as holding the Sprint button
does. Sprint engages once the finger is farther from the stick's centre than
0.7 of the stick's width (the ring's edge is 0.5), in any direction, and stops
when the finger comes back inside 0.6, so a thumb resting at the boundary cannot
flicker. While it sprints, the stick and its knob brighten. It holds the same
Walk / Sprint bind natively, but only while the Sprint setting is on, so pushing
farther never becomes the half-speed walk; the Sprint button keeps holding its
bind either way. Releasing the stick, a reset or leaving gameplay ends it. The
thresholds are `STICK_SPRINT_ENGAGE` and `STICK_SPRINT_RELEASE` in
`web/src/touch-input.ts`; the native hook is `portWebTouchStickSprint` in
`port/web/touch.c`. Checks: the Wasm bridge regression (Sprint on/off, button,
release, reset), unit tests for the thresholds and hysteresis, and the touch UI
fixture audit (engage, visible state, release, lift).

A second gray Z trigger appears above the left stick in both layouts, beside the
conditional gadget shortcut. It fills the row when no gadget is available. Its
contact fires immediately without moving the camera; the right finger can look
from free space or a button. Both Fire contacts share an independent hold, so
releasing either trigger cannot stop the other. The row follows tablet scaling
and safe-area insets. Modern Watch moves clear of that row on short landscapes.
Sources: `touch-controls.tsx`, `touch.css` and touch layout/contact audits.
Validation: 173 unit tests, TypeScript, scoped formatting, production assembly
and exact artifact verification pass. Edge/Windows WebKit fixtures verify swipe
versus tap, independent trigger release, left-trigger aim isolation, hidden-target
cancellation and non-overlap through 1366×1024. The Modern More tray lifts clear
of left Z on narrow phones. Edge's actual-ROM audit holds left Fire while a
right-finger swipe from R turns and walks, preserving the existing aim latch;
watch navigation, focus/audio and rotation also pass. Native rendering inputs
are unchanged; gallery freshness/integrity pass. Private evidence:
`build/left-z-*.log` and `build/web-touch/left-z-*`. Physical iPad/Safari checks
remain distinct from browser emulation.
Controller/fade refinement validation: TypeScript, 167 unit tests, formatting, optimized Wasm build,
ROM-free logo exclusion and exact public-artifact verification pass. Edge and
Windows WebKit fixtures pass five-second hints/fades, gadget action and target
geometry through 1366×1024. Edge's actual-ROM audit reads watch indices through
an optional private heap observer and matching linker map: all four arrows move
one item and remain stable throughout 1.2-second holds. Gameplay, focus/audio and
rotation pass. Both gallery passes reproduce all 107 PNGs unchanged; standards
and freshness checks pass. Sources include `helper-text.tsx`/`.css`,
`touch-controls.tsx`, `touch-hints.ts`, `touch.css`, `port/web/touch.c`,
`test_web_touch.c`, and `touch-watch-audit.mjs`. Evidence remains private under
`build/web-touch/refinement-*` and `build/touch-refinement-*.log`; physical
iPad/Safari acceptance is not inferred from emulation.
Touch regressions exercise both ends of the enlarged Z hit target.
The controller background rule also wins over generic button-hover styling,
so hovering or holding a button keeps its translucent controller color.
These use existing raw native A/B/Start/Z button bits in gameplay and the watch,
respecting the game's controller mapping. Aim remains a toggle during gameplay;
the left analog movement stick and right-side drag-to-look remain. A gadget-icon
**Switch gadget** button calls the existing next-inventory action, replacing
the gun-icon weapon-cycle shortcut; A still cycles weapons in N64 mode. The
Modern More tray retains both weapon and gadget cycling actions.
The primary gadget shortcut appears only when the live inventory contains an
item on that cycle action's existing usable-gadget whitelist. A browser-only
`portWebTouchHasGadgets` query shares that whitelist, excluding guns, keys and
documents and hiding the shortcut outside gameplay or while driving the tank.
Availability is read each frame so item gains/removals update immediately. Losing
the last usable gadget releases that action alone, preserving held Fire, movement
and other contacts. Unknown/older runtimes conservatively hide the shortcut.
Availability/quiet-HUD validation: TypeScript, 168 unit tests, the Wasm bridge,
scoped formatting, optimized Wasm/site build and exact artifact checks pass.
Edge and Windows WebKit fixtures verify empty/restored inventory, hiding while
held without releasing Fire, quiet gameplay/watch and retained five-second menu
hints. Edge's actual-game audit verifies the quiet mission HUD and shortcut
visibility against the live getter, plus watch directions, movement/fire, audio,
focus recovery and responsive bounds. Both gallery passes leave all 107 PNGs
unchanged; standards and freshness pass. Evidence is private under
`build/mission-ui-*.log` and `build/web-touch/mission-ui-*`.
Dragging right Z turns while firing; left Z leaves aiming to the other finger.
**Modern actions** restores the previous Fire/Aim/Use/Reload/Crouch/Watch layout,
Select/Back menu buttons and More tray with weapon/gadget cycling, zoom,
detonation and sprint. Below 361 CSS pixels that tray uses three columns.
Both layouts retain directional watch controls and direct menu taps. Layout
changes release held buttons, movement, look and latched Aim; the preference
persists separately from game files and safely defaults to N64 if unavailable.
Touch mode bypasses mouse capture; keyboard/mouse mode keeps its existing policy.
Touch-enabled browser startup inserts `mouse_auto_aim=1` when absent, preserving
explicit saved On/Off values and the game's other auto-aim rules. Mobile defaults
fit the visible viewport to the existing 16:9 presentation and multiply by full
device pixel ratio (including 3x), rather than always rendering at 960x540.
A one-time browser-preference migration upgrades exactly that old resolution;
custom dimensions and explicit render-quality settings remain. The migration
marker is saved only after user-file synchronization succeeds. Fresh settings
receive the mobile defaults even if a previous migration marker remains.
The browser-only SDL window-fit bypass prevents CSS display bounds from clamping
the physical-pixel framebuffer. Touch-enabled browser windows also disable SDL resizability,
which otherwise replaces requested render dimensions with the canvas's CSS size;
the frontend owns responsive presentation. Both hooks use the browser-only
`GE_WEB_FIXED_FRAMEBUFFER` startup flag; desktop browser/native fitting stays intact.
CSS presentation and pointer coordinates retain their existing scale; rotation
fits the framebuffer without selecting a new render resolution. The separate
introductory room keeps its existing pixel-ratio cap.
Controls respect safe-area insets and the game's aspect; browser panning is
disabled on the game surface while touch mode is enabled. Layout follows the
smaller visible/layout viewport during rotation or browser-chrome changes.
Pointer gestures resume the pinned SDK's suspended SDL Web Audio context.
Canvas-only blur from touching native on-screen buttons no longer pauses touch
play; window blur and document hiding still pause/reset input. Touch pitch uses
the same Upright/Reverse policy as mouse look instead of bypassing inversion.

The menu touch surface forwards cursor motion without SDL's immediate synthetic
mouse press. A stationary release selects through the native Accept action;
movement beyond 10 CSS pixels cancels selection for that entire gesture, even
when the finger returns to its starting point. Arrows and native A (Modern Select)
remain available in the watch.
Non-passive touch/gesture cancellation on the overlay supplements `touch-action`
to prevent repeated Fire taps from zooming Safari; launcher zoom remains available.
The active overlay also owns gaps between controls so near-button taps cannot
fall through to SDL or bypass gesture cancellation.
Contact ownership now releases at document capture phase on pointer up/cancel
and owner capture loss. An empty touch stream recovers missing pointer terminals;
fresh downs reclaim stale uncaptured owners. Page hiding, pagehide, rotation,
pause and mode changes clear contacts and release captures. This closes the
missed-release path that could leave the movement owner and analog vector latched;
the exact interruption behind the reported physical-iPhone incident is unconfirmed.

`port/web/touch.c` owns player-one input, bounded motion and reset state, maps
semantic actions by existing binding names and combines with the ordinary game
input path. Small browser-only hooks in `libultra_shim.c` and `bondview2.c` merge
button/actions and analog movement/look. Keyboard bindings and saved action IDs
do not change. Reset on pause/focus loss, pointer cancellation, rotation, mode
changes and room return prevents stuck input; look is discarded during locked
controls. The module is selected only by the CMake web video target.

**Sources:** [touch state](web/src/touch-input.ts),
[layout setting](web/src/touch-layout-setting.tsx),
[launcher settings](web/src/app.tsx), [player guide](README.md), [web guide](web/README.md),
[contact lifecycle](web/src/touch-contacts.ts), [menu gestures](web/src/touch-menu.tsx),
[hint lifecycle](web/src/touch-hints.ts),
[native context decoder](web/src/touch-context.ts),
[context unit tests](web/src/touch-context.test.ts),
[menu tap tests](web/src/touch-menu.test.ts),
[overlay](web/src/touch-controls.tsx), [layout](web/src/touch.css),
[viewport fitting](web/src/touch-viewport.ts),
[mobile settings defaults](web/src/browser-settings.ts),
[SDL framebuffer hooks](port/fast3d/gfx_sdl2.cpp),
[capture](web/src/capture.ts), [browser capability gate](web/src/engine.ts),
[native bridge](port/web/touch.c), [bridge header](port/web/touch.h),
[bridge regression](scripts/port/test_web_touch.c),
[bridge runner](scripts/port/test_web_touch.sh),
[UI fixture](web/tests/touch-driver.tsx), [fixture page](web/tests/touch.html),
[fixture build](web/tools/build-touch-test.ts),
[UI audit](web/tools/touch-ui-audit.mjs),
[context transition audit](web/tools/touch-context-audit.mjs),
[N64 layout audit](web/tools/touch-n64-audit.mjs),
[gesture-surface audit](web/tools/touch-surface-audit.mjs),
[private game audit](web/tools/touch-game-audit.mjs),
[Linux game runner](web/tools/linux-game-audit.sh),
[Pages checks](.github/workflows/web-pages.yml),
[web CMake](cmake/emscripten.cmake), [focus reset](port/web/lifecycle.c),
[input hook](port/src/libultra_shim.c), [movement hook](src/game/bondview2.c),
[accepted extension](scripts/port/web_target_plan.md),
[milestone](scripts/port/web_target_progress.md).
**Context visibility validation:** Synthetic Wasm tests cover every combination
of stage/watch/live-input flags. TypeScript, 163 unit tests, formatting,
production assembly/allowlist and Chromium/Windows WebKit fixtures pass. Both
layouts hide controls in navigation/cinematics, clear held actions and stale
menu gestures on transitions, retain watch controls and respect reduced motion.
Chrome real-ROM acceptance reaches Dam using cursor-only menus, observes zero
buttons before gameplay, checks the engine-ready fade and verifies movement,
look/fire, Watch, hint deadlines, focus resume, audio and rotation. Gallery
regeneration changes zero image cells; freshness/integrity pass. Private evidence
is `build/web-touch/mobile-context-*` and `build_win/mobile-context-*.log`;
physical iPhone remains a device check.

**Gesture/hint refinement validation:** The new free-space grid check fails on
the previous layout with 113 dead sample points in its first wide gameplay
case and passes after the fix. Chromium/Windows WebKit fixtures cover both
layouts in gameplay/menu/Files, non-primary and uncaptured cross-button drags,
release/reset, top hint placement, three/five-second deadlines and playing-state
updates that must not restart them. Chromium's native multi-contact stream
keeps walking through repeated right-thumb drags at the top/bottom and N64 grid
gaps. Chrome real-ROM acceptance verifies bottom-edge look while walking/firing,
A/Start menus, Watch, actual five-second hint dismissal, focus resume, audio and
rotation. TypeScript, 161 unit tests, formatting, site assembly/allowlist and
native gallery freshness pass. Evidence is `build_win/mobile-look-*.log` and
`build/web-touch/mobile-look-*`; physical iPhone Safari remains a device check.

**Overlay cleanup validation:** TypeScript, 161 browser unit tests, formatting,
production assembly/allowlist and Chromium/Windows WebKit touch fixtures pass.
The fixtures check hidden Pause/Options, vertical Start/B/A, shared insets and
separate targets across 320/390/568/844/1024 widths, including short landscape.
Chrome's assembled real-ROM check passes Files cursor/taps, native A/Start
navigation to Dam, movement/look/fire, Start watch, focus pause/Resume, audio and
portrait/tablet rotation. The audit now uses Start to launch from mission briefing
directly rather than relying only on repeated A presses. Native gallery freshness
remains verified; no native rendering inputs changed. Private evidence is under
`build/web-touch/mobile-layout-*` and `build_win/mobile-controls-*.log`.

**Earlier mobile-defaults validation:** Edge and WebKit fixtures pass the 2026-10-28 Files hiding,
state release, restored controls
and additional margins. The mobile refinement passes 161 browser unit tests,
Windows/optimized Wasm builds, production packaging, desktop launcher storage
checks and actual-ROM Chrome/WebKit touch acceptance. Measured canvas/WebGL
dimensions match full-DPR startup requests and survive rotation; the emulated
iPad case is 2048x1152 at DPR 2. Edge's actual-ROM save import/export/restart
regression also passes. Physical iPad sharpness/performance remain unverified;
WEB-011 records local headless measurements and private evidence paths.
Native gallery regeneration and verification pass with zero changed image cells.
Earlier acceptance includes the synthetic Wasm bridge, TypeScript and unit checks. Edge and
WebKit ROM-free UI checks cover independent contacts, cancellation, menu actions,
mode changes and portrait/tablet bounds. Chrome real-ROM acceptance reaches Dam
through touch menus, verifies simultaneous movement/look/fire, watch,
pause/resume, Options and live audio. Optimized Wasm and all 275 strict-callback
translation units pass. Linux WebKit additionally passes real-ROM menus,
thumbstick, Watch, Pause/Resume, Options, audio and layout integration using its
documented single-pointer/resize-driver accommodations. Native ADS/HUD/wardrobe/upstream-merge/state regressions
pass; isolated native gallery regeneration changes no image cells. Physical
iOS/Android acceptance remains distinct from these automated checks.
The focus regression additionally dispatches canvas blur on every on-screen
pointer press: Chrome and audio-enabled Linux WebKit still reach Dam, move,
use Watch/Pause/Options and retain audio/layout. Chrome covers simultaneous
move/look/fire and genuine window-blur pause. The C fixture checks both pitch
directions; native gallery regeneration changes zero image cells.
The earlier primary weapon-button refinement passed 144 unit tests, TypeScript,
production assembly/allowlist and Edge/WebKit pointer UI checks. Those checks
cover repeated next-weapon press/release without opening More, retained +/−
buttons, hidden gameplay actions in menus and non-overlapping phone/tablet
layouts, including 320-pixel portrait. No native/render inputs changed.
The iPhone gesture corrections pass 148 unit tests, TypeScript, production
assembly/allowlist and Chromium/WebKit UI regressions for rapid Fire taps,
cancelled touch defaults, menu tap versus drag, missing pointer terminals,
descendant capture loss, document releases and successful stick reuse. These
automated checks do not establish physical iOS acceptance.
Chrome real-ROM touch acceptance also passes menus into Dam, simultaneous
movement/look/fire, Watch, pause/resume, Options, audio and portrait/tablet bounds
against the assembled frontend with the existing optimized runtime.

The N64-default refinement passes 154 unit tests, TypeScript, production
assembly/allowlist, native A/B/Z/Start tap-bit regressions in both game and menu
modes, and Edge/WebKit fixture checks for colors/shapes, saved layout reload,
layout changes while inputs are held, both menu layouts and phone/tablet bounds.
Chrome actual-ROM acceptance passes N64 A navigation into Dam, simultaneous
movement/look/Z, Aim, Start watch, Pause/Resume, Options, audio and portrait/tablet
layouts. Private evidence is `build/web-touch/n64-{edge,webkit,game}`. The existing
optimized runtime is reused; no native render inputs change and gallery freshness
verifies. These checks do not establish physical iPhone acceptance.

**Touch-cancel check under load (2026-11-06).** CI run 42279844036 failed
`touch-ui-audit.mjs` once while the room audit's software WebGL loaded every
core: right after a CDP `touchCancel` the fixture still reported the stick at
`[0, 1]`. Investigated as a possible real stale-movement bug: on cancel,
`touch-contacts.ts` ends the contact (from `pointercancel`, or the
`touchcancel` fallback) and the stick handler sends `move(0, 0)`; a later
`pointermove` is ignored once the contact is gone, and the per-frame monitor
never sends stick movement, so nothing can re-apply movement after a handled
cancel. Locally (Edge, 16 cores) the failure did not reproduce: 7 runs under
CDP CPU throttling at rates 1 to 40, 8 runs with 32 CPU-burning processes and
2 more with 24 all passed the cancel check, and a probe found that
`Input.dispatchTouchEvent` returned after the page had handled `pointercancel`
and `touchcancel` in all 600 rounds, idle and loaded. The remaining explanation
is the overloaded 4-core runner, whose input pipeline can acknowledge a touch
before the page handles it, letting the audit's snapshot overtake the queued
cancel; a device handles the cancel a moment later. So the audit, not the
controls, changed: every check that follows a dispatched touch in its CDP block
now waits up to 5 s for the expected state (`settled`), keeping its failure
message and reporting the last state, and the fire press is confirmed before
the simulated focus loss so a late press cannot follow it. The touch audit
passed 3 runs idle and 2 under load afterwards. Not reproduced on CI itself,
since CI no longer runs the audits side by side (FORK-024).

Corner ammo stays visible: the touch buttons rise above it. On a 4:3 iPad the
game's 4:3 HUD reaches the window corners, so the N64 cluster (or Modern
actions) covered the right-hand ammo icon and counts, and the stick a dual-wield
left-hand counter or a gadget name; on phone landscape the 4:3 band already kept
them clear. During gameplay and the watch the overlay tells the runtime which
bottom corners its controls cover (`portWebTouchHudCorner`). `gunfire.c`
brackets the right-hand ammo, the left-hand ammo and the gadget name with a
renderer tag (`portHudTouchClearTag`, G_NOOP `0x5F`,
`port/fast3d/hud_touch_clearance.h`). In a covered corner `gfx_pc.cpp` draws them
6 native pixels lower (3 at HUD Scale), so the row's lowest icon ends about 5
pixels above the bottom and the buttons need to rise less, and records where
each drew in full, after HUD Scale and HUD Centering. The overlay reads that box
each frame (`portWebTouchAmmoBox`, canvas fractions) and, where it overlaps a
corner's controls at rest, raises that corner by CSS variables
(`--touch-lift-left` for the stick and left Z/gadget row, `--touch-lift-right`
for the N64 cluster or Modern actions, with the More tray following) until their
bottom is 8 CSS pixels above the ammo's top (`web/src/touch-hud-clearance.ts`).
Ammo beside or wholly below the controls moves nothing. A context, layout,
gadget, More, Files or viewport change measures the controls again; for 30
frames after it a lift may also fall, and afterwards lifts only grow, so a
shorter icon, a weapon switch or a hidden counter never drops the buttons.
Menus, cutscenes and switching touch off uncover the corners: the drop ends, the
recorded boxes are forgotten and the controls return to rest. Desktop frames are
unchanged. This supersedes the first version of this fix, never pushed, in which
the renderer lifted the ammo above the buttons; the owner preferred the ammo to
keep its place. Not changed: other bottom HUD (pickup messages, mission timer).
Shared hooks for later merges: the three tag pairs in upstream
`generate_ammo_total_microcode`, and `gfx_draw_rectangle`'s new step beside the
text-offset tag. Sources also include `port/web/touch.c`, `port_hud_ammo.c`,
`port_hud.h`, `gfx_api.h`, `touch-controls.tsx`, `touch.css`, `runtime-types.ts`,
the touch fixture, `web/tools/touch-clearance-audit.mjs` and
`scripts/port/test_hud_clearance.c`. Validation: `test_hud.sh`, which now also
runs that check (box union, canvas fractions, pillarbox, empty box, drop range),
the touch bridge regression compiled with host gcc and a stub `emscripten.h` (the
pinned emsdk image was not available locally), 226 browser unit tests,
TypeScript, and the Edge touch UI fixture audit (at 1024×768 the N64 cluster,
the stick with its left Z row and the Modern actions rise to 8 pixels above a
reported box and stay up when it disappears; a new layout starts at rest; ammo
beside the actions at 844×390 lifts nothing; leaving play uncovers both corners)
pass; an isolated Windows build (`build_win/touch-clearance`) compiles, and the
render gallery regenerated from it with zero changed images. Not yet run: the
browser build with the real ROM, or a physical iPad, so the drop's look and the
lift's size on a device are unreviewed.

### FORK-026 — Upstream 1.0.2b integration

**Status:** Implemented; local acceptance passes.

Merged upstream `9062f9f120b7f64d39e5db59500ed50e8e7a6686` into the fork
based on `dde3aaa63b8aeb427d9212f0521ae12e8b98cd3f`. This deliberately advances
the comparison baseline from 1.0.1 `6be5920beed453b6a81fdf9935fb9851928b718d`
to 1.0.2b. The original fork history is retained by a merge, rather than copied
or squashed upstream patches.

The imported audio retry/pacing rewrite, high-tick aiming/zoom/effect fixes,
collision fixes, startup swirl fix, controller improvements, Project64 EEPROM
import, optional Weapon FOV/HUD Scale/armor pickup HUD, setting help, FPS display,
fast watch and new watch up/down controls are **upstream work**. Their inherited
source, changelog, ABI guide, launcher/settings documentation and expanded
dependency notices remain attributed to upstream. The browser's supported
campaign-backup boundary does not gain raw EEPROM upload UI from that import.

Fork resolutions preserve ADS/sprint, calibrated hands/arms, vector sights,
feedback HUD, mission/cheat hints, suspend points, fonts and web lifecycle/storage:

- Existing fork action IDs 21–25 retain their meanings; upstream watch up/down
  append at 26/27 in both action tables. Keyboard defaults are Up/Down.
- Optional Weapon FOV retains upstream hip-fire presentation and projectile
  placement. Its projection blends back to the world projection during ADS,
  preserving the fork's fully raised sight calibration and watch face mapping.
  The near-depth hand/sleeve pass receives the same selected gun projection,
  retaining its XY aim correction instead of overwriting it with the world view.
  Weapon FOV remains off by default. HUD Scale scales vector sight strokes
  around the same aim point; it remains off by default.
- The vector sight path supersedes upstream's sprite/texel construction; no
  ROM-derived target table is embedded. Enhanced font upload remains before
  upstream's dynamic ordinary-texture buffer allocation.
- Audio uses upstream retry/pacing instead of the former permanent-failure flag.
  Browser metrics report retry failures, and pause/resume/suspend restore clear
  queue, field/clock credits, stall tracking and drift trim before restarting.
  Instrumentation records one execution per mixer frame.
  SDL's browser fallback retains the pre-merge 512-sample period through
  `port/web/audio_config.h`; upstream's new 256-sample native period gives only
  5.3 ms at a 48 kHz Web Audio context and caused audio-block discontinuities.
  FORK-041 supersedes ScriptProcessor as the normal browser output.
  Native device timing stays upstream-owned. Private real-ROM audio-quality
  tooling records rendered samples in an AudioWorklet, downstream of the output,
  rather than treating nonzero callback samples as continuity proof.
  It checks idle Dam, Watch, pause silence and sound recovery; WAV evidence stays
  local. ROM-free continuity-metric regressions run with the web unit suite.
  The audit asserts native Watch state and holds its P-key pulse across input
  polls so a same-tick key-down/up pair cannot silently skip that coverage.
- The fork's uppercase `README.md` supersedes upstream's lowercase filename,
  avoiding a Windows case collision while retaining fork setup and identity.
  The fork's mission timer stays off by default with cheat/best-time detail;
  other deliberate fork defaults remain unchanged.

**Sources:** [upstream changelog](CHANGELOG.md),
[upstream ABI guide](docs/port/abi-and-portability.md),
[input](port/src/input_pc.c), [actions](port/src/joy_port.c),
[audio](port/src/audio_pc.c), [gun projection](src/game/gunfire.c),
[browser audio period](port/web/audio_config.h),
[audio quality audit](web/tools/audio-quality-audit.mjs),
[render-thread probe](web/tools/audio-quality-worklet.js),
[continuity metrics](web/tools/audio-quality.mjs),
[continuity regression](web/tools/audio-quality.test.mjs),
[sight policy](port/src/port_sight.c), [renderer](port/fast3d/gfx_pc.cpp),
[settings](port/src/video.c), [option help](port/src/port_option_help.c),
[INI help](tools/midas_ini_help.txt), [notices](tools/THIRD_PARTY_NOTICES.txt),
[integration regression](scripts/port/test_upstream_merge.c),
[regression runner](scripts/port/test_upstream_merge.sh),
[arm contracts](scripts/port/arm_rendering.md),
[visual audit](scripts/port/audit_ads.ps1),
[native CI](.github/workflows/windows-release.yml).
**Validation:** Native Windows build and optimized Wasm build pass; strict Wasm
callback compilation passes 274/274 C units. ROM-free ADS, wardrobe, HUD, mission
cheat, suspend, image-bank and collision checks pass. New integration checks cover
Weapon FOV/ADS/watch projection and vector sight visibility/colour/HUD scale;
ADS tests lock action IDs and exercise upstream watch navigation. Actual-ROM
native suspend write/restore/relaunch passes. Windows and Linux video builds
pass. Fresh Clang recovery reproduces all 205 generated declarations exactly.
The final gallery passes two independent processes for 72 views and 35 icons,
with zero changed image cells. Weapon FOV at wide FOV 110 passes PP7/AR33
raise/lower and dual-PP7 captures; wrist joins and sight alignment were reviewed.
Edge passes real-ROM insertion/flight, Dam movement/audio/watch/focus, durable
reload and cached automatic startup, plus native Exit Game/retreat/warm resume
and save-management restart. Chrome passes uninterrupted cutscene-to-control,
watch/capture/focus recovery, loading UI and first/cached shader preparation
before graphics. Linux Firefox passes actual campaign/profile/settings backup,
reload/restore, ROM recall/removal, Dam capture/audio/focus/watch. TypeScript,
119 browser unit tests, Python tooling checks and artifact allowlist/hash policy
pass. Safari remains experimental. Private evidence is in `build/merge` and
`build_win/hand-gallery-337riqu0`; local checks are not a publication claim.
Audio follow-up: the 48 kHz Chrome comparison found 34 sample jumps above 0.15
over 12 seconds in the 256-sample build, all on 128-sample render boundaries.
The pre-merge and restored 512-sample builds had zero such jumps. Final Chrome
has no silent gaps; Edge's final run has two isolated 128-sample silent quanta
(2.7 ms each), with zero large sample jumps. Watch/pause/resume checks
pass in both browsers, including exactly silent paused output. TypeScript and 130 web unit tests
pass; optimized Wasm and native Windows builds pass. Fresh independent gallery
passes change no image cells. Private evidence is in `build/web-audio`.

### FORK-027 — Small upstream hooks and focused fork modules

**Status:** Implemented.

Repo guidance now prefers focused fork/platform files and small explicit hooks
in upstream-owned code, preserving upstream layout and names and avoiding
unrelated formatting/refactors. Separation is conditional on a clear boundary
and equivalent behavior; it must not add needless indirection or weaken ABI or
render contracts. This is fork-local guidance outside the managed standards.

Applied that rule during FORK-026: PC sight policy moves from `gunfire.c` into
`port/src/port_sight.c`, using the existing fork HUD interface; the original N64
path stays in place. Browser shader preparation moves from `gfx_pc.cpp` into
`port/web/shader_prewarm.inl`; SDL browser pause/metric functions move from
`audio_pc.c` into `port/web/audio_bridge.inl`. These two private includes retain
access to renderer/audio static state without exposing engine internals through
new public APIs. Their narrow include sites are intentional shared-file hooks.
The browser's parallel shader batch follows the same pattern:
`port/web/shader_parallel.inl` is included at the end of `gfx_opengl.cpp`
(FORK-025) and reuses that file's split shader build/finish steps.
The native audio-reset hook stays beside upstream pacing state; small metrics
hooks remain at validation, command execution and first nonzero output.
The common CMake port-source glob picks up the sight module automatically.

**Sources:** [agent guidance](AGENTS.md), [contributing](CONTRIBUTING.md),
[sights](port/src/port_sight.c), [HUD interface](port/include/port_hud.h),
[browser shader helper](port/web/shader_prewarm.inl),
[browser shader batch](port/web/shader_parallel.inl),
[browser audio bridge](port/web/audio_bridge.inl).
**Validation:** FORK-026 build, sight/projection, native render and browser
lifecycle checks cover these boundaries. No unrelated game subsystem is split.

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

Named this independent fork **Iron Sights** (formerly **Project Midas ADS**),
retaining its Project Midas upstream link, license/credits and documented aiming,
movement and HUD features. The repository is `pRizz/Iron-Sights` and its Pages
path is `/Iron-Sights/`. Current README/contribution/release/browser references,
repo-specific badge URLs, source/provenance links, ZIP name, Windows manifest,
native window/About/icon-preparation/startup copy, browser metadata/chrome/CRT
and gallery title use the new identity. The ZIP is
`Iron-Sights-windows-x86.zip`; its Windows player is `IronSights.exe`, superseding
the earlier `ProjectMidas.exe` filename. Linux/Steam Deck now produce `IronSights`,
superseding `ProjectMidas`; README, gallery capture and manual release bundles/
Deck launcher use that filename together. The `ge` target, internal map/resource
names and deployed Wasm filenames remain stable.
Project Midas fork attribution remains visible in native startup/About and
browser startup/footer. Older dated evidence below retains its historical name.

The reference-audit follow-up repairs the optional C# launcher's native artifact
discovery and messages, the AI linter's default Windows path, and the Natalya
vision rig's checkout root, executable/process guard and ignored log location.
The rig supports Windows/Git Bash and Linux, accepts `GE_TEST_EXE` for another
build, and propagates game failures. Portable controller/ROM/input comments now
refer to the game executable; Windows crash-debugging examples use its actual
filename. Browser package/plugin
names use Iron Sights. Existing upstream attribution, history, file-format and
storage identities remain intact; the inherited application icon is deliberately
unchanged for a later asset discussion. Room-model metadata is covered by FORK-025.
The [optional launcher](tools/GELauncher.cs), [AI linter](scripts/port/ai_lint.py),
[Natalya rig](scripts/port/nat_test.sh) and
[launcher regressions](scripts/port/test_native_tool_launchers.py) retain their
existing debug environment contracts. Local validation passes Windows, Linux
i386 and optimized Wasm builds, 148 browser tests, type checking, exact public
allowlist/digest verification, seven launcher subprocess tests, five distribution
tests and native Start/upstream checks. The C# launcher compiles and passes
Windows/Linux/missing-build discovery fixtures; the AI linter checks 18 lists
without errors. The current Windows ZIP has the renamed executable and corrected
shipped help, with every checksum verified. Edge room/startup audits pass; model
comparison proves only the controller generator label changed. Full manual
Linux/Deck packaging and physical Steam Deck execution are not claimed.
Private evidence is under `build/reference-cleanup`, `build/web-room/audit` and
the ignored gallery capture directory. Both independent gallery passes reproduce
all 107 PNGs byte-for-byte; the source fingerprint is refreshed and visual review
confirms the unchanged room and native presentation. Eight gallery workflow
tests pass. Release-path traversal/rooted-name rejection checks also pass.

The Windows filename follow-up updates the CMake output, release allowlist and
EXE attestation, first-launch worker copy/command, crash module name, shader/icon/
gallery/runtime audit tools and current player/developer instructions together.
The credits heading, prototype page, contribution heading, Data/Mods instructions,
settings help and notice headings identify Iron Sights. Credits explicitly say
it is a Project Midas fork, retaining the original developer, artwork and tester
attributions; inherited contribution guidance and license notices remain credited.
The font audit includes Credits and disables its render thread so a simulation
frame limit cannot finish before the requested rendered capture.

Validation passes native Windows and optimized Wasm builds, 146 browser tests,
type/site-policy checks and browser startup recovery. Five distribution tests
require `IronSights.exe` and reject a stale `ProjectMidas.exe`; the actual local
player ZIP has the new executable/instructions and every checksum verifies.
The renamed helper successfully prepares all 35 native images. Both independent
gallery passes reproduce all 107 game images unchanged, with fresh source metadata.
Credits captures fit both native and enhanced fonts, preserving every attribution.
The native Start/HUD/upstream regressions and eight gallery workflow tests pass.
Private logs/captures are under `build/ironsights-exe*` and
`build_win/font-audit-Credits-*`.

The browser's deployed ROM database, remember preference, user-files mount,
lock and backup-format identities are deliberately unchanged. Both Pages paths
share the `https://prizz.github.io` origin, preserving cached ROMs, campaign saves,
settings and old backups without migration code. The private
[rename audit](web/tools/rename-storage-audit.mjs) exercises old/new paths on one
origin, byte-preserving export/restore and remembered-ROM autostart. The accepted
web plan and WEB-010 track this follow-up and verification. GitHub repository
redirects preserve source links; former Pages paths are not assumed to redirect.

Rename validation passes Windows/optimized Wasm builds, 146 browser unit tests,
type checking, exact public allowlist/Wasm digest, native Start/HUD/ADS/upstream
integration checks and five distribution tests with the new ZIP/metadata names.
Edge's private real-ROM rename audit preserves exact EEPROM/settings exports
across both paths, reads the deployed ROM namespace and automatically starts the
remembered ROM at the new path. Windows UI review confirms the Iron Sights title,
Classic hub entry/back route and reachable Video 2/2 without a Classic tab.
The gallery's two independent passes reproduce all 72 views and 35 icons unchanged;
only its title and source fingerprint change. Diagnostics remain private under
`build/iron-sights` and `build_win/iron-sights-menu-audit`.

The README features the promo video directly under the fork's introduction, ahead
of the downloads, using YouTube's hosted thumbnail so no game image is committed.
It keeps a text link to the earlier ADS video, and replaces the former "Watch the
latest gameplay demo" section and its committed
`.github/images/iron-sights-gameplay-thumbnail.png`.
The README introduces the fork, puts the Windows ZIP download and three-step
setup straight after the video (FORK-018), prominently links this full
inventory near the top, and summarizes ADS, sniper zoom, sprint, smoother
sights/combat feedback, objective alerts, the weapon strip, adaptive
health/armor, solo suspend points, and mission timer/selection cheat-target
and earned-status hints. It also highlighted enhanced community fonts until
FORK-021 was removed.
Contributor inventory-maintenance instructions sit under
Contributing, alongside the existing contribution links.

**Sources:** [README.md](README.md),
[settings reference](tools/midas_ini_help.txt).
**History:** `9e46d0d`, `aefb55a`, `74ce634`, `5edcf1a`, `4e37864`.
**Verification:** README video/image targets and documented settings.

### FORK-003 — ADS controls, movement and weapon alignment

**Status:** Implemented.

Throwing knives now also support single-hand ADS. **Gameplay > Iron Sights > Aiming and Movement
> Knife ADS** selects **Butt Centered** (default, `[display] knife_ads=0`) or
**Tip Centered** (`knife_ads=1`) in the frontend and watch. The simple style
retains the stock blade grip/orientation and depth, translating the handle butt
onto the center ray. The advanced style moves the knife within the unchanged
native hand into a handle grip, centers its actual tip and exposes a little blade
face. Its view distance leaves the grip visible. Both styles use continuous,
simulation-time raise/lower transitions and the existing hold/toggle/dedicated
ADS controls. Red-reticle aiming, dual wielding and other thrown items retain
their inherited behavior.

The tip style flips back into the native blade grip during the original wind-up;
releasing aim cannot unwind a committed throw. Native release timing, ammo,
gravity/upward launch bias and inherited player velocity remain authoritative.
The tip is a direction reference, not a promise of impact at every distance.
Recovery returns to the requested aim state; switch/watch/death/input reset clear
transient presentation per player. Focused `port_knife_ads.c` owns settings,
presentation and temporary knife-only matrix-list copies. Small existing fork
hooks in `joy_port.c`/`gunfire.c` connect it; no model renderer refactor is needed.
Original knife/hand vertices, UVs, materials and display lists remain immutable,
and temporary node pointers are restored after drawing. Unknown list layouts
fall back to the native draw. `port_knife_ads_audit.c` exercises real native throws
only under the private `GE_ADS_AUDIT=3` harness.

Knife regressions run inside `test_ads.sh` and cover defaults/persistence,
projection, rigid grip transforms, four tick rates, reversals, committed-throw
interruption/recovery, player isolation/input reset, immutable lists and partial
patch rollback. The ADS test disables compiler builtins to catch legacy-header
ABI discrepancies that native Windows exposes. Early native captures found that
issue in float min/max calls; the final bounded interpolation uses plain C.
Native simple/tip raise/lower is captured and reviewed in wide/4:3, with alternate
weapon FOV/HFOV 110 at 120 Hz. Wide native throw captures include aim release
during the second wind-up. A no-reserve case verifies the last knife releases and
auto-switches cleanly to the pistol. All 20 existing firearm captures still pass.
Native Windows/optimized Wasm builds, all 278 strict callback compilations,
ADS/wardrobe/HUD checks, five Linux distribution/metadata tests and 146 browser
unit tests pass. Edge renders both actual-ROM styles and its native menu's
touch-accept input changes Butt Centered to Tip Centered; the saved choice
survives a cached-ROM reload. Gallery integrity/eight workflow tests pass and
both independent passes reproduce all 72 views/35 icons without changed pixels.
Private evidence is under `build/knife-ads`; raw captures/ROM data are not staged.
Main sources: [knife implementation](port/src/port_knife_ads.c),
[interface](port/include/port_knife_ads.h),
[throw audit](port/src/port_knife_ads_audit.c),
[regressions](scripts/port/test_knife_ads.c),
[pose/validation guide](scripts/port/knife_ads.md), shared option/help tables,
frontend/watch settings and [capture driver](scripts/port/audit_ads.ps1).

The rocket launcher now joins the supported ADS weapons. At the owner's
2026-11-02 request, its near and far pins are stacked on the centered ray again,
using the initial -0.026732 pitch and 6.143 sight height. The launcher is canted
0.575 radian (about 33 degrees, raised from 0.40 after a play-test) about that
sight line (`portAdsWeaponRoll`), so the tube rests on
the right shoulder as if the head tilts behind the sights. It stays at its hip
depth (eye distance 14.5, matching WeaponStats PosZ) instead of travelling out to
60 units, and its ADS keeps the unzoomed field of view (`portAdsWeaponFovScale`
is 1.0 for it and, since a later request, both shotguns; 0.875 for the grenade launcher; 0.75 for other guns). ADS therefore rolls the launcher into line
without seeming to push it forward. This supersedes
both the upright hold, which looked aimed into the face, and a brief shoulder
offset with inward yaw, which no longer lined up the pins. The intermediate
refinement described next centered only the native near sight's aiming pin,
using a -0.005 radian pitch, 7.040 sight height, launcher-only -0.3 lateral
offset and 60-unit eye distance. This tilts the loaded
muzzle down about 1.25 degrees from the initial -0.026732/6.143 pose, which
centered both sight pins but let the rocket nose overlap the aiming reference.
The near pin now has clear background above the farther sight and loaded nose.
This supersedes the original two-pin alignment while keeping the long rear tube
ahead of the eye and preserving original model/sight geometry, materials and
the loaded rocket. It shares the existing hold/toggle/dedicated controls and
smooth transitions; dual wield, tank controls,
reload/switch gates and native projectile behavior are retained. The existing
rigid grip renderer supplies trigger and tube-support hands (FORK-004).
The focused [rocket regressions](scripts/port/test_rocket_ads.c) check the near
pin against the center ray, farther-pin/nose/rear-tube clearance, aim modes,
exclusion gates and four tick rates. The
[alignment guide](scripts/port/ads_alignment.md) records the measurements; the
visual audit now includes all 21 firearms and launcher details. The initial
full wide audit captures all 21 firearms and 231 transition frames.
The pin-clearance refinement passes the ADS/wardrobe regressions and native
Windows/optimized Wasm builds. Its wide and 4:3 native captures, plus FOV 110 at
120 Hz with weapon FOV and snow wardrobe, each include 11 raise/lower frames.
These views confirm that the centered near pin clears the loaded nose. Private
evidence is indexed by `build_win/rocket-clearance-*.log`.
Both independent gallery passes reproduce all 72 views and all 35 icons with
zero changed image cells; the refreshed manifest/HTML records the new rendering
inputs. Gallery integrity and all eight workflow tests pass.

Owner pose refinements (2026-11-02, [alignment guide](scripts/port/ads_alignment.md)):
Klobb 28.8 → 26.5, ZMG 32 → 29.5, shotgun 33.6 → 31 and grenade launcher 40 → 36.5
eye distances, with 1.08x generated arms on those four (FORK-004). The ZMG
pitches -0.090 to look along its top, the shotgun -0.090 to show its front bead
above the red shells, and the Phantom -0.002 to level its far sight posts inside
the near notch. Silver and gold PP7 sight heights rise from 1.0 to 1.35, lowering
their taller tips onto the shot ray; a play-test moved them to 1.5. The grenade
launcher pitches -0.200, muzzle well up to suggest its arcing lob, with half the
usual ADS zoom (87.5% FOV). The RC-P90 draws only its support hand (FORK-004). Following play-tests, the
PP7 and silenced PP7 sit at 1.25 and the DD44 at 1.65, centering their tips like
the plated PP7s. A pre-change capture shows this offset was long-standing, not a
regression. The automatic shotgun moves from 44.8 to 32 units, with its trigger
grip on the model's pistol grip, so its right hand stays out of view. The rocket
launcher's raise eases out over 250 ms, and its support hand stays under the tube
through the cant. A further play-test asked for those trigger hands back, looking
natural. Both shotguns got re-measured grips and a raised trigger elbow, so the
fist wraps the pistol grip; the grip then moved towards the body. At the owner's
request, both shotguns then took the rocket launcher's treatment. They stay at
`WeaponStats` hip depth (shotgun 19.5, automatic shotgun 19) with no ADS zoom
(`portAdsWeaponFovScale` 1.0), which puts that hand out of view. The grenade
launcher's support hand moves from inside its cylinder to the underside of the
body. A later request lowered all five pistols until screen center just touches
the topmost point of each sight, measured from capture pixels. The final heights
are PP7 1.55, silenced PP7 1.49, DD44 2.01, silver PP7 1.56 and gold PP7 1.62.
The Klobb's trigger grip moves from z -1 to -2.5, towards the body, so the fist
wraps its wooden grip instead of the grip showing through the hand. `test_ads.c`
bounds the new PP7 pose.
The rocket launcher uses the canted stacked
pose above. The automatic shotgun and stock PP7 were unchanged in that first pass. The follow-up
shotgun/PP7/rocket captures use an isolated `build_win/hd-prototype` build
while the main executable was in use; rocket transitions and hands were reviewed. ADS/rocket/wardrobe regressions and
the Windows build pass. The full 21-firearm wide audit, sight details, and
Klobb/rocket transitions and hand details were reviewed in native captures.
Both independent gallery passes regenerate 72 views and 36 icons. Only the 12
Klobb ADS views (stock and fork, six outfits) and the new tank icon change;
the changed views were reviewed. The optimized Wasm build and its logo-payload
audit pass.

Added aim-down-sights for 21 supported firearms with individual weapon poses
and sight alignment. Right mouse defaults to ADS; **Gameplay > Iron Sights > Aiming and Movement
> Aim Mode** can restore the red reticle. The existing hold/toggle aim setting
applies to either mode, with Hold as the default. The separate hold-to-ADS
action is now unbound by default on desktop and web, replacing its middle-mouse
default. An `ads_mouse_upgraded` marker in the keyboard device's binds section
removes that old mouse key once from loaded bindings, retaining other custom
keys and later intentional rebindings. The ordinary Aim binding remains Mouse
Right by default. The optional dedicated action remains available to custom
keyboard/controller layouts and the browser touch adapter. Documentation and
the ADS default regression now match this policy. Players can walk and strafe at half speed while aiming. Per-player ADS
state and simulation-time transitions support different tick rates. Refined the
view to reveal front sights with a slight perspective along the top of each gun.
Recalibrated the AR33/M4 square sight aperture and RC-P90 sight tip vertically
against the centered shot ray after the perspective adjustment left them low.
That earlier correction changed only those ADS poses. The AR33 also uses a little more
muzzle-up tilt to keep the receiver clear of its square opening; it retained eye
distances and the RC-P90's pitch at that checkpoint. A subsequent long-gun audit also raised
the Klobb, ZMG, both D5K variants and regular shotgun to correct smaller
vertical sight offsets. These use individual height corrections; the other
audited long-gun poses are retained.
The silenced D5K explicitly compensates for its longer muzzle node so its
shared sight mesh receives the same pose as the unsilenced D5K.
Crosshair calibration (2026-11-08, [alignment guide](scripts/port/ads_alignment.md)):
with ADS Crosshair (FORK-060) showing the aim point, the owner saw the
Magnum's tip right of it and the D5K rings off centre. An all-guns audit at
1600 × 900 measured each sight against the cross: the D5K pair's ring centres
sat 5 px high (heights 5.8 → 6.11 and 5.0 → 5.31, centred to the pixel), the
Magnum's tip 1-1.5 px right and 1.5 px low (height 4.0 → 3.92 and a new
optional `sightX` of 0.075 view units in the pose table, the tip's offset right
of the muzzle axis), and the Golden Gun's post about 2.3 px low (1.7 → 1.56).
At the owner's request the ZMG rose until its rear fins' tops sit flush with
the cross's horizontal arms (height 4.0 → 3.41; they had been 14 px below).
The other guns were within about a pixel or deliberately posed (shotgun,
laser and grenade launcher sit lower); they are listed in the guide.

The owner's camera-proximity refinement moves the 12 unscoped long-gun ADS
origins 20% closer along camera Z. It retains each calibrated sight height/pitch,
the world/ADS FOV and the existing interpolation/recoil path, so the centered
aiming reference is preserved while the gun and its attached grip hands occupy
more of the view. KF7 distance is now 33.6 rather than 42 view units; AR33 is 48
rather than 60. Pistols and the scoped sniper retain their previous poses.
The hand/arm size increases in FORK-004 remain intact. This is a small data-only
change within the fork's existing pose table, without new upstream hooks.
The closer KF7 receiver occupies more of the lower frame, leaving less trigger
hand visible at its bottom edge. The support hand keeps its grip connection.
Proximity validation passes ADS/wardrobe regressions, native Windows/optimized
Wasm builds, Wasm logo policy and site allowlist. All 20 guns were captured in
wide/4:3; wide includes 220 transitions. Further 11-frame checks cover KF7 at
FOV 110/120 Hz and AR33 in 4:3, both with optional weapon FOV enabled. Sight and
hand/transition crops were reviewed. Both gallery passes reproduce all 72 views;
the 24 KF7/Klobb ADS stock/fork views change across six outfits, while the other
48 views and all 35 icons stay byte-identical. Changed outfit views and the
stock controls were reviewed, and the eight gallery workflow tests pass.

**Sources:** [ADS interface](port/include/port_ads.h),
[input/state and poses](port/src/joy_port.c), [mouse input](port/src/input_pc.c),
[camera and movement](src/game/bondview2.c), [gun rendering](src/game/gunfire.c),
the shared port settings table and frontend/watch settings pages.
**History:** `d017318`, `79d3892`.
**Earlier height-correction checks:** [ADS tests](scripts/port/test_ads.sh),
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

Added hands and forearms for the 14 firearms that lack them during ADS, with
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
the proximal arm stretches toward the elbow. The owner's additional thickness
request increases the rigid grip scale from 0.045 to 0.052 (about 16% larger
hands), with wrist, cuff and watch scaled uniformly alongside it to retain their
joint and case/face proportions. Its body now smoothly increases
to 2.5 times the native width/depth behind the protected wrist/cuff region,
making forearms about 26% wider/deeper than the previous 0.045/2.3 profile.
The palm origin stays anchored to the weapon; authored finger shapes, UVs,
materials and source vertices are preserved. Hand scale, protected wrist length, bulk ramp
and body multiplier now have a
single named profile (`sAdsArmProfile`); pose tests lock the protected transforms,
body radius and continuity at the boundary independently.
The owner's 2026-11-02 request scales the Klobb, ZMG, regular shotgun and grenade
launcher grips by a further 1.08x (`adsArmScale`): hand, wrist, cuff, watch and
forearm grow together around the anchored palm origin, with the same 2.5 body
profile; other long guns keep 0.052. The [arm contracts](scripts/port/arm_rendering.md)
and pose tests record the per-weapon multiplier. The RC-P90 draws only its generated support
hand in ADS (`adsGripHands`), at the owner's request: its trigger grip sits under
the body, where the generated right hand read badly. The canted rocket
launcher's support hand uses a counter-rolled basis (`adsCounterRoll`) and stays
under the tube relative to the ground. Side-on debug captures re-measured the
stocked guns' grips. Trigger points are shotgun (1.25,-3,-5), automatic shotgun
(1.25,1,-5), which supersedes the earlier (-0.6,-4.5,-11), and grenade launcher
(1.25,-2.5,-3.5). The shotgun points first sat at z -2; the grip showed through
the middle of the fist there, so they moved towards the body. The grenade
launcher's support palm moves from inside its cylinder (1.1,0.5,25) to the
underside of the body, (1.1,-3,10). That replaces an interim cupping of the
cylinder's outside at (6.5,-4,17). `adsShoulderedGrip`
raises those three guns' trigger elbows to (14,-12,6), so the rigid closed hand
wraps a slanted pistol grip. The rocket regression checks the restored hand
basis and its offset from the tube axis. `test_ads.c` checks that the
shouldered elbow is higher and behind the eye, with a less steep forearm and an
unchanged palm. It also checks the shotguns' hip depths, and `test_rocket_ads.c`
checks their unzoomed field of view.
The preceding body-only refinement increased 1.75 to 2.3 (approximately 31%
in width/depth) while retaining the old hand scale. The new hand/body adjustment
supersedes those size values and is confined to the generated long-gun ADS arm transform;
native pistol/chopping arms and weapon icons do not use this shaping profile.
The protected 40-native-unit wrist band and 35-unit bulk ramp remain unchanged
in source space; elbow targets stay fixed. The rendering contract and pose
regression deliberately adopt the new hand scale and body radius, including an
explicit grip-origin anchor check for both hands and all 13 grip pairs/blends.
Shaping writes frame-owned generated arm vertices; the shared wardrobe source
model stays immutable, so separate duplicated source models are unnecessary.
The hand-size adjustment itself leaves weapon poses, sight alignment and recoil
anchors intact; FORK-003 separately owns the closer unscoped ADS placement.
At the hand-size-only checkpoint, the thickness refinement passed the ROM-free ADS/wardrobe regressions,
native Windows and optimized Wasm builds, Wasm logo policy and public-site
allowlist. The Windows Runway audit verifies 20 weapons and 220 raise/lower
frames; additional Jungle/Snow KF7 audits verify 11 transition frames each.
Overview, both-hand and intermediate-pose crops were visually reviewed.
That gallery reproduced all 72 views across repeated samples and
an independent second process. Exactly the 12 fork KF7/Klobb ADS views changed
across six outfits; the other 60 views and all 35 icons were byte-identical.
All changed views and the native PP7 stock/fork comparison were reviewed.
Private captures/logs stay under `build/ads-thickness` and the audit directories
referenced there; the approved final gallery is updated alongside its sources.
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
At the bracelet-only checkpoint, wardrobe/ADS regressions and Windows/Linux
builds passed. Its Windows audit captured all 20 guns and 220 ADS transition frames in the blue wardrobe,
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
face at the camera. Since the owner's 2026-11-03 request, the support hand
cradles the fore-end palm up on every generated long-gun arm. Its back faces
the ground with a slight outward cant (`adsNativePose` knuckle direction
0.2·x − 1.0·y, replacing 0.85·x − 0.55·y), and the palm rises 0.4 units to meet
the fore-end. That rolls the watch under the wrist, away from the camera; its
band can be the only visible part. The earlier hold faced the palm inward/up
beneath the fore-end. Side-on debug captures checked the AK47 cradle; a full
21-gun ADS audit with hand details and transitions checked the rest.
`test_ads.c` asserts the downward knuckle direction and that the watch never
faces the camera. The trigger hand rolls outward around its grip as well.
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

Added **Gameplay > Iron Sights > Aiming and Movement > Sprint**, on by default. Holding the
rebindable **Walk / Sprint** action (Left Shift by default) boosts movement to
1.5 times normal speed. Aiming and crouching prevent the sprint boost. Turning
the setting off restores the original half-speed Shift-to-walk behavior; aim
and walk slowdowns do not stack.
**Ludicrous Sprint**, on the same page and off by default (`[display]
ludicrous_sprint=0`), raises the sprint boost from 1.5x to 5x for fun. It has
no effect while Sprint is off, and aiming and crouching still prevent it. The
1.5x default matches the classic Call of Duty sprint scale; modern Call of Duty
sprints at roughly 1.15x and tactical-sprints at about 1.4x. In the browser,
dragging the touch move stick well past its ring also holds Walk / Sprint
(FORK-028).
FORK-035 shows the live Walk / Sprint binding in mission-start hints only while
this setting is on.

**Sources:** [movement/input](port/src/joy_port.c),
[option definitions](port/src/port_options.c), frontend/watch settings pages,
[settings reference](tools/midas_ini_help.txt).
**History:** `939b400`; Ludicrous Sprint added later without a commit
reference yet. **Checks:** ADS test suite movement/settings cases
(`scripts/port/test_sprint.c`, included by `test_ads.c`), including the 5x,
crouch, ADS, Sprint-off and saved-setting cases for Ludicrous Sprint.

### FORK-007 — Optional combat feedback

**Status:** Implemented.

Added default-on **Hit Markers** and **Damage Direction** HUD settings. Confirmed
character damage produces a thin white hit marker; headshots use gold.
Directional red damage arcs point toward the source and track the player's view
as it turns. Fixed the initial left/right reversal and replaced blocky lines
and segmented arcs with smooth strokes/curves. Feedback state is per player,
uses simulation time, and hides when gameplay controls are unavailable.
FORK-043 adds the separate Hit Sounds setting from the same character-damage hook.

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
When every difficulty-active prerequisite is complete, a separate queued alert
says **All main objectives complete** and prompts **Finish the mission**.
The progress check excludes listed final actions (including Dam's
bungee jump and Depot's train) and the casualty safeguards in Facility, Silo,
Streets and Caverns; Streets' pursuit objective is also an exit action. Any
failed objective suppresses the cue. Missions with no earlier prerequisite do
not show it at mission start. This is guidance only: the inherited objective
rules still decide mission success, and Objective Alerts still control whether
the cue appears. If progress becomes invalid, a queued cue is removed.
The description is normalized to lowercase only in its display copy: Bank
Gothic renders these as uniform small capitals, avoiding the taller initial
capital in sentence-case objective names. Original localized strings and queue
contents keep their spelling; native two-byte glyph codes are preserved. The
panel leaves space below the optional mission clock, including taller font
metrics (FORK-015). Alerts appear immediately during dialogue, below the actual
measured subtitle bar with a four-unit gap, so multiline subtitles (including
Statue Park's Janus conversation) stay unobscured without delaying the completion
signal used by speedrunners. With Mission Timer enabled, subtitles sit below
the complete elapsed/cheat-target/BEST block (FORK-015), and alerts sit below
those subtitles. Native subtitle rendering reports its final bar
bottom after HUD scaling, including regional fonts and camera offsets, and resets
that bound each render so hidden/expired dialogue reserves no space. Queue order,
alert lifetime and other HUD timers keep their normal behavior.
An expiring alert now fades out over its last 300 ms (18 timer units) instead of
disappearing abruptly: its panel, accent bar and both text lines share one opacity.
The fade sits inside the alert's lifetime, and the next queued alert appears at
full opacity as soon as the fade ends. Native and browser builds share this.
At the owner's 2026-11-02 request, each alert now shows for 3.5 seconds instead
of the original 4.5 seconds. `HUD_TOAST_LIFETIME` is 210 timer units, including
the 300 ms fade. Queued alerts still each receive the full lifetime, in game
time. The HUD audit's `-ToastFade` capture derives its mid-fade frame from the
same constant.
Objective descriptions now join briefing-wrapped strings into one line: the
original text breaks long objectives with a newline plus an indent (for example
Facility's "destroy all tanks in bottling\n    room" and Bunker's "disrupt all
surveillance\n   equipment" and "get personnel to activate\n    computer"), which
previously left a run of spaces inside the alert. Only queued objective text
collapses whitespace runs; other HUD labels keep deliberate spacing. An audit of
the English objective strings found only space-indented wraps of this kind and no
hyphenated word splits. The **All main objectives complete** cue now queues after
pending alerts instead of jumping ahead of them, so the objective that completed
the set (for example Silo's satellite photograph) is announced first. The
queue's eight-alert cap still drops the oldest pending alert.

**Sources:** [objective updates](src/game/objective_status.c),
[mission reset](src/game/objective.c), [notification queue](port/src/port_hud.c),
[progress rules](port/src/port_objective_progress.c),
[shared layout](port/src/port_mission_clock.c),
[native subtitle bounds hook](src/game/bondview2.c).
**Checks:** HUD queue/filter/timing, display-case/buffer/native-glyph tests and
main-objective progress cases for final actions, casualty failures, difficulty
filtering and once-per-completion/reset behavior in `scripts/port/test_hud.sh`.
The suite also covers wrapped Bunker/Facility objective text, preserved label
spacing and the summary cue queueing behind a pending objective alert.
The ROM-free HUD and ADS suites and an isolated Windows build pass in the
`goldeneye` container. Gallery regeneration verified both capture passes;
no image cells changed, and its HTML/manifest source fingerprint was refreshed.
The shorter cue copy passed the ROM-free HUD suite; a fresh two-pass gallery
verified 72 views and 35 icons with no changed image cells.
Prior checks include completed/failed objective captures. Subtitle/clock
clearance across font metrics, line counts, HUD scales and viewport offsets,
plus immediate queueing and normal alert duration during dialogue, have
regressions in `scripts/port/test_hud.sh`.
`scripts/port/audit_hud.ps1 -Dialogue` reproduces simultaneous native Statue Park
dialogue and its objective alert in an isolated capture; add `-MissionTimer`,
`-SmallHud` or `-Aspect Classic` to check the adjacent clock, scaled subtitles
and alternate viewport. `-ToastFade` adds a capture halfway through an alert's
fade-out. A native Dam capture with it logs opacity 0.50 and shows the whole panel
half-faded over the scene, then the next alert fully opaque. `test_hud.sh` covers
the fade curve, unchanged lifetime and queue order. The two-pass gallery changes
zero image cells, because its fixture disables objective alerts. The browser build
was not captured separately. The visual-audit fixture moved unchanged from
`port_hud.c` into [`port_hud_audit.inl`](port/src/port_hud_audit.inl), keeping that
file within the 628-line standard.
Native Statue Park captures at 960x540 and 800x600 (small HUD) verify the actual
three-line Trevelyan subtitle and completion panel are visible together, with
clear space between them. The Windows build and HUD regressions pass.
The raised-initial appearance was reproduced in native and Edge actual-ROM
captures before the presentation fix. Matched Edge
pixel bounds show the first capital five screen pixels higher before the fix;
the normalized description's measured letters share top/bottom bounds afterward.

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
While Bond drives the tank, its cannon (`ITEM_TANKSHELLS`, which the game adds
to the inventory on entry) appears as a **tank** card and is highlighted when the
cannon is in hand. Icon mode uses the tank's own image (slot 38, FORK-019); text
mode and a missing cache show its name. Leaving the tank removes the card with
the inherited inventory entry. The watch laser is now listed as a weapon card,
with its own icon (slot 23, FORK-019). Holding the watch detonator (`ITEM_TRIGGER`,
filtered from the strip) keeps the Remote Mine card selected instead of clearing
the selection.

Two-gun pairs are cards of their own, one card per weapon-cycle stop. The
inherited inventory index (the watch list) omits pair entries, so the strip
previously merged a held pair into its right-hand gun's card: Jungle's AR33 +
RC-P90 highlighted the plain RC-P90. The strip now also walks the cycle list for
picked-up pairs (`INV_ITEM_DUAL`) and, with All Guns, the doubles
`bondinvCycleForward` offers, using the native `bondinvItemAvailableForHand`
rule (solo play, dual-wield flag, knife exclusion). Pairs follow their right-hand
gun, ordered by left-hand gun as the native sort and cycle visit them; a pair
whose single gun a mission removed is appended. Selection matches both hands,
falling back to the right-hand gun's card. Icon cards draw the left hand's gun
behind and up-left (dimmed) and the right hand's in front; text cards stack the
two names. Doubles add an `x2` badge. The selected caption reads `PP7 x2` or
`AR33 + RC-P90`, left hand first, keeping mission name overrides for each gun.
A pair change counts as a weapon change for caption/emphasis timing. No setting
is added; the native watch inventory, cycling and dual-wield rules are
unchanged. With All Guns, the strip shows every double as its own card because
the cycle stops on each. Jungle captures (960x540, All Guns) checked an AR33 +
RC-P90 pair in icon and text styles and PP7 doubles in icon style. The Windows
build and HUD tests pass; the gallery regenerated with no changed image cells.

While Bond owns the sniper rifle, the inherited switch to unarmed clubs with the
rifle (its model and butt-strike keyframes) instead of chopping; the swap code
records that choice in `cur_item_weapon_getname` (`gunfire.c`). The unarmed card
previously still showed the wardrobe hand and "Unarmed". It now shows the rifle
butt image (icon slot 39, FORK-019) and reads **Buttstroke**, the military
term for a strike with a long gun's stock, in text style and the caption. In hand, the card follows the recorded club, so a sniper picked up
while unarmed keeps the hand until the next switch; otherwise owning the rifle
(including All Guns) predicts the next unarmed switch. Without the image the
card shows its name rather than a hand. Melee behavior is unchanged.

**Sources:** [inventory strip](port/src/port_hud.c),
[pair cards and names](port/src/port_hud_weapons.c),
[inventory reset](src/game/bondinv.c), [HUD integration](src/game/bondview2.c),
[message positioning](port/src/joy_port.c).
**Checks:** HUD inventory filtering/selection/scrolling (including the tank card)
and idle timing tests; [pair card tests](scripts/port/test_hud_weapons.c)
(run by `test_hud.sh`) cover picked-up and All Guns pairs, order, selection,
orphaned pairs, capacity, names and the rifle-butt rule; HUD tests check its
icon choice. Dam (All Guns) captures showed the rifle-butt card in text style
and, with a freshly prepared version-9 cache, in icon style. `audit_hud.ps1 -LeftItem <id>` fixtures a
held pair for captures;
visual captures checked active, quiet and weapon-switch states, text/icon
styles, and ammo/player-name spacing in solo, two- and four-player layouts.

### FORK-010 — Smooth aiming reticles and cursors

**Status:** Implemented.

**Gameplay > Iron Sights > HUD Additions > Always Crosshair Size** adjusts the always-on target from 10%
to 200% in five-point steps; 100% preserves the existing default. The watch's
Iron Sights page exposes the same explicit label, clarifying that other sights are unaffected;
both menus leave room for the longer label beside the slider and percentage.
A live vector preview sits beside each slider,
using the current crosshair colour and HUD Scale. Mouse dragging, wheel, keys
and controller editing share the same grid; endpoints clamp rather than wrap.
`[display] always_crosshair_size` persists through the existing options/INI and
browser save-backup paths. Missing keys retain 100%; out-of-range values clamp.
It affects solo and multiplayer always-on targets, preserving their visibility,
colour, fractional aim position, wide-aim mapping and HUD Scale. The red aiming
reticle, sniper optic and menu/watch cursors retain their original sizing.
The focused [size/slider helper](port/src/port_crosshair.c) and
[interface](port/include/port_crosshair.h) own policy and presentation, with small
hooks in the existing HUD setting bridge, sight path and two menu drawers.
The [sight audit](scripts/port/audit_sights.ps1) covers HUD-menu/watch previews,
size and HUD Scale alongside existing gameplay modes. It disables watch static
for stable glyph review and removes temporary executables even after startup
failures. The [private browser audit](web/tools/crosshair-size-audit.mjs) uses
the matching local linker map to observe the virtual cursor without writing
native state, and drives real browser input rather than assuming an OS warp.
It now targets the slider directly through the centred 4:3 menu band and asserts
cursor agreement; the former iterative offset compensation is removed as part
of the browser menu-pointer correction in FORK-024.
Windows, Linux i386 and optimized Wasm builds pass. Native ADS/HUD/Start/upstream
regressions cover defaults, INI loading/saving, range clamps, pointer bounds,
preview parity and composition with HUD Scale/wide aim; other sight sizes remain
unchanged. Native captures review the 10% minimum, 100% default, 200% maximum,
custom cyan and High HUD Scale in both menus and gameplay. Type checking, 152
browser unit tests, public allowlist/digest and standards checks pass. The final
two-pass gallery reproduces all 107 PNGs byte-for-byte. Diagnostics stay private
under `build/crosshair-size` and `build_win/sight-audit-*`.
Edge's real-ROM audit passes actual pointer drags through 100%/200%/10%, preview
captures, export of the selected INI value in the existing backup format, and
remembered-ROM reload with 10% restored. Synthetic frontend key presses were not
reliably observed in headless acceptance; native value-step regressions cover
the shared grid, without claiming a physical keyboard/controller UI test.

Replaced the PC red target's pixel/sprite construction with thin, smoothed
lines and a circle. The same renderer supports sniper and always-on crosshairs,
menu/watch target cursors, hit markers and damage arcs. Preserved custom
crosshair colors, fractional aim positions, off-center/wide aiming and
split-screen positioning. The N64 sight path remains separate.

**Sources:** [overlay commands](port/fast3d/gbiex_compat.h),
[PC renderer](port/fast3d/gfx_pc.cpp), [sights](port/src/port_sight.c),
[menu/watch cursor wrapper](src/game/bondwalk2.c), [shared HUD API](port/include/port_hud.h).
The 1.0.2b integration moves sight policy into `port_sight.c` behind the existing
game hook and supports upstream HUD Scale around the same aim point. It replaces
upstream's new sprite/texel sight implementation with this fork's vector path;
no ROM-derived texel table is added to the browser artifact.
**Checks:** Sight captures with multisampling off, custom colors, off-center
aiming, menu cursors and split-screen layouts.

### FORK-011 — Weapon icon mode and model-image generation

**Status:** Implemented.

Added **Gameplay > Iron Sights > HUD Additions > Weapon Strip Style: Text / Icons** (`weapon_hud_icons`; Icons by default since 2026-11-07, FORK-019),
with Text as the default. Icon mode now uses 35 full-color, transparent
512 x 160 renders of actual weapon models, retaining original materials and
separate silenced/gold/silver appearances. Unarmed uses the hand/forearm model: six extra wardrobe variants match the
current mission cuff, resolving Folder's Bond and changing immediately with
player/stage changes or restores. While Bond owns the sniper rifle, the unarmed
card shows the rifle butt image instead (slot 39, FORK-009). The native cache has
40 indexed slots, including the tank and the rifle butt (FORK-009/FORK-019);
runtime preparation supersedes the older embedded/bootstrap packs (FORK-019).
These images replace the initial hand-drawn vector silhouettes. Cached textures
and a subtle edge keep the icons readable without extra model rendering in
normal gameplay; the selection highlight and strip scrolling remain intact.
The selected icon also shows its weapon name centered underneath, with a
shadow for contrast and viewport-edge clamping. That shadow now shifts one
framebuffer pixel right/down after projection, aspect and HUD scaling, replacing
the original one-HUD-unit offset that expanded several pixels on large displays.
Shadow/foreground share the same centered/clamped layout. The scoped font-only
`G_NOOP` tag is decoded by the PC/WebGL renderer, reset before foreground text
and at frame start, and does not shift other glyph runs or non-font rectangles.
Browser CSS scaling still scales the complete framebuffer. The caption stays opaque for
three seconds after selection, then fades to zero over one second; the icon
strip retains its separate 20% idle opacity. Icon mode reserves a small label
row above the ammo counters, including after the name disappears, and pickup
messages account for that row. Text fallback builds keep the text-only layout.
HUD timing tests cover complete caption disappearance and restoration on
weapon changes at 30/60/120/240 Hz. Solo wide/4:3 and four-player captures
checked placement, idle disappearance and selection refresh; Windows/Linux
builds and ADS/settings regression tests passed. The tighter shadow's shared
[offset encoding/projection helper](port/fast3d/hud_text_offset.h),
[HUD emission](port/src/port_hud.c) and small
[renderer hook](port/fast3d/gfx_pc.cpp) keep layout/font sizing intact. HUD
regressions check signed tag/reset decoding and a one-framebuffer-pixel shift
across 320–3840 output extents, including zero-size handling. Native Windows/
optimized Wasm builds, HUD/ADS checks, TypeScript, 146 browser unit tests and site
allowlist/logo checks pass. A matched 1600x900 before/after crop and verified
1920x1080/3840x2160 drawable captures show the tighter shadow. The isolated
capture resizes the actual SDL game window after its display-size clamp, with
BMP dimensions verified. Edge's actual-ROM caption check uses a validated local
icon cache; no cache/ROM/captures enter Git. Both gallery passes reproduce all
72 views and 35 icons unchanged; integrity/eight workflow tests pass. Evidence
is private under `build/text-shadow`, with publication tracked in WEB-010.

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

**Icon antialiasing (2026-11-07).** The strip draws each 512x160 icon about
3.4 times smaller at 1080p (151 x 44 px; a pair's icons about 110 x 35 px).
Plain bilinear sampling without mips read only 4 of the ~11 texels under each
pixel, so edges stair-stepped and thin parts (Klobb stock, KF7 magazine) broke
up. The sampler latch now treats a native HUD image (`tex_flags & 0x80000000`,
used only by these icons) like an Enhanced glyph under Text Antialiasing:
`ge_mip_active` 3 generates the GL mip chain and minifies with
`GL_LINEAR_MIPMAP_LINEAR`, with a `GL_TEXTURE_LOD_BIAS` of -0.5 so the chosen
level is half a step sharper than plain trilinear (desktop GL only; GLES has no
bias, and icons are prepared only on Windows today). The bias is tracked per
texture object and written back to 0 when a recycled name serves another
texture; deleting a texture clears its record, since a new object under that
name starts at GL's default bias (the first version kept the record, so a
later icon reusing the name could draw unbiased). The mip chain lives only in
GPU memory: `glGenerateMipmap` builds it on first antialiased use and again
after a new upload; the cache file holds only the full-size images. When the cache loads, transparent texels take their nearest visible
neighbours' colour for 32 rings (`bleedEdges`), so filtering and mip averaging no
longer pull black into silver and gold edges; alpha, visible texels and the cache
file format are unchanged, so no regeneration is needed. Always on, with no
setting. The level was chosen by the user from review sheets of five draw-side
levels (none, mip bias -1, -0.5, full trilinear, exact area average) at 1080p,
paired-icon and 1440p sizes. Capture settings were compared too: 2048 and 4096 px
captures with no, 4x and 8x MSAA changed the stored icons' edges by about 2/255
on average, and the old HUD draw discarded most of even that (56-60% of edge
pixels partial in every variant, against ~100% with mips), so capture settings
stay as they were. Review sheets and captures are ROM-derived and stayed local
(`build_win/icon-aa`).

**Sources:** [generator](scripts/port/generate_weapon_icons.py),
[export harness](port/src/port_weapon_export.c), [model drawing](src/game/gunfire.c),
[capture backend](port/fast3d/gfx_sdl2.cpp), [image renderer](port/fast3d/gfx_pc.cpp),
[icon sampler](port/fast3d/gfx_opengl.cpp), [icon cache](port/fast3d/weapon_cache.cpp),
[packet decoder](port/include/port_icon_decode.h),
[embedded pack](port/src/port_weapon_images.c), [build wiring](CMakeLists.txt).
**Regeneration guide:** [weapon_icons.md](scripts/port/weapon_icons.md).
**Checks:** [generator tests](scripts/port/test_weapon_icons.py), HUD pack decoding
and coverage tests, solo/split-screen captures, Windows/Linux builds. Two full
generation runs of the earlier 29-image pack matched all 33 output files
byte-for-byte on the tested machine;
cross-GPU byte identity is not assumed. Icon antialiasing: Windows build; matched
1920x1080 in-game strip captures before and after (HUD audit with icons) show
smooth edges and no dark fringe; HUD, ADS, wardrobe, Enhanced font and
generator tests pass. Not checked: split-screen sizes in-game, and Linux.

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
The Iron Sights settings rule (FORK-044) keeps fork settings on their own menu
pages, with a ROM-free check in the same hook.

**Sources:** [AGENTS.md](AGENTS.md), [FORK_CHANGES.md](FORK_CHANGES.md),
[CONTRIBUTING.md](CONTRIBUTING.md), [README.md](README.md),
[PR template](.github/pull_request_template.md),
[arm rendering contracts](scripts/port/arm_rendering.md).
**Verification:** Local Markdown paths and Git whitespace checks; no runtime
behavior changed by the inventory/rule addition.

### FORK-013 — Persistent adaptive health and armor gauges

**Status:** Implemented.

Added **Gameplay > Iron Sights > HUD Additions > Health Display: Automatic / Always**, defaulting to
**Always** (`[display] always_health=1`) on all platforms. Both the shared
in-memory default and first-run INI writer use Always; existing saved choices
remain authoritative. This restores the original fork default and supersedes
the temporary Automatic default shipped in `2680157` at the owner's request.
Always keeps the native curved gauges
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
[first-run defaults](port/src/video.c),
frontend/watch Iron Sights pages, [README](README.md),
[INI reference](tools/midas_ini_help.txt).
**Checks:** [HUD tests](scripts/port/test_hud.sh) cover idle timing at
30/60/120/240 Hz, pause/player isolation, damage refresh with indicators off,
visibility gates and gauge geometry/alpha. [ADS tests](scripts/port/test_ads.sh)
cover menu availability and setting persistence, including the Always default
and explicit Always/Automatic choices. The HUD audit explicitly opts into Always
for persistent-gauge captures, supports `-AutomaticHealth` for temporary gauges,
and captures active/idle health with a refreshed weapon strip.
Windows and Linux builds passed. Visual captures checked the HUD settings page,
full/quiet gauges in solo and split-screen, and Automatic mode without
persistent gauges.
Persistent gauges are now built in per-frame display memory. Previously, the
game thread rebuilt the player-owned gauge vertices at full stock size and alpha,
then restyled them in place. Meanwhile the render thread (on by default whenever
`tickhz` is not 60) could still be drawing the previous frame from the same
buffer. A frame read between those two steps drew segments at about five times
the quiet thickness and up to four times the opacity, which explains the
intermittent "blown out" gauges. The in-game path now allocates its vertices and
both gauge display lists with `dynAllocate`, from the double-buffered frame heap.
The watch keeps its player-owned buffers. N64 builds keep the original in-place
path. Sources: `bondviewRenderGaugeBars` in `src/game/bondview2.c` and
`PORT_GAUGE_BAR_GFX` in `port/include/port_hud.h`. The fix was inferred from
code: the report could not be reproduced, and no capture shows the race. HUD
tests and the Windows build pass.
The restored default passes HUD/ADS/upstream regressions, Windows and optimized
Wasm builds, 155 browser unit tests and type/site checks. An isolated first-run
Windows INI and HUD settings capture verify Always; imported Automatic stays
intact through the actual-game backup restart. The two-pass gallery reproduces
all 107 images unchanged; freshness/integrity and eight gallery workflow tests
pass. Evidence is private under `build/file-menu-*`, `build/web-files` and
`build_win/health-default-first-run-*`. Publication is tracked in WEB-006.

### FORK-014 — Solo suspend points and restore/undo controls

**Status:** Implemented.

Added four manual slots per campaign folder, a separate quicksave and one
undo-load image. F6 opens Suspend Points, F5 quicksaves, F9 quickloads and F8
undoes a load; all actions are rebindable for keyboard/controllers. The watch's
Gameplay > Iron Sights page also opens the menu (FORK-044). Rows show mission, difficulty, save date/time
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

The issue #4 investigation reproduced green/gray striped textures on Aztec smoke
after quickload. The saved texture bytes already contained frame display-list
commands: AR33 ADS transitions had exhausted the auxiliary frame pool
(FORK-017). Quickload's existing GPU-cache invalidation exposed those damaged
CPU pixels. The allocator fix prevents this overwrite; it does not change the
snapshot format or weaken exact-build compatibility. The opt-in
[effect audit](port/src/port_state_effects_audit.c), called at the existing
save-command boundary, checks effect texture bytes before/after ADS and repeated
loads, then captures baseline and two restored sequences. Its
[isolated launcher](scripts/port/audit_state_effects.ps1) supports stage/spawn,
tick rate, threaded rendering, MSAA and active effects at save time.
At the recorded Aztec camera, 60 Hz synchronous and 120 Hz threaded/MSAA-2
same-session runs passed all 24 captures and source-byte checks. The separate
fresh-process Dam/Facility native arm crash observed during that investigation
was subsequently fixed by FORK-036's arena-backed cache ownership registration;
the threaded fresh-process audit now passes. This does not change the snapshot
format or relax exact-build compatibility.

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
clock now occupies a dedicated top-left area at 4:3 and wide aspect ratios.
This replaces the inherited bottom-left placement, where elapsed/best rows
overlapped gadget names such as Bunker's watch magnet. Measured font height
(including the required trailing newline) determines row spacing and objective
panel clearance. The clock remains visible at its fixed top-left position during
dialogue. Native subtitles move below the complete clock block, including the
optional BEST row, with a four-unit visible gap above their background bar;
objective alerts appear immediately below the subtitles (FORK-008). This replaces
the earlier dialogue-time hiding behavior for speedrun timing. With the timer
disabled, subtitles retain their native position. Legacy half-size HUD scaling
shrinks the clock about the top-left corner; clearance rounds its bottom outward
before reserving the gap, so odd font heights cannot produce an overlap.
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
Existing solo/HUD visibility rules remain, including drawing independently of
Ammo On Screen. Pickup messages no longer reserve bottom space for the relocated
clock; gadget/ammo/weapon-strip spacing still applies.
The setting remains off by default (`[display] mission_timer=0`); the INI guide
now matches the actual video default, correcting its inherited claim of 1.

**Sources:** [timer rendering/layout](port/src/port_mission_clock.c),
[clock interface](port/include/port_mission_clock.h),
[small ammo-render hook](src/game/gunfire.c),
[pickup-message spacing](src/game/bondview2.c),
[target lookup/format/colors](port/src/port_mission_clock.c), [HUD API](port/include/port_hud.h),
[campaign time API](src/game/file2.h),
[setting/default](port/src/video.c), [README](README.md),
[INI guide](tools/midas_ini_help.txt).
**Checks:** [HUD tests](scripts/port/test_hud.c) cover stage/index resolution,
difficulty exclusions, saved-record stage/folder lookup and missing records,
formatting, deadline boundaries, warning colors and
restored-clock comparisons. Clock layout/visibility regressions cover regular
and taller fonts, saved-best presence, shifted viewports, Ammo On Screen off,
menu/multiplayer gating and visibility during dialogue. Full-stack layout tests
cover odd/even font metrics, one to six subtitle lines, saved-best presence,
shifted viewports, both HUD scales, native camera offsets and timer-off placement.
Native Statue Park captures verify simultaneous elapsed/cheat target, BEST,
three-line Trevelyan dialogue and immediate objective completion at 960x540 and
800x600 with the small HUD. A timer-disabled capture retains the native subtitle
position. The Windows build and HUD regressions pass.
[HUD audit](scripts/port/audit_hud.ps1)
accepts a
mission, difficulty and timer switch plus an optional RAM-only `-BestSeconds`
fixture (`GE_HUD_AUDIT_BEST`) for targeted captures. Whitelisted gadgets can be
equipped through the native watch-selection path in this isolated diagnostic,
after opening AI finishes resetting equipment. Explicit ammo visibility and
retained stdout/stderr make gadget captures reviewable; temporary audit
executables are removed after exit (FORK-001). HUD, ADS and suspend-state
regression tests and Windows/Linux builds passed. Windows captures checked
the Dam Secret Agent 02:40 target at wide and 4:3 aspect ratios alongside text
and icon weapon strips, and elapsed-only output on Facility Agent. Warning
and expired colors are covered by unit tests; runtime captures cover the green
target. Linux was compile-checked. The main Windows executable was replaced
without retaining a previous build.

The 2026-10-25 spacing/typography follow-up passes HUD/ADS regressions and native
Windows/optimized Wasm builds. Native wide/4:3 and Edge captures verify Bunker's
watch magnet with elapsed/cheat/best rows separated above the objective panel.
Edge also checks half-size HUD, Ammo On Screen off and no saved best time,
without page errors. All four clock lookup/format/color helpers are unchanged
after extraction. Type checking, 146 browser unit tests and site/logo policy
checks pass. Both independent gallery passes reproduce 72 views/35 icons
unchanged; gallery integrity and eight workflow tests pass, with only source
fingerprint metadata refreshed. Private reproduction/capture evidence stays
under `build/hud-layout`.

### FORK-016 — Mission-selection cheat status and difficulty hints

**Status:** Implemented.

Added an always-visible summary for the highlighted mission on the solo mission
grid and for the chosen mission on the difficulty screen. It shows the timed
cheat's required difficulty and target, and distinguishes **Cheat to unlock**,
**Cheat unlocked** (earned in the selected campaign save) and **Cheat available
- Unlock All** by wording and the dossier's stock colors.

Earned bottom banners on both screens also name the unlocked cheat using its
existing localized cheat-menu label, for example **Cheat unlocked - Paintball
Mode**. Trailing line endings are omitted so the name stays on the status row.
Pending and Unlock All availability labels retain their existing wording.
The shared footer retains its difficulty/time row and earned emphasis color.
The mission-to-cheat mapping matches upstream's unlock cases in `front.c`.
The footer and difficulty-row badge print on the dossier paper as standard
game text: Zurich Bold capitals through `frontPrintText`, like **DIFFICULTY:**
and the Best Time column, with no background panel. Unlocked/available status
uses the game's dark red emphasis (`0xA00000FF`, as in the debrief's cheat
notice); pending status and requirement rows are black. This supersedes the
earlier dark translucent panels with green/blue/amber Bank Gothic HUD text, at
the owner's request. Wording is unchanged; the README's cheat hint description
was updated with it. On the mission grid, the stock film strip's dark lower edge
ends just above a light paper strip (about game y 224-240). The footer's two rows
therefore use 0.5 scale with seven-unit spacing, from `viGetY()-18`, centered
in that strip instead of touching the film edge. The difficulty page shares the
footer position. Isolated earned captures at 960x540 (grid and difficulty) and
800x600 (grid) were reviewed. At 960x540, 0.5 scale rounds one glyph advance in
"Paintball" slightly wide.

A required difficulty still locked by progress is identified in the summary.
The matching visible difficulty row also has a target/unlocked/available badge
and time; other rows, including 007,
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
**Checks:** The stock-text restyle passes the status tests and Windows build;
isolated earned/pending captures of the mission grid and difficulty page were
reviewed at 960x540. [ROM-free status tests](scripts/port/test_mission_cheats.sh) exercise
the actual save-bit reader with runtime and compile-time Unlock All, earned vs
available labels, selected-folder isolation, absent saves, target difficulty/time,
locked requirements and invalid entries. Summary tests also cover representative
mission-to-name mappings, name line endings, pending/available states, invalid
missions and bounded output buffers. The isolated
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

The named-banner refinement passed the runtime/compile-time status regressions,
Windows and Linux builds, and six isolated Windows captures: earned Dam and
Surface on the mission grid, earned Facility and Egyptian on the difficulty
screen, plus pending and Unlock All states. The 960x540 and 800x600 captures
confirmed readable names on the status row with requirements retained below.
The required gallery regeneration verified both independent render passes;
all 72 image cells and 35 icons retained their pixels, with only the source
fingerprint updated. Gallery workflow tests passed all eight cases.


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
selected gun pass's XY projection (the scene projection by default) and restores
scene projection/depth state after each draw. FORK-026 carries upstream's optional
Weapon FOV through this layer so hands, sleeves and gun stay aligned.
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
Fix for [issue 5](https://github.com/pRizz/Iron-Sights/issues/5): ending this
layer used to clear the `G_ZBUFFER` geometry mode. Upstream's `lvlRender`
enables it only once per frame (`dlZBufferGeometry`) before the player loop, so
in split screen every view drawn after a player holding one of these pistols
(or aiming a long gun with added ADS hands, which shares the layer) rendered
without depth testing and its rooms flickered as draw order changed.
Single player hid it because the next frame re-enabled the mode. The layer now
ends in `portAdsEndHandLayer` ([ads_hands.c](port/src/ads_hands.c)), which only
reloads the scene projection; `gunfire.c` calls it instead of its inline
clear/reload. `test_hand_layer.c` (run by `test_ads.sh`) requires that the end
of the layer emits just the projection load. A display-list trace of a 2-player Temple frame showed the
clear and player 2's 382 triangles drawn without depth testing. In isolated
2-player Temple runs, player 2's view changed on about half of 250-390 sampled
frames with the PP7 or DD44 before the fix and on none after it with the PP7,
DD44 or Cougar Magnum; the AK47 control showed none either way.

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

Generated arm commands, vertices and transition depth lists now receive a
native auxiliary frame-buffer floor of 256 KiB per player per half, retaining
larger inherited stage budgets. The inherited PC doubling alone allowed Aztec's
AR33 transition to use 93,504 bytes in an 81,920-byte half and overwrite the
following smoke/fire texture pool. GPU texture caching could hide that damage
until a quickload cleared the cache (FORK-014). All four dynamic allocation
entry points now reject exhausted/invalid requests before advancing or writing
outside the active half. Native hand geometry, materials and arm fits are
unchanged; N64 keeps its inherited allocator. The shared non-N64 path also
protects browser frame data, without claiming to resolve issue #3's flicker.
The focused [capacity/allocator module](port/src/port_dyn.c) and
[API](port/include/port_dyn.h) attach through small hooks in
[dyn.c](src/game/dyn.c). The [ROM-free regression](scripts/port/test_dyn.sh)
links the real allocator and covers per-player capacity, larger budgets,
alignment, vertices/matrices/lights, exact exhaustion, negative/overflowing
requests, swapping halves and untouched texture sentinels. Existing ADS test
runs invoke this allocator regression, including existing release CI. It and
wardrobe, ADS, HUD and suspend regressions passed. Both Aztec stress runs reached
the formerly overflowing 93,504-byte usage within the new 262,144-byte budget,
preserved effect bytes and rendered normal smoke after repeated quickloads.
The same camera passed all 21 supported gun captures and 231 raise/lower
transition captures, with peak auxiliary usage of 114,624/262,144 bytes and no
pool-overflow reports. Windows/Linux video and optimized Wasm builds passed.
The review gallery regenerated 72 views and 35 icons, checked repeated samples
and an independent second pass, and reported zero changed image cells.

FORK-036 also registers the native arm/grip model cache owners with the saved
world image. Those headers reference the restored high arena and must not
remain attached to the pre-load world; fresh-process cross-mission restoration
now passes. This is an ownership fix with no model, fit or material changes.

**Sources:** [wardrobe mapping](port/src/port_wardrobe.c),
[API](port/include/port_wardrobe.h), [native arms](port/src/port_native_arms.c),
[closed grip source](port/src/port_grip_mesh.c),
[scoped draws](src/game/gunfire.c), [model hook](src/game/model.c),
[ADS arms](port/src/ads_hands.c), [icon selection](port/src/port_hud.c),
[exporter](port/src/port_weapon_export.c), [generator guide](scripts/port/weapon_icons.md),
[depth-layer end regression](scripts/port/test_hand_layer.c).
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

FORK-031 extends publication to Linux tar.gz/AppImage packages.
FORK-030 adds a separate MSVC x86 reporter build, exact symbol/source uploads and
a packaged Windows crash-reporting smoke gate. The game itself remains a GCC x86
build. The player ZIP now includes the optional Sentry runtime, license and
recorded SDK hashes; existing attestation and immutable-release behavior remains.

The Windows player/attested EXE is now `IronSights.exe`. CMake changes the Windows
artifact name; Linux/Steam Deck now output `IronSights`, while `ge` stays the build
target. The allowlisted ZIP, standalone
attestation subject, release copy/notes and verification instructions use that
name. The package regression deliberately puts the retired `ProjectMidas.exe`
beside the current build and verifies it cannot leak into a player ZIP. Current
manual packaging/audit/shader/icon/gallery tools use the new Windows filename;
Wasm artifacts retain their deployed names. Manual Linux/Deck packaging uses the
renamed player and copies the checked-in LF-only
[Deck launcher](scripts/port/steamdeck_run.sh) instead of embedding shell source
inside PowerShell. The packager checks its resolved output stays inside
`releases/` before replacing a release directory. Seven ROM-free subprocess
checks exercise diagnostic process guards, argument/environment forwarding,
failure propagation and both bundled/system SDL launch paths; the release test
job runs them. See FORK-002 for visible branding and attribution preservation.

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
[run](https://github.com/pRizz/Iron-Sights/actions/runs/40016371907)
passed tests, compilation and publication. Its immutable release ZIP and EXE
passed attestation verification against this main workflow and source commit;
packaged metadata and all file checksums matched. Verification instructions
accept both configured push builds and deliberate manual main dispatch/retries.
Initial SSH push probes did not create Actions runs despite enabled settings
and no event policy. Registering the fork-specific workflow restored automatic
push execution: [run 40019645555](https://github.com/pRizz/Iron-Sights/actions/runs/40019645555)
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

**Faster job graph (2026-11-06).** At the owner's request: the Windows build,
Sentry runtime and Linux jobs no longer wait for the `tests` job; they start
together, the Windows build waits only for the Sentry runtime, and `publish`
now needs `tests` as well as both packages and the smoke test, so a failing
test still stops a release (a failing test no longer stops the builds
themselves). The pinned MSVC x86 Sentry runtime is cached under a key of
`sentry_config.json`, `sentry_tools.py` and the runner image
(`ImageOS-ImageVersion`, since the image fixes the MSVC version recorded in
`sentry-build.json`); a hit skips the 54 s build, and `sentry_tools.py stage`
still checks every runtime file against its recorded hashes. An image rollout
only costs one rebuild. Measured before: tests 74 s, then Sentry 71 s, then the
Windows build 48 s, then the smoke test 30 s, then publish 16 s (4 min 11 s for
the workflow). After: 188 s on the first run (Sentry built and cached) and
132 s once cached (Sentry 16 s with the build skipped; tests 58 s and Linux
104 s alongside; then Windows 53 s, smoke 25 s and publish 21 s), all jobs
passing.

### FORK-019 — Native first-launch weapon image cache

**Status:** Implemented on Windows and Linux (Linux since 2026-11-07); other
targets read no cache and use names.

Replaced rebuild-time embedding (FORK-011) with local preparation by an isolated
copy of the current game's renderer. First launch with a validated ROM renders
all 35 weapon/wardrobe images at 2048x1536, checks exact repeated black/white
pixels, recovers transparency and downsamples with premultiplied area sampling
to 512x160 RGBA. Players need no Python, compiler, admin rights or downloads.
(Since 2026-11-07 preparation runs in the background; see below. Before,
a progress window allowed Esc/close to skip and continue with names.) Scoped
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
Version 7 adds the tank image in slot 38 (39 slots, 36 rendered images), for
the weapon strip's tank card (FORK-009). The tank has no first-person gun model,
so the export harness loads the world prop, hides the parked tank's muzzle-flash
parts and draws its highest-detail LOD. The usual 35-degree camera frames it by
bounding radius as a side profile, cannon to the left. An isolated full
preparation rendered 36/36 images with exact repeats and wrote a correctly sized
cache. Sources: `port/src/port_weapon_export.c`,
`port/include/port_icon_cache_format.h` and `port/fast3d/weapon_cache.cpp`;
`scripts/port/test_icon_cache.c` covers the 39-slot layout.
Version 8 adds the watch laser in slot 23 (37 rendered images; only the
detonator slot 30 stays empty). The inherited watch-item renderer already draws
it as Bond's watch. An isolated full preparation rendered 37/37 images with
exact repeats and wrote a correctly sized cache.
Version 9 adds the rifle butt in slot 39 (40 slots, 38 rendered images) for the
weapon strip's unarmed card while Bond owns the sniper rifle (FORK-009). The
export harness draws the sniper rifle's watch-menu model turned stock-first and
tilted 20 degrees muzzle-down so the butt is raised, rotating its framed point
with the model. A first pose dipped the stock instead; play-testing preferred
the raised butt.
`test_icon_cache.c` checks the new slot. An isolated preparation rendered all 38
images with exact repeats and wrote a cache that the HUD audit then used.
The validator, runtime loader (`weapon_cache.cpp`) and tests now share one
slot rule, `portIconSlotStored`, in `port_icon_cache_format.h`. A play-test found
that the loader still treated slot 23 as empty: it rejected the new cache, so icon
mode fell back to weapon names. With the shared rule, an isolated icon-mode HUD
audit reuses the existing version-8 cache and shows weapon images.
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

**In-process icon stage for the web player (2026-11-07).** The browser
cannot start a worker process, so a web launch without a cache prepares the
icons inside the game, behind the startup screen. `portIconCacheInit` arms the
stage (the cache now lives in the persistent `/user/Data/Cache`); the front
end's direct launcher (`portDirectLaunchTick`) plays Dam in place of the boot
sequence; export mode (`portWeaponExportEnabled`) is on for that stage only, so
the worker's hooks (8-frame schedule, intro, swirl and world skips) apply
unchanged and no cheats are needed (a worker without `GE_CHEATS` wrote a
byte-identical cache). `gfx_pc.cpp` treats the window as 2048x1536 (4:3) and
renders single-sampled into the game framebuffer for the stage, so a canvas of
any shape, widescreen, low resolution or MSAA settings change nothing: earlier
attempts leaked the window's aspect into the tank (a world prop drawn through
the game's widened projection) and clipped it. Each tagged frame is read back
as RGBA (`gfx_opengl_read_framebuffer_rgb`; WebGL guarantees no RGB read)
before presenting. Failures end the stage instead of exiting. When the last
image is recovered the cache is written and, from the game thread
(`portIconStageTick`; captures may arrive on a render thread), the stage fades
out the attract demos' way (`ramromFadeToTitle`); `MENU_RUN_STAGE` then routes
to the legal screen as for a demo (`portIconStageIsCurrent`, asked as the
stage starts) and the boot sequence runs normally. The first front-end tick
after the stage ends it, also when it ended early (counted as failed); it gives
up after 60 s. Audio output is muted during it. The web wrapper shows a new
startup step, **Preparing weapon icons** (0-38), between graphics and the
start, waits in `engine.start()` while `_portWebIconStageBusy()` (at most
90 s), and persists the cache with the saves but keeps it out of backups
(`icon-cache-paths.ts`). Natively `GE_ICON_STAGE=1` runs the same stage for
testing. Checks: natively the stage took 4.6-4.7 s at default settings; its
caches were byte-identical across 1280x720, 1280x960 and 2048x1536 windows and
the player's defaults (widescreen, MSAA, threaded rendering) and within 0.1/255
of the worker's (sub-pixel edge differences on the rocket launcher, not
visible); the campaign EEPROM it leaves is byte-identical to a launch without
the stage; screenshots show the legal screen, logos, gun barrel and title
after it. `web/tools/icon-stage-audit.mjs` (private, actual ROM, Chrome): the
first launch showed Preparing graphics > Preparing weapon icons > Starting
GoldenEye, the stage took 12.8-15.7 s, a 12,779,712-byte cache reached
`/user/Data/Cache`, a reload reused it without the step (2.7-5.0 s to start)
and the first Dam mission latched the icons (the strip shows them). Web unit
tests (222), TypeScript, site verification, the startup, launcher, room,
storage, filesystem, touch and sampler audits, the touch and logo bridges and
the menu-pointer test pass; the startup audits now count seven steps. Not
checked: Safari and Firefox, phones, and the browser's 12.7 MB of IndexedDB on
a low-quota device.
The native worker process code (its settings, child process, lock and
leftover-folder cleanup) moved out of `weapon_cache.cpp` into
`port/fast3d/weapon_worker.cpp` (`weapon_worker::withLock`, `removeStale`,
`generate`), keeping the cache file under the file-length limit; after the
move the forced worker and background preparation still wrote caches
byte-identical to the baseline, the in-process stage its own, and none left a
worker folder. The private browser audit is `bun run audit:icon-stage <rom>
<out> <origin>` (`web/tools/icon-stage-audit.mjs`).

**Strip shaders compiled ahead (2026-11-07).** The strip's two icon shaders
(`00000000d1d81000 10000001`, the alpha rim, and `00000000d1d88000 10000001`,
the image) were missing from the shipped prewarm list `tools/shaders_base.txt`
(87 to 89 lines), because missions with text names never use them. So the first
mission with icons compiled them in the frame the strip first drew, a pause
of a few hundred ms on mobile web just as the strip appeared. Natively a run
with icons recorded exactly these two programs beyond the list, and a run with
names recorded none. Browser check: `bun run audit:strip-hitch <rom> <out>
<origin> [cpu-throttle]` (`web/tools/strip-hitch-audit.mjs`; private, actual
ROM, fresh Chrome profile, 4x CPU throttle; optional CPU profile and trace with
`MIDAS_PROFILE=1` / `MIDAS_TRACE=1`) served one build with only that file
swapped. The frame where the player gets control took 120 and 330 ms with the
old list and 27 ms in both runs with the new one. No phone was tested. There
are no setting or input changes.

**Background preparation (2026-11-07).** A launch without a cache no longer
waits: `portIconCacheInit` starts an SDL thread that takes the cache lock,
removes `worker-*` folders a launch that quit mid-preparation left behind,
runs the worker and reads the result, while the game boots. The finished
cache is published with an atomic flag, and the HUD latches it at a mission
start (`portIconCacheLatch`, called from `portHudReset`, which runs for every
player at stage start and on respawn): `portIconAvailable` and
`portIconCacheReady` answer from the latch, so a mission started before the
icons were ready shows names throughout, and icons never appear mid-mission.
The `icons` array is written only before the flag is set and is never
destroyed, since a quitting game may run static destructors while the thread
still writes it. The worker runs below normal priority and dies with the
game: on Windows a kill-on-close job object (created running, then assigned:
a suspended child orphaned by a game quitting in between stayed suspended
forever in testing), on Linux `PR_SET_PDEATHSIG` from the waiting thread
plus `nice(10)`. The progress window and its Esc skip are gone; About reads
`Preparing in the background (weapon names until ready)`, then `Ready`
(with "shown from the next mission" until a mission latches it).
`GE_ICON_CACHE_FORCE=1` keeps foreground preparation for harnesses such as
the review gallery; `GE_ICON_CACHE_BACKGROUND=1` lets a scripted run prepare
in the background. Weapon Strip Style now defaults to **Icons**
(`weapon_hud_icons=1`, FORK-009); existing settings files keep their value.
Checks: `test_hud.sh` (the new default, `portHudReset` latching) and
`test_ads.sh` (the option row's default) pass. Windows, fresh cache folders,
no `weapon_hud_icons` line: a Dam mission launched after about 15 s showed
icons; one launched at once kept names for the whole mission although the
cache finished early in it; a game quitting about 4 s in left no worker
process, and the next launch removed its `worker-*` folder and prepared the
cache. Slow-frame logs of a 900-frame start were alike with and without
background preparation (one frame over 30 ms each; the worst, 55 ms, in a
control run). The review gallery's forced preparation still works. Linux
(Docker, Xvfb, software GL): background preparation finished in 18 s, a
quick quit left no worker, and the next launch cleared its folder.

**Faster preparation (2026-11-07).** A first launch's icon preparation took
about 27 s of worker time (about 31 s with the game's own start). Measured
on the audited laptop, it was 38 images x 32 frames at the worker's 60 fps
cap, plus the Dam intro and Bond swirl that three Start presses cut short;
each frame also drew the whole level, which the capture then cleared. Now:
- each image takes 8 frames (`PORT_ICON_FRAMES`, `port_weapon_export.c`): 4
  frames to settle (3 sufficed, 2 failed the repeat check on the first
  image) and the black, white, black, white captures on consecutive frames;
- in export mode the game ends the overview camera and the swirl itself
  (two hooks in `bondview2.c`; the swirl still waits for the AI pass that
  Silo and Statue need) and skips `bgLevelRender` (`lv.c`), so the replay is
  neutral input;
- the worker runs at `maxfps=240`, as fast as the GPU draws (240, 500 and
  1000 measured the same; uncapped stalled on Xvfb's 0 Hz display), with a
  3,000-frame safety limit;
- each image's alpha recovery (about 40 ms) runs on an SDL thread while the
  next model renders (SDL's C threads: `<thread>` pulls in `<semaphore>`,
  which this target's own `limits.h` breaks on Linux).
The worker now takes 5.4-6 s and a forced first launch about 9 s, and every
run's cache was byte-identical to the 27 s baseline's (12,779,712 bytes). The
optional `generate_weapon_icons.py` exporter also sends neutral input now
(a Start press once gameplay began would open the pause watch) and still
produced its 35 PNGs. Parallel workers were considered and not used: each
pays about 3 s of start-up, more than the remaining rendering. On Linux the
software-GL check above now finishes in 10 s, with a cache byte-identical to
the slow schedule's.

**Linux generation (2026-11-07).** Linux builds had read a cache but never
made one, so their weapon strip always showed names. `generate()` now has a
Linux branch: it runs the game's own binary (`/proc/self/exe`) as the worker,
forked and exec'd with a private environment (the player's `GE_` variables
dropped, the worker's added) and `GE_DATA_ROOT` set to a temporary folder, so
the worker's `midas.ini`, input replay and logs live in that folder and
nothing is copied: a copied binary could miss the libraries bundled beside it.
(main.c appends `/IronSights` to `GE_DATA_ROOT` and then drops the last part
as the executable's name, so the root is the folder itself; a first version
wrote the worker's files one level down, where it never read them.) Checks: the Linux build compiles; in a Docker container with Xvfb and
Mesa's software renderer, the 32-bit Linux worker (its settings as
`generate()` writes them, a 1024x768 window and a raised frame limit, since
software GL draws a frame in about 5 s and the real 1,800-frame limit ends
first) rendered all 38 images with exact repeats and wrote a 12,779,712-byte
cache whose 39 slots decode to correct renders. A forced first launch there
reached the worker and lock as designed but timed out for the same reason.
On Windows a forced isolated generation still wrote a full cache in 31 s.
Not checked: a full first launch on Linux hardware GL. A per-user `flock` on `weapon-icons.lock`
replaces the Windows named mutex (released if its holder dies); 180 s send
SIGTERM, then SIGKILL. The worker settings,
replay and log copy are now shared by both branches
(`kWorkerIni`, `kWorkerReplay`, `kWorkerEnv`, `showProgress`, `keepLog`).

**Sources:** [cache](port/fast3d/weapon_cache.cpp), [worker](port/fast3d/weapon_worker.cpp),
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

Frontend and watch Iron Sights > System pages offer **About / Build Info**.
The frontend now honors the action row's label instead of falling through to
the invalid-option `?` fallback; its helper explains build version, commit,
timestamp and source links through `port_option_help.c`.
The executable embeds UTC time, build number/date, full Git commit, branch,
dirty status and available source, Actions run and release/download URLs, with
matching packaged JSON. CI labeling requires an exact commit and clean tree.
Local/modified/no-Git builds do not invent an attested run. Controlled builds
can fix timestamps with MIDAS_BUILD_TIME_UTC or SOURCE_DATE_EPOCH.

The startup legal screen identifies **Iron Sights - a fork of Project
Midas**, retaining the inherited Nintendo/Rare, film and music credits. Its
footer shows build number/date, short commit, modified status when applicable,
and the baked build instant converted to the player's local time zone. Native
startup/About use a readable month/day/year and 12-hour clock with an explicit
UTC offset, including daylight saving at the build instant. Windows uses 64-bit
time conversion in the x86 player. Missing timestamps show `Unavailable`;
invalid or unconvertible timestamps retain their exact UTC source value.
A focused port helper owns the copy/layout through a small `front.c` hook.
The browser's returning-player loading screen uses the same fork attribution
and formats the runtime manifest's build instant with the browser's locale and
time zone, including a zone label. Its semantic `time` element and tooltip retain
the exact ISO timestamp. Packaged JSON, generated headers and crash tags remain
UTC for reproducible provenance; display localization does not change the stamp.
Compact spacing on short browser viewports keeps this copy visible in phone
landscape; overflow remains scrollable at smaller sizes.

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
[frontend](src/game/front.c), [startup attribution](port/src/port_startup.c),
[startup interface](port/include/port_startup.h),
[local time formatter](port/src/port_build_time.c),
[time interface](port/include/port_build_time.h),
[native time tests](scripts/port/test_build_time.sh),
[browser time formatter/tests](web/src/build-time.ts),
[browser startup audit](web/tools/startup-audit.mjs),
[browser loading screen](web/src/startup-screen.tsx),
[loading styles](web/src/startup.css), [watch](port/src/port_watchpages.c).
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
The startup attribution refinement passes native Windows/optimized Wasm builds,
all five distribution/metadata tests in Linux, browser type checking and 144
unit tests. Real-ROM native and Edge startup captures were reviewed in wide/4:3;
the cached-ROM browser audit checks manifest timestamp equality and loading
content bounds at desktop, portrait-phone and landscape-phone sizes. Private
evidence is under `build/startup-attribution`; no ROM/captures are packaged.
Startup download/retry checks and eight gallery workflow tests pass. Both
independent gallery passes reproduce all 72 views and 35 icons unchanged;
gallery metadata tracks the updated sources.
The local-time refinement passes Linux and Windows formatter tests covering
standard/daylight offsets, date/year rollover, non-hour zones, invalid inputs,
bounded buffers and Windows dates after 2038. CI includes the native formatter
regression. Windows and optimized Wasm builds, browser locale/time-zone tests,
all 176 Bun tests, strict TypeScript and exact site allowlisting pass. Native
wide/4:3 startup captures and Edge desktop/portrait/landscape loading captures
show readable local times without clipping; the browser checks raw ISO metadata
and its UTC tooltip as well. The broader startup audit passed on retry after
one initial progress-label sampling gap during visible cartridge insertion.
Both independent gallery passes reproduce all 72 views and 35 icons unchanged.
Private evidence is under `build/build-time` and ignored `build_win` logs.

### FORK-021 — Enhanced community font textures (removed)

**Status:** Removed on 2026-11-05. No target embeds, loads or ships the masks.

From 2026-10-23 the fork embedded 186 eight-times-resolution Bank Gothic and
Zurich Bold glyph masks from Carnivorous's GoldenEye/GoldFinger texture pack
(Graslu's GEPDTexturePacks, CC0, `ca06e063`). A default-on **Gameplay > Iron
Sights > System > Enhanced Fonts** setting (`[display] enhanced_fonts=1`) swapped
them in for matching original glyph textures by content fingerprint, grew glyph
quads to show ink past the native advance, and kept them smooth-sampled
independently of 2D Filtering.

The owner removed them after the GoldenEye 007 HD maintainer disallowed bundling
that pack's art in ports
([GhostlyDark/GoldenEye-007-HD#3](https://github.com/GhostlyDark/GoldenEye-007-HD/issues/3),
see FORK-047 and FORK-033). The font masks came from a different pack, but they
are redrawn versions of Rare's own glyphs, so a CC0 dedication by the pack's
author could not clear their underlying rights, and the issue had credited
GhostlyDark for fonts. The planned replacement is original, MIT-licensed fonts
drawn from scratch (never traced from the ROM or any pack) and a renderer that
draws text sharply at any resolution ([plan](scripts/port/open_assets_plan.md),
FORK-050); until then every target draws the original glyphs from the
player's ROM. The accepted font styles plan (FORK-052) will let players
download the pack themselves, on request, without Iron Sights bundling it.

The removal deleted the masks, notice, license and provenance
(`port/fast3d/fonts/`), the decoder and glyph-extent code
(`port/fast3d/enhanced_fonts.*`), the generator, guide and tests
(`generate_enhanced_fonts.py`, `enhanced_fonts.md`, `test_enhanced_fonts.*`),
the renderer's upload swap, cache-key field, smooth-sampling exception, quad
growth and setting-change flush in `gfx_pc.cpp`, the setting, Options and watch
rows and Nintendo 64/Modern preset entry, the CI step, the Windows/Linux
`Enhanced-Fonts-*.txt` files, the browser site's notice entries, the
`build-inputs.json` record, and the LICENSE, third-party notice, README,
release and ini-help text. Glyphs again follow 2D Filtering (smoothed by
default), with their half-texel transparent border when filtered. An existing
`enhanced_fonts=` line in midas.ini is ignored as an unknown key. The package
test now requires that no `Enhanced-Fonts-*` file ships. `audit_fonts.ps1`
stays as a text audit that captures point-sampled and smoothed glyphs
(`filter_2d` 0 and 1) on the video, difficulty, credits or HUD screens.
Releases published before the removal still contain the masks.

**Sources:** the removal across [renderer](port/fast3d/gfx_pc.cpp),
[cache key](port/fast3d/gfx_pc.h), [renderer API](port/fast3d/gfx_api.h),
[settings](port/src/video.c), [options](port/src/port_options.c),
[watch](port/src/port_watchpages.c), [frontend](src/game/front.c),
[help](port/src/port_option_help.c), [ini reference](tools/midas_ini_help.txt),
[build](CMakeLists.txt), [CI](.github/workflows/windows-release.yml),
[Windows](scripts/port/package_windows.py)/[Linux](scripts/port/package_linux.py)/[browser](web/tools/build-site.ts)
packaging, [release inputs](scripts/port/release_inputs.py),
[package test](scripts/port/test_distribution.py), [text audit](scripts/port/audit_fonts.ps1),
[notices](tools/THIRD_PARTY_NOTICES.txt), [license scope](LICENSE),
[README](README.md) and [release notes](scripts/port/releases.md).
**Checks:** the reconfigured Windows build (Docker `goldeneye`, `build_win`)
compiles and links without the masks. The distribution test (now asserting no
`Enhanced-Fonts-*` files ship), CI dependency, Linux launcher and Steam art
tests, `test_hud.sh`, `test_hitmap.sh`, settings placement, the standards
check and the web package's `bun test` (209 tests) pass. The regenerated
gallery verifies 72 views and 38 icons with zero changed image cells. A
1280x960 `audit_fonts.ps1 -Mode Video` capture shows the original glyphs,
smoothed and point-sampled. The audit's `-Mode Hud` run stayed in the stage
intro and captured no HUD text, an existing gap in that mode. The browser build
was left to CI.

### FORK-022 — Versioned hand and weapon review gallery

**Status:** Implemented.

Added a local HTML review tool and generator absent from upstream. The matrix
has 72 views: PP7, AK47/KF7 and Klobb; regular and settled ADS; original and fork
models; blue, boiler, jungle, snow, tuxedo and Connery wardrobes. It includes all
38 native generated icons (weapons, the watch laser, the tank, the rifle butt and
wardrobe-specific chopping arms, grouped separately); the tank was added with
icon cache version 7 and the rifle butt with version 9.
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

**Faster regeneration (2026-11-06).** At the owner's request, generation went
from 5 min 53 s to 38 s. Each case waits 120 fields to settle, and
`GE_FIXEDTICK` used to wait for each field in real time (`waitForNextFrame`
spun on the clock), so a pass took at least 144 s whatever the GPU could do;
two passes and the icon run ran one after another. Now `GE_FIXEDTICK` makes
`waitForNextFrame` count every frame as one field without waiting
(`src/game/frametiming.c`, port-only code under `#ifndef TARGET_N64`; an
upstream merge touching that function must keep this early return), the
capture passes run unpaced (`GE_NOSLEEP`, `maxfps=0`, the frame cap raised so
only the 600 s timeout bounds a stuck capture), and the icon run and both
passes run side by side as three processes, each in its own folder with its
own icon cache. The two passes stay independent processes compared pixel for
pixel, the repeated-sample check is unchanged, and the recipe is unchanged.
Nothing else sets `GE_FIXEDTICK`. Checks: two full regenerations in 38 s each
with zero changed image cells against the committed gallery (so pixel-identical
to the paced captures), `verify` and `test_hand_gallery.py` passed; the earlier
sequential baseline measured icons 30 s, pass 1 158 s and pass 2 164 s, and
turning off pacing alone left each pass at 158 s.
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
user-owned TSV records 96 exact-file exceptions and six generated/vendor
directories, with reasons. New source files outside those directories remain
checked; this is a documented adoption baseline, not a whole-game refactor or
claim of complete standards compliance. The managed checker is unchanged.
The overrides preserve native interfaces and the scoped verification workflow.

The owner's concurrent-play rule now requires checking running executable paths
and using separate build/runtime directories when development could collide
with a live Windows player. The main executable and player data must remain
untouched; task-owned audit processes/copies are separate. Gallery generation
accepts `--executable` for an already built isolated binary and refuses its
default Windows player build while that binary is running. The unmodified puff
vendor directory had a narrowly scoped file-length exception, removed with it
in FORK-033. Sources include
AGENTS.md, the gallery script/guide and the user-owned checker baseline; HD
prototype delivery and its later removal are FORK-033.

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

The owner's 2026-10-25 delivery preference is now recorded in the repo-local
AGENTS/CONTRIBUTING guidance and `standards-overrides.md`: validated authorized
work is committed and pushed directly to `main`, without creating a feature
branch or PR by default. Explicit user requests and external requirements remain
exceptions; PR templates/checklists apply only when a PR is needed. This changes
the fork workflow, preserves normal checks/protections and leaves managed upstream
files intact. The in-progress web refinement is moved to direct-main delivery;
its existing PR is closed without using a PR merge.

The rules change passed the scoped Markdown check and pre-commit standards/
gallery checks and was pushed directly to main as `69842af`, together with the
verified room refinements in `ed459f2`. PR #2 was closed rather than merged.

### FORK-025 — Immersive browser room

Room rendering performance, 2026-11-04: the CRT canvas texture now holds raw
sRGB bytes (`NoColorSpace`) and `crt.frag.glsl` decodes them with Three.js's
`sRGBTransferEOTF`. In Chrome/ANGLE D3D11, each upload into the previous sRGB
texture cost about 5 ms of GPU time against about 0.6 ms for the whole scene;
RGBA uploads cost almost nothing. `loading-screen.ts` exports `crtScreenKey`, and
`scene.ts` repaints/uploads the CRT only when that key changes instead of at a
fixed 15 Hz, so the static NO SIGNAL screen uploads once. `room.tsx` raises the idle,
reduced-motion and preparing cap from 30 to 60 fps and paces it with a carried
deadline. The old strict `now - previous < 1000/30` test skipped
frames that arrived a fraction early, so high-refresh displays drew about 27 fps.
`scene.ts` compiles programs with `renderer.compileAsync` before the first frame. The
floating cartridge's shadow updates every frame; it previously updated every third frame. `artwork.ts`
decodes the label image before upload. Filtering the CRT texture before decoding
changes only text-edge antialiasing on the screen; the rest of the reduced-motion
room is pixel-identical. Unlike before, the static screen no longer re-uploads
unchanged pixels.

`web/tools/room-benchmark.mjs` measures room frame pacing in headed Chrome on the
host GPU (occlusion throttling disabled, 1536x864 at 1.25x by default). It reports
rAF main-thread work, timer-query GPU time, long-animation-frame blocking, idle
and pointer-hover scenarios. With a ROM it also measures one cold-shader-cache and
several warm-cache insertion/flight runs, attributing frame gaps to startup stages.
`room-benchmark-compare.mjs` prints a before/after table; reports stay in ignored
`build/web-room/benchmark`. On the RTX 3070 Laptop host (medians of three runs),
idle went from 24-28 to 60 fps with p95 frames from 36.7 to 16.8 ms and GPU time from about
6.7 to 2.6 ms per frame. The first 2.5 s worst frame fell from 210 to 97 ms, and
warm-cache insertion went from 23 to 51 fps. Cold-cache insertion/flight improved only from
roughly 9-15 to 13-25 fps. Native `portWebPrepareShaders` then linked about
87 programs synchronously for about 7 s, blocking the page for 85-140 ms per
program. That native compilation, not the room, was the cold-cache stutter; the
parallel shader preparation below replaces it.
Rare 0.2-1.4 s idle gaps occurred before and after; traces show rAF stops for the
whole page, so they are browser/host pauses rather than room work.
Validation: TypeScript, 200 Bun tests, Prettier, production assembly and exact
public-artifact verification; Chrome ROM-free room audit (59.6 fps SwiftShader
idle), actual-ROM insertion/handoff/reload/power and Exit Game/return audits.

Parallel browser shader preparation, 2026-11-04: `portWebPrepareShaders`
(`port/web/shader_prewarm.inl`) no longer links its 87 listed programs one at a
time. Where the context offers `KHR_parallel_shader_compile` (enabled explicitly
in `port/web/shader_parallel.inl`), it keeps a window of eight programs issued
without any status query and refills it as programs finish. It polls
`GL_COMPLETION_STATUS_KHR`, yields between polls with real completed counts, and
bounds each finish slice to about 8 ms. Only completed programs get their
compile-status checks, attribute/uniform lookups and load. Issuing all 87 at
once removed the per-program waits but queued the room's own WebGL frames behind
the whole list in Chrome's shared GPU process (single blocks of 130-760 ms);
the window removes those. Without the extension, the previous sequential path
remains. To allow this, `gfx_opengl_create_and_load_new_shader` in
`port/fast3d/gfx_opengl.cpp` is split into `gfx_opengl_shader_build_begin`
(sources, compile, attach, link) and `gfx_opengl_shader_build_finish` (status
checks, attribute/uniform lookups, load). Desktop calls both back to back. The
only desktop difference is that the compile-status checks now run after
`glLinkProgram` instead of after each compile, with the same fatal error
report. Shader sources, the program pool, rendering and desktop prewarm
scheduling are unchanged. The browser batch is included at the end of
`gfx_opengl.cpp` (FORK-027 hook pattern). The final `total/total` progress is
still reported. The native log names the window, issue time and longest finish
slice.
`room-benchmark.mjs` on the same RTX 3070 Laptop host, before → after (cold cache
is one run each, warm is the median of three): cold insertion went 13 → 48 fps
(p95 frame 640 → 20 ms), powering 22 → 60 fps and TV flight 12 → 150 fps
(p95 107 → 17 ms). Cold select-to-handoff fell from 8.0 to 3.1 s, and cold
graphics preparation from about 7.1 s to about 1.3-1.5 s. A separate probe of
four fresh-profile cold starts measured 0-2 ms of long-animation-frame blocking
during preparation; issuing all 87 at once had measured 130-490 ms. Warm
insertion went 51 → 65 fps. Single 110-190 ms gaps remain at the preparation
boundaries; they come from runtime startup before and after preparation, which
the baseline also has. `startup-audit.mjs` used to slow preparation by
delaying each zero-ms native yield 80 ms, which assumed one yield per program.
In the room, preparation now finishes inside the cinematic, so the audit holds
those yields until the TV is fitted and then delays them 400 ms. It still
exercises the fitted-screen wait, F2 and Resume startup. Validation: optimized
Wasm via `web_build.py` with the logo-policy audit; Chrome `startup-audit.mjs`,
`room-game-audit.mjs` and `room-exit-audit.mjs` on the actual ROM; 201 Bun
tests; Prettier; the web prototype tests; Bright Builds checks; and a two-pass
gallery regeneration with zero changed image cells, plus `verify`. Desktop was
rebuilt by the gallery and captured unchanged. Reports stay in ignored
`build/web-room/benchmark/parallel-*.json`. Firefox and Safari were not measured;
without the extension they keep the sequential path.

Startup stage track, 2026-11-04: the stage gauge restarts at every stage, so it
never showed how far the whole startup had come. A second row under it now does:
six segments (Saves, Download, Runtime, Cartridge, Graphics, Start), with
finished stages green, the current stage gold and pending stages outlined, plus
"Stage N of 6" and the stage name. `STARTUP_STAGES`/`stageTrack` in
`web/src/startup-progress.ts` map every label the engine, app and native runtime
report to a stage. "Ready to play", which the room shows only after start, counts
as complete. Unknown labels and "Startup paused" hide the row. It is a stage
count, not a weighted overall percentage: stage durations vary too much between
cold and cached launches (download size, 0.1-1.5 s of graphics preparation), and
Compiling WebAssembly and Starting GoldenEye cannot be measured. A skipped stage
such as a cached download just counts as passed, so the row never goes back. The
TV loading screen (`room/loading-screen.ts`) paints the row between the gauge
and its footer. `MissionProgress` adds it to the startup screen and the standard
launcher's bar, with a screen-reader "Startup stage N of 6" text. The room
canvas exposes `data-room-loading-stage`. `startup-ui-audit.mjs` asserts 2/6
during the held download, 3/6 during the held compile, and an empty value once
loading ends. `startup-audit.mjs` asserts stage 3 on the remembered-ROM startup
screen, whose no-overflow checks at 1440x900, 390x844 and 844x390 still pass.
This is a display-only change with no native or gallery inputs. Validation:
stage-order and CRT-track unit tests, all 207 Bun tests, TypeScript, Prettier
on the changed files, and Chrome `startup-ui-audit`, `startup-audit` (actual ROM)
and `launcher-ui-audit`. Screenshots of the TV, the startup screen at three sizes
and the phone launcher bar were reviewed; they are private in `build/web-startup/{ui-stages,stages-audit,stage-shots}`.

Room transitions use 0.9-second cartridge insertion, 0.3-second power-on,
1.3-second TV flight and 1.5-second return to the room. The return camera retains
its 0.3-second crossfade hold and completes its retreat within the shared phase
duration, avoiding a camera snap when interaction resumes. This replaces the
earlier 1.9/0.9/2.8/3.1-second sequence. Slow idle cartridge rotation, easing,
reduced-motion fades and the real engine-readiness gate remain intact. Sources:
`web/src/room/flow.ts`, `cinematic.ts` and their boundary/endpoint regressions.
Validation: 167 Bun tests, TypeScript, scoped Prettier, production assembly and
exact public-artifact verification pass. Edge room checks pass responsive,
reduced-motion and fallback behavior; actual-ROM entry/Exit Game audits pass
normal and reduced motion, warm power resume and exact save preservation using
the existing local optimized runtime. Native sources are unchanged and the
gallery freshness check passes. Private evidence: `build/web-room/faster-timings-*`.

**Import** and **Export** now belong to GoldenEye's four-campaign
Files screen, as clarified by the owner. The web action area has two rows
and three columns: Import/Export at left, Copy/Options in the middle and
Erase/Exit Game at right (FORK-034). This replaces
the earlier upper-right room toolbar, whose component/styles are removed; the
room returns to its canvas-only presentation. Native labels and cursor actions
use small `GE_WEB` hooks in `src/game/front.c`, `port/web/runtime.c` and
`port/web/lifecycle.c`; `web/src/engine.ts` and `web/src/runtime-types.ts` carry
the typed callback/metric boundary.
Accessible transparent browser targets follow the measured native bounds and
the 4:3 menu band, preserving direct file-picker gestures on touch devices.
Their hover state remains transparent so the native menu supplies the same
highlight as Copy, Options, Erase and Exit Game. Browser context menus are
suppressed on these targets while ordinary browser UI keeps its own menu.
The scoped focus hook in `web/src/capture.ts` permits focus on these game-menu
targets; real window blur/visibility loss still pauses the game.
`web/src/capture.test.ts` covers that scoped exception and unrelated focus pause.
They are absent outside the idle Files screen, including copy/erase/exit modals.
Desktop native builds now share the six-button layout through FORK-034's
`GE_SAVE_TRANSFER` adapter; browser callbacks and accessible targets stay web-only.

Both actions reuse the portable JSON codec: all campaigns, profiles and settings
travel without a ROM. Import confirms full replacement and restart. The backup
is staged as `.pending-save-restore.json` in the existing IDBFS mount, outside the
export allowlist; the running game's campaign files and cached EEPROM stay
intact. A fresh runtime validates/applies the marker before native startup,
then clears it durably. Failed staging rolls back the marker; failure during
application stops initialization for safe retry. The ordinary live-file restore
guard, database/mount/lock identities and existing backup format remain intact.
Legacy F2 backup and restart/restore controls remain available.

Focused sources are [browser targets](web/src/file-save-actions.tsx),
[styles](web/src/file-save-actions.css), [backup handlers](web/src/app.tsx),
[game stage](web/src/player.tsx),
[browser dialogs](web/src/save-transfer-dialogs.tsx),
[staging/persistence](web/src/persistent-files.ts),
[actual-game audit](web/tools/file-menu-save-audit.mjs),
[launcher audit](web/tools/room-save-audit.mjs) and
[live-game lifecycle audit](web/tools/room-exit-audit.mjs).
The native Files/Exit audit clicks now use direct screen coordinates, without
the former edge-sweep resynchronization workaround (FORK-024). Moving the transfer
column left left that audit clicking Options at the old Exit Game position, so it
failed waiting for the room while the product worked. It now clicks Exit Game at
its current position, fails if the click opens nothing or No does not return to
Files, and requires the audio tap to have heard the game before room silence counts.
The actual-game Files audit checks transparent hovered targets and scoped
context-menu cancellation alongside the existing save transfer flow.
The real IDBFS regression passes staged import/reload, marker exclusion/cleanup,
quota-failure rollback, failed application/retry, exact campaign/profile
replacement and the live-file guard. The retained launcher audit passes confirmation/cancel, invalid-data
preservation and exact reload while asserting the room has no transfer toolbar.
Actual optimized-game Edge checks pass direct and native-cursor export, import
confirmation/cancel, invalid-data preservation, exact campaign/profile reload,
imported HUD preference and phone/tablet bounds. Touch-mode checks pass the same
flow with controls clear of the action area. Chrome's native Exit Game audit
passes return-to-room, warm power resume and legacy save management. The touch
fixture also exercises the actual browser targets and direct file-picker gesture
without a ROM (`web/tests/touch-driver.tsx`, `web/tools/touch-ui-audit.mjs`).
WebKit's touch-end gesture opens the picker directly; compatibility clicks are
suppressed to avoid double activation. Edge/WebKit fixture acceptance covers
touch import, single export activation and hiding the targets outside Files.
Native controller/cursor import opens a small Choose backup file prompt to
obtain the browser gesture; direct pointer/touch targets open the picker
immediately. Its confirmation UI is extracted into the focused dialogs module.
Current publication is tracked in WEB-003. Physical Safari
acceptance is not claimed; the older broad Windows WebKit room fade-marker
limitation remains separate from these save-transfer checks.
The save audit waits for the export button to become visible and enabled before
starting its download-event timeout, including after reload. Click/download
promises are observed together. This prevents slow engine initialization from
consuming the download deadline; Edge passes with the reload's Wasm response
deliberately held for 32 seconds, and the focused WebKit save audit passes.
Private timing evidence is `build/web-room/opacity-slow-save`.

The controller GLB's generator label now identifies Iron Sights. Regenerated it
from the same pinned/hash-checked sources and updated the public allowlist and
model digest table. Geometry, texture bytes, source authors and CC BY 4.0
attribution remain unchanged; the console GLB is byte-identical. This supersedes
the old Project Midas generator label without changing prop appearance.

The ROM dialog and retained launcher no longer show the former marketing
tagline. Outside clicks/taps dismiss the idle dialog, while padding clicks and
drags beginning inside leave it open; the existing validation guard remains.
Native dialog focus restoration is preserved. Panel/backdrop use a 160 ms
crossfade through discrete top-layer/display transitions, removed under reduced
motion. The dialog fits portrait and short landscape viewports.

Fullscreen startup, retained launcher progress and in-TV startup share a dark
watch/HUD treatment: beveled panels, compact capitals, gold headings and segmented
green gauges. [Shared theme](web/src/mission-theme.ts),
[accessible meter](web/src/mission-progress.tsx) and [styles](web/src/mission.css)
keep DOM/CRT palette and progress presentation consistent. Real percentages and
indeterminate native progress semantics remain; reduced motion holds the activity
indicator still. Existing scanlines/interlacing, blue idle no-signal, loading stages,
startup pause/retry, attribution/build time and storage/game lifecycle stay intact.
The [ROM dialog](web/src/room/rom-dialog.tsx), [dialog CSS](web/src/room.css),
[startup panel](web/src/startup-screen.tsx), [startup CSS](web/src/startup.css),
[launcher](web/src/player.tsx) and [CRT painter](web/src/room/loading-screen.ts)
own the presentation. The accepted room plan and WEB-010 record scope/verification.
The room paints during active cartridge checking beneath the dialog fade, and
stage/percentage changes invalidate the CRT immediately rather than waiting for
its normal 15 Hz texture refresh. This closes a fast-selection gap in initial
loading feedback; an idle open dialog still suspends room rendering.

Validation: browser type checking, 146 unit tests, production assembly and exact
site allowlist/Wasm digest verification passed. Edge room checks cover outside
mouse/touch dismissal, inside/drag guards, fade entry/exit, focus restoration and
reduced motion. Actual-ROM first/cached startup checks pass uninterrupted loading
feedback, overlapping preparation/insertion, fitted-screen hold and interrupted
handoff recovery. Remembered-ROM loading panels fit 1440×900, 390×844 and 844×390;
indeterminate progress has no percentage value and stops animating under reduced
motion. Captures/traces remain private under `build/web-ui-refresh`.

Current first-visit startup prepares the native game as soon as a valid ROM and
runtime are available, overlapping cartridge insertion, power and flight.
`room/loading-screen.ts` draws real download/initialization stages and progress
into the TV texture. Existing CRT noise/scanlines gain subtle alternating fields;
reduced motion disables these animations. `flow.ts` holds a screen-fitted phase
until preparation completes. The first game frames wait behind the cinematic;
handoff resumes the same instance and waits for fresh graphics before fading.
The room keeps rendering, capped at 60 fps while preparing (30 fps before the
2026-11-04 performance pass), replacing the prior
static-TV hold. Actual pauses offer Resume startup within the room, including an
interruption during handoff. The game canvas/storage lifecycle stays intact.
Chrome and audio-enabled Linux WebKit pass deliberately slowed actual-ROM
first/cached startup, overlapping preparation/insertion, fitted-screen progress,
F2 and handoff focus recovery, and native Upright settings checks. Edge passes
the room/fallback and native Exit Game/warm-power/save-management regressions.

The earlier centered-feedback slice shipped in `ce782cb`; acceptance history is
WEB-010. `startup-screen.tsx`/`startup.css` retain centered download/initialization
feedback for remembered-ROM launches that skip the room, plus actionable errors.
Progress uses actual download percentages and completed graphics-preparation
counts; unknown work remains indeterminate on either surface.
`engine.ts` paints before entering Wasm and keeps feedback until graphics begin.
Real pauses during preparation expose **Resume startup**, so an F2/focus
interruption cannot strand initialization behind its progress screen.
The existing native incremental prewarm of 87 known shader programs stalled
visible browser logos because a single compile can exceed its 3 ms budget.
Browser-only preparation in `port/fast3d/gfx_pc.cpp`, `port/src/main.c` and
`port/web/lifecycle.c` now finishes that list before animation, yielding and
reporting each completed program. `room.tsx` now animates the CRT during that
preparation. Desktop scheduling/rendering is unchanged.

`UserFilesInUse` distinguishes another tab's save-writer lock. Startup failures
now open an accessible centered dialog with explanation, Retry and controls;
no scrolling is needed, and retry can acquire the lock after the other tab closes.
The save protection itself is unchanged. `player.css` removes the game canvas's
gold focus outline while keeping UI keyboard-focus indicators.
`capture.ts` replaces its 100 ms gameplay-arrival pause with a live mouse-free
state and a small capture affordance. Cutscene/watch transitions keep keyboard
play running; click/key gestures can capture the mouse. Real focus loss, manual
pause and unexpected pointer release still pause, including focus loss during a
pending capture request. `capture.test.ts` covers those boundaries.

The private startup observer/report/audit records actual program progress,
frame counters, browser focus and GL stalls. In comparable Edge/SwiftShader
cold samples, the first eight logo seconds improve from about 8-11 graphics fps
to 57-60 fps; observed long stalls move behind approximately 7 seconds of cold
preparation, or 0.92 seconds on a cached reload. This moves startup work before
animation rather than claiming to eliminate compilation or establish hardware
performance. Traces remain under `build/web-startup/{baseline-sources,prepared}`.
Actual control-transition audits pass in Edge and isolated Linux Firefox. The
full Edge room/Dam/audio/watch/focus/capture/reload audit, Chrome launcher
campaign/profile/settings/backup/ROM removal, and repeated native Exit Game/warm
resume pass. Existing private game audits now navigate by exported native stage/
control counters instead of assuming fixed logo timing or a false Resume overlay.
`startup-ui-audit.mjs` holds real download/compilation to test progress and F2;
the Pages workflow runs it without a ROM. The gallery's two independent passes
reproduce all 72 views and 35 icons with zero changed image cells; its source
fingerprint/HTML are refreshed. Optimized Wasm, 272 strict callback compilations,
logo/spike and gallery regressions pass. All 119 unit tests, strict TypeScript,
formatting and exact artifact checks pass. Edge and Windows WebKit pass loading/
compilation/F2, centered-lock/retry and room/fallback checks. Chrome also passes
reduced-motion cutscene/control/watch/focus acceptance. The final startup audit
asserts preparation finishes before graphics, with loading feedback throughout.
No private captures or ROM data are added; Safari remains experimental.

Source `ce782cb` passed Pages `40223366320`, Windows builds/provenance
`40223366337` and Bright Builds `40223366331`, deploying clean runtime
`653be0dbfb70c828`. Public Wasm size/digest/MIME, frontend chunks, models and
artwork match the exact artifact. The live Edge UI audit passes progress/F2 and
centered save-lock recovery; live Chrome traces prove preparation finishes before
graphics and loading remains visible for both first and remembered-ROM startup.
Live Edge also passes reduced-motion startup interruption/resume, the complete
Dam cutscene/control/watch transition, capture and real focus/Escape pauses.
Evidence is private in `build/web-startup/published-{ui,startup,control}` and
`build/web-room/public-artifact-report.json`. This documentation records acceptance.

**Status:** Refined desktop room and cinematic are implemented, verified and
published on GitHub Pages. Safari remains experimental.

The licensed console/controller, background-drag cartridge pivoting, proportional
game presentation and screen-fitted TV-flight endpoint shipped directly on main
in `ed459f2`/`69842af`. Pages `40164551815`, Windows builds/provenance `40164551755`
and Bright Builds `40164551825` passed. Publication acceptance is tracked in WEB-010.

The lifecycle follow-up on `main` makes native EXIT GAME return to a paused
browser room with a reverse TV transition. `port/src/video.c` routes only web
quit requests through `port/web/lifecycle.c`; desktop SDL_QUIT/atexit behavior
remains inherited. `engine.ts`/`app.tsx` pause and release input/audio, flush saves
and retain the live native instance and its save-writer lock. `flow.ts`,
`cinematic.ts`, `scene.ts` and `room.tsx` rebuild the room over the paused
framebuffer, crossfade and reverse the flight/power sequence. The inserted
cartridge's power switch resumes that same instance; handoff waits for new
graphics tasks, without a second `main` call or new game canvas. Reduced motion
uses a short fade. Explicit exit cannot automatically re-enter the game.

Ordinary fresh page loads now auto-start a successfully validated remembered ROM
without constructing the room or fetching prop GLBs. `rom-manager.ts` reports
restore success; `session.ts` makes entry decisions explicit and waits for engine
readiness. F2 holds startup while controls are open. Storage controls add
**Restart to manage saves**, which flushes and reloads into `?controls=1` so
backup restoration remains available before native EEPROM initialization. Closing
controls removes the query and allows startup. This supersedes the earlier fresh
returning-player power prompt; room power resume remains after explicit exit.

Lifecycle verification: 112 unit tests, strict TypeScript, optimized Wasm and all
272 strict callback compilations pass. Both native gallery capture passes produce
zero changed image cells; the source fingerprint/HTML are refreshed, integrity
and eight gallery tests pass. Edge's actual native No/Yes confirmation, reverse
flight, frozen frames/silent audio/released input, physical warm power resume,
repeated exit, unchanged save files and management restart pass. The same native
exit cycle passes isolated Linux Firefox and Chrome with reduced motion. The full Edge launcher audit passes
Dam/audio/capture/focus/watch, campaign/profile/settings reload, actual backup
restore and ROM removal. The room gameplay audit verifies cached auto-start and
no prop downloads after reload. Windows WebKit's ROM-free room/fallback checks
pass; actual Safari remains experimental. New `room-exit-audit.mjs` and the Linux
helper's `exit` mode keep private evidence under `build/web-room/exit-*`; no ROM
or private captures enter Git.

Lifecycle source `ea804df` passed Pages `40167224238`, Windows builds/provenance
`40167224278` and Bright Builds `40167224228`, then deployed clean runtime
`04853c2b485e431a`. The public Wasm size/digest/MIME, frontend graph, models and
artwork match the artifact. Live Edge passes native Exit Game/reverse/warm-resume,
repeated exit, stable saves and management restart. Live Chrome passes normal
Dam movement/audio/watch/focus/capture and unchanged save backups after cached
auto-start, without loading room props. WEB-010 records final acceptance and
private evidence locations. This documentation update persists those results.

The user requested a canvas-only initial experience with realistic room props,
a CRT, rotating cartridge ROM selection and an animated insertion/power/TV
transition. Its original returning-player power prompt is superseded by the
cached auto-start behavior above; explicit exit still presents the inserted prop.
This replaces the initial visible launcher presentation introduced by FORK-024;
its storage/game interfaces and retained HTML controls remain authoritative.
F2 opens controls, Alt+Enter toggles fullscreen and P remains the native watch.
The upper-right Exit fullscreen overlay button is removed at the owner's
request; Alt+Enter and the browser's Escape behavior provide fullscreen exit.
The retained launcher's Fullscreen control remains available. The launcher UI
audit checks the overlay is absent and Alt+Enter exits fullscreen.
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
GLBs; `console.ts` retains the physical switch and places the red power LED at
the front center between the controller sockets, replacing its incorrect
player-one-port location. An original gray controller plug seats flush at the
measured player-one socket center/face/rim radius. Its rounded shell has a
sculpted thumb recess and a smaller rounded gray cable collar before the black
strain relief. `controller-plug.ts` owns these procedural details, using the
owner's real-plug photo only as a shape reference. The cable starts inside the
relief and follows its axis before bending toward the floor. The model-local
attachment keeps the plug/cable aligned with console placement. Other cables
retain their curves. Geometry regressions compare the seating ring with the
actual licensed GLB's rim vertices and check normals/transformed cable alignment.
The refined plug passes TypeScript, all 146 unit tests, production assembly/
allowlist and Edge's interaction, responsive, reduced-motion, framing and
context-loss room audit. Front/side neutral-light and powered/unpowered room
captures were visually reviewed under ignored `build/web-room/plug-detail` and
`console-details`. Native/gallery inputs are unchanged; gallery verification
passes without regeneration. Sources include
[plug geometry](web/src/room/controller-plug.ts) and its
[actual-model regressions](web/src/room/controller-plug.test.ts).
The procedural additions leave the licensed GLBs and their hashes unchanged;
`MODELS.md` includes them in the runtime modification notice. `controller.ts`
uniformly fits the controller. The CRT/cartridge remain original.

The front-detail correction passes TypeScript, 144 unit tests, site assembly/
allowlist and Edge room interaction/responsive/framing/fallback checks. Powered
and unpowered console closeups are visually reviewed in ignored
`build/web-room/console-details`; native rendering inputs do not change.

The Joseph Rohm candidate remains optional and is not claimed as shipped. Native
GLSL CRT effects include restrained tracking/noise, scanlines and a power-on
phosphor line. Reduced motion disables those temporal effects. `cinematic.ts`
aligns the cartridge over the slot before descent and eases the TV approach;
the room crossfades only after game graphics begin. Idle room drawing is capped
(now 60 fps; originally 30), static geometry is batched without absorbing interactive/LED parts,
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
28 fps at the then-intentional 30 fps idle cap (raised to 60 fps on 2026-11-04), with about 1.2 ms average JS render
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

The shared native frame-data capacity/guard fix in FORK-017 also compiles in
the browser target. Optimized Wasm compilation passed; no browser flicker fix
or new browser gameplay acceptance is claimed by the native Aztec investigation.

The browser renderer restores WebGL transparent-border shader uniforms from
each draw's cached texture sampler state, including when no GL texture parameter
change is needed. Previously a HUD glyph could leave `uWebBorder0` at
`[1,1,0]` for a following sky or water draw whose sampler was already cached,
making a single frame sample transparent black. The web-only hook in
`port/fast3d/gfx_pc.cpp` flushes queued triangles before the uniform changes;
`port/fast3d/gfx_opengl.cpp` and `port/web/sampler.*` own the sampler flag and
uniform synchronization. No user setting or default changes. In a matched
headed Chrome Dam cutscene, two one-frame reservoir drops in the baseline
became zero with the fix while the location caption remained visible. A
scripted early-gameplay firing capture removed the three large one-frame sky
patch anomalies seen in its baseline. The reported gate/mountain-wall symptom
was not separately isolated; broader browser and GPU acceptance remains open.
The clean production build reached gameplay without a page error. The render
gallery's two independent passes verified 72 views and 35 icons with zero
changed image cells; only its source fingerprint and HTML changed. Browser
unit tests, typecheck, site artifact verification, gallery integrity/workflow
checks and Bright Builds checks passed. Private ROM-derived captures stay
ignored in `build/web-flicker/`. See WEB-002 in
`scripts/port/web_target_progress.md` for evidence and the next action.

The shared immediate OpenGL renderer now synchronizes `frame_count` and
`noise_scale` with each program before a draw when either value changed since
that program's last upload. Previously these noise/dither uniforms updated on
shader load or restore, so a program that stayed bound across frames could
reuse an old frame count. The retained draw path already synchronized them per
draw. A headed Chrome probe temporarily kept these uniforms active in shaders
without changing the game source: eight opening/menu samples had a stale frame
count before this change, and none of 3,110 sampled draws did afterward. The
ordinary Dam shaders sampled in that audit did not use noise, so this is a
separate conditional state bug, not an identified cause of the Dam texture
flicker. The optimized Wasm/site build, 181 browser tests, typecheck and ten
WebGL sampler cases passed. The native build and two-pass gallery verified 72
views and 35 icons with zero changed image cells; only the gallery's source
fingerprint and HTML changed. No user setting or default changes.

Unlocked browser menus now follow absolute SDL pointer coordinates. Native
menu/controller snaps remain virtual until actual mouse motion or a fresh
button press takes ownership, including the first motion after focus recovery.
This replaces the browser's inherited ignored-warp fallback, which integrated
physical deltas around a virtual snap and retained an offset until the pointer
was swept to window edges. An actual-ROM baseline reproduced offsets of up to
88 native menu pixels. The frontend's small `GE_WEB` hook avoids swallowing
the first absolute handoff; native desktop warp/Wayland fallback and captured
gameplay deltas retain their existing behavior. An ungranted legacy capture
request cannot force a free browser menu back into relative tracking, including
with a saved `smart_mouse_lock=0` preference. No preferences are rewritten.
The browser game stage cancels the browser context menu on right-click, leaving
the native menu's back action available. This applies only inside the game;
the surrounding page retains its normal context menu.
The actual-ROM menu-pointer audit right-clicks from the mode menu back to Files,
then reopens the mode menu; its Chromium run passes all 34 cursor samples.

The focused policy is [menu_pointer.h](port/web/menu_pointer.h), connected in
`port/src/input_pc.c` and `src/game/front.c`. The ROM-free
[C regression](scripts/port/test_web_menu_pointer.c) covers repeated snaps,
stationary clicks, ownership, focus reset, first-motion delivery and reentry;
its [runner](scripts/port/test_web_menu_pointer.sh) runs in Pages CI. The private
[actual-ROM audit](web/tools/menu-pointer-audit.mjs) requires a matching local
Wasm digest/linker map and observes native state without writing it. It drives
Files, mode, mission, difficulty and briefing screens, rightmost mission
selection, native back/forward navigation, resize, canvas reentry, F2 and
fullscreen with direct coordinates. Prior compensating edge sweeps/offset loops
are removed from the Files, Exit and crosshair-slider audits. Private evidence
stays under `build/web-menu-pointer` and `build/menu-pointer-*`. Earlier debug lock
settings and optimized Edge/Chrome (including double pixel density) passed all
32 baseline menu samples. Captured mouse conservation/Watch recapture, direct slider
dragging/backup/reload, native Files transfer and Exit Game/room resume pass.
Native Start/HUD/ADS/
upstream checks, 155 web unit tests, TypeScript, site allowlist/digest and standards
checks pass. The Windows rebuild and independent gallery passes reproduce all
107 images unchanged; freshness/integrity and eight workflow tests pass.
Publication is tracked in WEB-006.

The launcher has its own Iron Sights icons, which replaced the inherited Project
Midas satellite-dish icon (`port/win/ProjectMidas.ico`) on 2026-11-08. FORK-059
later the same day replaced the desktop icons too. `web/index.html` links
`web/icons/favicon.ico` (16/32/48 px, a rounded tile for browser tabs) and
`web/icons/apple-touch-icon.png` (an opaque 180 px square), and names the Home
Screen entry "Iron Sights" with `apple-mobile-web-app-title`. iOS ignores ICO
favicons for Home Screen icons. Without a touch-icon link, it tried
`/apple-touch-icon.png` at the domain root, outside the `/Iron-Sights/` Pages
path, and then drew a letter tile from the title, so the owner's iPhone showed
a plain "I". Both icons show FORK-046's three-dot sight picture without text,
framed close so the dots stay legible at 16 px.
`scripts/port/make_brand_art.py --icons-only` regenerates them, deterministically.
The PNG and ICO outputs are committed, so the web build needs no Pillow. Vite
emits them under hashed, deployment-relative asset paths. `frontend-files.ts`
allows only these two non-code sources (`ICON_SOURCES`), each under its own
output name. `verify-site.ts` checks both HTML links and that the emitted bytes
equal the committed files. The launcher browser audit fetches both icons beneath
the Pages project path and checks their MIME types and bytes. On 2026-11-08,
the following passed: 19 manifest tests among 228 web tests, TypeScript,
production assembly, the exact artifact policy, and the launcher audit in
system Chrome under `/Iron-Sights/`. `test_steam_art.py` checks the ICO sizes,
the opaque 180 px PNG, and both links. The new icons have not yet been tried on
a real iPhone Home Screen.

Search and link-preview metadata (2026-11-08): the title is now "Iron Sights ·
Play GoldenEye 007 in your browser" (previously "Iron Sights · Browser preview"),
and the description says what the site does and that players bring their own
ROM. Static Open Graph and Twitter tags give the title, description, a
`summary_large_image` card and the preview image's size and alt text. Chat apps
and social sites read these without running JavaScript. The page had
previously offered only its title. The absolute-URL tags (canonical, `og:url`,
`og:image`, `twitter:image`) need the deployed address. `vite.config.ts` injects
them only when `MIDAS_SITE_URL` is set, so a local or forked build never points
at this deployment. The Pages workflow sets it from the repository owner and
name. `web/tools/site-meta.ts` validates the address (https, ending in /, no
query) and builds the tags. A `<noscript>` block gives crawlers and readers
without JavaScript a description, links to the releases and source, and a
statement that the fan project is not affiliated with Nintendo, Rare, Microsoft
or the James Bond franchise owners. `web/public/manifest.webmanifest` names the
app and its 192/512 px and maskable icons for Android and Chrome home screens.
Since 2026-11-08 it uses `display: standalone`, at the owner's request, after
starting with `browser`. This mainly affects Android and desktop Chrome and
Edge, which offer to install the launcher as an app only when the manifest asks
for an app window. It also affects iOS 18 and earlier, where a Home Screen icon
opened in Safari unless the site asked otherwise. Since iOS 26, Safari's Add to
Home Screen sheet turns "Open as Web App" on by default for every site, with or
without a manifest. The owner's Home Screen copy already opened full screen
before this change, so on iOS 26 the manifest supplies only the name and icons.
The player can still turn that switch off for a plain Safari bookmark.

A Home Screen web app on iOS keeps storage separate from Safari. This comes from
iOS 26's default, not from this setting. The remembered ROM, campaign saves,
profiles and settings therefore do not carry over between the two: players pick
the ROM again and can move saves with Export Save and Import Save. Removing the
Home Screen app can remove its storage. No first-run notice explains this.
Standalone behaviour has not been tried on an Android device or older iOS.

The 1200x630 preview image shows the gold title beside the sight picture,
over two lines reading "PLAY GOLDENEYE / IN YOUR BROWSER" (the owner's
wording; previously "PLAY IN YOUR BROWSER" on one line). The two lines share one
size, so the text stays legible when previews are shown at half size. The
preview image and the manifest icons are `make_brand_art.py`
outputs (FORK-059), published unhashed from `web/public` so the absolute
`og:image` URL stays stable. `verify-site.ts` allows the manifest and exactly
the `web/public` images recorded in `scripts/port/brand-art.sha256`
(`brand-assets.ts`), and checks their bytes against that record. It also checks
the manifest link, that every manifest icon is published, and every absolute tag
when `MIDAS_SITE_URL` is set. The launcher audit loads the manifest, its icons
and the preview image under `/Iron-Sights/`, and asserts the `standalone`
display mode. The audit server maps `.webmanifest` to `application/manifest+json`.

Robots and sitemap files are not added: crawlers read `robots.txt` only at the
`prizz.github.io` root, another repository, and one page needs no sitemap. The
owner can add the site to Google Search Console. A Lighthouse 12.6.1 baseline
of the deployed site before this change scored 100 for accessibility, best
practices and SEO. On mobile it measured a 1.4 s first contentful paint, a
4.7 s speed index, no layout shift, and 6.4 s of JavaScript boot-up (desktop:
0.4 s, 1.8 s and 1.8 s). It gives no performance score, because the page's
largest paint is the WebGL canvas, which Lighthouse never counts as Largest
Contentful Paint. The only non-passing audit, missing source maps, carries no
weight. The local build with these changes keeps all three scores at 100. The
14 new unit tests among 242 web tests, TypeScript, assembly with the Pages
address, `verify:site` (and its rejection of a mismatched address or an
altered image), the launcher audit in system Chrome and `test_steam_art.py`
pass. Real link previews and Android's manifest handling can be checked only
after deployment.

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
to the watch; Enter/Return now acts as the full Start button on web, including
opening and closing the watch. The former browser-only exception in the shared
Return-only suppression hook now applies on desktop too: Enter's existing Start
binding opens/closes the watch on every platform. Explicit custom Start bindings
still win; no fixed shortcut or saved-binding rewrite is introduced. The native
regression and FORK-029 record this extension. The browser capture handler leaves Enter to native Start processing,
matching P, while Alt+Enter remains the fullscreen shortcut.
`web/src/browser-settings.ts` owns the browser bootstrap INI and supplies
`look_updown=1` (Upright), including migration of partial configs missing the
key. Explicit saved Reverse/Upright choices remain intact. Upstream 1.0.2b
already initializes its player-option array to Upright; our partial precreated
INI omitted the option's override-mask bit, allowing the original N64 Reverse
default through. This corrects browser bootstrap rather than changing upstream
options code. Tests cover missing keys, explicit choices, CRLF and idempotence;
the private startup audit checks the native-written settings on first and cached
launches. Touch inversion follows the same option as recorded in FORK-028.
`launcher-ui-audit.mjs` checks byte-preserving restore, then the exact missing-key
migration at engine initialization and stable migrated files after reload/ROM
removal. This replaces its pre-migration expectation that a partial INI remain
byte-identical across initialization; the first Pages run caught that stale
expectation and withheld deployment.
Enter activation of the HTML Resume button is kept out of SDL's global key
handler, preserving button activation without an extra in-game Start press.
The Enter extension passes 138 unit tests, TypeScript, native ADS regression,
optimized Wasm/native Windows builds and Chrome/Edge actual-ROM Start/Watch/
Resume/fullscreen checks. Independent gallery refresh changes zero image cells.
CSS aspect ratio avoids SDL's 1x1 sizing probe producing a square canvas.
Only a page gesture requests capture through a C entrypoint that updates SDL's
mouse focus/relative-mode bookkeeping. Automatic SDL deferred capture requests
are disabled on web: a pending automatic request could collide with Resume.
The page waits for confirmed capture and handles intentional menu releases.
Browser capture now explicitly requests `unadjustedMovement`; SDL's native
raw-input hints do not configure the browser's Pointer Lock options. Unsupported
raw input retries ordinary capture, and intermediate error events do not abort
that retry. Permission failures still reach the normal recovery UI. Native
input and touch controls retain their existing paths; mouse deltas are neither
clamped nor smoothed by this change.

F2's retained player toolbar offers **Download mouse diagnostics**. A passive,
session-only recorder keeps at most 1,024 locked mouse events and 512 frame
samples (at most about 60 per second), recording delivery age, deltas, heading,
frame gaps, canvas scaling, request mode and runtime identity. Pausing freezes
frame history until resume, so opening controls cannot erase the incident.
High-frequency motion reuses typed-array storage. No keys, world positions,
ROM/save contents or uploads are involved; nothing persists across reload.
The toolbar wraps on narrow screens.

Investigation: both normal 40-count movement and the same input across a 250 ms
main-thread hitch turn Bond about 2.6 degrees, with no motion replay afterward.
This does not reproduce the reported intermittent physical-mouse snap or prove
that acceleration caused it. The raw-input setup mismatch is corrected; the
downloadable trace supports identifying browser spikes versus game rotation
if it recurs. Unit checks cover raw/legacy/unsupported/denied capture and error
ordering; private real-ROM audits exercise conservation, Watch recapture and
diagnostic export. Evidence is local under `build/web-mouse`.
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
Both the launcher and room ROM inputs omit HTML `accept` filters: iOS Files can
disable unregistered `.z64`/`.v64`/`.n64` extensions when a filter also includes a
recognized extension such as `.bin`. Selection permits any file; the existing
12 MiB size, N64 header/byte-order and normalized US-release SHA-256 checks
remain authoritative before a ROM can be remembered or played. Save-backup JSON
selection keeps its separate filter. The UI audits exercise both real chooser
buttons, assert no `accept` attribute and verify invalid files still fail.
Synthetic ROM tests also reject misleading `.z64` names across cloud MIME types.
TypeScript, 122 unit tests, production assembly/allowlist and scoped formatting
pass, as do the updated Edge launcher and desktop WebKit room audits.
This fixes the site's picker restriction; actual iPhone/iCloud selection still
requires device confirmation and does not imply mobile gameplay acceptance.

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
Actions-based Pages configuration originally used
`https://prizz.github.io/Project-Midas-ADS/` with HTTPS enforced; FORK-002 records
its rename to `https://prizz.github.io/Iron-Sights/`. First CI
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
[raw mouse capture](web/src/mouse-lock.ts),
[mouse diagnostics](web/src/mouse-diagnostics.ts),
[mouse unit regressions](web/src/mouse-lock.test.ts),
[capture regressions](web/src/capture.test.ts),
[mouse gameplay audit](web/tools/mouse-input-audit.mjs),
[Start/Enter gameplay audit](web/tools/start-input-audit.mjs),
[player controls](web/src/player.tsx), [player layout](web/src/player.css),
[runtime interface](web/src/runtime-types.ts), [capture lifecycle](web/src/capture.ts),
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

**Browser CI speed attempt, reverted (2026-11-06).** At the owner's request
two changes were tried and removed after measuring them on GitHub. Before: the
workflow took 7 min 38 s; its test step 242 s (room audit 164 s, startup 57 s,
the other five about 20 s) and its build step 158 s (SDK pull 33 s, compile
and link 90 s, logo audit 32 s). (1) Running the audits side by side: all three
lanes at once failed the touch audit's check right after a `touchCancel`
(`touch-ui-audit.mjs`, movement `[0, 1]` instead of `[0, 0]`, run
42279844036) while the room audit's software WebGL kept every core busy; with
the quick audits first and only room and startup together, the room audit took
248 s and the step 268 s, slower than running them one after another, since
both render the room in software on the runner's four cores. (2) A compiler
cache (ccache 4.14.2's static binary, pinned by SHA-256 and mounted into the SDK
container through `EM_COMPILER_WRAPPER`, `web_build.py --ccache`): locally a
rebuild into an emptied folder hit 302 of 306 compiles and fell from 654 s to
266 s, but on CI the build step went from 158 s to 149 s with the cache filled,
within noise (the link and Wasm optimization dominate there), and the
filesystem fixture's build got 27 s slower. Both were removed (the workflow
step, `web_build.py`, `web_toolchain.json` and `web_runtime.md` are back to
their earlier form; `web_ccache.py` and its test are deleted). Then untested
whether the touch-cancel failure is only a test race or a real stale-movement
race on slow devices: investigated in FORK-028, which found no way for the
controls to keep movement after a handled cancel and made the audit wait for
each state. Remaining browser costs are the room audit's software
rendering and the link; a faster room audit would need fewer room loads or a
smaller viewport.
