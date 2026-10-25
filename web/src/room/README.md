# Immersive room implementation

The accepted [experience plan](../../../scripts/port/web_room_plan.md) and
WEB-007 through WEB-010 in [progress](../../../scripts/port/web_target_progress.md)
own scope and remaining work. Solid owns the controls; Three.js owns only the
introductory scene. The game canvas remains mounted across both presentations.

## Current prop provenance

`geometry.ts`, `props.ts` and `scene.ts` create original procedural geometry,
materials, cable curves, wood grain and the CRT signal. The cartridge now uses
the [user-supplied label image](ARTWORK.md), mapped directly from the unchanged
product photograph. No assets are extracted from the ROM.
These are interaction/lighting stand-ins, not claims of final console fidelity.
Their source follows the repository license. Three.js is MIT licensed; assembly
includes its installed LICENSE in THIRD_PARTY_NOTICES.txt.

The approved Joseph Rohm console candidate's official Sketchfab download endpoint
returns HTTP 401 without authentication. It remains a final-prop dependency; no
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
- `props.ts`: named console/cartridge/TV objects and switch/LED materials.
- `room.tsx`: browser gestures, ROM dialog and runtime handoff orchestration.
- `rom-dialog.tsx`: accessible file/drop selection and remember-ROM controls.
- `../room.css`: viewport presentation, dialogs and keyboard-only affordances.

The render loop sleeps while controls/a dialog are open or the document is
hidden. After the first game graphics tasks, Solid unmounts the room, cancels
its RAF and disposes scene geometry/materials/textures/lights and its WebGL
renderer/context. Fullscreen belongs to the shared experience container.

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
