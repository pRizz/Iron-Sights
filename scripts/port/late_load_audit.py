"""Find loading work that waits for the player: every stage, toured room by room.

    python scripts/port/late_load_audit.py
    python scripts/port/late_load_audit.py --stage runway --stage dam --check

Each stage runs in its own folder with an isolated copy of the game, unpaced,
with GE_LOADTOUR putting Bond in every room in turn and GE_LATELOG naming
each room, model, texture, shader and upload loaded after the stage's first
frame. The report goes to <out>/report.md and the console; --check exits 1
when a frame after the load spent --budget-ms or more loading, or a shader
compiled during play. --learn-prefetch records which HD textures from other
missions' folders each stage drew and writes them to
port/fast3d/texture_packs/hd_stage_prefetch.json, so the next build reads them
while the stage loads. --video-driver offscreen runs it in a Linux container with
no display (LP64-005 Task 14). Needs the ROM; see scripts/port/late_loads.md.
"""
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent/'bench'))

import bench_metrics  # noqa: E402
import bench_plan  # noqa: E402
import late_load_report as report  # noqa: E402

REPO = Path(__file__).resolve().parents[2]
SOLO = ('dam', 'facility', 'runway', 'surface', 'bunker1', 'silo', 'frigate', 'surface2', 'bunker2', 'statue',
        'archives', 'streets', 'depot', 'train', 'jungle', 'control', 'caverns', 'cradle', 'aztec', 'egypt')
ARENAS = ('temple', 'complex', 'caves', 'library', 'basement', 'stack')
TITLE_STAGE = 90
LEARNED = REPO/'port'/'fast3d'/'texture_packs'/'hd_stage_prefetch.json'
RUN_TIMEOUT_S = 600
QUIET_WAIT_S = 900
BASE_INI = ('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=0')
# With --video-driver, the folders of a run's own HOME and icon cache (GE_ICON_CACHE_DIR), as
# lp64_play_run.py gives a video case: the game writes nothing outside the run's folder.
CONTAINER_FOLDERS = ('home', 'icons')


def log(message):
    print('late-load:', message, flush=True)


def find_rom():
    for candidate in (os.environ.get('GE_ROM'), REPO/'baserom.u.z64'):
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise SystemExit('late-load: no ROM; put baserom.u.z64 in the repository root or set GE_ROM')


def wait_until_quiet():
    """Wait for other game processes (the player's game, another audit) to exit."""
    if os.name != 'nt':
        return
    deadline = time.monotonic()+QUIET_WAIT_S
    while True:
        listing = subprocess.run(['tasklist', '/fo', 'csv', '/nh'], capture_output=True, text=True).stdout
        others = bench_plan.other_games(listing, os.getpid())
        if not others:
            return
        if time.monotonic() > deadline:
            raise SystemExit(f'late-load: other game processes kept running: {", ".join(others)}')
        log(f'waiting for {", ".join(sorted(set(others)))} to exit')
        time.sleep(5)


def link_pack(source, folder):
    """The pack's files, hard-linked into the run's Data/TexturePacks (nothing copied or changed)."""
    target = folder/'Data'/'TexturePacks'
    for path in source.rglob('*'):
        if path.is_file():
            destination = target/path.relative_to(source)
            destination.parent.mkdir(parents=True, exist_ok=True)
            try:
                os.link(path, destination)
            except OSError:
                shutil.copy2(path, destination)


def container_env(folder, options):
    """With --video-driver, the SDL driver and a HOME and icon cache inside FOLDER; else nothing."""
    if not options.video_driver:
        return {}
    home, icons = (str(folder/name) for name in CONTAINER_FOLDERS)
    return {'SDL_VIDEODRIVER': options.video_driver, 'HOME': home, 'GE_ICON_CACHE_DIR': icons}


def run_env(item, rom, folder, options):
    env = dict(os.environ)
    env.update(GE_ROM=str(rom), GE_WINDOW=options.window, GE_NOINPUT='1', GE_SEED='1', GE_NOINISAVE='1',
               GE_NOMSGBOX='1', GE_FRAMES='2000000000', GE_FIXEDTICK='1', GE_NOSLEEP='1', GE_HITCHMS='0',
               GE_LOADTOUR=f'{options.dwell},{options.settle}', GE_LATELOG=str(folder/'late.txt'),
               GE_BENCH=str(folder/'frames.csv'), GE_BENCH_END=str(options.max_ticks), SDL_AUDIODRIVER='dummy')
    env.update(container_env(folder, options))
    env.update(options.env)
    if options.learn_prefetch:
        env['GE_HD_USED'] = str(folder/'hd_used.txt')
    if item in ARENAS:
        env.update(GE_MP=item, GE_MPPLAYERS='2')
    else:
        env.update(GE_STAGE=item, GE_STAGE_AFTER='1', GE_SKIPINTRO='1', GE_CHEATS='invuln,invis')
    return env


def play(item, rom, folder, options):
    """Run one stage to the tour's end; returns the process exit code."""
    shutil.rmtree(folder, ignore_errors=True)
    folder.mkdir(parents=True)
    if options.video_driver:
        for name in CONTAINER_FOLDERS:
            (folder/name).mkdir()
    copy = folder/options.exe.name
    shutil.copy2(options.snapshot/options.exe.name, copy)
    for dll in options.snapshot.glob('*.dll'):
        shutil.copy2(dll, folder/dll.name)
    ini = list(BASE_INI)+(['texture_pack=1'] if options.pack else ['texture_pack=0'])+list(options.ini)
    (folder/'midas.ini').write_text('\n'.join(ini)+'\n', encoding='utf-8')
    # As a fresh install has it: the shipped shader list, not the player's own Data/shaders.txt.
    (folder/'Data').mkdir(exist_ok=True)
    shutil.copy2(REPO/'tools'/'shaders_base.txt', folder/'Data'/'shaders_base.txt')
    if options.pack:
        link_pack(options.pack, folder)
    wait_until_quiet()
    try:
        with open(folder/'stdout.txt', 'w', encoding='utf-8') as out:
            process = subprocess.run([str(copy)], cwd=folder, env=run_env(item, rom, folder, options),
                                     stdout=out, stderr=subprocess.STDOUT, timeout=options.run_timeout)
        return process.returncode
    except subprocess.TimeoutExpired:
        return 'timeout'
    finally:
        copy.unlink(missing_ok=True)  # audit executables never outlive their run
        shutil.rmtree(folder/'Data'/'TexturePacks', ignore_errors=True)


def collect(item, folder, budget_us):
    """The run's summary and its late events, or None when it left no logs."""
    try:
        loaded, events = report.parse_latelog((folder/'late.txt').read_text(encoding='utf-8'))
        _, frames = bench_metrics.parse_log((folder/'frames.csv').read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        log(f'{item}: no usable logs ({error})')
        return None
    stages = [(stage, tick) for stage, tick in loaded if stage != TITLE_STAGE]
    if not stages:
        log(f'{item}: the stage never finished loading')
        return None
    stage, load_tick = stages[0]
    summary = report.summarize_run(item, stage, frames, events, budget_us, load_tick)
    summary['events'] = [event for event in events if event.stage == stage]
    done = re.search(r'ge: loadtour: done, (\d+) of (\d+) rooms(.*)',
                     (folder/'stdout.txt').read_text(encoding='utf-8', errors='replace'))
    summary['toured'] = done is not None
    summary['rooms'] = f'{done.group(1)} of {done.group(2)}{" (mission ended)" if "ended" in done.group(3) else ""}'         if done else 'unfinished'
    return summary


def learn(options):
    """Merge each run's GE_HD_USED records into hd_stage_prefetch.json."""
    found = {}
    for item in options.stages:
        try:
            text = (options.out/item/'hd_used.txt').read_text(encoding='utf-8')
        except OSError:
            continue
        for stage, keys in report.parse_used(text).items():
            if stage != TITLE_STAGE:
                found.setdefault(stage, set()).update(keys)
    doc = report.merge_learned(json.loads(LEARNED.read_text(encoding='utf-8')), found)
    LEARNED.write_text(json.dumps(doc, indent=2)+'\n', encoding='utf-8')
    log(f'{LEARNED.relative_to(REPO)}: {sum(len(keys) for keys in found.values())} keys for {len(found)} stage(s)')


def parse_options(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--exe', type=Path, default=REPO/'build_win'/'IronSights.exe')
    parser.add_argument('--stage', action='append', help=f'a stage or arena (repeatable); default every one of '
                        f'{len(SOLO)} missions and {len(ARENAS)} multiplayer-only arenas')
    parser.add_argument('--pack', type=Path, default=REPO/'build_win'/'Data'/'TexturePacks',
                        help='HD texture pack folder to link in (default the main build\'s); --no-pack for none')
    parser.add_argument('--no-pack', action='store_true', help='original textures')
    parser.add_argument('--ini', action='append', default=[], help='extra midas.ini line (repeatable)')
    parser.add_argument('--env', action='append', default=[], help='extra NAME=VALUE for the game (repeatable), '
                        'e.g. GE_LOAD_AHEAD=0 for first-use loading')
    parser.add_argument('--dwell', type=int, default=24, help='ticks in each room (one full turn)')
    parser.add_argument('--settle', type=int, default=60, help='ticks after control before the tour')
    parser.add_argument('--max-ticks', type=int, default=30000, help='end a run here if the tour has not')
    parser.add_argument('--budget-ms', type=float, default=4.0, help='load work a frame after the load may do')
    parser.add_argument('--window', default='1280x720')
    parser.add_argument('--video-driver', help='SDL video driver for the game, with a HOME and icon cache in '
                        'each run\'s folder (offscreen: a container with no display)')
    parser.add_argument('--run-timeout', type=int, default=RUN_TIMEOUT_S,
                        help='seconds a stage may run (raise it for an emulated host)')
    parser.add_argument('--out', type=Path, help='default build_win/late-load-audit/<time>')
    parser.add_argument('--keep', action='store_true', help='keep each run\'s folder (logs) after the report')
    parser.add_argument('--check', action='store_true', help='exit 1 on frames over budget or late shaders')
    parser.add_argument('--learn-prefetch', action='store_true',
                        help=f'write the other-folder HD textures each stage drew to {LEARNED.relative_to(REPO)}')
    options = parser.parse_args(argv)
    options.exe = options.exe.resolve()
    if not options.exe.is_file():
        raise SystemExit(f'late-load: no executable at {options.exe}')
    if options.no_pack or not (options.pack and options.pack.is_dir() and any(options.pack.iterdir())):
        options.pack = None
    options.env = dict(item.split('=', 1) for item in options.env if '=' in item)
    options.stages = options.stage or list(SOLO+ARENAS)
    unknown = [item for item in options.stages if item not in SOLO+ARENAS]
    if unknown:
        raise SystemExit(f'late-load: unknown stage(s): {", ".join(unknown)}')
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    options.out = (options.out or REPO/'build_win'/'late-load-audit'/stamp).resolve()
    return options


def main(argv=None):
    options = parse_options(argv)
    rom = find_rom()
    budget_us = int(options.budget_ms*1000)
    settings = ('HD texture pack' if options.pack else 'original textures')+''.join(
        f', {line}' for line in options.ini+[f'{k}={v}' for k, v in options.env.items()])
    # One copy of the executable for the whole audit: a rebuild meanwhile must not change later stages.
    options.snapshot = options.out/'_exe'
    options.snapshot.mkdir(parents=True, exist_ok=True)
    shutil.copy2(options.exe, options.snapshot/options.exe.name)
    for dll in options.exe.parent.glob('*.dll'):
        shutil.copy2(dll, options.snapshot/dll.name)
    summaries = []
    for item in options.stages:
        folder = options.out/item
        log(f'{item}: touring')
        code = play(item, rom, folder, options)
        summary = collect(item, folder, budget_us)
        if summary is None:
            summaries.append({**report.summarize_run(item, -1, [], [], budget_us), 'events': [], 'toured': False,
                              'rooms': 'no logs'})
            continue
        log(f"{item}: exit {code}, load {summary['load_ms']:.0f} ms, {summary['late_count']} late events, "
            f"{summary['frames_over_budget']} frame(s) over budget, rooms {summary['rooms']}")
        summaries.append(summary)
        if not options.keep:
            for name in ('frames.csv', 'midas.ini'):
                (folder/name).unlink(missing_ok=True)
            for name in CONTAINER_FOLDERS:
                shutil.rmtree(folder/name, ignore_errors=True)
    if options.learn_prefetch:
        learn(options)
    shaders = report.late_shaders(summaries)
    text = report.markdown(summaries, shaders, {'settings': settings, 'dwell': options.dwell,
                                                'budget_ms': options.budget_ms})
    unfinished = [summary['name'] for summary in summaries if not summary['toured']]
    if unfinished:
        text += f"\nTours that did not finish (a level script ended the mission, or a crash): {', '.join(unfinished)}\n"
    options.out.mkdir(parents=True, exist_ok=True)
    (options.out/'report.md').write_text(text, encoding='utf-8')
    (options.out/'summary.json').write_text(json.dumps(
        [{key: value for key, value in summary.items() if key not in ('events', 'top')} for summary in summaries],
        indent=1), encoding='utf-8')
    print(text)
    log(f'report: {options.out/"report.md"}')
    shutil.rmtree(options.snapshot, ignore_errors=True)  # audit executables never outlive the audit
    problems = report.check(summaries, options.budget_ms)
    if options.check and problems:
        for problem in problems:
            log('FAIL '+problem)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
