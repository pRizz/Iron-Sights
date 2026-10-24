# Browser tooling and launcher groundwork

This subpackage currently supplies ROM validation, the versioned save-backup
codec, unit tests and a private runtime audit. The persistent SolidJS launcher is
still under development. It does not yet provide a frontend build or deployable
site. See the [runtime runbook](../scripts/port/web_runtime.md) and
[milestone tracker](../scripts/port/web_target_progress.md).

```sh
bun install --frozen-lockfile
bun run typecheck
bun test src
bunx playwright install chromium firefox webkit
```

With the video prototype built and its output directory served locally:

```sh
bun run audit:runtime ../baserom.u.z64 ../build/web-audit
```

On PowerShell, `$env:MIDAS_BROWSER = 'msedge'` selects installed Edge instead of
Playwright Chromium. Captures, console logs and runtime reports remain under the
ignored build directory. Never point the HTTP server at the repository root or
upload these private ROM-derived captures as CI artifacts.

The test fixtures contain synthetic bytes, never a ROM or extracted assets.
Validation accepts only the original 12 MiB US ROM, including byte-swapped file
formats, by checking the normalized SHA-256. The backup codec allows campaign
EEPROM, profiles, device bindings, settings and custom controller layouts. It
rejects ROM/suspend paths, traversal, duplicate paths, oversized data, damaged
checksums and unsupported schema versions. Filesystem transactions, persistence
errors and browser UX need integration tests before those milestones complete.

Dependencies are pinned in `package.json` and `bun.lock`. SolidJS, Vite and its
stable Solid plugin are pinned for the accepted launcher design. The TypeScript
configuration skips dependency declaration checks because Bun and Vite declare
incompatible `ImportMeta.hot`/environment interfaces; repository source still
uses strict type checking.
