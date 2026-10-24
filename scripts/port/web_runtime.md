# Browser runtime bring-up

The root CMake project now compiles both a headless Wasm core and an SDL2/WebGL 2
prototype. This is implementation groundwork for the accepted
[plan](web_target_plan.md); use [progress](web_target_progress.md) for acceptance
gates. The prototype is not the persistent launcher or a Pages release.

## Build and run locally

The runner uses the immutable [SDK pin](web_toolchain.json), a read-only source
mount and separate ignored outputs. Docker must be running. SDL's source port
may be downloaded into a named SDK cache during the first video build. The core
build and local ROM smoke run disable container networking.

```sh
python scripts/port/web_build.py core --smoke --rom baserom.u.z64
python scripts/port/web_build.py video
python -m http.server 4174 --bind 127.0.0.1 --directory build/web/video
```

Open `http://127.0.0.1:4174`, choose the original US ROM and press Play. Press
Resume after startup to capture the mouse. Escape releases it and pauses;
Resume continues. P opens/closes the watch without using the browser's reserved
Escape key; Return still confirms menu choices. This browser-only initial binding
does not change native defaults. This temporary harness has no persistent storage. Serve only
the output directory, never the repository root or a directory containing ROMs.
The Node smoke mounts the selected ROM read-only and copies it into ephemeral
MEMFS. Its output directory contains no ROM copy.

`--link-opt 0` accelerates iterative runtime debugging. The default is `2` and
must be retested before treating an optimized build as verified. The complete
legacy engine still compiles with the root Release settings; this option selects
the final Wasm optimization/Asyncify pass. Independent Make errors can be
collected with `--keep-going`. Logs remain in `build/web/core` or `video`.
Do not run a browser audit while relinking the directory it serves: the loader
and Wasm can briefly belong to different builds. Audit only a completed build.

`--callback-types` additionally recompiles the selected translation units with
function-pointer diagnostics enabled and fatal, overriding the inherited `-w`.
Its JSON/logs are under `build/web/video/callback-types`; all 271 current video
units passed. This catches local callback assignments that cross-unit linker
signature checks cannot see.

With the pinned SDK installed and its environment activated (`EMSDK` set), the
same `ge` target can use presets. The preset explicitly selects the SDK toolchain:

```sh
emcmake cmake --preset web
cmake --build --preset web
emcmake cmake --preset web-core
cmake --build --preset web-core
```

The Docker runner supplies its own `/out` build directory instead of the preset
directory. Native targets retain their existing configuration and toolchain.
Do not enable `GE_ROMDATA_FALLBACK` for either browser configuration.

## Runtime decisions

- `GLOBAL_BASE=0x10000000` keeps static, stack, arena and ROM pointers outside
  N64 segment-word ranges. This is the initial correctness strategy. A compact
  pointer representation would require a separately verified address decoder.
- The initial linear capacity is 512 MiB, grows to at most 1 GiB, and includes
  a 256 MiB empty prefix. These are Wasm capacities, not resident RAM estimates.
  The tested core stayed at 536870912 bytes. The initial video prototype grew
  to 730726400 bytes while preparing shaders/assets. Optimize only with real
  measurements and pointer-contract coverage.
- Scheduler waits yield with Asyncify. The ordinary C stack is 16 MiB and the
  Asyncify stack is 1 MiB. Native render threads and USB adapter polling are
  disabled for the browser even when an ini/environment setting requests them.
- Wasm assembler sections declare actual object sizes. Startup checks critical
  segment spans and host address classification. DMA bounds and texture IDs/
  scratch-buffer spans fail explicitly rather than continuing after corruption.
- Native suspend snapshots are unavailable. Campaign EEPROM/profile/settings
  persistence is a separate required milestone, not replaced by suspend stubs.
- Set diagnostic environment fields in `preRun`, before libc initializes its
  environment. Changing `Module.ENV` after module creation is too late. The
  smoke keeps the runtime alive so Asyncify faults surface rather than being
  masked by an implicit exit; success requires the explicit frame-limit report
  with Dam's actual level ID, not merely process exit status zero.
- Let the normal menu initialization settle before a diagnostic direct stage
  launch (`GE_STAGE_AFTER=120`). Launching at tick 1 can adopt uninitialized zero
  volume settings. The final launcher must use the normal game menus.
- Give the canvas an explicit CSS aspect ratio. SDL probes its CSS size with a
  1-by-1 backing canvas; width-only CSS accidentally makes the game square.
- Browser focus reset clears SDL keyboard/buttons, relative deltas and port pad
  caches, and pauses/clears queued audio. The page distinguishes an intentional
  menu pointer release from an unexpected lock loss, and follows the actual
  Pointer Lock state. Resume resets input before requesting capture.
  Only the page's Play/Resume gesture calls the web capture entrypoint: automatic
  SDL deferred requests are disabled on web to avoid two requests colliding.
  The page waits for an actual capture event and observes rejection.

The port previously depended on several native ABI accidents: missing float/
pointer returns, implicit function declarations, ignored callback arguments and
GCC's ordering of separate texture-bank objects. Shared fixes use declared
callback types/returns and snapshot every real bank object explicitly. They do
not redesign native hands or alter authored game assets. SDK symbol collisions
are isolated from the engine's fixed-point inverse trig and legacy `strtol`/
ctype helpers. Web shaders use GLSL ES and the GLES loader.

The browser compilation currently retains the native legacy allowance for
implicit C declarations and integer/pointer conversions. These three diagnostic
classes are demoted, but linker signature warnings are fatal and real runtime
tests remain necessary. Do not interpret compilation alone as ABI correctness.

## Recover missing declarations

`port/generated/web_prototypes.h` and its JSON report record 205 real game
function signatures recovered from Clang definitions. `port/web/compat.h`
declares missing hosted functions separately. The linker treats warnings,
including cross-unit signature mismatches, as fatal. Do not add guessed casts
or enable `EMULATE_FUNCTION_POINTER_CASTS` to suppress a bad ABI.

To refresh the existing declaration set inside the pinned SDK, mount the source
at `/src` and the core build at `/out`, then run:

```sh
python3 /src/scripts/port/web_prototypes.py \
  --database /out/compile_commands.json \
  --types-file /src/port/generated/web_prototypes.json \
  --output /out/web_prototypes.h --jobs 8
```

Review/copy both outputs into `port/generated/` from the host. For a new linker
mismatch, `--log /out/build.log` replaces `--types-file` and recovers the reported
definition owners. Merge newly recovered authoritative records with the existing
set; that diagnostic mode does not automatically retain earlier records.

## Verification

```sh
python scripts/port/test_web_spike.py
python scripts/port/test_web_prototypes.py
python scripts/port/web_build.py files
bash scripts/port/test_image_bank.sh
bash scripts/port/test_stan_callbacks.sh
# Inside the pinned SDK:
bash scripts/port/test_image_bank.sh --web
bash scripts/port/test_stan_callbacks.sh --web
```

The image-bank regression runs real bank objects through repeated mutation/
restoration without a ROM, under both GCC/native and Clang/Wasm. Shared source
changes also require the applicable ADS, wardrobe, HUD, state and native build/
gallery checks. The [browser tools](../../web/README.md) include private local
runtime captures and ROM-free ROM/backup boundary tests.

The optimized Edge prototype now demonstrates Dam movement, mouse turning,
nonzero Web Audio, zero simulation steps while unfocused, resumed simulation
without a stuck key, and watch pointer release/recovery. Ten ROM-free GPU
sampling cases pass with no real-ROM sampler/mipmap warnings. Native regressions
and the repeated 72-view/35-icon gallery pass with zero changed image cells.
Save durability and the final launcher/deployment still need acceptance work.
Screenshots and other ROM-derived runtime evidence stay in ignored build paths.
The private runtime audit's `audit=1` query enables existing invisibility and
invulnerability diagnostics plus a fixed seed to isolate focus/input checks
from enemy knockback. Normal prototype launches do not enable them.
