# Immersive browser room plan

Accepted on 2026-10-24. This extends the working [web target](web_target_plan.md);
the existing storage identities, ROM policy and native game remain intact.
Track implementation in WEB-007 through WEB-010 of
[milestone progress](web_target_progress.md).

WEB-007 through WEB-010 are complete for the published desktop preview. The
tracker records clean-source CI, actual published gameplay and save preservation
across deployment. Actual Safari and hardware-GPU measurements remain outside
this preview's completed acceptance.

The accepted follow-up replaces the procedural console/controller with
permissively licensed, attributed third-party models, adds background-drag
cartridge pivoting while it floats, preserves the game's native canvas aspect
ratio with black bars, and fits the TV-flight endpoint to its visible screen.
Track these additions in the existing WEB-007 through WEB-010 blocks.

The owner's 2026-10-25 follow-up supersedes the earlier fresh returning-player
power prompt: a validated remembered ROM starts the game directly on a normal
page load. F2 and an explicit reload-to-manage-saves path can hold startup for
backup restoration. Native EXIT GAME maps to a browser return-to-room action:
pause the live instance, release input/audio, flush saves and reverse the TV
flight into an inserted-cartridge room. Its power switch resumes that instance;
an explicit exit must not trigger automatic re-entry. Reduced motion uses a fade.

The accepted startup follow-up adds loading feedback while the game
downloads and initializes, including remembered-ROM launches. Percentages report
actual download bytes or completed graphics-preparation work; unmeasured stages
remain indeterminate. Browser shader preparation finishes before visible logos,
yielding between programs so progress paints. The owner's next refinement starts
native preparation as soon as a valid ROM and runtime are available, overlapping
insertion and flight. Show those real stages and progress on the CRT texture,
with restrained scanlines/interlacing, and hold at the fitted screen if work
outlasts the zoom. Hold the game's first rendered frames until the cinematic
finishes, then resume and crossfade after fresh frames arrive. Continue room
drawing at a capped rate while preparing; this supersedes the earlier static-TV
hold and full-page first-visit loading surface. Remembered-ROM launches retain
the centered loading surface because they skip the room. Real interruptions
expose Resume startup without skipping the room, including during final handoff.
Center startup failures, especially
another tab's save-writer lock, with explanation and retry rather than requiring
scrolling. Remove the game canvas's decorative gold focus outline while preserving
keyboard focus indicators on UI controls. Reaching gameplay from a cutscene/watch
must not pause merely because mouse capture needs a gesture; keep keyboard play
live with a small capture affordance. Actual focus loss, explicit pause and
unexpected pointer release still require Resume.

## Experience and decisions

The accepted ROM/loading refinement removes the selection dialog's marketing
tagline, allows an outside click/tap to dismiss it, and uses a quick 160 ms fade
for its panel/backdrop. Inside clicks and drags that begin inside must not dismiss;
keep native focus restoration and the existing validation-in-progress guard.
Reduced motion removes the fade. Fullscreen remembered-ROM loading, retained
launcher progress and the CRT share a dark watch/HUD palette, compact capitals,
gold headings and segmented green gauges. Measured percentages stay real;
unmeasured work remains indeterminate with a still indicator under reduced motion.
Keep CRT scanlines/interlacing, the idle blue no-signal screen, first-visit loading
inside the TV, attribution/build time and existing interruption/retry recovery.
Paint active cartridge checking beneath the fading dialog; stage/percentage
changes update the TV immediately. An idle open dialog still suspends the room.

- First visits open into a full-viewport 3D room, without the visible HTML launcher.
  Realistic prop materials, a CRT on a stand, an N64 on the floor and connected
  cables establish a dark room lit mainly by the blue television.
  The owner's 2026-10-27 clarification places **Import Save** and **Export Save**
  inside GoldenEye's four-campaign Files screen, in a two-row/three-column action
  area alongside Copy, Erase, Options and Exit Game. Remove the earlier room
  toolbar interpretation. Reuse the portable JSON codec and replacement
  confirmation; stage an in-game import for application before the next native
  startup, then restart safely. Game canvas, storage identities and ROM handling
  stay intact, and ordinary live-file replacement remains prohibited.
- The initial CRT displays VIDEO 3 / NO SIGNAL with restrained scanlines,
  flicker, noise and tracking distortion. Keep effects on the introductory TV;
  gameplay becomes the clean existing game canvas.
- For first visits, a large cartridge floats near the viewer above the console
  and slowly turns around its vertical axis (a turntable spin, not a roll).
  Hover changes the cursor and highlights the interactive prop. Clicking opens
  a ROM dialog with drop and file-picker paths; dropping onto the room also works.
- Primary-pointer dragging that starts off the cartridge pivots the floating
  prop, with bounded vertical tilt. Direct cartridge clicks retain ROM selection;
  drag release must never activate an underlying prop. Insertion eases manual
  rotation back into alignment. Do not pivot an inserted cartridge or steal input
  during transitions, dialogs or gameplay. Deliberate dragging works with reduced
  motion while automatic rotation stays disabled.
- Validate through the existing ROM parser. Reject invalid files without
  inserting the cartridge. A valid selection aligns and inserts it with eased
  settling, moves the power switch and illuminates a red LED. Then fly into the
  TV and hand off to the game when the runtime is ready.
  Finish facing the screen head-on at a distance calculated from its dimensions,
  viewport aspect and camera field of view. Contain every screen corner and fit
  one axis flush; aspect differences must not crop or stretch the glass.
- A normal fresh launch with a validated remembered ROM starts directly in the
  game, without constructing the room or requesting the ROM again. Native
  EXIT GAME pauses that instance and reverses the TV flight into the room with
  its cartridge inserted and power off. Clicking the power switch resumes the
  same game through the shorter power-on/TV transition.
- F2 opens/closes the retained HTML controls and pauses/releases gameplay when
  opened. Preserve saves, backup import/export, errors, source, provenance and
  credits there. Provide an in-scene controls affordance and keyboard access.
- Alt+Enter requests/exits browser fullscreen from the key gesture. The viewport
  fills the page independently of fullscreen permission. P retains the native
  watch binding; Escape keeps normal browser pointer/fullscreen release behavior.
- Preserve the actual game framebuffer aspect ratio in viewport, controls and
  fullscreen. Center the canvas against black letterbox/pillarbox bars, track
  native resolution changes and keep visible bounds correct for mouse mapping.
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
is allowed for real pauses; a capture gesture by itself must not force a pause.
Fullscreen requests must happen inside the actual gesture, not after a
multi-second animation.

## Assets and ownership

Selected replacements for the procedural console/controller:

- [Simple N64 model by Swayne](https://sketchfab.com/3d-models/simple-n64-model-a17efae58ebd4f1d8e7db0f5cf6f0023),
  CC BY 4.0, selected console geometry with an isolated/red power switch.
- [N64 Controller by Marshle](https://sketchfab.com/3d-models/n64-controller-8fb694b8c4bf4bbb8bc66f38503f4ffb),
  CC BY 4.0, compact redistributed mesh and unchanged embedded base-color texture.

These are independently authored hardware models, not official Nintendo files.
[Model provenance](../../web/src/room/MODELS.md) records pinned public acquisition
sources, original credits, transformations, hashes and regeneration. The earlier
procedural console/controller are superseded; the original CRT/cartridge remain.

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

The first continuation refined the original reconstruction into the preview's
props: shaped console/cartridge shells, detailed controls, a controller,
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
