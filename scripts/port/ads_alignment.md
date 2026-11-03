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
| Shotgun          | Eye distance 33.6 → 31; pitch -0.040 → -0.090; arms 1.08×.                  |
| Grenade launcher | Eye distance 40 → 36.5; arms 1.08×.                                         |
| Phantom          | Pitch -0.040 → -0.002, levelling the far sight posts inside the near notch. |
| Silver/gold PP7  | Sight height 1.0 → 1.35, lowering the taller tip onto screen center.        |
| Rocket launcher  | Both pins stacked at center, canted 0.40 radian onto the right shoulder.    |

The automatic shotgun and stock PP7 are unchanged.

Other sight heights are unchanged; `portAdsWeaponPose` still keeps each muzzle
below the aim point for its pitch. The shotgun's extra muzzle-up pitch lifts its
front bead above the red shells on the receiver. The plated PP7 models' sight
tips stood about 7 screen pixels above center at 960x540 (the stock PP7's about
2), so the shot landed just below the tip. The 1.08 multiplier enlarges the whole generated
hand, wrist, cuff, watch and forearm uniformly around the grip origin; see the
[arm contracts](arm_rendering.md). Other long guns keep the 0.052 grip scale.

The rocket launcher restores the initial 2026-10-28 sight solution, pitch
`-0.026732`, sight height `6.143` and X `-0.3`, which stacks the near and far pins
on the centered ray. `portAdsWeaponRoll` then cants the gun 0.40 radian (about
23 degrees) about the view axis, with the ADS blend, in gameplay and the review
harness. `portAdsWeaponPose` rolls the settled origin by the same angle, so
on-axis points such as both pins stay centered. The tube runs down and right
onto the shoulder, as if the head tilts behind the sights. The owner chose this
over two earlier options: a shoulder offset with an inward yaw, which separated
the pins, and the upright stacked hold, which looked aimed into the face. The
loaded rocket's nose again sits close to the pin tips, the trade-off that the
earlier -0.005 pitch avoided.

Phantom pitches of -0.015 and +0.025 bracketed the result. At -0.015, the posts
stood well above the housing; at +0.025, the housing hid them. `test_ads.c` now
accepts eye distances down to 26 units and per-weapon arm scales of 1.0 or 1.08.
`test_rocket_ads.c` projects the rolled pose, requiring both pins within 0.005
view units of the center ray and the muzzle/rear rim below and right of it.

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
