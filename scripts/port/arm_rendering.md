# First-person arm rendering contracts

Hands, wardrobe arms and rifle poses have different owners. Keep these boundaries
when adjusting their appearance; a uniform scale or shared material override can
fix one pose while breaking another.

| Region                       | Owner                                     | Changes allowed                                                                                                                                                                                                                                                                                     |
| ---------------------------- | ----------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Native pistol palm/fingers   | Original weapon model and `model.c`       | Suppress only proximal forearm triangles in a frame-owned display-list copy. Retain original vertices, UVs, RGBA, matrix/material/LOD commands, triangle order and native model render setup.                                                                                                       |
| Wrist interface              | `port_native_arms.c` measured joint       | Fit the added arm to the native hand, never enlarge or move the native hand to fit the arm. Both sides must use the same nonplanar ring, including inserted edge corners.                                                                                                                           |
| Wardrobe forearm/cuff        | Pause-watch model and shared mesh builder | Retarget the arm behind its measured wrist. Preserve UVs/material ownership; clothing must not become skin. Close the clothing rim with its own material.                                                                                                                                           |
| Rifle grip hand              | Closed PP7 hand plus rigid pose           | One rigid transform and fixed 0.052 hand scale. No independent finger curl or forearm bulk applied to hand vertices. The palm origin remains anchored to the weapon's grip.                                                                                                                         |
| Rifle forearm body           | `sAdsArmProfile` in `ads_hands.c`         | Body bulk ramps to 2.5 after a protected 40-native-unit wrist band, over 35 units. This profile is used only for added long-gun ADS arms; pistols, chopping arms and icons use separate transforms. Body-bulk tuning must not change the hand, wrist seam or watch.                                 |
| Left watch                   | Original watch meshes and grip roll       | Preserve case/face proportions, moving clock hands, arm/cuff fit and grip orientation. Complete the pause model's missing bracelet underside on left long-gun arms from the arm-slab hull and original metal material. Right hands have no watch. Native pistol/chop/icon transforms stay separate. |
| Skin shading                 | Native forearm material mapping           | Match generated wrist UV/RGB to the original weapon's same-material skin triangles; keep geometry and alpha fixed. Never recolor/remap the stock hand or change clothing/watch UVs to hide a skin seam.                                                                                             |
| Pistol hand/sleeve occlusion | First-person weapon depth layer           | Both native hands and added arms use depth testing in the same near projection. A sleeve in front must cover skin regardless of display-list order. Restore the scene projection and clear weapon depth state after each weapon draw.                                                               |
| ADS fade                     | Shared color/depth topology               | Same triangles in both passes; depth lists contain geometry only. Opaque draws omit unused depth lists.                                                                                                                                                                                             |

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
world projection. Restore the world projection after the complete hand pass.
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

Long-gun camera proximity is a separate pose-table control in `joy_port.c`.
Unscoped ADS origins move 20% closer along camera Z, with calibrated sight
heights/pitches and world FOV retained. Gun and grip hands share that transform;
the forearms still fit their camera-space elbow targets. This does not change
the 0.052 hand scale or 2.5 body multiplier above. Pistol poses and the sniper's
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
the camera.

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
- Rigid hand/watch transforms, protected wrist continuity and the rifle body's
  radius independently of its centerline, across all 14 grip pairs and blends,
  including the rocket launcher's trigger grip and tube support.
- Skin-only corrections and terminal material reset on generated arm meshes,
  plus geometry-only fade depth lists.
- Native skin UV/RGB sampling, light scaling and unknown-material fallback;
  per-material ownership, clothing/watch preservation and unchanged geometry/alpha.
- Bracelet clearance around end sections and intermediate bulges in all six wardrobes;
  complete opaque/fading geometry, alpha and original source preservation.

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
