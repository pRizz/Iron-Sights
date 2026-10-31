# Recurring browser hitch investigation

This record covers the 2026-10-31 local investigation of reported multi-second
freezes with disrupted audio in the browser game. It uses the private
[`audit:hitches`](../../web/README.md) runner against an isolated optimized Wasm
build and assembled site. The local US ROM and all trace files remain ignored
under `build/web-hitches/`. No ROM data, screenshots or raw audio are recorded.

## Reproduction

All three runs used Windows headless Chrome/Edge 156, ANGLE/D3D11 on an NVIDIA
GeForce RTX 3070 Laptop GPU, an 853×480 game canvas, and runtime
`12db55087ee6b48f` built from source commit `9e5d38b`. Each run reached the
menu and Dam through normal game navigation. The script repeatedly moved, aimed
down sights and fired in Dam. `invuln` was enabled only where marked to keep
the longer mission sample alive; enemies remained active.

| Local run        | Invulnerable | Menu sample  | Dam gameplay sample                                    | Frame gaps ≥250 ms | Largest gap in those samples |
| ---------------- | ------------ | ------------ | ------------------------------------------------------ | ------------------ | ---------------------------- |
| `chrome-long`    | No           | 60 s, 5 gaps | roughly 60 s before death within a 123 s phase, 4 gaps | 9                  | 1.85 s in Dam                |
| `edge-invuln`    | Yes          | 30 s, 2 gaps | 78 s, 4 gaps                                           | 6                  | 2.17 s in Dam                |
| `chrome-minimal` | Yes          | 30 s, 2 gaps | 45 s, 4 gaps                                           | 6                  | 2.03 s in Dam                |

The `chrome-long` script continued after Bond died; later stage-90 samples are
not counted as Dam gameplay. Menu navigation between the measured phases also
showed long gaps, up to 4.7 seconds in Edge. The `chrome-minimal` run disabled
the audio AudioWorklet and WebGL timing wrappers. It still reproduced the
freezes, reducing the likelihood that those probes created them. These runs
show recurrence on this one host, not general browser or hardware prevalence.
After syncing the newer Files/context-menu change, an optimized site with
runtime `c1ce09118bbc0768` passed a short normal-menu/Dam audit with the same
diagnostic runner. That short run checks integration, not hitch frequency.

## Correlated evidence

- Native `ge: hitch:` lines attribute many large intervals to
  `SDL_GL_SwapWindow` (for example 1,856 ms in `chrome-long` Dam and 2,179 ms
  in `edge-invuln` Dam). Other intervals appear as renderer idle time before
  the next frame. The game simulation and measured GL work were usually about
  1–12 ms on these lines, with no timed shader/texture event. A swap interval
  is elapsed time at that call; it alone does not identify a GPU driver fault.
- The browser `requestAnimationFrame` probe saw corresponding gaps. Its long-task
  observer found no task over 250 ms in any run; the timed WebGL wrappers saw
  no calls over their 4 ms threshold in the fully instrumented runs. This
  narrows the visible symptom to frame scheduling/presentation, but the browser
  compositor, GPU process, OS scheduler and Emscripten's yielding behavior
  remain unresolved.
- The native audio health log reported queue underruns in all three runs and
  pump gaps matching the large visual pauses. Sampled queue depth reached zero
  around several Dam hitches. The audio interruption is therefore consistent
  with missed game/audio pumps. The worklet's exact 128-sample repeat counter
  did not detect repeated nonzero blocks; it does not rule out a longer audible
  loop or characterize the reported sound fully.
- Sampled Wasm linear memory stayed at 536,870,912 bytes throughout all three
  runs. The only measured save synchronizations were near startup, outside the
  menu/gameplay hitch windows. Neither memory growth nor IDBFS sync explains
  these captured pauses.

## Next diagnostic step

Capture a short Chrome DevTools/CDP performance trace centered on one recurring
hitch, including main, compositor and GPU activity, then compare a headed
hardware run with the headless measurements. Preserve browser version,
GPU/driver, display mode and runtime identity. If presentation is waiting on
browser scheduling, inspect the Emscripten SDL swap/yield path; if the GPU or
compositor is blocked, inspect driver/WebGL submission separately. Change one
cause at a time and repeat the same menu and Dam sample, tracking gaps over
100/250 ms, worst gap and audio underruns. Do not infer a fix from average FPS.

The reports are diagnostic evidence, not a gameplay or audio quality pass.
