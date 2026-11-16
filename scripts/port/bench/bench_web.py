"""Benchmark the browser build with the same scenarios, routes and metrics.

Three steps: this script plans the runs, the Playwright tool plays them in a
browser and saves each frame log, and this script turns the logs into the
same results file and report as the Windows runner.

    python scripts/port/bench/bench_web.py plan build/web-bench/current
    (in web/)  node tools/bench-audit.mjs ../baserom.u.z64 ../build/web-bench/current http://127.0.0.1:4191/Iron-Sights/
    python scripts/port/bench/bench_web.py collect --build web=build/web-bench/current

See scripts/port/benchmarks.md.
"""
import argparse
import datetime
import json
import platform
import sys
from pathlib import Path

import bench
import bench_metrics
import bench_plan
import bench_report

PLAN = 'plan.json'
HOST = 'host.json'
NOTES = (
    'Browser build. The browser schedules frames itself and the game hands it the event loop every frame, '
    'so average fps and frame times say more about that scheduling than about the game; compare Render ms '
    'and Game ms, the processor time a frame costs',
)


def command_plan(options, config):
    scenarios = bench.select(config, options.scenario, 'scenarios')
    presets = bench.select(config, options.preset or ['default'], 'presets')
    folder = Path(options.folder)
    folder.mkdir(parents=True, exist_ok=True)
    runs = bench_plan.web_runs(config, scenarios, presets, options.repeats)
    for run in runs:
        run['log_name'] = bench_plan.web_log_name(run)
        run['env'].update(item.split('=', 1) for item in options.env or [])
    (folder/PLAN).write_text(json.dumps({'schema': 1, 'runs': runs}, indent=1)+'\n', encoding='utf-8')
    bench.log(f'{len(runs)} runs planned in {folder/PLAN}')
    return 0


def read_build(item):
    """One browser session's folder: its plan, host facts and measured runs."""
    label, separator, path = item.partition('=')
    folder = (bench.REPO/path).resolve() if separator else None
    if folder is None or not (folder/PLAN).is_file() or not (folder/HOST).is_file():
        raise SystemExit(f'bench: --build wants label=folder holding {PLAN} and {HOST}, got {item!r}')
    plan = json.loads((folder/PLAN).read_text(encoding='utf-8'))
    host = json.loads((folder/HOST).read_text(encoding='utf-8'))
    runs = []
    for planned in plan['runs']:
        frame_log = folder/planned['log_name']
        if not frame_log.is_file():
            raise SystemExit(f'bench: {frame_log} is missing; the browser runner did not finish')
        metrics = bench_metrics.run_metrics(frame_log.read_text(encoding='utf-8'),
                                            planned['first_tick'], planned['end_tick'])
        runs.append({'preset': planned['preset'], 'scenario': planned['scenario'], 'repeat': planned['repeat'],
                     'build': label, 'metrics': metrics, 'disturbed': False, 'cpu_busy': None})
    build = {'label': label, 'path': str(folder), 'sha256': host.get('wasm_sha256', ''),
             'identity': f"runtime {host.get('runtime', 'unknown')} in {host.get('browser', 'unknown')}"}
    return build, host, runs


def command_collect(options, _config):
    builds, runs, hosts = [], [], []
    for item in options.build:
        build, host, build_runs = read_build(item)
        builds.append(build)
        hosts.append(host)
        runs += build_runs
    presets = list(dict.fromkeys(run['preset'] for run in runs))
    scenarios = list(dict.fromkeys(run['scenario'] for run in runs))
    first = hosts[0]
    results = {
        'schema': 1, 'target': 'browser',
        'created': datetime.datetime.now().astimezone().isoformat(timespec='seconds'),
        'host': {'cpu': platform.processor() or 'unknown', 'gpu': first.get('renderer', 'unknown'),
                 'os': first.get('platform', 'unknown')},
        'resolution': f"{first.get('canvas', 'unknown')} canvas", 'visible': bool(first.get('headed')),
        'notes': list(NOTES), 'builds': builds, 'runs': runs,
        'summary': bench.summarize(runs, presets, scenarios, builds),
    }
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    out = Path(options.out) if options.out else bench.BUILD_ROOT/'bench-results'/(f'web-{stamp}.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(results, indent=1)+'\n', encoding='utf-8')
    bench.log(f'results: {out}')
    print(bench_report.render(results, options.base or builds[0]['label']))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest='command', required=True)
    plan = commands.add_parser('plan', help='write the run list for the browser runner')
    plan.add_argument('folder', help='session folder, e.g. build/web-bench/current')
    plan.add_argument('--scenario', action='append', help='scenario name (repeatable; default all)')
    plan.add_argument('--preset', action='append', help='settings preset (repeatable; default: default)')
    plan.add_argument('--repeats', type=int, default=3)
    plan.add_argument('--env', action='append', metavar='NAME=VALUE',
                      help='extra environment for the game in every run, e.g. GE_GLSHADOW=0 (repeatable)')
    collect = commands.add_parser('collect', help='turn measured sessions into results and a report')
    collect.add_argument('--build', action='append', required=True, metavar='LABEL=FOLDER',
                         help='a measured session folder, path from the repository root (repeatable)')
    collect.add_argument('--base', help='label to compare against (default: the first)')
    collect.add_argument('--out', help='results file (default: build_win/bench-results/web-<time>.json)')
    options = parser.parse_args()
    config = bench_plan.load_config((bench.HERE/'scenarios.json').read_text(encoding='utf-8'))
    return {'plan': command_plan, 'collect': command_collect}[options.command](options, config)


if __name__ == '__main__':
    sys.exit(main())
