# Immersive room implementation

The accepted [experience plan](../../../scripts/port/web_room_plan.md) and
WEB-007 through WEB-010 in [progress](../../../scripts/port/web_target_progress.md)
own scope and remaining work. Solid owns the controls; Three.js owns only the
introductory scene. The game canvas remains mounted across both presentations.

## Current prop provenance

`geometry.ts`, the cartridge/TV modules and `scene.ts` create original geometry,
materials, cable curves, wood grain and the CRT signal. The cartridge uses
the [user-supplied label image](ARTWORK.md), mapped directly from the unchanged
product photograph. No assets are extracted from the ROM.
The original procedural console/controller are superseded by Swayne's console
and Marshle's controller, both CC BY 4.0. [Model provenance](MODELS.md) records
their pinned redistribution sources, original author/license links, modifications,
source/output hashes and reproducible preparation command. The existing console
switch is isolated for animation; the controller retains its mesh, normals, UVs
and unchanged embedded PNG. Runtime downloads use only the site's local GLBs.
The CRT and cartridge remain original models with reproducible source geometry.
Three.js is MIT licensed; assembly includes its LICENSE and the complete model
attribution/change notice in THIRD_PARTY_NOTICES.txt.

The approved Joseph Rohm console candidate's official Sketchfab download endpoint
returns HTTP 401 without authentication. It remains an optional alternative; no
authenticated download was attempted and no files from it are shipped.

The approved Poly by Google television candidate is listed CC BY 3.0 at
<https://poly.pizza/m/6Fa29JN3A3F>. Its public inspection GLB is
<https://static.poly.pizza/0b632a99-ea29-4b5c-bb1f-7154365a6515.glb>:
9,292 bytes, SHA-256
`2f99b14890598398fed6c471439f8b0a6671d7ecbd2fb511920ab02e62f6e3ed`.
The inspected file contains 100 triangles, two plain materials and no images or
external buffer URIs. It is a very simple flat display, so it is not shipped as
the requested realistic CRT. The local inspection copy stays in ignored
`build/web-room`; the current CRT is original procedural geometry.

The diskturbo cartridge candidate is CC BY-ND. It is not downloaded, modified or
shipped. The custom cartridge uses original rounded geometry and the explicitly
requested label artwork. The earlier text-only placeholder is superseded.

## Boundaries

- `flow.ts`: pure action/timing decisions; no DOM, ROM bytes or storage.
- `scene.ts`: scene lighting, rendering, CRT painting, raycasting and teardown.
- `models.ts`: asynchronous local GLB loading, shadow setup and failed-load cleanup.
- `console.ts`, `controller.ts`: licensed-model placement and switch/LED/cable setup.
- `cartridge.ts`, `television.ts`: original prop geometry.
- `materials.ts`, `environment.ts`: deterministic grain/bump detail, wood, room
  lighting and a generated low-resolution reflection environment.
- `crt.*.glsl`: rounded phosphor screen, scanlines/noise/tracking and cold-power
  effect; reduced motion disables flicker/noise/boot flashing.
- `cinematic.ts`, `framing.ts`: tested alignment/descent and a screen-fitted camera
  endpoint. The final view faces the glass head-on and contains all screen corners;
  one axis reaches the viewport edge and differing aspect ratios are not cropped.
- `pivot.ts`: background-pointer drag capture, bounded tilt/yaw, click suppression
  and cancellation; direct cartridge clicks keep the ROM dialog interaction.
- `batch.ts`: tested opaque static batching; interactive parts, glass and the
  emissive LED stay independent. The flat `props.ts` prototype is removed.
- `room.tsx`: browser gestures, ROM dialog and runtime handoff orchestration.
- `rom-dialog.tsx`: accessible file/drop selection and remember-ROM controls.
- `../room.css`: viewport presentation, dialogs and keyboard-only affordances.

The render loop sleeps while controls/a dialog are open or the document is
hidden. After the first game graphics tasks, Solid unmounts the room, cancels
its RAF and disposes scene geometry/materials/textures/lights and its WebGL
renderer/context. Fullscreen belongs to the shared experience container.
Idle/reduced-motion room drawing is capped at 30 fps. Stationary prop shadows
are reused, while moving props update them. Only the room scene chunk imports
Three.js; a validated Vite manifest allowlists that dynamic graph. The launcher
stays available if the chunk/image/context fails, and a failed room cannot strand
the user behind a disabled Return-to-room control. The TV stays until the first
game graphics tasks, then a 350 ms crossfade (160 ms reduced) reveals the game.
`quality.ts` reduces only the room's pixel ratio to 75%, then 50%, after sustained
frame intervals above 80 ms; fast intervals reduce the slow-frame score and
isolated compilation hitches do not trigger it.
At reduced room quality the shadow map is 512 pixels. The game canvas/settings
are untouched. Original AVIF artwork retries the lossless PNG when unsupported.

`../presentation.ts` observes the game stage and actual framebuffer dimensions.
It centers a proportionally fitted canvas against black bars without changing
buffer resolution; mouse coordinates use that same visible element rectangle.
This applies to viewport, retained controls and fullscreen, including native
resolution changes. Background dragging affects only the floating cartridge;
insertion eases its manual tilt/yaw back into alignment. Reduced motion disables
automatic spin but still permits deliberate direct dragging.

Room construction/context failure opens the retained standard launcher. F2 never
automatically recaptures the mouse after closing controls; Resume remains a
gesture. P continues through the native watch binding. Reduced motion suppresses
idle rotation, bob and screen flicker and shortens the transition without flight.

## Local checks

```sh
bun run typecheck
bun run test
bun run build
bun run verify:site
bun run test:room http://127.0.0.1:4176
bun run test:launcher http://127.0.0.1:4176
node tools/room-game-audit.mjs ../baserom.u.z64 ../build/web-room/game
```

Run the fixed-root `tools/serve-site.mjs` first. Set `MIDAS_BROWSER=msedge` or
`chrome` on this Windows host. Actual-ROM evidence stays private under build/;
the ROM-free room audit is also part of the Pages build's production-prefix check.
`MIDAS_AUDIT_OUTPUT` separates ROM-free evidence directories. The audit writes
steady room FPS, draw calls, triangle count and JavaScript render-submission time
after initial shader warm-up; these are host-specific software-renderer samples,
not GPU timer queries or hardware guarantees. Set `MIDAS_ROOM_GAMEPLAY=1` for
normal-menu Dam, movement, P watch and focus/capture recovery after the cinematic;
`MIDAS_REDUCED_MOTION=1` verifies the short fade path with the actual ROM.
`tools/linux-game-audit.sh` accepts an optional `room` or `ui` audit mode for the
isolated Firefox/Xvfb/PulseAudio host described in the compatibility runbook.

The ROM-free audit also covers background pivoting, click/drag separation,
letterboxing and failed model loads. `room-framing-check.mjs` renders the actual
production scene at its endpoint in landscape and portrait for projected-corner
assertions and private visual captures. The actual-game audit checks native
framebuffer sizing through controls, wide/portrait viewport changes and fullscreen.
