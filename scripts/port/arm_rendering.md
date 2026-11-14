# First-person arm rendering contracts

Hands, wardrobe arms and rifle poses have different owners. Keep these boundaries
when adjusting their appearance; a uniform scale or shared material override can
fix one pose while breaking another.

| Region                       | Owner                                     | Changes allowed                                                                                                                                                                                                                                                                                     |
| ---------------------------- | ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Native pistol palm/fingers   | Original weapon model and `model.c`       | Suppress only proximal forearm triangles in a frame-owned display-list copy. Retain original vertices, UVs, RGBA, matrix/material/LOD commands, triangle order and native model render setup.                                                                                                       |
| Wrist interface              | `port_native_arms.c` measured joint       | Fit the added arm to the native hand, never enlarge or move the native hand to fit the arm. Both sides must use the same nonplanar ring, including inserted edge corners.                                                                                                                           |
| Wardrobe forearm/cuff        | Pause-watch model and shared mesh builder | Retarget the arm behind its measured wrist. Preserve UVs/material ownership; clothing must not become skin. Close the clothing rim with its own material.                                                                                                                                           |
| Rifle grip hand              | Closed PP7 hand plus rigid pose           | One rigid transform; 0.065 hand scale, 1.08x for Klobb/ZMG/shotgun/grenade launcher. No independent finger curl or forearm bulk applied to hand vertices. The palm origin remains anchored to the weapon's grip.                                                                                    |
| Rifle forearm body           | `sAdsArmProfile` in `ads_hands.c`         | Body bulk ramps to 2.5 after a protected 40-native-unit wrist band, over 35 units. This profile is used only for added long-gun ADS arms; pistols, chopping arms and icons use separate transforms. Body-bulk tuning must not change the hand, wrist seam or watch.                                 |
| Left watch                   | Original watch meshes and grip roll       | Preserve case/face proportions, moving clock hands, arm/cuff fit and grip orientation. Complete the pause model's missing bracelet underside on left long-gun arms from the arm-slab hull and original metal material. Right hands have no watch. Native pistol/chop/icon transforms stay separate. |
| Skin shading                 | Native forearm material mapping           | Match generated wrist UV/RGB to the original weapon's same-material skin triangles; keep geometry and alpha fixed. Never recolor/remap the stock hand or change clothing/watch UVs to hide a skin seam.                                                                                             |
| Pistol hand/sleeve occlusion | First-person weapon depth layer           | Both native hands and added arms use depth testing in the same near projection. A sleeve in front must cover skin regardless of display-list order. Restore the scene projection after each weapon draw and leave the per-frame `G_ZBUFFER` geometry mode on for later split-screen views.          |
| ADS fade                     | Shared color/depth topology               | Same triangles in both passes; depth lists contain geometry only. Opaque draws omit unused depth lists.                                                                                                                                                                                             |
| Hip-Fire Arms (FORK-075)     | `adsHipPose` in `ads_hands.c`             | Optional, off by default. Below a settled ADS the same rigid hands draw opaque; the elbow is carried with the gun from the settled ADS pose, and the per-gun `sAdsHipGrips` fit moves only the support hand and elbow, both fading out as the blend rises. Settled ADS and the setting-off fade are unchanged. |

## Protected native pistol path

`portWardrobeRenderNode` draws the added arm first, then returns
`PORT_WARDROBE_NATIVE_HAND` with a copied display-list record. The original
`modelRenderNodeGundl` path draws the retained hand using its native model type,
culling, fog/environment colors and texture setup. The added wrist samples its
UVs and vertex shade from native forearm triangles of the same skin material.
The pistol arm also uses the native two-cycle shader and authored mip/texture
commands, then restores native mip LOD before the hand draw. Native pistols use the weapon's near depth layer in hip
fire, ADS and transitions: the added arm writes depth and native hand materials
test it, so drawing the hand last cannot paint it over the sleeve. This changes
occlusion, not the hand's vertices, texture/shader colors or XY projection.
With upstream's optional `weapon_fov=1`, this depth remap receives the selected
gun projection, including its aim correction, rather than reloading the world
projection. Gun, native hand and generated sleeve use the same XY transform.
The option remains off by default; fully raised ADS blends back to the calibrated
world projection. Restore the world projection after the complete hand pass
(`portAdsEndHandLayer`). Do not clear `G_ZBUFFER` there: `lvlRender` enables it
once per frame (`dlZBufferGeometry`) before every player's view, so clearing it
removed depth testing from each later split-screen view and made its rooms
flicker. `test_hand_layer.c` (run by `test_ads.sh`) checks that the end of the
layer only reloads projection.
No HD texture pack replaces a texture inside this first-person span (FORK-053):
`gunfire.c` opens it with `portHudViewmodelTag` before the hand layer and closes
it after `portAdsEndHandLayer`, and the renderer skips pack lookups there and
keeps those textures apart in its cache. Keep new viewmodel draws inside the
bracket.
The frame-owned filtered list bypasses the retained GPU part
cache, whose keys assume immutable display lists.

`paStockHandList` copies commands without copying or mapping any stock vertices.
Removed arm triangles become degenerate triangles in the existing TRI1/TRI4
slots. Every retained index and every non-triangle command remains identical to
the source. This avoids losing palm triangles from mixed hand/arm nodes and
avoids changing the hand's appearance through the shared arm material renderer.
The rifle grip renderer remains separate because it needs retargeting and fades.
Its body multiplier is applied when constructing frame-owned long-gun ADS arm
vertices. The shared wardrobe mesh stays immutable; native pistols, chopping
arms and icon generation do not use `adsGripPoint` or `sAdsArmProfile`. This
allows independent rifle proportions while retaining consistent wardrobe
textures and wrist/watch interfaces, without separate duplicated source models.

The owner's 2026-10-25 request explicitly enlarges the added long-gun ADS hands
as well as their arms. The fixed grip scale increases from 0.045 to 0.052
(about 16%), uniformly including the connected wrist, cuff and watch. Hand and
watch proportions, UVs/materials, finger poses and source vertices remain intact;
the generated instances grow together around the same grip origin. The body
multiplier increases from 2.3 to 2.5, for about 26% more forearm width/depth after
the combined scale change. Protected-band/ramp lengths in source space and elbow
targets remain unchanged. This intentional size adjustment supersedes the older
fixed-scale value; native pistol/chop/icon transforms retain their existing size.

The RC-P90 draws only its generated support hand in ADS (`adsGripHands`): its
trigger grip sits under the body, where the generated right hand read badly.
The grip table, transforms and other guns' two-hand rendering are unchanged.
The canted rocket launcher builds its support hand from a counter-rolled gun
basis (`adsCounterRoll`), about the tube axis, so the hand stays under the tube
relative to the ground. The rigid hand transform, scale and wrist/watch
contracts are unchanged; only that hand's input basis differs.
The shotgun, automatic shotgun and grenade launcher (`adsShoulderedGrip`) place
their trigger elbow at (14,-12,6) in place of (17,-28,-12). With the level
forearm, the rigid closed hand wraps a slanted pistol grip. Only the forearm
target changes; the palm anchor, hand transform and scale do not.

The owner's 2026-11-02 request enlarges the generated ADS arms of the Klobb,
ZMG, regular shotgun and grenade launcher by a further 1.08x (`adsArmScale` in
`ads_hands.c`), alongside moving those guns about 8% closer. The multiplier
applies to the same rigid grip transform, so the hand, wrist seam, cuff, watch
and forearm grow together around the anchored palm origin; the 2.5 body profile
and protected-band lengths are unchanged. Other long guns keep the base scale,
and native pistols, chopping arms and icons keep their own transforms.

The owner's 2026-11-11 requests enlarge every generated non-pistol hand and arm
again: the base grip scale rises from 0.052 to 0.058 and then to 0.065 (about
25% in all), in ADS and at the hip alike, so the hands keep one size through a
raise. As before it scales
the hand, wrist, cuff, watch and forearm cross-section together around the
anchored palm origin; the 1.08 multiplier, 2.5 body profile, protected-band
lengths and elbow targets are unchanged, and native pistols, chopping arms and
icons keep their own transforms. `test_ads.c` locks the new scale.

**Hip-Fire Arms** (`[display] hip_arms`, off by default; issue #7) keeps the
generated long-gun arms at the hip. `portAdsHandAlpha` replaces the ADS blend
as the arms' gate and opacity in `gunfire.c`: with the setting on it is 1, so
the arms draw opaque before the gun as a settled ADS does; off, it is the
blend, as before. `adsSettledGun` rebuilds the settled ADS gun matrix as the
gallery fixture does (view-facing basis, ADS pitch and roll, sight pose from
the muzzle), and `adsGunCarried` moves the settled camera-space elbow onto the
current gun, so the arm rides rigidly with the weapon through bob, raises,
lowers, reloads and switches; aiming lerps that carried elbow onto the ADS
target, and a settled ADS (blend 1) takes the original pose path, so its pixels
match the setting off. Not while dual wielding (no support grip) or in the
gallery's stock cells. The RC-P90 keeps only its support hand.

The hip camera sees the support hand from above and beside the gun, where the
ADS palm-up cradle floated beside some fore-ends and sank into others, so a
per-gun hip fit (`sAdsHipGrips`, owner review 2026-11-11) adjusts only the
support hand, faded out linearly as the gun rises: a weapon-local grip offset,
an added outward roll (the `cant` term of the hand's back vector), an offset
of the carried elbow and a tilt of the hand at the wrist (below). After three owner reviews: the Klobb and ZMG wrap the
fore-end's side, rolled out, neither clipping nor floating; the KF7's hand sits
against its wood and draws behind the gun (below);
both D5Ks, the RC-P90 and shotgun hold the gun from underneath, palm up,
rather than upright beside it; the AR33's hand is tilted thumb-up beside its
hand guard (below); the Phantom's hand sits 6 units back from the
muzzle; the rocket launcher's elbow
moves down and towards the body, bent below the tube instead of reaching for
the lower left corner. The automatic shotgun, laser and grenade launcher keep
the plain carried grip. The trigger hand, the rigid hand transform and scale,
and settled ADS are unchanged. `test_hip_arms.c` checks that only the support
hand moves and that the fit is gone at a settled ADS.

**A support hand behind the gun** (owner's choice, 2026-11-12). The closed
hand's finger tips pass through the KF7's fore-end and showed on its near side
as pale shards, wherever the hand sat. A hip fit can set `behind`: below a
settled ADS that support hand alone draws before the gun
(`portAdsDrawHandsBehindGun`), and the gun, in painter's order, covers
everything of it that overlaps. The trigger hand still follows the gun, and
the pass after the gun skips the hand already drawn (`adsHandInPass`). Only the
KF7 sets it, with its hand raised against the wood (support offset y 0, was
-1.2). The hand shows beside and below the fore-end; no finger shows on the
gun's near side. A settled ADS keeps the usual depth-tested order. Set
`behind` only where the hand's visible part lies wholly beside or under the
gun from the hip camera: the gun covers any part meant to wrap in front.

**A tilted support hand** (owner's choice, 2026-11-12). The closed hand's thumb
lies along the hand's axis, which otherwise runs from the elbow to the wrist,
so under a fore-end the thumb points along the barrel. A hip fit can set
`tilt`: `adsArmPose` leans that axis towards the gun's up before building the
hand basis. The protected hand, wrist seam, cuff and watch stay one rigid
piece and turn together about the palm; the bend falls in the
generated forearm behind the protected cuff, which still ends at the same
elbow. The tilt fades out with the raise, and ADS passes 0. Only the AR33 sets
it (1.0, with its hand 1.25 below the grip point, 0.25 to its left and rolled
out 0.7), so its
thumb rests on the hand guard's edge while the guard hides the index finger by
depth. Keep tilts moderate: on the AR33, 1.5 and above pushed finger tips
through the guard. A tilted hand shows more wrist skin above the cuff. A
tilted hand also raises its fingertips: 0.75 below the grip point, one showed
through the top of the AR33's guard at the hip, and 1.0 or more below clears
it. The tilt and the offset fade together, so partway through the raise a
short streak of fingertip still shows along the guard's front edge. Moving
the hand left shows more of the thumb, but from 0.75 left of the grip point
the index finger comes through the guard at the hip. The same grip for the KF7
was rendered and deferred; FORK-075 in `FORK_CHANGES.md` records what worked.

A tilted hand's forearm does not keep the hand's frame to the elbow, which
squeezed the sleeve: `adsGripPoint` eases the generated forearm's
cross-section from the hand's back and side at the protected cuff to
`adsElbowFrame`'s plain palm-up frame about the untilted forearm, reached by
mid-forearm (`ADS_UNTWIST_RATE`). A hip fit's `girth` multiplies that
generated forearm's bulk (the AR33's is 1.3) and fades out with the raise.
Both act only behind the protected cuff: neither moves the hand, wrist seam,
cuff or watch, and untilted hands and ADS take the old path with a girth of
exactly 1. The wrist and cuff are flatter than wide, so a hard roll turns
their narrow edge to the hip camera; prefer a light roll.

**A fit's wrist scale** (owner's request, 2026-11-14). This is the one
deliberate exception to the fixed wrist, cuff and watch. In the arm mesh the
wrist and cuff measure about 40 source units from palm to back and 62 across,
half the fist's 80 by 105 or more, so beside a fuller forearm the AR33's hip
wrist looked too small. A hip fit can set `wristDepth` (palm to back) and
`wristWidth` (across); the AR33's are 1.5 and 1.25, and no other fit sets
them. `adsWristPoint` scales hand, cuff and watch points about the arm's
centre line before the rigid grip transform: nothing ahead of the heel
(`wristRamp`, 45 source units before the wrist seam), a smooth ease along the
heel, and the whole scale from the seam through the protected band. The
fingers, thumb and palm anchor keep their size and place; the wrist seam,
cuff and watch grow together, so the watch still fits its wrist, and the
seam's hand-side and cuff-side points take the same mapping. The generated
forearm leaves the cuff at the wrist's scale and blends to its own bulk and
girth over the bulk ramp, so the elbow is unchanged. The scale fades out with
the raise: ADS, the trigger hand, native pistols and every other gun use
exactly 1, where `adsWristPoint` returns its input untouched. It changes
positions only: UVs, colours, triangles and materials are as before. A new
wrist scale needs the owner's request, as this one had.

**Dual wielding** (owner's choice, 2026-11-11). With Hip-Fire Arms on, taking
up a second gun keeps the right gun's trigger hand, while the support arm
slides down out of view: `sAdsSupportHold` per player eases from 1 to 0 over
300 ms at any tick rate (`portAdsHandsTick`, called from `portAdsTick` before
its dual-wield return) while a left gun is equipped, and back once it is gone.
`adsSupportLowered` translates the whole support pose rigidly by a smoothstep
of (-4,-30,6) camera units (left, down, towards the eye), so hand, watch and
forearm keep their fit; at 0 the support arm is not built. The RC-P90, which
draws only its support hand, has no arms while lowered. The left gun has no
hand. `GE_HIP_DUAL_AUDIT=<item>` captures a switch into and out of dual wield
every 4 ticks for review.

The sniper rifle's support grip itself moved, in `sAdsGrips`, from the muzzle
(1.7,6,35) to the wooden fore-end (0.1,1,19), back and below, instead of a hip
fit: a fit fading out over the raise slid the hand forward onto the muzzle as
the gun rose, where it clipped. Its scoped ADS hides gun and hands, so no
settled view changes; at the hip the hand now sits below the screen edge, even
at 4:3. The support point stays on the gun's left (x > 0), as `test_ads.c`
requires of every grip.

**Long-gun draw order below a settled ADS.** The stock first-person gun draws
without depth testing, in its authored painter's order, so a coplanar overlay
drawn later always shows. Inside the compressed hand layer a depth-tested gun
lost such overlays to the surface beneath: the rocket launcher's lettering and
arrow sit fractionally under its curved tube and flickered or vanished at the
hip (and could mid-raise). Whenever a long gun has generated arms and the ADS
blend is below 1 (`gunPainter` in `gunfire.c`), the gun's own draw is bracketed
by `portAdsGunPainterTag` (G_NOOP `0x64`): the renderer turns its depth compare
off but keeps the depth writes, and the arms draw after the gun, testing
against that depth, so they still hide behind it and wrap in front of it (a
support hand marked `behind` draws before the gun instead, above). A
settled ADS keeps the depth-tested gun with the opaque arms drawn first, as
before: the authored order was written for the hip camera, and from behind the
gun it lets a sliver of the Klobb's grip or the rocket's lettering paint over
nearer parts. Pistols, chopping arms and guns without generated arms are
unaffected.

Long-gun camera proximity is a separate pose-table control in `joy_port.c`.
Unscoped ADS origins move 20% closer along camera Z, with calibrated sight
heights/pitches and world FOV retained. Gun and grip hands share that transform;
the forearms still fit their camera-space elbow targets. This does not change
the hand scale (now 0.065) or 2.5 body multiplier above. Pistol poses and the sniper's
scope transition keep their separate eye distances.

The original pause bracelet models its upper surface and sides, leaving an
open underside. `portArmMeshBuildWatchBack` completes that underside for long-gun
grips. It derives attachment edges, width and texture setup from the original
bracelet. A convex hull of the wardrobe arm over the band's full longitudinal
slab includes both end intersections and intermediate bulges. The added band
has clearance, inner/outer surfaces and side rims, with matching color/depth
topology through fades. Source vertices, case/face/clock geometry and UVs remain
unchanged. Unrecognized materials retain the authored bracelet. Keep the
existing arm/cuff transforms; rescaling the wrist to expose the watch can make
shirt-cuff polygons intersect the jacket. Keep the natural grip roll: seeing
the band instead of the watch face is expected when the face points away from
the camera. Generated long-gun support hands cradle palm up (owner request,
2026-11-03): the back of the hand faces the ground with a slight outward cant,
and the palm rises 0.4 units onto the fore-end. The watch rolls under the
wrist. This changes only the rigid hand's roll and anchor, not its
vertices, scale or wrist/watch fit.

`paNativeSurface` finds the closest point on the native forearm's same-material
triangles and interpolates its UVs and RGB. Sampling uses the undeformed native
bone space, so forearm extension does not distort the mapping. `armSurfaceOwners`
limits the transfer to vertices belonging exclusively to one skin material;
clothing, watch parts and shared material vertices retain their authored UVs.
Only frame-owned generated wrist attributes change. The stock hand's vertex
buffer, texture pixels, geometry and material commands remain immutable.
Rifle wrists use this same mapping under their existing fade renderer, with
the same scene lighting as the native grip hand and no additive brightness lift.

## Regression checks

Throwing-knife ADS keeps its original native hand geometry/materials and uses
temporary knife-only matrix-list copies for its optional handle-grip animation.
See [knife ADS contracts and validation](knife_ads.md). It does not use the
generated rifle grips or alter native pistol paths.

Taser and watch laser ADS (FORK-064) move their native models rigidly: the
taser's hand and the watch laser's arm, watch and hand keep their vertices,
UVs, materials and render setup. Only the hip rotation and position change
with the ADS blend. Neither uses generated grips or wardrobe arms.

Run `bash scripts/port/test_wardrobe.sh` and `bash scripts/port/test_ads.sh`.
Both already run in the Windows release workflow without a ROM. Tests cover:

- Original pistol vertex-buffer identity, unchanged record metadata/material
  commands, preserved packed triangle indices and immutable source data.
- Integration with the actual `model.c` renderer: native two-cycle hand
  materials, original model setup and no immutable-part caching of filtered lists.
- Native pistol classification, arm depth writes, native hand depth comparisons
  and the geometry depth-enable bit, alongside the stock material checks.
- All six wardrobes, seven pistol variants and both native hands.
- Nonplanar wrist joins, intermediate edge corners and closed clothing rims.
- A hip fit's wrist scale: only the AR33's support hand has one, it fades with
  the raise, leaves the grip, centre line and elbow alone, moves hand, cuff
  and watch points at the wrist as one, and meets the sleeve without a step
  (`test_hip_arms.c`).
- Rigid hand/watch transforms, protected wrist continuity and the rifle body's
  radius independently of its centerline, across all 14 grip pairs and blends,
  including the rocket launcher's trigger grip and tube support.
- Skin-only corrections and terminal material reset on generated arm meshes,
  plus geometry-only fade depth lists.
- Native skin UV/RGB sampling, light scaling and unknown-material fallback;
  per-material ownership, clothing/watch preservation and unchanged geometry/alpha.
- Bracelet clearance around end sections and intermediate bulges in all six wardrobes;
  complete opaque/fading geometry, alpha and original source preservation.
- Hip-Fire Arms (`test_hip_arms.c`): default off, ini/menu keys, the opacity
  rule, dual-wield and native-hand gates, and for all 14 grip pairs an elbow
  fixed to the gun as at the settled ADS pose, unchanged ADS hands, the ADS
  elbow on the settled gun and the eased raise; per-gun hip fits that move only
  the support hand and vanish at a settled ADS, and the rocket's lower elbow;
  the dual-wield slide's timing at four tick rates, its rigid lowering, the
  per-player tick and the trigger hand kept on two-handed guns.

These tests enforce data and geometry contracts. They do not prove that every
pose looks natural. Review actual ROM renders after material, fit or pose edits:

```powershell
# Matched stock reference and wardrobe treatment, same camera/seed/ADS poses.
./scripts/port/audit_ads.ps1 -Item 4 -Stage runway -Transitions -HandDetails -StockHands
./scripts/port/audit_ads.ps1 -Item 4 -Stage runway -Transitions -HandDetails
# All weapons and transitions; dual pistols; bare and snow forearm samples.
./scripts/port/audit_ads.ps1 -Stage runway -Transitions -HandDetails
./scripts/port/audit_ads.ps1 -Stage runway -Dual -HandDetails
./scripts/port/audit_ads.ps1 -Item 8 -Stage jungle -Transitions -HandDetails
./scripts/port/audit_ads.ps1 -Item 4 -Stage surface -Transitions -HandDetails
# Hip-Fire Arms on: the blend-0 transition frames are the hip poses.
./scripts/port/audit_ads.ps1 -Stage runway -Transitions -HipArms -Wardrobe Blue
```

`-StockHands` disables wardrobe replacement for native pistols only while the
ADS audit is active. It is not a player setting. Compare palm/finger shape and
material appearance against this reference, then examine cuffs, exposed skin,
left watch and intermediate poses separately. Keep useful local captures/logs;
audit executables are removed automatically. The versioned
[review gallery](hand_gallery.md) is the explicit exception for generated review
PNGs; other captures, extracted models/textures and caches remain out of Git.
Optional audit flags treat empty environment values as disabled so consecutive
stock/wardrobe/dual runs cannot inherit a mode solely from an empty variable.

If an export's pixels change, bump `PORT_ICON_CACHE_VERSION`, regenerate all 35
images and review the six wardrobe variants. Do not use a cache version bump to
hide a regression. When changing an invariant deliberately, document the reason
in this guide and `FORK_CHANGES.md` and update the corresponding regression.
The exporter explicitly enables its own geometry depth state, so a gameplay
weapon restoring its depth layer cannot alter the next icon's occlusion.
