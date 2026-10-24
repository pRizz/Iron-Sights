# Browser launcher and tooling

This subpackage supplies a SolidJS launcher, ROM validation, the versioned save-backup
codec, storage adapters, tests and private runtime/menu audits. The launcher now
builds locally; deployment and broader browser compatibility remain in progress.
See the [runtime runbook](../scripts/port/web_runtime.md) and
[milestone tracker](../scripts/port/web_target_progress.md).

```sh
bun install --frozen-lockfile
bun run typecheck
bun test src
bunx playwright install chromium firefox webkit
bun run test:sampler
bun run test:storage
# After `python scripts/port/web_build.py files` from the repository root:
bun run test:filesystem
bun run dev
bun run build
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

The dev server serves the existing `build/web/video` runtime through an explicit
allowlist. The static build assembles `build/web-site` with a content-versioned
runtime directory, a Wasm size/digest manifest and the game's actual generated
build metadata. It includes no private diagnostic captures. Do not publish this
preview until the deployment packaging/compatibility gates pass.

The test fixtures contain synthetic bytes, never a ROM or extracted assets.
Validation accepts only the original 12 MiB US ROM, including byte-swapped file
formats, by checking the normalized SHA-256. The backup codec allows campaign
EEPROM, profiles, device bindings, settings and custom controller layouts. It
rejects ROM/suspend paths, traversal, duplicate paths, oversized data, damaged
checksums and unsupported schema versions. Native profile names, including dots,
punctuation and valid Unicode, retain the native 23-byte filename limit.
The ROM store snapshots original bytes into a separate IndexedDB Blob and
survives reload; its real-browser test verifies failed-transaction preservation
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

Dependencies are pinned in `package.json` and `bun.lock`. SolidJS, Vite, MysticUI
(exact GitHub commit) and its documented Tailwind 3 setup implement the accepted
launcher design. The TypeScript
configuration skips dependency declaration checks because Bun and Vite declare
incompatible `ImportMeta.hot`/environment interfaces; repository source still
uses strict type checking.

New launcher sources and assembly tools use the exact pinned Prettier version
and `web/.prettierrc.json`; run `bunx prettier --check` on changed frontend files.
