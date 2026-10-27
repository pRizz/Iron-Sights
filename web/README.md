# Browser launcher and tooling

This subpackage supplies a SolidJS launcher, ROM validation, the versioned save-backup
codec, storage adapters, tests and private runtime/menu audits. The launcher is
[live on Pages](https://prizz.github.io/Iron-Sights/), with deployment/update
preservation verified. See [browser compatibility and measured limits](../scripts/port/web_compatibility.md);
Safari remains experimental by the user's decision.

The Iron Sights rename changes the project path, not the Pages origin. Keep the
deployed storage identifiers stable (`project-midas-ads:rom`, remember preference,
`/midas-ads-user`, user-files lock and `project-midas-saves` backup format) so ROMs,
campaign saves and settings remain available. These internal keys are not network
paths. With a private ROM, run `node tools/rename-storage-audit.mjs ../baserom.u.z64`
against the assembled site at `http://127.0.0.1:4179/Iron-Sights/` to verify old/new
paths, exact exported save bytes and remembered-ROM autostart.
The immersive room revision is tracked separately in the
[accepted room plan](../scripts/port/web_room_plan.md) and WEB-007 through WEB-010.
Its entrypoint fills the viewport; F2 opens the retained launcher, saves and
About controls, Alt+Enter toggles fullscreen, and Enter or P acts as Start
(including opening and closing the watch). Existing saved bindings are respected.
A validated remembered ROM starts immediately on normal page loads. The native
Exit Game confirmation returns through a reverse TV transition to the room;
its power switch resumes the paused game. To restore a backup after startup,
open F2 and choose **Restart to manage saves**. This reloads with controls open
and holds automatic startup until Play or closing controls. Save restoration
still occurs before the native game starts, so its cached EEPROM stays coherent.
Download and startup stages appear centrally; graphics preparation completes
before the logos animate. Another tab's save lock opens a centered retry dialog.
Cutscene/watch transitions keep keyboard play live; a small capture control
appears if the browser needs a mouse gesture. Real focus loss still pauses.
See [room implementation and prop provenance](src/room/README.md) for local
checks, original prop models and candidate-source decisions.
See the [runtime runbook](../scripts/port/web_runtime.md) and
[milestone tracker](../scripts/port/web_target_progress.md).

Touch controls (WEB-011) auto-enable on coarse touchscreens and touch devices
without Pointer Lock. The launcher checkbox saves an explicit override. On-screen
Options opens the launcher; Pause/Resume does not need mouse capture. Use the
analog left thumbstick and drag right to look. **Touch layout** defaults to
**N64 controller**: blue circular A, green circular B, red circular Start and a
gray rectangular Z on the right send native controller buttons. Hold/drag Z to
shoot and turn, tap Aim to toggle ADS, and use the weapon icon to cycle weapons.
Start opens the watch; A/B also navigate native menus. **Modern actions** retains
Fire/Use/Reload/Crouch/Watch and the More tray for weapon/gadget selection, zoom,
sprint and detonation. The layout choice is saved separately from game files;
switching releases held controls and Aim. Safe-area
insets and proportional canvas fitting apply in portrait and landscape. Pausing,
focus loss, cancelled touches, rotation, mode changes and room return clear input.

`bun run test:touch` builds a ROM-free Solid fixture and verifies actual
multi-pointer capture/cancellation and layout at the native boundary. The Pages
workflow also runs the synthetic Wasm bridge regression. For private real-ROM
mobile-viewport acceptance, use `MIDAS_BROWSER=chrome bun run audit:touch ../baserom.u.z64 ../build/web-touch/game http://127.0.0.1:4177/Iron-Sights/`.
Desktop WebKit emulation needs a simulated touch-capability property and cannot
establish physical iOS support. Its mobile viewport driver also needs an explicit
resize event after changing emulated orientation and uses single-pointer drags;
Chromium verifies simultaneous contacts. Keep ROMs, screenshots and backups local.

For audio changes, run `MIDAS_BROWSER=chrome bun run audit:audio ../baserom.u.z64 ../build/web-audio/current http://127.0.0.1:4177/Iron-Sights/`
(or `MIDAS_BROWSER=msedge`). The private real-ROM audit reaches Dam, records a
12-second mono WAV and continuity/queue metrics on the audio render thread,
then checks Watch, pause silence and resumed sound. It requires AudioWorklet
and measures a quiet reference scene; its sample-jump threshold is not a general
gunshot/music classifier. Nonzero ScriptProcessor callback/analyser samples
alone do not prove that the browser met playback deadlines. The browser's
512-sample SDL period is configured separately from native audio.

Mouse capture requests unaccelerated browser movement, with an automatic
compatibility retry if raw input is unsupported. No motion clamp or smoothing
is applied. For an intermittent mouse snap, press F2 immediately afterward and
choose **Download mouse diagnostics** in the player toolbar. The session-only
report contains the latest bounded movement/timing/heading/scaling samples and
build identity; paused frame history freezes until resume. It contains no key,
ROM/save or world-position data and is never uploaded automatically.

`MIDAS_BROWSER=chrome bun run audit:mouse ../baserom.u.z64 ../build/web-mouse/game http://127.0.0.1:4177/Iron-Sights/`
checks known movement with and without a 250 ms hitch, no subsequent replay,
Watch recapture and exported diagnostics. Set `MIDAS_MOUSE_FALLBACK=1` to test
unsupported-raw fallback; use `MIDAS_BROWSER=msedge` for Edge. Controlled browser
injection does not reproduce hardware acceleration or prove a physical snap's
cause.

```sh
bun install --frozen-lockfile
bun run typecheck
bun run test
bunx playwright install chromium firefox webkit
bun run test:sampler
bun run test:storage
# After `python scripts/port/web_build.py files` from the repository root:
bun run test:filesystem
bun run dev
bun run build
bun run verify:site
# Serve ../build/web-site on 127.0.0.1:4176, then:
bun run test:launcher
bun run audit:launcher ../baserom.u.z64 ../build/web-ui/game
```

With the video prototype built and its output directory served locally:

```sh
bun run audit:runtime ../baserom.u.z64 ../build/web-audit
bun run audit:menus ../baserom.u.z64 ../build/web-audit/menu
```

On PowerShell, `$env:MIDAS_BROWSER = 'msedge'` selects installed Edge instead of
Playwright Chromium. Captures, console logs and runtime reports remain under the
ignored build directory. Never point the HTTP server at the repository root or
upload these private ROM-derived captures as CI artifacts.
The runtime audit measures keyboard/mouse motion, real audio samples, focus
pause/input reset and watch release/recovery. It waits for stable gameplay,
since the first stage frames precede camera/intro initialization. The menu audit
requires visual review of its local screenshots. Serve a completed build; do not
relink its files during an audit. Installed Chrome is selected with `chrome`.
Input/focus measurements enable the existing invisible/invulnerable diagnostics
and a fixed seed through the prototype's private audit query, isolating them
from enemy impulses. Normal launches keep ordinary gameplay.

`node tools/serve-site.mjs 4176 /Iron-Sights/` serves only the assembled site
for local/container audits. Launcher audits also accept `MIDAS_BROWSER=firefox`
or `webkit`; `MIDAS_HEADED=1` uses a display server. The ROM-free
`node tools/capability-audit.mjs <preview-url>` reports initialization and graphics
capabilities. A WebKit run is engine evidence, not macOS Safari certification.

The dev server serves the existing `build/web/video` runtime through an explicit
allowlist. The static build assembles `build/web-site` with a content-versioned
runtime directory, a Wasm size/digest manifest and the game's actual generated
build metadata. It includes no private diagnostic captures. Assembly checks
the selected CMake source set and rejects compiled ROM fallback/icon flags or
the inherited initialized logo. The web logo source is generated without
texture/vertex bytes; runtime fills its 38 backing arrays from the player ROM.
`verify:site` checks an exact public file allowlist and the assembled Wasm digest.
The site carries port/runtime/frontend licenses in `THIRD_PARTY_NOTICES.txt`.

`.github/workflows/web-pages.yml` uses the pinned SDK and locked Bun dependencies,
builds/tests PRs without publishing, and uploads/deploys successful `main` runs.
Its deploy job alone has Pages write/OIDC permissions. The game builder forwards
only explicit public GitHub provenance fields into Docker. No ROM is needed in CI.
Local URLs and the stable browser storage identities remain separate from each
content-versioned runtime directory; an active game owns its already-loaded
Wasm and virtual filesystem across site updates.

The test fixtures contain synthetic bytes, never a ROM or extracted assets.
Validation accepts only the original 12 MiB US ROM, including byte-swapped file
formats, by checking the normalized SHA-256. The backup codec allows campaign
EEPROM, profiles, device bindings, settings and custom controller layouts. It
rejects ROM/suspend paths, traversal, duplicate paths, oversized data, damaged
checksums and unsupported schema versions. Native profile names, including dots,
punctuation and valid Unicode, retain the native 23-byte filename limit.
The ROM store snapshots original bytes into a separate IndexedDB Blob and
survives reload. Backends that reject Blob preparation with `DataCloneError`,
`UnknownError` or `NotSupportedError` retry the immutable snapshot as bytes in
the same schema-1 row; recall reconstructs a Blob. Quota/permission/abort errors
remain visible failures. Its real-browser test verifies failed-transaction preservation
and deletion. A Web Lock prevents concurrent save writers. The serialized sync
queue retains pending changes after errors and exposes retry/status. These
adapters now back the actual launcher. A private real-ROM audit creates campaign,
profile and settings files and compares downloaded backups across fresh launches.
The IDBFS adapter also passes a ROM-free fixture through real C stdio saves, reload,
successive writes, backup replacement and failed-sync/partial-write rollback.
The launcher UX audit tests invalid drop/import, confirmed restore and cancel,
download, storage-protection rejection, fullscreen, cross-tab locking and retry,
damaged remembered-ROM removal, preference reload and an aborted IDBFS write.
Failed restoration preserves memory files and durable files; Retry commits the
preserved snapshot. The failure tests inject errors; they do not exhaust the
host browser's quota.

For an actual deployed update check, set `MIDAS_WAIT_FOR_DEPLOY=1` and run
`audit:launcher` against the live site. Wait for `READY_FOR_DEPLOY_UPDATE`, then
push the reviewed next change. The private audit keeps the watch animating,
requires a new deployed runtime and continued frames, then verifies recovery,
ROM recall and unchanged save backups after reload. It fails after 15 minutes
without an update. Captures/backups remain in the ignored local output directory.

Dependencies are pinned in `package.json` and `bun.lock`. SolidJS, Vite, MysticUI
(exact GitHub commit) and its documented Tailwind 3 setup implement the accepted
launcher design. The TypeScript
configuration skips dependency declaration checks because Bun and Vite declare
incompatible `ImportMeta.hot`/environment interfaces; repository source still
uses strict type checking.

New launcher sources and assembly tools use the exact pinned Prettier version
and `web/.prettierrc.json`; run `bunx prettier --check` on changed frontend files.
