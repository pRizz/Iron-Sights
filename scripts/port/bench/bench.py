"""Benchmark the game: fixed work, repeated, compared across builds.

    python scripts/port/bench/bench.py run --build fork=build_win/IronSights.exe \\
        --build stock=build_win/bench-stock/build/ProjectMidas.exe
    python scripts/port/bench/bench.py report build_win/bench-results/<file>.json

Each run copies the executable into its own folder with its own midas.ini, so
the player's game, settings and saves are never touched, and removes the copy
afterwards. See scripts/port/benchmarks.md.
"""
import argparse
import datetime
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

import bench_metrics
import bench_plan
import bench_report

REPO = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
BUILD_ROOT = REPO/'build_win'
RUN_TIMEOUT_S = 300
# A run another game process overlapped is repeated this many times, then kept and flagged.
DISTURBED_RETRIES = 3
# --smoke: the quick check AGENTS.md asks of a render change.
SMOKE_SCENARIOS = ('cradle-fight', 'dam-walk', 'temple-4p')
SMOKE_REPEATS = 3
QUIET_WAIT_S = 900
WATCH_EVERY_S = 2.0
# First match of each in the game's own logs describes the run's host and build.
# (fact, line prefix, the part of the prefix the fact keeps): a Linux system line keeps its word.
LOG_FACTS = (('gpu', 'GL: renderer: ', ''), ('identity', 'ge: sys: build ', ''), ('cpu', 'ge: sys: cpu ', ''),
             ('os', 'ge: sys: Windows ', ''), ('os', 'ge: sys: Linux', 'Linux'), ('power', 'ge: sys: power: ', ''))
PROC = Path('/proc')


def log(message):
    print('bench:', message, flush=True)


def find_rom():
    for candidate in (os.environ.get('GE_ROM'), REPO/'baserom.u.z64'):
        if candidate and Path(candidate).is_file():
            return Path(candidate)
    raise SystemExit('bench: no ROM; put baserom.u.z64 in the repository root or set GE_ROM')


def parse_builds(items):
    builds = []
    for item in items:
        try:
            label, path, env = bench_plan.split_build(item)
        except ValueError as error:
            raise SystemExit(f'bench: {error}') from error
        executable = (REPO/path).resolve()
        if not executable.is_file():
            raise SystemExit(f'bench: no executable at {executable}')
        builds.append({'label': label, 'path': executable, 'env': env,
                       'sha256': hashlib.sha256(executable.read_bytes()).hexdigest()})
    if len({build['label'] for build in builds}) != len(builds):
        raise SystemExit('bench: build labels must differ')
    return builds


def proc_names():
    """(pid, name) of each process in /proc; one that exits meanwhile is skipped."""
    entries = []
    for entry in PROC.iterdir():
        if not entry.name.isdigit():
            continue
        try:
            entries.append((int(entry.name), (entry/'comm').read_text(encoding='utf-8').strip()))
        except OSError:
            continue  # the process exited between the listing and the read
    return entries


def other_games(own_pid=None):
    """Other running game processes (another session's audit, the player's game)."""
    if os.name != 'nt':
        return bench_plan.linux_other_games(proc_names(), own_pid) if PROC.is_dir() else []
    listing = subprocess.run(['tasklist', '/fo', 'csv', '/nh'], capture_output=True, text=True).stdout
    return bench_plan.other_games(listing, own_pid)


def machine_busy():
    """The one-minute load average on Linux when other work fills the processors, else None."""
    loadavg = PROC/'loadavg'
    if os.name == 'nt' or not loadavg.is_file():
        return None
    return bench_plan.busy_load(loadavg.read_text(encoding='utf-8'), os.cpu_count() or 1)


def processor_times():
    """(idle, kernel, user) processor time totals, or None where unavailable."""
    if os.name != 'nt':
        stat = PROC/'stat'
        return bench_plan.proc_stat_times(stat.read_text(encoding='utf-8')) if stat.is_file() else None
    import ctypes
    times = [ctypes.c_ulonglong() for _ in range(3)]
    if not ctypes.windll.kernel32.GetSystemTimes(*[ctypes.byref(value) for value in times]):
        return None
    return tuple(value.value for value in times)


def wait_until_quiet():
    """Hold the next run until no other game process is running and, on Linux,
    the load average is below the processor count."""
    deadline = time.monotonic()+QUIET_WAIT_S
    maybe_said = None
    maybe_load = None
    while time.monotonic() < deadline:
        if other_games():
            reason = 'another game process is running; waiting for it to finish'
        elif (maybe_load := machine_busy()) is not None:
            reason = f'the load average has reached the {os.cpu_count()} processors; waiting for it to fall'
        else:
            return
        if reason != maybe_said:
            log(reason)
            maybe_said = reason
        time.sleep(WATCH_EVERY_S)
    load = 'not read' if maybe_load is None else f'{maybe_load}'
    log(f'still not quiet after {QUIET_WAIT_S} s ({maybe_said}); last load average {load}; running anyway')


def prepare_run(executable, folder, env, environ):
    """Copy the executable and what it loads from beside it into FOLDER.

    Returns the copy and the environment to start it with (bench_plan.launch_plan).
    """
    siblings = {name for name in bench_plan.RUN_SIBLINGS if (executable.parent/name).exists()}
    plan = bench_plan.launch_plan(executable, folder, siblings, environ, env)
    for source, destination in plan['copies']:
        if source.is_dir():
            shutil.copytree(source, destination, dirs_exist_ok=True)
        else:
            shutil.copy2(source, destination)
    return plan['copies'][0][1], plan['env']


def launch(executable, folder, env, ini, visible):
    """Run one isolated copy of the game to its end.

    Returns its exit code (None on a timeout), whether another game process
    ran alongside it, and the share of all processor time in use meanwhile.
    """
    folder.mkdir(parents=True, exist_ok=True)
    copy, full_env = prepare_run(executable, folder, env, os.environ)
    (folder/'midas.ini').write_text(ini, encoding='utf-8')
    startup = None
    if os.name == 'nt' and not visible:
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    disturbed, maybe_code = False, None
    try:
        with (folder/'stdout.log').open('wb') as out, (folder/'stderr.log').open('wb') as err:
            before, deadline = processor_times(), time.monotonic()+RUN_TIMEOUT_S
            process = subprocess.Popen([str(copy)], cwd=folder, env=full_env, stdout=out, stderr=err,
                                       startupinfo=startup)
            while maybe_code is None and time.monotonic() < deadline:
                try:
                    maybe_code = process.wait(timeout=WATCH_EVERY_S)
                except subprocess.TimeoutExpired:
                    disturbed = disturbed or bool(other_games(process.pid))
            if maybe_code is None:
                process.kill()
                process.wait()
            after = processor_times()
    finally:
        copy.unlink(missing_ok=True)  # audit executables never outlive their run
    maybe_busy = bench_plan.busy_fraction(before, after) if before and after else None
    return maybe_code, disturbed, maybe_busy


def facts_from_lines(lines):
    """The host and build facts of a game log's lines (LOG_FACTS); the first match of each wins."""
    facts = {}
    for key, prefix, kept in LOG_FACTS:
        maybe_line = next((line for line in lines if line.startswith(prefix)), None)
        if maybe_line is not None and key not in facts:
            facts[key] = (kept+maybe_line[len(prefix):]).strip()
    return facts


def log_facts(folder):
    lines = []
    for name in ('stdout.log', 'stderr.log'):
        lines += (folder/name).read_text(encoding='utf-8', errors='replace').splitlines()
    return facts_from_lines(lines)


def write_route(scenario, folder):
    """Put the scenario's route in the run folder and return its path."""
    if 'route_file' in scenario:
        return HERE/scenario['route_file']  # a hand-played GE_INPUTREC recording
    route = folder/'route.rec'
    route.write_text(bench_plan.route_text(scenario), encoding='utf-8', newline='\n')
    return route


def measure(build, scenario, preset_lines, folder, options):
    """One measured run; returns its record and log facts, or raises with the folder kept."""
    folder.mkdir(parents=True)
    frame_log = folder/'frames.csv'
    env = bench_plan.run_env(scenario, options.rom, options.resolution, frame_log, write_route(scenario, folder))
    env.update(item.split('=', 1) for item in options.env or [])
    env.update(build['env'])
    code, disturbed, maybe_busy = launch(build['path'], folder, env, bench_plan.ini_text(preset_lines),
                                         options.visible)
    if code != 0 or not frame_log.exists():
        raise RuntimeError(f'run failed (exit {code}); logs kept in {folder}')
    metrics = bench_metrics.run_metrics(frame_log.read_text(encoding='utf-8'),
                                        scenario['warmup_ticks'], bench_plan.end_tick(scenario))
    return {'metrics': metrics, 'disturbed': disturbed, 'cpu_busy': maybe_busy}, log_facts(folder)


def measure_undisturbed(build, scenario, preset_lines, folder, options):
    """measure(), repeated while another game process overlaps the run."""
    for attempt in range(DISTURBED_RETRIES+1):
        wait_until_quiet()
        run, facts = measure(build, scenario, preset_lines, folder, options)
        if not run['disturbed'] or attempt == DISTURBED_RETRIES:
            return run, facts
        log(f"{scenario['name']} {build['label']}: another game process ran alongside; repeating the run")
        shutil.rmtree(folder)
    raise AssertionError('unreachable')


def select(config, names, kind):
    available = config[kind]
    chosen = names or list(available)
    unknown = [name for name in chosen if name not in available]
    if unknown:
        raise SystemExit(f"bench: unknown {kind[:-1]} {', '.join(unknown)}; have {', '.join(available)}")
    return chosen


def summarize(runs, presets, scenarios, builds):
    summary = {}
    for preset in presets:
        for scenario in scenarios:
            for build in builds:
                matching = [run['metrics'] for run in runs if (run['preset'], run['scenario'], run['build'])
                            == (preset, scenario, build['label'])]
                if matching:
                    summary.setdefault(preset, {}).setdefault(scenario, {})[build['label']] = (
                        bench_metrics.summarize(matching))
    return summary


def run_paths(maybe_out, os_name, stamp):
    """Return (results file, session folder). Off Windows --out is required and the run folders go
    beside it, so a Linux run never makes build_win; the session folder is absolute, since each run
    starts in its own folder."""
    if os_name != 'nt':
        if not maybe_out:
            raise SystemExit('bench: off Windows pass --out RESULTS.json; '
                             'the default puts the results and run folders under build_win')
        out = Path(maybe_out)
        return out, out.resolve().parent/('bench-run-'+stamp)
    out = Path(maybe_out) if maybe_out else BUILD_ROOT/'bench-results'/(stamp+'.json')
    return out, BUILD_ROOT/('bench-run-'+stamp)


def command_run(options, config):
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out, session = run_paths(options.out, os.name, stamp)
    builds = parse_builds(options.build or ['fork=build_win/IronSights.exe'])
    if options.smoke:
        options.scenario = options.scenario or list(SMOKE_SCENARIOS)
        options.preset = options.preset or list(config['presets'])
        options.repeats = min(options.repeats, SMOKE_REPEATS)
    scenarios = select(config, options.scenario, 'scenarios')
    presets = select(config, options.preset or ['default'], 'presets')
    runs, host = [], {}
    order = bench_plan.run_order(builds, scenarios, options.repeats)
    for preset in presets:
        for number, (repeat, name, build) in enumerate(order, start=1):
            folder = session/f"{preset}-{name}-{build['label']}-{repeat}"
            run, facts = measure_undisturbed(build, config['scenarios'][name], config['presets'][preset],
                                             folder, options)
            metrics = run['metrics']
            build.setdefault('identity', facts.get('identity', 'unknown'))
            host = host or {key: facts.get(key, 'unknown') for key in ('cpu', 'gpu', 'os', 'power')}
            runs.append(dict(run, preset=preset, scenario=name, build=build['label'], repeat=repeat))
            log(f"{preset} {number}/{len(order)} {name} {build['label']} #{repeat}: "
                f"{metrics['avg_fps']:.0f} fps, 1% low {metrics['low_1pct_fps']:.0f}, "
                f"render {metrics['render_ms']:.2f} ms, game {metrics['sim_ms']:.2f} ms"
                +(' (another game process ran alongside)' if run['disturbed'] else ''))
            if not options.keep:
                shutil.rmtree(folder)
    if not options.keep:
        shutil.rmtree(session, ignore_errors=True)
    results = {
        'schema': 1, 'created': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
        'host': dict(host, python=platform.python_version()), 'resolution': options.resolution,
        'visible': options.visible, 'repeats': options.repeats,
        'builds': [{key: str(value) for key, value in build.items()} for build in builds],
        'scenarios': {name: config['scenarios'][name] for name in scenarios},
        'presets': {name: config['presets'][name] for name in presets},
        'runs': runs, 'summary': summarize(runs, presets, scenarios, builds),
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=1)+'\n', encoding='utf-8')
    log(f'results: {out}')
    print(bench_report.render(results, options.base or builds[0]['label']))
    return 0


def command_report(options, _config):
    results = json.loads(Path(options.results).read_text(encoding='utf-8'))
    text = bench_report.render(results, options.base or results['builds'][0]['label'])
    if options.out:
        Path(options.out).write_text(text+'\n', encoding='utf-8', newline='\n')
        log(f'wrote {options.out}')
    else:
        print(text)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    run = commands.add_parser('run', help='measure builds over the scenarios')
    report = commands.add_parser('report', help='render a results file as Markdown')
    run.add_argument('--build', action='append', metavar='LABEL=PATH[@NAME=VALUE,...]',
                     help='executable to measure, path from the repository root, with optional '
                          'environment of its own (repeatable)')
    run.add_argument('--scenario', action='append', help='scenario name (repeatable; default all)')
    run.add_argument('--resolution', default='1920x1080')
    run.add_argument('--keep', action='store_true', help='keep run folders (logs, frame logs, routes)')
    run.add_argument('--env', action='append', metavar='NAME=VALUE',
                     help='extra environment for the game, e.g. GE_PROFILE=profile.txt (repeatable)')
    run.add_argument('--preset', action='append', help='settings preset (repeatable; default: default)')
    run.add_argument('--repeats', type=int, default=5)
    run.add_argument('--smoke', action='store_true',
                     help='the quick check for a render change: three scenarios, every preset, three repeats')
    run.add_argument('--hidden', dest='visible', action='store_false',
                     help='hide the window; quicker to work beside, but frames run slower and noisier')
    run.add_argument('--out', help='results file (default on Windows: build_win/bench-results/<time>.json); '
                                   'required off Windows, where the run folders go beside it')
    report.add_argument('results')
    report.add_argument('--out', help='write the Markdown here instead of printing it')
    for command in (run, report):
        command.add_argument('--base', help='label of the build to compare against (default: the first)')
    options = parser.parse_args()
    config = bench_plan.load_config((HERE/'scenarios.json').read_text(encoding='utf-8'))
    if options.command != 'report':
        options.rom = find_rom()
    return {'run': command_run, 'report': command_report}[options.command](options, config)


if __name__ == '__main__':
    sys.exit(main())
