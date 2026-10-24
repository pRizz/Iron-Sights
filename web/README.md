# Browser tooling and launcher groundwork

This subpackage currently supplies ROM validation, the versioned save-backup
codec, storage adapters, unit tests and private runtime/menu audits. The persistent SolidJS launcher is
still under development. It does not yet provide a frontend build or deployable
site. See the [runtime runbook](../scripts/port/web_runtime.md) and
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

The test fixtures contain synthetic bytes, never a ROM or extracted assets.
Validation accepts only the original 12 MiB US ROM, including byte-swapped file
formats, by checking the normalized SHA-256. The backup codec allows campaign
EEPROM, profiles, device bindings, settings and custom controller layouts. It
rejects ROM/suspend paths, traversal, duplicate paths, oversized data, damaged
checksums and unsupported schema versions. Filesystem transactions, persistence
errors and browser UX need integration tests before those milestones complete.
The ROM store snapshots original bytes into a separate IndexedDB Blob and
survives reload; its real-browser test verifies failed-transaction preservation
and deletion. A Web Lock prevents concurrent save writers. The serialized sync
queue retains pending changes after errors and exposes retry/status. These
adapters are not yet connected to the actual game launcher. The IDBFS adapter
also passes a ROM-free integration fixture through real C stdio saves, reload,
successive writes, backup replacement and failed-sync/partial-write rollback.
The failure tests inject errors; they do not exhaust the host browser's quota.

Dependencies are pinned in `package.json` and `bun.lock`. SolidJS, Vite and its
stable Solid plugin are pinned for the accepted launcher design. The TypeScript
configuration skips dependency declaration checks because Bun and Vite declare
incompatible `ImportMeta.hot`/environment interfaces; repository source still
uses strict type checking.
