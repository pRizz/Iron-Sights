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
the rendered weapon. These corrections retain each weapon's pitch and eye
distance.

| Weapon | Result |
| --- | --- |
| KF7 / AK47 | Sight hood already centered; retained. |
| Klobb | Raised the end of the sight channel: height 3.2 → 2.7. |
| ZMG | Raised the open notch: height 4.4 → 4.0. |
| D5K | Raised the circular sight: height 6.3 → 5.8. |
| Silenced D5K | Raised the shared sight geometry: height 6.3 → 5.0. Its longer muzzle node needs a different compensation from the unsilenced gun. |
| Phantom | Forked sight tips already meet the reference; retained. |
| Shotgun | Raised the barrel-top aiming reference: height 3.7 → 2.6. |
| Automatic shotgun | Existing raised-barrel reference retained. |
| Sniper rifle | Scope crosshair centered, weapon hidden in full ADS; retained. |
| Laser | Open ladder leaves the reference visible; retained. |
| Grenade launcher | Visual barrel/receiver clearance reviewed; retained. Projectile trajectory is not calibrated as a hitscan sight. |
| AR33/M4 and RC-P90 | Rechecked the previous aperture/tip corrections; retained. |

The D5K regression test uses the two actual muzzle-node coordinates and checks
that the shared sight mesh receives the same final pose despite the suppressor
length. It checks the relationship, rather than pinning two independent tuning
constants. See [test_ads.c](test_ads.c).

## Validation

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
