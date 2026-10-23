# Solo suspend points, version 1

The fork adds four manual slots per campaign folder, plus separate quicksave
and undo-load images. F6 opens the menu; F5 saves, F9 loads, and F8 undoes a
load. Controls exposes all four actions for keyboard and controllers. The
watch has a Gameplay > Suspend Points action. Slot rows show the mission,
difficulty, local save date/time, and elapsed mission time. An occupied slot
requires a second Accept before overwriting; quicksave deliberately replaces
its own slot without confirmation.

Saving is allowed during solo play with Bond alive and in control, or with
the watch fully open. Cutscenes, demo playback, multiplayer and death cannot
be saved. Existing points can be loaded after death or from the title screen
in the same campaign folder. Before replacing a running mission, loading
writes its current state to Undo Load. If that write fails, loading stops.
Undo is one level, with no redo; another regular load replaces it. Loading
from the title screen has no running mission to capture as an undo point.

The restored world resumes immediately, replacing the initial version's
full-screen restore confirmation/pause. A compact toast identifies the loaded
manual slot, quicksave or undo point and its mission. Successful manual saves
and quicksaves show matching toasts. Notifications last three seconds, fading
over their last half-second; another action replaces the current toast. The
automatic pre-load undo capture is silent, so it cannot announce a manual save.
Toasts stay visible over the slot browser and do not depend on combat/HUD
feedback settings. A saved watch closes through its usual animation.
HUD feedback and held aim/input presentation reset, so stale damage alerts or
a banked action do not replay. Menu keys stay out of gameplay bindings until
released; closing the slot menu waits for a fresh input sample so confirm does
not also open the watch or cycle weapons.
Campaign progress, input bindings, display preferences and player profile
files remain owned by the current process and are not rolled back. Resumed
missions otherwise use the existing completion/progression rules.

## Compatibility and storage

Files live beside the executable under
`Data/SaveStates/profile-<folder>-slot-<number>.mstate`. Slots 1–4 are manual,
5 is quicksave, and 6 is undo. An overwritten file is retained as `.bak`.
The writer flushes and syncs a temporary file before atomically replacing
the destination. Failed writes leave the prior destination intact.

The initial format deliberately requires the **exact executable**, ROM and
memory arena addresses. Rebuilding, changing platform or using a different
release makes old images incompatible. Our local workflow replaces executables
without retaining previous builds; obsolete executables and suspend images
matched to their build fingerprints may be purged. Current-build and unknown
images, ordinary campaign saves, profiles and settings are kept. Audit copies
of the executable are removed when the audit finishes. This format does
not migrate snapshots between releases. Files contain game assets and live
mission data and are ignored by Git; do not distribute them.

The reader checks the format version, header checksum, executable SHA-256,
original normalized ROM fingerprint, campaign folder, stage/difficulty,
region descriptors, sizes and per-region CRC32. It decodes and validates the
entire image before changing the world. Truncated, damaged or incompatible
files are rejected. Checksums detect damage; they do not authenticate files.
Load only your own local images.

## Implementation

This native port does not have an emulator's virtual machine to serialize.
`embed_state_layout.py` registers the engine's owned writable linker sections
and appends that registry and build fingerprint to the executable. It includes
live gameplay, AI, scripts, inventory, objectives, RNG and engine audio state,
plus asset data. Host CRT/SDL/GL state, process stacks, native scheduler/input,
file handles, diagnostic heap caches, settings and campaign saves are excluded.
The registry is generated for both Windows archive and Linux object map formats.

`port_state.c` captures those sections, the fixed game allocation arena, the
used high asset arena, and the live ROM buffer. ROM-backed data is converted
in place during play, so the original fingerprint is retained separately and
the converted bytes are included in the image. ROM directory strings now use
module-owned storage rather than a relocatable host heap allocation. The high
arena's allocation watermark never moves backwards, preserving space still
owned by process-side caches.

The background conversion registry travels with the ROM bytes: revisiting a
previously loaded mission after restoring must convert its now-raw background
again, rather than relying on the previous process-side conversion record.

Commands run at the boss frame boundary. A render-thread barrier completes
outstanding frames before accessing/replacing world memory, without changing
the scheduler's completion accounting. Cross-mission loads initialize the
target solo stage through the ordinary loader before applying the image.
Renderer caches and queued native audio are cleared after restoring. New
missions discard an old suspend-menu pause. Version 1 has no rewind history,
automatic checkpoints, thumbnail previews or cross-build migration.

When adding a module with live mission state or a persistent native pointer,
review the registry ownership rules. Do not extend them to whole-process
memory. Re-run the fresh-process and cross-mission audit after such changes.

## Checks

Run in the build container:

```sh
bash scripts/port/test_states.sh
bash scripts/port/test_ads.sh
bash scripts/port/test_hud.sh
```

The ROM-free state tests exercise zero/literal runs, mixed and random data,
truncation, invalid run bounds, damaged payloads and compatibility rejection.
Input tests cover default controller Back, expired/one-shot shortcuts, and a
D-pad reported simultaneously as a stick and buttons without double navigation.
State tests also cover immediate restore without a pause, automatic watch
closure, slot/mission labels, replacement and wall-clock toast fade timing.
Python tests cover linker ownership filters, COMMON sections, static host-cache
exclusion, and bounded/idempotent executable overlays.

On Windows with a built executable and the local ROM:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/port/audit_states.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/port/audit_states.ps1 -CrossProcessOnly -RenderThread
```

The launcher copies the executable and SDL into a new ignored audit directory,
with its own settings and save files. It checks all manual slots, quickload,
undo-load, recovery after death, a watch-menu save, fresh-process restore from
another mission and from the title screen, automatic resumption without a
confirm press, resumed movement, and rejection of
a truncated image without world changes. Probes compare the complete player
and character records, mission timer, selected weapon and all three RNG seeds
at the application boundary. Input playback also exercises firing and weapon
changes. The cross-mission run revisits the previous mission after restoring,
then restores again to check conversion-cache consistency. The shorter render-thread run checks manual/quick/undo/death and
fresh-process cross-mission restoration with asynchronous rendering enabled.
It retains captures and logs for inspection.

The Windows audits passed with synchronous and threaded rendering, including
Dam/Facility transitions and a second restore after revisiting Facility.
Windows and Linux builds and the ROM-free state, ADS and HUD tests passed.
Linux was compile-checked; runtime captures and gameplay checks ran on Windows.
