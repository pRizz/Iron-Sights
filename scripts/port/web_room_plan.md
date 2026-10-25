# Immersive browser room plan

Accepted on 2026-10-24. This extends the working [web target](web_target_plan.md);
the existing storage identities, ROM policy and native game remain intact.
Track implementation in WEB-007 through WEB-010 of
[milestone progress](web_target_progress.md).

## Experience and decisions

- Open into a full-viewport 3D room, without the current visible HTML launcher.
  Realistic prop materials, a CRT on a stand, an N64 on the floor and connected
  cables establish a dark room lit mainly by the blue television.
- The initial CRT displays VIDEO 3 / NO SIGNAL with restrained scanlines,
  flicker, noise and tracking distortion. Keep effects on the introductory TV;
  gameplay becomes the clean existing game canvas.
- For first visits, a large cartridge floats near the viewer above the console
  and slowly turns around its vertical axis (a turntable spin, not a roll).
  Hover changes the cursor and highlights the interactive prop. Clicking opens
  a ROM dialog with drop and file-picker paths; dropping onto the room also works.
- Validate through the existing ROM parser. Reject invalid files without
  inserting the cartridge. A valid selection aligns and inserts it with eased
  settling, moves the power switch and illuminates a red LED. Then fly into the
  TV and hand off to the game when the runtime is ready.
- Returning players see the cartridge already inserted after cache validation.
  Clicking the console power switch starts a shorter power-on/TV transition.
  Never start automatically or request the ROM again while the cached one works.
- F2 opens/closes the retained HTML controls and pauses/releases gameplay when
  opened. Preserve saves, backup import/export, errors, source, provenance and
  credits there. Provide an in-scene controls affordance and keyboard access.
- Alt+Enter requests/exits browser fullscreen from the key gesture. The viewport
  fills the page independently of fullscreen permission. P retains the native
  watch binding; Escape keeps normal browser pointer/fullscreen release behavior.
- Brief post-interaction hints explain F2, Alt+Enter and P. No initial HTML banner.
  Honor reduced motion with no idle spin/flicker and a short fade instead of flight.
  Errors and permission recovery may use accessible HTML dialogs/overlays.

## Architecture

Keep SolidJS as the UI owner. Three.js owns a separate introductory WebGL canvas;
the existing Wasm/SDL game canvas stays mounted and keeps its context throughout.
Do not transfer WebGL contexts or remount the engine when opening controls.
Stop the room render loop and release its GPU resources after handoff. Cap pixel
ratio and avoid sustained double rendering during gameplay. Pause room animation
when the page is hidden; reduced motion and failure fallback remain usable.

Use a pure transition/timing module for idle, insertion, loading, power, flight
and handoff decisions. Treat ROM readiness, engine readiness, animation and
permissions separately. Downloads start in the background; the TV shows actual
loading stages instead of fictitious percentage progress. A later click/Resume
is allowed if audio or Pointer Lock needs a fresh gesture. Fullscreen requests
must happen inside the actual gesture, not after a multi-second animation.

## Assets and ownership

Approved candidate references, subject to actual-file inspection:

- [N64 by Joseph Rohm](https://sketchfab.com/3d-models/nintendo-64-0bec1e1ed1bf4a96af858008b72437a3),
  listed Creative Commons Attribution, approximately 16.8k triangles.
- [Television by Poly by Google](https://poly.pizza/m/6Fa29JN3A3F), CC BY 3.0,
  with a publicly served GLB. Inspect shape/materials before selection.
- [Cartridge by diskturbo](https://sketchfab.com/3d-models/nintendo-64-game-cartridge-44576ac55e164f4487360d3a0aff8b46)
  is CC BY-ND; do not modify it for the custom prop. An original cartridge with
  an original label was the initial implementation default. The user's later
  2026-10-24 request supersedes that label with their supplied cartridge image;
  see [artwork provenance](../../web/src/room/ARTWORK.md).

Keep author/source/license/modification records with any shipped models. Vendor
approved assets into the site, never hotlink runtime downloads. Inspect external
URIs and package only allowlisted files with hashes. Preserve the ROM-free site
policy: no ROM-extracted geometry/textures. The specifically user-approved
cartridge photograph is a separately recorded, exact public asset; do not infer
permission to include unrelated game art or local captures.
If an approved model download requires login, record that dependency and use an
explicitly documented original procedural stand-in to prove the interaction.
Do not claim that the stand-in is the downloaded candidate or final prop art.

The continuation refines the original reconstruction into the desktop preview's
shipped props: shaped console/cartridge shells, detailed controls, a controller,
curved CRT glass and generated material grain. The authenticated candidate is an
optional future replacement, not a release dependency. This choice avoids
shipping the unsuitable flat-TV candidate and keeps all geometry reproducible;
the result is identified as an original model rather than a scan/exact replica.

## Milestones and acceptance

1. WEB-007: persist this plan; introduce the viewport/controls architecture,
   F2/Alt+Enter handling, pure transitions and tests. Keep all storage controls
   usable and the game canvas stable through toggles.
1. WEB-008: build and visually review the room, lighting, CRT and prop hit targets;
   record asset provenance and any unresolved candidate download/artwork work.
1. WEB-009: connect real-ROM selection/cache recall, invalid-file recovery,
   insertion/power/flight and safe engine handoff. Verify first/repeat visits,
   full game startup and capture/focus/watch plus unchanged save durability.
1. WEB-010: verify responsive/reduced-motion/keyboard/failure paths, production
   project-base packaging and the desktop browser matrix. Measure scene size and
   frame cost, confirm teardown, update audits/docs, and publish only verified work.

Native engine/render changes are unnecessary for the first slice. If those become
necessary, follow the existing native regression/gallery contracts. Record exact
commands, private evidence paths and outstanding work before handing off.
