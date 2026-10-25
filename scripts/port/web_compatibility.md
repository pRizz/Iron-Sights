# Browser preview compatibility

The desktop preview is live at <https://prizz.github.io/Project-Midas-ADS/>.
These results were recorded on 2026-10-24. Safari remains experimental by the
user's explicit decision; WebKit automation is not actual macOS Safari testing.
The [milestone tracker](web_target_progress.md) owns completion and handoff status.

## Browser evidence

| Browser                 | Environment                                      | Evidence                                                                                                                                                                                           |
| ----------------------- | ------------------------------------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Edge 156                | Windows, headless SwiftShader                    | Full real-ROM launcher, campaign/profile/settings reload, actual backup restore, capture/audio/focus/watch; eight representative mission smokes; live deployment/update preservation.              |
| Chrome 155              | Windows, headless SwiftShader                    | Full real-ROM launcher and persistence/backup/capture/audio/focus/watch acceptance.                                                                                                                |
| Firefox 157             | Linux, Xvfb, Mesa llvmpipe, PulseAudio null sink | Full ROM-free launcher UX and actual-ROM Dam, audio/capture/focus/watch, campaign/profile/settings reload and backups.                                                                             |
| Playwright Chromium 156 | Linux, headless software rendering               | Production project-path launcher, ROM storage, C/IDBFS and ten GPU sampler cases.                                                                                                                  |
| WebKit                  | Windows Playwright runner                        | ROM-free launcher UX, actual-ROM remembering and native profile/settings input pass. The runner lacks Web Audio and cannot complete gameplay capture recovery. Full game acceptance is incomplete. |
| Safari                  | macOS                                            | Experimental; no macOS host was available.                                                                                                                                                         |

Playwright's full Chromium and Firefox executables failed to launch on this
Windows host with `spawn UNKNOWN`. Installed Chrome/Edge and Linux Firefox
provided independent coverage. Linux headless Firefox had no WebGL 2; Xvfb/Mesa
provided a working context. These host limitations are distinct from game traps.

Firefox's initial silent result came from its isolated host audio connection:
even a standalone oscillator stayed suspended. Connecting an explicit local
PulseAudio Unix socket/null sink made that probe and the game audit pass. This
measures real Web Audio processing, not audible output through physical speakers.

Secure-origin WebAssembly, WebGL 2, IndexedDB, Web Crypto, Web Locks and Pointer
Lock are required. Pages uses HTTPS; localhost works for development. Gamepads
are optional for keyboard/mouse play. Mobile/touch and netplay are outside this
preview's accepted scope.

## Representative mission and effect smoke

The first optimized Edge sweep used a 960x540 prototype canvas and fresh browser
process per mission. It exercised spawning, short movement, ADS, four trigger
presses, continued rendering and nonzero Web Audio. Existing invisibility,
invulnerability and fixed-seed diagnostics isolated input/render behavior.
Normal launcher play does not enable these diagnostics.

| Mission   | Reviewed coverage                                         | Stable gameplay (s) | Sample graphics tasks/s |
| --------- | --------------------------------------------------------- | ------------------: | ----------------------: |
| Dam       | Outdoor geometry, sky, pistol/depth/ADS, fired ammunition |                30.5 |                    59.6 |
| Facility  | Vent geometry, close textured surfaces, weapon/HUD        |                20.6 |                    60.0 |
| Surface 2 | Snow ground, night/fog palette, distant fade              |                22.4 |                    60.0 |
| Jungle    | Fog, foliage/alpha surfaces, subtitle text, pistol ADS    |                22.5 |                    49.1 |
| Frigate   | Water/sky, boat/ship surfaces, silenced long gun          |                23.5 |                    54.1 |
| Caverns   | Close metal/rock, automatic weapon ADS                    |                25.8 |                    57.6 |
| Cradle    | Long-distance depth/fog, railings and sky                 |                22.2 |                    58.4 |
| Egypt     | Lit stone columns, blue floor surfaces and sky            |                19.2 |                    59.7 |

Every case kept capture, advanced more than 60 submitted graphics tasks, emitted
nonzero audio and reported no shader/GL/runtime error. Representative scene,
ADS and fired-weapon images were reviewed. These short spawn-area checks do not
claim completed missions, all rooms/objectives/effects or a pixel comparison
against original hardware. Some views deliberately face close spawn geometry.

Linear capacity stayed at 730,726,400 bytes (about 697 MiB). This is Wasm address
space capacity, not measured resident/peak RAM. Initial capacity is 512 MiB,
maximum 1 GiB, with a high data base for the inherited pointer encoding. The
measurements include software rendering, compilation/model/shader warmup and
ordinary host load; they are not a hardware performance guarantee. Startup and
new shader/model work can hitch. Exact build metadata and raw measurements stay
with the private local reports under `build/web-compat`.

The renderer retains its documented mirror-clamp fallback when the corresponding
extension is unavailable. The ten ROM-free sampler cases cover transparent
borders with point/linear filtering; they are not exhaustive material coverage.

## Deployment and data acceptance

Commit `2360bc8` passed the first Pages workflow
[40145610255](https://github.com/pRizz/Project-Midas-ADS/actions/runs/40145610255).
The subsequent `e4a81be` deployment passed
[40146369375](https://github.com/pRizz/Project-Midas-ADS/actions/runs/40146369375).
A private live-site audit kept the watch animating while the runtime version
changed: frame logging advanced from 2,820 to 32,820 over 511 seconds. Gameplay
capture recovered afterward. Fresh initialization recalled the ROM and preserved
every campaign/profile/settings backup byte; confirmed restore and ROM removal
also passed. No ROM upload or page error occurred.

The final compatibility source `2c890b2` passed
[Pages run 40148810530](https://github.com/pRizz/Project-Midas-ADS/actions/runs/40148810530).
Its published runtime `6521ef4a7a8c672b` also passed the full actual-ROM audit
with the Gamepad API deliberately removed, proving keyboard/profile/capture/watch
startup survives that optional API's absence. Audio, save reload, actual backup
restore and remembered-ROM removal all passed on the live site.

Content-versioned application downloads are separate from the stable user-file
mount, Web Lock and ROM database. Backup replacement remains restricted to before
main starts because the game caches its EEPROM. Browser storage can be cleared;
downloaded backups remain the portable recovery path.

## Reproduce locally

Build using [the runtime runbook](web_runtime.md), then assemble and verify with
`bun run build` and `bun run verify:site` in `web/`. The private prototype is not
part of the published site. Serve only its completed output, never the checkout:

```sh
python -m http.server 4174 --bind 127.0.0.1 --directory build/web/video
node web/tools/mission-audit.mjs baserom.u.z64 build/web-compat/missions
```

Set `MIDAS_BROWSER` to `msedge`, `chrome`, `chromium`, `firefox` or `webkit` as
appropriate. An optional final argument selects comma-separated mission names.
Keep ROMs, private captures and backup reports out of Git/CI uploads. Do not
relink/rebuild served files during an audit.

For isolated Linux Firefox audio testing, build the optional test-host image:

```sh
docker build -f web/tests/Dockerfile.browser-audio -t midas-browser-audio web/tests
mkdir -p build/web-compat/firefox
docker run --rm --init --ipc=host --network=none \
  --mount type=bind,source="$PWD",target=/src,readonly \
  --mount type=bind,source="$PWD/build/web-compat/firefox",target=/evidence \
  -w /src/web midas-browser-audio \
  bash tools/with-browser-audio.sh bash tools/linux-game-audit.sh /src/baserom.u.z64 /evidence
```

The base browser image is pinned by digest in that Dockerfile. Its optional
PulseAudio packages use Ubuntu's signed apt metadata; the observed server was
16.1. This image is a test host, not a game build/publication input. The audio
helper refuses to run outside a container and opens only a local Unix socket.
`tools/audio-capability-audit.mjs` can independently check the sound backend.

## Explicit deferrals

- Weapon icons retain the existing name fallback. Native executable-driven
  generation cannot run in the browser; a browser-local generator requires a
  separate design and must not ship a ROM-derived icon pack.
- Offline application support is deferred. Remembering the ROM/saves does not
  guarantee the application can load without a connection. No service worker
  owns save data or update lifecycle.
- Browser suspend points remain disabled. Native PE/ELF snapshots are incompatible
  with Wasm; restoring its heap/Asyncify stack, JS state and GPU/audio resources
  needs a separate exact-build design and a much larger storage budget than
  ordinary campaign backups.
- Threading/compact-address optimization remains measurement-driven follow-up.
  The preview keeps single-threaded scheduling; Pages does not provide the normal
  custom cross-origin-isolation headers needed for pthreads.
