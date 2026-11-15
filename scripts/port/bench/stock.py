"""Build a reference executable for the benchmark suite from a commit.

By default that commit is the inherited upstream baseline, stock Project Midas
1.0.2b. The commit is exported into an ignored folder, given only what a
benchmark needs to measure it, and built with the project's own container and
compiler flags:

  * port/src/port_bench.c and port_profile.c, this checkout's frame logger
    and sampling profiler
  * one call to it in port/src/hitchlog.c
  * an early return in waitForNextFrame so GE_FIXEDTICK does not wait on the
    clock (a fixed-work run needs it)

A commit that already has one of the two hooks keeps its own. Nothing else in the
exported tree changes. The generated ROM-derived assets are copied from this
checkout, since Git ignores them.

    python scripts/port/bench/stock.py                         # stock 1.0.2b
    python scripts/port/bench/stock.py --ref HEAD~1 --name before
    python scripts/port/bench/stock.py --name before --clean   # delete a folder

The executable lands in build_win/bench-<name>/build.
"""
import argparse
import io
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
# FORK_CHANGES.md "Comparison baseline": change both together, deliberately.
BASELINE_REF = '9062f9f120b7f64d39e5db59500ed50e8e7a6686'
CONTAINER_ROOT = '/home/dev/project'
SDL2_ROOT = '/home/dev/sdl2-mingw/SDL2-2.30.11/i686-w64-mingw32'
REF_MARKER = 'BENCH_REF'
HARNESS_FILES = ('port_bench.c', 'port_profile.c')

HITCH_ANCHOR = '        us[i] = __atomic_exchange_n(&sHitchUs[i], 0ull, __ATOMIC_RELAXED);\n    }\n'
HITCH_CALL = 'portBenchFrame(interval_us, run_us, swap_us, lim_us, draws, simmax, us);'
HITCH_HOOK = f'''    {{ /* port_bench.c: GE_BENCH's per-frame log (benchmark harness) */
        extern void portBenchFrame(unsigned long long, unsigned long long, unsigned long long, unsigned long long,
                                   unsigned int, unsigned long long, const unsigned long long *);
        {HITCH_CALL}
    }}
'''
TICK_ANCHOR = ('void waitForNextFrame(void) //maybe WaitForTick\n{\n'
               '  u32 nextFrameTime; //next frame time?\n\n#ifndef TARGET_N64\n')
# The fork's own early return starts this way; a commit that has it needs no hook.
TICK_PRESENT = '  if (portFixedTick())\n  {\n      frameDelay = 1;'
TICK_HOOK = '''  { /* benchmark harness: GE_FIXEDTICK counts every frame as one field, no wait */
      extern char *getenv(const char *);
      static s32 benchFixedTick = -1;
      if (benchFixedTick < 0) { benchFixedTick = (getenv("GE_FIXEDTICK") != NULL); }
      if (benchFixedTick) { frameDelay = 1; updateFrameCounters(1); return; }
  }
'''


def insert_after(text, anchor, addition, present, name):
    """Return text with addition after its single anchor, unless `present` shows it is there."""
    if present in text:
        return text
    if text.count(anchor) != 1:
        raise ValueError(f'{name}: expected one anchor, found {text.count(anchor)}')
    return text.replace(anchor, anchor+addition)


def resolve_ref(ref):
    return subprocess.run(['git', '-C', str(REPO), 'rev-parse', '--verify', ref+'^{commit}'],
                          check=True, capture_output=True, text=True).stdout.strip()


def export_source(commit, source):
    archive = subprocess.run(['git', '-C', str(REPO), 'archive', '--format=tar', commit],
                             check=True, capture_output=True).stdout
    source.mkdir(parents=True)
    with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
        tar.extractall(source)
    (source/REF_MARKER).write_text(commit+'\n', encoding='utf-8')


def copy_ignored_assets(source):
    """Copy generated assets the export lacks; its tracked files stay as exported."""
    copied = 0
    for path in (REPO/'assets').rglob('*'):
        target = source/'assets'/path.relative_to(REPO/'assets')
        if path.is_dir() or target.exists():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        copied += 1
    return copied


def apply_harness(source):
    # The measuring code is always this checkout's, so every build is timed
    # and profiled by the same instrument.
    for name in HARNESS_FILES:
        shutil.copy2(REPO/'port'/'src'/name, source/'port'/'src'/name)
    for relative, anchor, hook, present in (('port/src/hitchlog.c', HITCH_ANCHOR, HITCH_HOOK, HITCH_CALL),
                                            ('src/game/frametiming.c', TICK_ANCHOR, TICK_HOOK, TICK_PRESENT)):
        path = source/relative
        text = path.read_bytes().decode('utf-8')
        path.write_bytes(insert_after(text, anchor, hook, present, relative).encode('utf-8'))


def container(command):
    mount = f'{REPO.as_posix()}:{CONTAINER_ROOT}'
    return subprocess.run(['docker', 'run', '--rm', '-v', mount, '-w', CONTAINER_ROOT, 'goldeneye',
                           'bash', '-lc', command]).returncode


def build(work, jobs):
    relative = work.relative_to(REPO).as_posix()
    configure = (f'cmake -S {relative}/src -B {relative}/build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-i686.cmake '
                 f'-DCMAKE_BUILD_TYPE=Debug -DGE_VIDEO=ON -DGE_ROMDATA_FALLBACK=OFF -DSDL2_MINGW_ROOT={SDL2_ROOT}')
    log = f'{relative}/build.log'
    return container(f'({configure} && cmake --build {relative}/build -j{jobs}) > {log} 2>&1')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--ref', default=BASELINE_REF, help='commit to build (default: the inherited baseline)')
    parser.add_argument('--name', default='stock', help='folder name: build_win/bench-<name> (default: stock)')
    parser.add_argument('--jobs', type=int, default=8)
    parser.add_argument('--clean', action='store_true', help='delete the folder and stop')
    args = parser.parse_args()
    if not args.name.replace('-', '').isalnum():
        parser.error('--name takes letters, digits and dashes')
    work = REPO/'build_win'/('bench-'+args.name)
    source = work/'src'
    if args.clean:
        shutil.rmtree(work, ignore_errors=True)
        print('removed', work)
        return 0
    commit = resolve_ref(args.ref)
    marker = source/REF_MARKER
    if source.exists() and (not marker.exists() or marker.read_text(encoding='utf-8').strip() != commit):
        shutil.rmtree(work)
    if not source.exists():
        print('exporting', commit)
        export_source(commit, source)
        print('copied', copy_ignored_assets(source), 'generated asset files')
    apply_harness(source)
    print('building; log:', work/'build.log')
    if build(work, args.jobs) != 0:
        print('build failed; see', work/'build.log', file=sys.stderr)
        return 1
    executables = [path for path in (work/'build').glob('*.exe') if path.stem in ('IronSights', 'ProjectMidas')]
    if len(executables) != 1:
        print('build produced no game executable in', work/'build', file=sys.stderr)
        return 1
    print('built', executables[0])
    return 0


if __name__ == '__main__':
    sys.exit(main())
