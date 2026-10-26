# Throwing-knife ADS

Gameplay > General Settings > Knife ADS is available in the frontend and watch.
`[display] knife_ads=0` defaults to **Butt Centered**, keeping the stock blade
grip, orientation and view depth and translating the knife's butt onto screen
center. `knife_ads=1` selects **Tip Centered**, a procedural handle grip with the
tip centered. It shares the normal ADS hold/toggle and dedicated ADS binding.
The global Red Reticle mode retains normal aiming unless dedicated ADS is held.
Two equipped weapons and other thrown items keep their original controls.

## Presentation and native contracts

The stock model's aiming points are butt `(1, 338, 150)` and tip `(1, -97, 1)`.
The advanced grip rotates the knife about a moving grip point from the stock
blade grip `(0, 50, 65)` to handle center `(0, 270, 150)` in native model units.
A half-turn around X and a 0.5-radian Y rotation expose the blade face. Only the
knife moves within the unchanged native hand. The simple view origin retains
Z = -34; the tip style uses -95 so the longer upright silhouette and grip fit.
The actual basis transforms each reference before centering its projected X/Y.

The existing 150 ms ADS blend drives raise/lower. Presentation state follows
continuously, including aim reversal and changing style while aiming. During
the original draw/throw/recover sequence, the presentation lowers and the tip
style regrips over 100 ms, finishing before the original release. Releasing aim
mid-throw does not reverse that regrip. Recovery resumes the current requested
aim state once the native animation permits it. Input reset, unsupported weapon
states, watch/death and weapon changes clear transient state per player.

The renderer copies only three knife display lists beneath the stock model's
weapon-visibility switches, replacing their root matrix load with a temporary
rigid transform. It restores original node-data pointers after building the
draw list, including partial failure. No original vertices, materials, UVs,
triangle commands or hand lists change. Unrecognized nested/matrix list layouts
use the native draw. Native pistol/arm contracts and weapon icons are retained.

Throw logic still owns ammo consumption, release, spawn position/rotation and
the existing trajectory/FOV launch handling. The knife's visual rotation has
returned to stock by release. Gravity, upward bias and player velocity mean the
centered sight indicates direction rather than a guaranteed impact point.

## Verification

`bash scripts/port/test_ads.sh` includes `test_knife_ads.c`: settings/default/save
bridges, projection, rigid grip mapping, 30/60/120/240 Hz, reversal/interruptions,
recovery/reset, player isolation and temporary-list rollback. Its compiler
builtins are disabled so missing legacy-header declarations cannot be masked by
host builtin implementations. Run wardrobe regressions and regenerate/verify
the hand gallery as required by the arm guide.

```powershell
./scripts/port/audit_ads.ps1 -Item 3 -Stage runway -KnifeStyle Butt -Transitions
./scripts/port/audit_ads.ps1 -Item 3 -Stage runway -KnifeStyle Tip -Transitions -KnifeThrows
./scripts/port/audit_ads.ps1 -Item 3 -Stage runway -KnifeStyle Tip -Aspect Classic -WeaponFov
```

The isolated audit enables `GE_ADS_AUDIT=3`; `GE_KNIFE_ADS_THROWS=1` adds two
real trigger sequences, releasing aim during the second. It uses the native
throw state machine, logs state/ammo/visibility and captures its frames.
`GE_KNIFE_ADS_BASELINE=1` retains hip fire for a paired control. Private captures
and extracted game data stay ignored. The normal 20-firearm audit remains intact.

The 2026-10-25 local acceptance covers both styles in wide/4:3 and the 4:3
alternate weapon FOV/HFOV 110/120 Hz combinations, committed/interrupted native
throws, and a no-reserve last-knife auto-switch to the pistol. All 20 firearm
captures and the unchanged 72-view/35-icon gallery control pass. Windows and
optimized Wasm builds pass; the strict callback survey compiles all 278 sources.
Edge renders both real-ROM styles and verifies native-menu touch selection and
persisted Tip Centered after a cached-ROM reload. Private evidence is indexed
by `build/knife-ads/*-audit.log`, with browser captures/settings checks in `web/`
and `web-settings/` beneath that ignored directory.
