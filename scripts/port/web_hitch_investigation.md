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

## Browser-wide trace capture, 2026-10-31

The optional CDP audit captured separate menu and Dam traces in dedicated
headless and headed Chrome 156.0.8078.79 sessions against the same completed
local runtime `c1ce09118bbc0768`. Both reported the NVIDIA GeForce RTX 3070
Laptop GPU through ANGLE/D3D11. The headless canvas was 853×480; the headed
window yielded 839×471, so these are diagnostic comparisons rather than matched
resolution benchmarks. The tab stayed focused. Traces and adjacent JSON reports
are private under `build/web-hitches/cdp-{headless,headed}`. The runner records
no screenshots, resource contents, ROM bytes or raw sound.

| Mode     | Phase     | First gap | Gaps ≥500 ms | Lossless trace | Relevant tracks                      |
| -------- | --------- | --------- | ------------ | -------------- | ------------------------------------ |
| Headless | Menu      | 1.17 s    | 1            | Yes            | Main, compositor, viz, GPU, profiler |
| Headless | Dam retry | 1.03 s    | 3            | Yes            | Main, compositor, viz, GPU, profiler |
| Headed   | Menu      | 3.18 s    | 1            | Yes            | Main, compositor, viz, GPU, profiler |
| Headed   | Dam retry | 6.05 s    | 2            | Yes            | Main, compositor, viz, GPU, profiler |

The first Dam capture in each mode lost trace data. The automatic larger-buffer,
30-second retries above were lossless. The headless first Dam search had no
qualifying gap before its 60-second cap; it is a nonreproduction for that window,
not a negative result for the later retry. Both headed captures and the
headless menu/retry captures reproduced. Every accepted trace retained the
phase-start timing mark, browser version/build identity and browser tracks.
A final 10-second menu/Dam smoke on the completed runner produced two lossless
no-incident traces with the correct phase/build metadata; that short window
does not revise the longer reproduction above.

In the headless menu gap, native timing showed about 1.19 s of renderer idle
with a 4 ms swap. In the headless Dam retry, the main renderer had no extended
visible task, while GPU vsync activity continued; the native audio log also
reported a queue underrun. The headed menu gap coincided with a roughly 1.57 s
browser-main `RunTask`, a native 3.20 s swap, and audio queue exhaustion. The
headed Dam retry coincided with a roughly 5.89 s browser-main `RunTask`, a
native 6.73 s swap, and audio queue exhaustion. Viz/GPU threads continued to
emit short events during these windows; no multi-second GL or GPU task is
identified in the lossless traces. The game renderer CPU profile has idle and
`dynCall` samples, but those samples do not resolve what the opaque browser-main
task was doing. The differing swap/idle attribution across modes suggests a
browser presentation/scheduling interaction; it does not establish a GPU driver
fault or a specific Chrome internal root cause. Browser-wide tracing may add
overhead, so the earlier minimal-probe reproduction remains an important check.

## SDL swap-yield and host comparison, 2026-10-31

The pinned Emscripten SDK's SDL2 2.32.10 implementation performs an Asyncify
`emscripten_sleep(0)` after `SDL_EGL_SwapBuffers` unless
`SDL_EMSCRIPTEN_ASYNCIFY=0`. The private audit can now set that environment
variable before module creation with `MIDAS_HITCH_SWAP_YIELD=0`; the ordinary
site and default audit keep SDL's behavior. All comparisons below used the same
optimized runtime `c1ce09118bbc0768`, headed Chrome, an 839×471 canvas,
NVIDIA ANGLE/D3D11, scripted Dam movement/ADS/firing, invulnerability, and
disabled audio/WebGL probes. Reports retain the option and `performance.timeOrigin`
to align browser frame gaps with one-second Windows counters.

| Run | Menu window | Dam window | Dam gaps ≥250 ms | Dam gaps ≥500 ms | Worst Dam gap |
| --- | ----------- | ---------- | ---------------- | ---------------- | ------------- |
| Default yield | 10 s | 125 s | 10 | 8 | 8.00 s |
| Yield disabled | 10 s | 122 s | 9 | 7 | 2.96 s |

The disabled-yield run still had multi-second Dam freezes. Native hitch logs
confirmed that disabling the yield moved long elapsed intervals from the swap
timer to renderer idle; it did not remove them. This single sequential pair is
not a performance improvement claim. Earlier 30-second pilots also froze in
both modes. A 120-second default-yield menu run entered the game's Facility
attract/demo stage partway through; only its stage-90 portion is a title-menu
sample. Its subsequent Dam capture failed the first pointer-lock gesture, so
it is excluded from the matched gameplay comparison. The runner now retries
the trusted capture gesture and records pointer-lock errors.

The laptop has 16 GB RAM and had roughly 0.5–1 GB available during many audit
samples, so resource pressure is real. It does not coincide with every stall:
the default-yield 2.03-second Dam gap at 20:22:00 UTC had 692 MB available,
zero sampled page reads, at most 0.21 ms average disk transfer time, and at
most 22.8% total CPU in nearby one-second samples. In the disabled-yield run,
a 2.96-second gap at 20:28:04 UTC had at least 902 MB available, at most six
page reads/s, 0.33 ms disk transfer time, and 17.9% total CPU. Concurrent
scheduler counters for that latter gap showed at most 0.19% DPC time, 0.29%
interrupt time and a processor queue length of one. One-second counters can
miss shorter spikes and aggregate CPU can hide a single blocked thread; these
readings only rule out obvious sustained saturation during those examples.
The private host CSVs and audit JSON remain ignored under `build/web-hitches/`.

The user also reported hitches in other Chrome pages while the native game has
not reproduced. The earlier attempt to start Windows Performance Recorder from
this session failed with `0xc5585011` because it lacked the system profiling
policy. The user then started and stopped WPR in an elevated PowerShell window,
allowing the synchronized capture below. The filtered Windows System event log
showed no matching WHEA, display-reset, disk or storage event in the earlier
sample interval.

## Synchronized Chrome, Codex and Windows trace, 2026-10-31

The user confirmed that **both** the game and Codex Desktop spinner froze in
the same capture. The scripted headed Chrome/Dam run, on optimized runtime
`c1ce09118bbc0768`, had one 2,035.5 ms frame gap from
21:40:21.530–21:40:23.566 UTC (4:40:21.530–4:40:23.566 p.m. CDT). Its CDP
trace is lossless, has the expected browser GPU/viz/compositor/profiler tracks,
and reports Chrome 156.0.8078.79, NVIDIA RTX 3070 Laptop GPU via ANGLE/D3D11,
839×471 canvas, Dam stage 33, and no focus loss. The menu's shorter trace had
no qualifying gap. The CDP renderer-main track contains no multi-second task
during this Dam gap; its longest reported `RunTask` is 8.1 ms. This differs
from the earlier headed incident with a long browser-main task, so the two
stalls should not be collapsed into one mechanism.

The concurrent one-second host samples near the Dam gap showed roughly
2.35 GB available memory, 12–44 page reads/s, 0.25–0.49 ms average disk
transfer time and 12.5–30.5% total CPU. They do not show sustained machine-wide
CPU, disk or paging saturation during this incident; their resolution cannot
exclude a short spike. The traces remain private and ignored under
`build/web-hitches/etw-headed/`, with host counters in
`build/web-hitches/etw-headed-host.csv` and the elevated WPR recording in
`build/web-hitches/host-repro.etl`.

Streaming ETW scheduling analysis compared equal 2.036-second windows before,
during and after the marked gap. Chrome process CPU time fell from 2,675.6 to
445.6 ms, DWM from 940.0 to 50.7 ms and Codex `ChatGPT.exe` from 642.9 to
140.5 ms; they returned to 2,615.6, 688.8 and 668.0 ms afterward. The
Windows `System` process continued to receive about 2.18 seconds of CPU time
during the gap. Chrome's compositor thread was switched out at 21:40:21.533
and did not run again until 21:40:23.764 (2,230.5 ms); its decoded state was
`Ready` with `WrPreempted`. Chrome's GPU main thread waited 1,894.3 ms with
`UserRequest`; Codex's viz compositor thread waited 2,210.6 ms with
`WrPushLock`. These are scheduling clues, not proof of the lock owner or a
driver defect. The ETW reader warned about one invalid classic event in the
trace, so individual wait classifications warrant visual confirmation
in Windows Performance Analyzer.

Raw ETW event counts add an independent display signal: DWM-Core emitted about
2,400–2,900 events per 100 ms before the gap, dropped to zero in many 100 ms
bins during it, then returned to about 2,400 per bin. DxgKrnl continued
emitting events throughout. Together with the user's simultaneous observation,
this makes a shared Windows graphics presentation/composition stall the leading
hypothesis for this capture. It does not distinguish DWM, the graphics driver,
GPU scheduling, a cross-process lock, or another dependency upstream of them.

## Next diagnostic step

Open the synchronized ETL in Windows Performance Analyzer and inspect the GPU
queues/presents, DWM frame history and wait chains at 21:40:21.530–23.764 UTC.
Correlate the first blocked present with the Chrome and Codex GPU/compositor
threads before assigning a driver or DWM cause. A controlled follow-up should
compare the same headed Chrome build and scripted Dam sequence under one
alternate ANGLE backend, with all other audit settings fixed, while collecting
frame gaps and underruns. Repeat an idle/no-game interval if the freeze also
appears there. A lower-memory experiment is separate: this capture shows that
low available RAM is not required for a shared freeze. Keep ETL local because
it can contain process and file metadata; do not infer a fix from average FPS.

The reports are diagnostic evidence, not a gameplay or audio quality pass.
