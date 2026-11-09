# ADS long-gun alignment audit

The 2026-10-22 audit checks the remaining long guns after the AR33/M4 and
RC-P90 corrections. It includes SMGs and shotguns that share the same ADS pose
system. Pistols keep their existing poses.

## Reference and method

The nominal shot ray passes through the screen center in full ADS. The engine
still applies each weapon's normal spread; sight alignment is not a guarantee
that every bullet follows exactly the same ray. See
[`bullet_path_from_screen_center`](../../src/game/gunfire.c) and the ADS input
path in [bondview2.c](../../src/game/bondview2.c).

The [audit script](audit_ads.ps1) captures settled, fully raised weapons and
can capture the raise/lower transitions. Persistent HUD elements are hidden.
The overview marks screen center; `sight-details.png` enlarges the central
region of each long gun. Magenta ticks stop short of the reference point so
they do not cover the sight itself. Full-resolution source captures remain
available alongside the contact sheets.

Check the aiming feature, receiver clearance, horizontal centering, and hand
placement. Circular/square apertures use their opening; open sights use the
tip/channel. A barrel or ladder without a conventional sight receives a visual
clearance review rather than an invented optical reticle.

## Findings and corrections

Height values are the `sightHeight` entries in
[joy_port.c](../../port/src/joy_port.c), in view units. Reducing a value raises
the rendered weapon. The earlier height corrections retained each weapon's
pitch and eye distance; the proximity refinement below separately changes
unscoped long-gun distance.

| Weapon             | Result                                                                                                                             |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| KF7 / AK47         | Sight hood already centered; retained.                                                                                             |
| Klobb              | Raised the end of the sight channel: height 3.2 → 2.7.                                                                             |
| ZMG                | Raised the open notch: height 4.4 → 4.0.                                                                                           |
| D5K                | Raised the circular sight: height 6.3 → 5.8.                                                                                       |
| Silenced D5K       | Raised the shared sight geometry: height 6.3 → 5.0. Its longer muzzle node needs a different compensation from the unsilenced gun. |
| Phantom            | Forked sight tips already meet the reference; retained.                                                                            |
| Shotgun            | Raised the barrel-top aiming reference: height 3.7 → 2.6.                                                                          |
| Automatic shotgun  | Existing raised-barrel reference retained.                                                                                         |
| Sniper rifle       | Scope crosshair centered, weapon hidden in full ADS; retained.                                                                     |
| Laser              | Open ladder leaves the reference visible; retained.                                                                                |
| Grenade launcher   | Visual barrel/receiver clearance reviewed; retained. Projectile trajectory is not calibrated as a hitscan sight.                   |
| AR33/M4 and RC-P90 | Rechecked the previous aperture/tip corrections; retained.                                                                         |

The D5K regression test uses the two actual muzzle-node coordinates and checks
that the shared sight mesh receives the same final pose despite the suppressor
length. It checks the relationship, rather than pinning two independent tuning
constants. See [test_ads.c](test_ads.c).

## Closer unscoped ADS, 2026-10-25

The owner requested bringing the rifle assembly closer to the camera while
keeping the recent larger hands/forearms. The 12 unscoped long-gun pose origins
move 20% closer along view Z. Sight heights/pitches, world/ADS FOV, grip offsets,
recoil and simulation-time interpolation stay intact. Gun and hands share the
resulting pose; the existing 0.052 hand scale and 2.5 body multiplier are retained.
Pistols keep their previous positions, and the sniper retains its 70-unit scope
transition distance.
The closer KF7 receiver fills more of the lower frame and leaves less of the
trigger hand visible at the bottom edge; support-hand attachment stays intact.

| Weapon                                   | Previous distance | Current distance |
| ---------------------------------------- | ----------------: | ---------------: |
| Klobb                                    |                36 |             28.8 |
| KF7, D5K, silenced D5K, Phantom, shotgun |                42 |             33.6 |
| ZMG                                      |                40 |               32 |
| AR33                                     |                60 |               48 |
| RC-P90                                   |                48 |             38.4 |
| Automatic shotgun                        |                56 |             44.8 |
| Laser                                    |                45 |               36 |
| Grenade launcher                         |                50 |               40 |

The wide and 4:3 audits capture all 20 weapons, and the wide pass includes
220 raise/lower frames. Sight-detail sheets confirm the existing centered
openings/tips remain visible, including the AR33 receiver clearance and matching
D5K pair. KF7/Klobb/AR33/RC-P90 hand/transition crops were reviewed for clipping.
Private evidence is indexed in `build/ads-proximity/*-audit.log`; the earlier
sight sheet is retained there for comparison. Additional 11-frame checks pass
for KF7 at FOV 110/120 Hz with weapon FOV enabled, and AR33 in 4:3 with weapon
FOV enabled. Their settled sights and raise/lower crops were visually reviewed.
Both independent gallery passes reproduce all 72 views. The 24 KF7/Klobb ADS
views change across stock/fork controls and all six outfits; the other 48 views
and all 35 icons remain byte-identical. Changed outfit views and the common
stock-model controls were reviewed. Native Windows/optimized Wasm builds,
ADS/wardrobe regressions, logo/site policy, gallery integrity and its eight
workflow tests pass.

## Rocket launcher sights, 2026-10-28

The rocket launcher now supports single-weapon ADS through the existing hold,
toggle and dedicated ADS controls. Its textured near/far sight pins sit near
model coordinates `(-3,200,25)` and `(-3,190,399)`. The initial `-0.026732`
radian pitch and `6.143` sight height centered both pins, but the loaded rocket
nose overlapped the aiming reference. The clearance refinement tilts the loaded
muzzle down about 1.25 degrees to `-0.005` radians, pivoting around the near pin.
The `7.040` sight height keeps that pin at screen center while the farther pin
and rocket nose drop below it, leaving clear background around its tip.
This intentionally supersedes the two-pin common ray: the near pin is the
aiming reference. The launcher alone uses X offset `-0.3` because its sights
are off the model origin. The 60-unit eye distance keeps the rear tube ahead
of the eye, and its upper rim stays below the aiming pin.
Original model vertices, textures, sight geometry and loaded rocket are retained.

Trigger and support grips use the existing rigid hand/wardrobe renderer; no hand,
wrist or watch contract changes. The normal 150 ms blend handles raise/lower,
with the existing single-gun, tank, switch and reload gates. Red Reticle mode
and native rocket launch speed, spread and inherited player velocity remain
unchanged. As with other projectile weapons, the sights provide a direction
reference rather than a guarantee of impact at every range.

`test_rocket_ads.c`, included by `test_ads.sh`, projects the near pin onto the
center ray, checks normalized farther-pin/loaded-nose clearance and rear-tube
eye/height clearance, and exercises aim modes, tank/dual/reload gates and
30/60/120/240 Hz transitions. The capture driver now
includes item 25 in its 21-firearm matrix and enlarged sight details.
Initial Windows captures verify the launcher in wide/4:3 and FOV 110 at 120 Hz with
weapon FOV enabled (snow wardrobe), each with 11 transition frames. The complete
wide audit captures 21 firearms and 231 transitions. Rocket/ADS and wardrobe
regressions, Windows/optimized Wasm builds and Wasm logo-payload policy pass.
Private logs/captures are indexed by `build_win/rocket-ads-*.log`.
The pin-clearance refinement passes ADS/wardrobe regressions, native Windows and
optimized Wasm builds. Native wide/4:3 and FOV 110/120 Hz with weapon FOV enabled
in the snow wardrobe each capture 11 raise/lower frames. The near pin has clear
background above the loaded nose in all three settled views. These captures are
indexed by `build_win/rocket-clearance-*.log`.
Both independent gallery processes reproduce all 72 views/35 icons with no image
changes; the regenerated manifest/HTML updates their rendering-input fingerprint.
Gallery integrity and all eight workflow tests pass.

## Owner pose refinements, 2026-11-02

The owner reported thin-looking ADS arms on the grenade launcher, Klobb, shotgun
and ZMG, a ZMG view straight down its sights, a rocket tube that read as aimed
into the face, and Phantom rear/front sights that did not line up. These choices
were confirmed with the owner before implementation:

| Weapon           | Change                                                                      |
| ---------------- | --------------------------------------------------------------------------- |
| Klobb            | Eye distance 28.8 → 26.5; generated arms 1.08× (`adsArmScale`).             |
| ZMG              | Eye distance 32 → 29.5; pitch -0.035 → -0.090; arms 1.08×.                  |
| Shotgun          | Hip depth 19.5 (was 33.6), no ADS zoom; pitch -0.040 → -0.090; arms 1.08×.  |
| Grenade launcher | Eye distance 40 → 36.5; arms 1.08×; pitch -0.200; ADS FOV 87.5%.            |
| Phantom          | Pitch -0.040 → -0.002, levelling the far sight posts inside the near notch. |
| Silver/gold PP7  | Sight height 1.0 → 1.56 (silver), 1.62 (gold); top of tip touches center.   |
| Rocket launcher  | Pins stacked at center at hip depth, canted 0.575 radian; no ADS zoom.      |
| RC-P90           | Only the support hand is drawn; its under-body trigger hand read badly.     |
| PP7s, DD44       | Sight heights 1.0 → 1.55 (PP7), 1.49 (silenced); DD44 1.1 → 2.01.           |
| Auto shotgun     | Hip depth 19 (was 44.8), no ADS zoom; trigger fist wraps the pistol grip.   |
| GL hands         | Support palm under the body behind the cylinder; trigger fist on its grip.  |

The automatic shotgun was unchanged in the first pass; later play-tests moved it
closer, then back out once its trigger hand wrapped the grip, and finally to
its hip depth without zoom. Every pistol now
shares the plated PP7s' centering; the DD44 sits a little lower still.

Other sight heights are unchanged; `portAdsWeaponPose` still keeps each muzzle
below the aim point for its pitch. The shotgun's extra muzzle-up pitch lifts its
front bead above the red shells on the receiver. The plated PP7 models' sight
tips stood about 7 screen pixels above center at 960x540 (the stock PP7's about
2), so the shot landed just below the tip. The 1.08 multiplier enlarges the whole generated
hand, wrist, cuff, watch and forearm uniformly around the grip origin; see the
[arm contracts](arm_rendering.md). Other long guns keep the 0.052 grip scale.

The rocket launcher restores the initial 2026-10-28 sight solution, pitch
`-0.026732`, sight height `6.143` and X `-0.3`, which stacks the near and far pins
on the centered ray. `portAdsWeaponRoll` then cants the gun 0.575 radian (about
33 degrees, raised from 0.40 at the owner's request) about the view axis, with the ADS blend, in gameplay and the review
harness. `portAdsWeaponPose` rolls the settled origin by the same angle, so
on-axis points such as both pins stay centered. The tube runs down and right
onto the shoulder, as if the head tilts behind the sights. The owner chose this
over two earlier options: a shoulder offset with an inward yaw, which separated
the pins, and the upright stacked hold, which looked aimed into the face. The
loaded rocket's nose again sits close to the pin tips, the trade-off that the
earlier -0.005 pitch avoided.

A further owner refinement keeps the launcher at its own hip depth: the eye
distance is 14.5, matching its `WeaponStats` PosZ of -14.5, instead of 60. ADS
therefore swings and rolls it into line rather than appearing to push it
forward. The near pin sits about 16 view units ahead of the eye, and the canted
tube passes below and right of it, behind the camera, as at the hip. The rocket
launcher also skips the ADS zoom: `portAdsWeaponFovScale` caps each weapon's
ADS field of view in `bondview2.c`, at 100% for the rocket launcher, 87.5% (half
the usual zoom) for the grenade launcher and the usual 75% for other guns.

The grenade launcher's -0.200 pitch raises its muzzle well above the receiver,
suggesting the grenade's arcing lob. As with every pose, the projectile still
leaves along the inherited aim, so the visual tilt does not change ballistics.
The silver and gold PP7 heights moved 1.35 → 1.5 after the owner's play-test, putting
the tip a couple of pixels below center at 960x540, where the shot lands. The
stock PP7, silenced PP7 and DD44 were then lowered to match: their tips sat
about 2-3 px (PP7) and 10 px (DD44's front post) above center. A capture from
before this session's pose work is pixel-identical to the pre-change PP7, so
this was long-standing, not a regression.

The owner then asked for the shotgun and automatic shotgun trigger hands back,
looking natural, and the DD44 lower again (1.65 → 1.95, about 5 px at 960x540).
The owner then asked that screen center just touch the topmost point of each
pistol's sight, where rounds land. Pixel measurement of the 960x540 captures
found the weapon tops still above center: PP7 4 px, silenced PP7 3 px, gold
2 px, silver and DD44 1 px. It classed neutral or warm weapon pixels apart from
the blue-tinted shutter and hint text, within 8 px of the center column. At about
16.7 px per sight-height unit, the final heights are PP7 1.55, silenced PP7 1.49,
DD44 2.01, silver 1.56 and gold 1.62. Each topmost weapon row is now the first row
below center (row 270).
Side-on debug captures showed that neither trigger hand held its grip: the
shotgun fist sat on the receiver ahead of the grip, and the earlier automatic
shotgun point (-0.6,-4.5,-11) left it at the bottom rear. Both points are now 1.25
left of the old line, which centres the palm on the grip: shotgun (1.25,-3,-2) and
automatic shotgun (1.25,1,-2). For these stocked guns and the grenade launcher,
`adsShoulderedGrip` raises the trigger elbow to (14,-12,6), beside the shoulder,
in place of (17,-28,-12). The default low elbow tilted the rigid closed hand off a
slanted grip; with the level forearm, the fingers wrap the front and the thumb
lies on the left. A later play-test found the grip still showing through the
middle of the fist, so both points moved 3 units towards the body, to z -5. The
web and palm now cover the grip's rear, which enters through the top of the hand.

The owner then chose the rocket launcher's treatment for both shotguns, since
their spread needs no magnified sight picture. They stay at their `WeaponStats`
hip depth (PosZ -19.5 and -19.0) with no ADS FOV change
(`portAdsWeaponFovScale` 1.0), keeping the same sight heights and pitches. At
that depth the trigger hand is below the frame through ADS and its transitions,
which the owner accepted. The muzzle stays clear of the shotgun's red shells.

The grenade launcher's support grip (1.1,0.5,25) lay inside its cylinder (z 16 to
27 in weapon-local units, about ±7 wide). Any hand ahead of the cylinder
disappears behind it from the eye, and a hand under it is hidden by the receiver.
A first fix cupped the cylinder's outside-left at (6.5,-4,17). At the owner's
request, the palm now holds the underside of the body between the trigger guard
and the cylinder, at (1.1,-3,10), closer to the body. From the eye it is mostly
tucked under the receiver; it shows in side-on captures. Its trigger fist moves from the cylinder front (-0.6,-1,-2)
onto the pistol grip (1.25,-2.5,-3.5).
Weapon-local grip coordinates equal the model vertex positions minus the root
group origin, divided by ten.

The rocket launcher's raise eases out over 250 ms instead of a linear 150 ms
(`portAdsBlend` returns `1-(1-t)^3` for it; 0 and 1 are unchanged), slowing as
the pins settle. Its support hand no longer twists with the cant:
`adsCounterRoll` rotates that hand's gun basis back about the tube axis
(weapon-local `(-0.3, 12.75)`, from the muzzle node), so the palm stays under the
tube relative to the ground. The trigger hand rolls with the launcher.

Phantom pitches of -0.015 and +0.025 bracketed the result. At -0.015, the posts
stood well above the housing; at +0.025, the housing hid them. `test_ads.c` now
accepts eye distances down to 26 units and per-weapon arm scales of 1.0 or 1.08.
`test_rocket_ads.c` projects the rolled pose, requiring both pins within 0.005
view units of the center ray, the hip depth, the muzzle and rear tube below and
right of it, and no ADS zoom for the launcher only.

## Crosshair calibration, 2026-11-08

ADS Crosshair (FORK-060) draws the always-on cross at the aim point in ADS,
so each sight can be compared with the shot ray directly. The owner saw the
Magnum's tip a few pixels right of the cross and the D5K rings off centre. A
local script captured every gun's settled ADS view at 1600 × 900 on Facility
with the cross off and on, took the cross centre from the changed pixels,
and found either an enclosed aperture around it (its centre) or the topmost
gun pixels below it (a tip), with a zoomed crop per gun for review. Offsets
are screen pixels at 1600 × 900; y is positive downwards.

| Weapon              | Before                                  | After                                                         |
| ------------------- | --------------------------------------- | ------------------------------------------------------------- |
| PP7, silenced PP7, DD44 | tips touch the centre                | retained                                                      |
| Klobb               | centred between its rails (no real sight) | retained                                                    |
| KF7                 | ring centred (−0.5 y)                   | retained                                                      |
| ZMG                 | rear fins' tops 14 px below the cross's arms, no front sight in view | owner's call: height 4.0 → 3.41, fin tops on the arms' top row (3.24 overshot by 4 px; about 0.042 units per pixel) |
| D5K, silenced D5K   | ring centre 5.0 px above the cross      | heights 5.8 → 6.11 and 5.0 → 5.31 (same 0.8 apart): 0.0, 0.0  |
| Phantom             | front-sight ears straddle the cross, tops just under it | retained                                    |
| AR33                | square aperture centre −1.0 x, −1.0 y   | retained (within a pixel)                                     |
| RC-P90              | block top on the cross, +1.4 x          | retained (within 1.5 px)                                      |
| Shotgun             | tip about 10 px below the cross         | retained (bead shown above the red shells); reported          |
| Automatic shotgun   | drum top about 2.5 px below             | retained (no conventional sight)                              |
| Cougar Magnum       | tip +1.0 to +1.5 x, 1.5 px low          | height 4.0 → 3.92 and a new 0.075 `sightX` (tip right of the muzzle axis): top +0.5 y, +0.1 x |
| Golden Gun          | post about 2.3 px low                   | height 1.7 → 1.56: post top on the centre                     |
| Silver and Gold PP7 | tips touch the centre                   | retained                                                      |
| Moonraker Laser     | body top about 15 px below              | retained (no sight); reported                                 |
| Grenade launcher    | top about 9 px below                    | retained (deliberate lob pitch)                               |
| Rocket launcher     | canted pins; the detector cannot judge them | retained (`test_rocket_ads.c` holds them)                 |
| Sniper rifle        | scoped: no cross (its scope mark)       | retained                                                      |

`sightX` is a new optional field in the pose table: view units the sight tip
sits right of the muzzle axis, subtracted from the pose's x. Only the Magnum
uses it. Each 0.05 units moved the Magnum about 0.75 px at 1600 × 900.

## Taser and watch laser, 2026-11-08

Neither item has sights, and upstream draws both with a fixed hip rotation
(FORK-064). In ADS, `portAdsBlendRotation` eases that rotation to an ADS
orientation, and the pose table places the result. Calibration used the
private `GE_ADS_AUDIT` capture of each item on Runway at 960x540, with the
screen centre marked.

| Item        | ADS orientation (XYZ radians)     | Pose (height, distance, pitch, sightX) | Sight picture                                           |
| ----------- | --------------------------------- | -------------------------------------- | ------------------------------------------------------- |
| Taser       | (pi/2, pi/2 - 0.5, pi/2)          | 13.6, 36, 0, -6.2                      | prongs either side of centre (midpoint within 2 px), tips level with it |
| Watch laser | (0, 0, 0.35)                      | 8, 27, -0.03, 1.2                      | watch face below centre; hand below and right of it     |

The taser's angles are a quarter turn about Y (face to the camera) followed by
a 0.5 radian tilt about the parent X axis (prongs towards the target),
expressed in `matrix_4x4_set_rotation_around_xyz`'s order. Removing its native
rotation alone showed the taser edge-on; a plain quarter turn stood it upright,
and a 0.3 radian tilt barely read. The model has no muzzle node, so its pose
is measured from the model origin and its `sightX` is large.

Removing the watch laser's small native tilt alone put the hand's knuckles on
the aim point at heights 2 and 4. Height 8 to 9 cleared it; a 0.35 radian roll
lays the hand flatter, pointing at the target, where -0.35 stood it up across
the centre. Both items' 11-frame raise/lower captures blend without flips.

## Earlier height-correction validation

- Captured all 20 supported ADS firearms at default wide FOV 90/60 Hz and
  Classic 4:3/60 Hz, before and after the corrections.
- Rechecked each of the five changed guns at wide FOV 110/120 Hz, including
  11 raise/lower transition captures per gun.
- Reviewed the enlarged long-gun details, full views, receiver clearance and
  hand placement; rechecked the existing AR33/RC-P90 alignment and sniper scope.
- ADS regression tests passed, including the new D5K relationship check and
  existing 30/60/120/240 Hz state/input checks.
- Windows and Linux builds passed; the local main Windows executable was rebuilt.

## Reproduction

From the repository root in PowerShell:

```powershell
# All 20 supported ADS firearms, with enlarged long-gun details.
& scripts/port/audit_ads.ps1
& scripts/port/audit_ads.ps1 -Aspect Classic

# A particular gun at a wider FOV, including its transition.
& scripts/port/audit_ads.ps1 -Item 11 -Fov 110 -TickHz 120 -Transitions
```

Item IDs for the corrected guns are 7 (Klobb), 9 (ZMG), 10 (D5K), 11
(silenced D5K), and 15 (shotgun). Default wide captures use 960 × 540,
90-degree horizontal FOV and 60 Hz; Classic captures use 800 × 600. The
engine's actual scene FOV also reflects its ADS zoom.

Captures are local, ignored build outputs containing ROM-derived imagery and
must not be committed. These are stationary sight/transition checks, not a
measurement of full-auto spread, recoil, or projectile drop.
