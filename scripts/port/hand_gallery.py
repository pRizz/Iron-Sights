#!/usr/bin/env python3
"""Generate/verify the versioned hand review matrix. No Python packages required."""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = Path('review/hand-gallery')
RECIPE = dict(version=1, width=960, height=540, seed=1, tick_hz=60, fov=90,
              background='#30343c', samples_per_case=2, independent_passes=2,
              stock_reference='Original models in the same fork camera/ADS pose; upstream has no ADS.')
WARDROBES = ('blue', 'boiler', 'jungle', 'snow', 'tuxedo', 'connery')
WEAPONS = ((4, 'pp7', 'PP7 pistol'), (8, 'ak47', 'AK47 / KF7'), (7, 'klobb', 'Klobb'))
ICON_NAMES = [None, 'unarmed', 'knife', 'throwing_knife', 'pp7', 'pp7_silenced', 'dd44',
              'klobb', 'kf7', 'zmg', 'd5k', 'd5k_silenced', 'phantom', 'ar33', 'rcp90',
              'shotgun', 'automatic_shotgun', 'sniper_rifle', 'cougar_magnum', 'golden_gun',
              'silver_pp7', 'gold_pp7', 'laser', None, 'grenade_launcher', 'rocket_launcher',
              'grenade', 'timed_mine', 'proximity_mine', 'remote_mine', None, 'taser']
ICON_NAMES += ['unarmed_' + name for name in WARDROBES]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def git(*args, root=ROOT):
    return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.PIPE)


def input_path(name):
    suffix = Path(name).suffix
    return ((name.startswith(('src/', 'include/', 'port/', 'cmake/')) and
             suffix in ('.c', '.cpp', '.h', '.inl', '.cmake', '.glsl', '.json')) or
            name in ('CMakeLists.txt', '.github/workflows/windows-release.yml',
                     'scripts/port/ci_dependencies.py', 'scripts/port/hand_gallery.py',
                     'scripts/port/hand_gallery_template.html'))


def blob_contents(entries, root):
    """Read an index snapshot in one Git process, including Windows CRLF blobs."""
    if not entries:
        return {}
    payload = b''.join(blob.encode()+b'\n' for blob in entries.values())
    result = subprocess.run(['git', '-C', str(root), 'cat-file', '--batch'], input=payload,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True).stdout
    at, output = 0, {}
    for name in entries:
        end = result.index(b'\n', at)
        header = result[at:end].split()
        if len(header) != 3 or header[1] != b'blob':
            raise ValueError('Missing staged blob: '+name)
        size = int(header[2]);at = end+1
        output[name] = result[at:at+size];at += size+1
    return output


def source_fingerprint(root=ROOT, staged=False):
    entries = {}
    for record in git('ls-files', '--stage', '-z', root=root).split(b'\0'):
        if not record:
            continue
        info, raw_name = record.split(b'\t', 1)
        mode, blob, stage = info.decode().split()
        name = raw_name.decode('utf-8')
        if input_path(name):
            if stage != '0':
                raise ValueError('Resolve merge conflicts before rendering: ' + name)
            entries[name] = blob
    if staged:
        for name, raw in blob_contents(entries, root).items():
            raw = raw.replace(b'\r\n', b'\n')
            entries[name] = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    else:
        names = git('ls-files', '--others', '--exclude-standard', '-z', root=root)
        for raw_name in names.split(b'\0'):
            if raw_name and input_path(raw_name.decode('utf-8')):
                entries[raw_name.decode('utf-8')] = ''
        for name in list(entries):
            path = root / name
            if not path.is_file():
                del entries[name]
                continue
            raw = path.read_bytes().replace(b'\r\n', b'\n')
            entries[name] = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    return digest(json.dumps(entries, sort_keys=True, separators=(',', ':')).encode())


def cases():
    return [dict(id=i, wardrobe=wardrobe, weapon=weapon, weapon_id=item, label=label,
                 mode=mode, variant=variant,
                 file=f'images/{wardrobe}/{weapon}-{mode}-{variant}.png')
            for i, (wardrobe, item, weapon, label, mode, variant) in enumerate(
                (w, item, key, label, mode, variant)
                for w in WARDROBES for item, key, label in WEAPONS
                for mode in ('regular', 'ads') for variant in ('stock', 'fork'))]


def png_chunk(kind, payload):
    return struct.pack('>I', len(payload)) + kind + payload + struct.pack('>I', zlib.crc32(kind + payload))


def png_encode(width, height, pixels, channels):
    if channels not in (3, 4) or len(pixels) != width * height * channels:
        raise ValueError('Invalid pixel dimensions')
    stride = width * channels
    raw = b''.join(b'\0' + pixels[y:y+stride] for y in range(0, len(pixels), stride))
    return (b'\x89PNG\r\n\x1a\n' + png_chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8,
            2 if channels == 3 else 6, 0, 0, 0)) + png_chunk(b'IDAT', zlib.compress(raw, 9)) + png_chunk(b'IEND', b''))


def png_decode(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Not a PNG')
    at, packed, header, ended = 8, bytearray(), None, False
    while at < len(data):
        if at + 12 > len(data):
            raise ValueError('Truncated PNG')
        size = struct.unpack_from('>I', data, at)[0]
        kind, payload = data[at+4:at+8], data[at+8:at+8+size]
        if at+12+size > len(data) or zlib.crc32(kind+payload) != struct.unpack_from('>I', data, at+8+size)[0]:
            raise ValueError('PNG checksum/length mismatch')
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT':
            packed.extend(payload)
        elif kind == b'IEND':
            ended = True
        else:
            raise ValueError('Unexpected PNG chunk')
        at += size+12
    if not ended or not header:
        raise ValueError('Incomplete PNG')
    width, height, depth, color, compression, filtering, interlace = header
    if not (0 < width <= 4096 and 0 < height <= 4096 and depth == 8 and color in (2, 6)
            and compression == filtering == interlace == 0):
        raise ValueError('Unsupported gallery PNG')
    channels = 3 if color == 2 else 4
    stride = width*channels+1
    decoder = zlib.decompressobj()
    raw = decoder.decompress(packed, stride*height+1)
    if len(raw) != stride*height or not decoder.eof or decoder.unused_data:
        raise ValueError('Invalid PNG data size')
    if any(raw[y] for y in range(0, len(raw), stride)):
        raise ValueError('Gallery PNG must use the canonical unfiltered encoding')
    return width, height, channels, b''.join(raw[y+1:y+stride] for y in range(0, len(raw), stride))


def bmp_pixels(path):
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b'BM':
        raise ValueError('Invalid capture: ' + str(path))
    offset = struct.unpack_from('<I', data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', data, 18)
    if width != RECIPE['width'] or abs(height) != RECIPE['height'] or planes != 1 or bits != 24 or compression:
        raise ValueError('Unexpected capture format/dimensions: ' + str(path))
    stride = (width*3+3)&~3
    if offset + stride*abs(height) > len(data):
        raise ValueError('Truncated capture')
    rows = [data[offset+y*stride:offset+y*stride+width*3] for y in range(abs(height))]
    bgr = b''.join(reversed(rows) if height > 0 else rows)
    rgb = bytearray(len(bgr))
    rgb[0::3], rgb[1::3], rgb[2::3] = bgr[2::3], bgr[1::3], bgr[0::3]
    return bytes(rgb)


def asset(path, width, height, channels, raw, root):
    target = root/path
    target.parent.mkdir(parents=True, exist_ok=True)
    data = png_encode(width, height, raw, channels)
    target.write_bytes(data)
    return dict(file=path, width=width, height=height, channels=channels,
                sha256=digest(data), pixels_sha256=digest(raw))


def run_game(exe, rom, directory, cache, name, gallery=False):
    env = {k: v for k, v in os.environ.items() if not k.startswith('GE_')}
    env.update(GE_ROM=str(rom), GE_WINDOW='960x540', GE_NOINPUT='1', GE_SEED='1',
               GE_STAGE='runway', GE_STAGE_AFTER='1', GE_SKIPINTRO='1',
               GE_CHEATS='invuln,invis,guns', GE_ICON_CACHE_DIR=str(cache),
               GE_FRAMES='14000' if gallery else '5')
    if gallery:
        env.update(GE_HAND_GALLERY='1', GE_FIXEDTICK='1')
    else:
        env['GE_ICON_CACHE_FORCE'] = '1'
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    startup = None
    if os.name == 'nt':
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
    # Keep streams separate: console printf and capture diagnostics can run
    # concurrently, and a shared file corrupts otherwise atomic stderr lines.
    engine = directory.parent/(name+'.engine.log')
    with (directory.parent/(name+'.stdout.log')).open('wb') as output, engine.open('wb') as errors:
        process = subprocess.Popen([str(exe)], cwd=directory, env=env, stdout=output, stderr=errors,
                                   creationflags=flags, startupinfo=startup)
        start, checked = time.monotonic(), set()
        try:
            while process.poll() is None:
                if time.monotonic()-start > 600:
                    raise ValueError('Capture timeout; see '+str(directory.parent))
                if gallery and engine.exists():
                    partial = engine.read_text(encoding='utf-8', errors='replace')
                    captures, pending = {}, None
                    for line in partial.splitlines():
                        match = re.search(r'hand-gallery capture case=(\d+) sample=(\d+)', line)
                        if match:
                            pending = tuple(map(int, match.groups()))
                        match = re.search(r'gfx: wrote (shot_\d+\.bmp)', line)
                        if match and pending:
                            captures[pending] = directory/match[1];pending = None
                    for case_id in range(72):
                        if case_id not in checked and (case_id, 1) in captures and (case_id, 2) in captures:
                            if bmp_pixels(captures[case_id, 1]) != bmp_pixels(captures[case_id, 2]):
                                raise ValueError('Non-deterministic repeated pose: '+cases()[case_id]['file'])
                            checked.add(case_id)
                            if len(checked) % 12 == 0:
                                print(f'  {name}: {len(checked)}/72 cases verified', flush=True)
                time.sleep(.25)
            if process.returncode:
                raise ValueError(f'Game capture exited {process.returncode}; see {directory.parent}')
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill();process.wait()
    return engine.read_text(encoding='utf-8', errors='replace')


def capture_pass(log, directory):
    captured, pending = {}, None
    for line in log.splitlines():
        match = re.search(r'hand-gallery capture case=(\d+) sample=(\d+) item=(\d+) wardrobe=(\d+) ads=(\d+) stock=(\d+)', line)
        if match:
            if pending:
                raise ValueError('A gallery capture was skipped')
            case_id, sample, item, wardrobe, aiming, stock = map(int, match.groups())
            case = cases()[case_id]
            if (item, wardrobe, aiming, stock) != (case['weapon_id'], case_id//12, int(case['mode']=='ads'), int(case['variant']=='stock')):
                raise ValueError('Capture does not match its matrix coordinates')
            pending = case_id, sample
        match = re.search(r'gfx: wrote (shot_\d+\.bmp)', line)
        if match and pending:
            if pending in captured:
                raise ValueError('Duplicate capture')
            captured[pending] = bmp_pixels(directory/match[1])
            pending = None
    if pending or len(captured) != 144 or 'hand-gallery complete cases=72 samples=144' not in log:
        raise ValueError('Incomplete 72-case gallery; inspect local capture logs')
    for case in cases():
        if captured[case['id'], 1] != captured[case['id'], 2]:
            raise ValueError('Non-deterministic repeated pose: ' + case['file'])
    return {case['id']: captured[case['id'], 1] for case in cases()}


def cache_icons(path, root):
    data = path.read_bytes()
    if data[:8] != b'MIDASICO' or len(data) < 32:
        raise ValueError('Invalid icon cache')
    version, rom, width, height, count, crc = struct.unpack_from('<6I', data, 8)
    if (width, height, count) != (512, 160, 38) or crc != zlib.crc32(data[32:]):
        raise ValueError('Invalid icon cache dimensions/checksum')
    expected = int(re.search(r'PORT_ICON_CACHE_VERSION\s+(\d+)', (ROOT/'port/include/port_icon_cache_format.h').read_text())[1])
    if version != expected:
        raise ValueError('Icon cache version is stale')
    result, at = [], 32
    for slot in range(count):
        if at+4 > len(data):
            raise ValueError('Truncated icon cache')
        size = struct.unpack_from('<I', data, at)[0]
        at += 4
        if size != (0 if slot in (23, 30) else width*height*4) or at+size > len(data):
            raise ValueError('Bad icon cache slot')
        if slot and ICON_NAMES[slot]:
            result.append(dict(id=slot, name=ICON_NAMES[slot], **asset(
                f'images/icons/{slot:02d}-{ICON_NAMES[slot]}.png', width, height, 4, data[at:at+size], root)))
        at += size
    if at != len(data) or len(result) != 35:
        raise ValueError('Incomplete icon coverage')
    return result


def page(manifest):
    template = (ROOT/'scripts/port/hand_gallery_template.html').read_text(encoding='utf-8')
    payload = json.dumps(manifest, sort_keys=True, ensure_ascii=True, separators=(',', ':')).replace('</', '<\\/')
    return template.replace('@@MANIFEST@@', payload)


def verify(root=ROOT, staged=False):
    indexed = {}
    if staged:
        entries = {}
        for record in git('ls-files', '--stage', '-z', '--', OUTPUT.as_posix(), root=root).split(b'\0'):
            if record:
                info, name = record.split(b'\t', 1)
                mode, blob, stage = info.decode().split()
                if stage != '0':
                    raise ValueError('Unmerged gallery files')
                entries[name.decode('utf-8')] = blob
        indexed = blob_contents(entries, root)
    def read(name):
        path = (OUTPUT/name).as_posix()
        if staged:
            if path not in indexed:
                raise ValueError('Missing staged gallery file: '+path)
            return indexed[path]
        return (root/path).read_bytes()
    manifest = json.loads(read('manifest.json'))
    if manifest.get('schema') != 1 or manifest.get('recipe') != RECIPE:
        raise ValueError('Gallery recipe changed; regenerate references')
    if manifest.get('inputs_sha256') != source_fingerprint(root, staged):
        raise ValueError('Gallery is stale for these rendering inputs; regenerate references')
    if not manifest.get('repeat_verified') or not manifest.get('independent_pass_verified'):
        raise ValueError('Unverified render determinism')
    if [{k: row[k] for k in case} for row, case in zip(manifest['cases'], cases())] != cases() or len(manifest['cases']) != 72:
        raise ValueError('Matrix coordinates/coverage changed')
    if [(row['id'], row['name']) for row in manifest['icons']] != [(i, name) for i, name in enumerate(ICON_NAMES) if i and name]:
        raise ValueError('Icon coverage changed')
    for row in manifest['cases']:
        if (row['width'], row['height'], row['channels']) != (RECIPE['width'], RECIPE['height'], 3):
            raise ValueError('Wrong matrix capture dimensions')
    for row in manifest['icons']:
        if row['file'] != f"images/icons/{row['id']:02d}-{row['name']}.png" or (row['width'], row['height'], row['channels']) != (512, 160, 4):
            raise ValueError('Wrong icon path/dimensions')
    expected_files = {'index.html', 'manifest.json'}
    for row in manifest['cases'] + manifest['icons']:
        name = row['file']
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name or not name.startswith('images/') or path.suffix != '.png':
            raise ValueError('Unsafe gallery image path')
        data = read(name)
        width, height, channels, pixels = png_decode(data)
        if (width, height, channels) != (row['width'], row['height'], row['channels']) or digest(data) != row['sha256'] or digest(pixels) != row['pixels_sha256']:
            raise ValueError('Gallery image mismatch: '+name)
        expected_files.add(name)
    if read('index.html').decode('utf-8') != page(manifest):
        raise ValueError('Generated HTML does not match template/manifest')
    if staged:
        found = {p[len(OUTPUT.as_posix())+1:] for p in indexed}
    else:
        found = {p.relative_to(root/OUTPUT).as_posix() for p in (root/OUTPUT).rglob('*') if p.is_file()}
    if found != expected_files:
        raise ValueError('Unexpected/missing gallery files: '+str(sorted(found ^ expected_files)))
    return manifest


def build(root, work):
    configured = git('config', '--get', 'midas.galleryBuild', root=root).decode().strip() if subprocess.run(
        ['git', '-C', str(root), 'config', '--get', 'midas.galleryBuild'], capture_output=True).returncode == 0 else ''
    if configured:
        command = json.loads(configured)
    elif os.name == 'nt':
        command = ['docker', 'run', '--rm', '-v', str(root.as_posix())+':/home/dev/project',
                   '-w', '/home/dev/project', 'goldeneye', 'cmake', '--build', 'build_win', '-j8']
    else:
        command = ['cmake', '--build', 'build_pc_video', '-j8']
    if not isinstance(command, list) or not command or not all(isinstance(x, str) for x in command):
        raise ValueError('midas.galleryBuild must be a JSON argv array')
    print('Building current rendering inputs...', flush=True)
    with (work/'build.log').open('wb') as log:
        subprocess.run(command, cwd=root, stdout=log, stderr=log, check=True, timeout=1200)
    return root/('build_win/ProjectMidas.exe' if os.name == 'nt' else 'build_pc_video/ProjectMidas')


def generate(root=ROOT):
    fingerprint = source_fingerprint(root)
    rom = root/'baserom.u.z64'
    if not rom.is_file():
        raise ValueError('Rendering needs your own baserom.u.z64 in the repository root')
    (root/'build_win').mkdir(exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='hand-gallery-', dir=root/'build_win'))
    print('Local capture logs: '+str(work), flush=True)
    exe = build(root, work)
    game = work/'game'
    game.mkdir()
    target = game/exe.name
    shutil.copy2(exe, target)
    for dll in exe.parent.glob('*.dll'):
        shutil.copy2(dll, game/dll.name)
    (game/'midas.ini').write_text('[display]\nfullscreen=window\nvsync=0\nmaxfps=60\ntickhz=60\n'
        'msaa=1\nrender_thread=0\nwidescreen=1\nhfov_wide=90\nlow_res=0\nfiltering=bilinear\nfilter_2d=1\n'
        'aim_mode=1\nsight_on_screen=0\nalways_crosshair=0\nweapon_hud=0\nalways_health=0\n'
        'mission_timer=0\nhit_markers=0\ndamage_indicators=0\nobjective_notifications=0\n[input]\nmouse=1\n', encoding='utf-8')
    output = work/'published'
    output.mkdir()
    try:
        print('Regenerating all 35 native weapon/wardrobe icons...', flush=True)
        run_game(target, rom, game, work/'cache', 'icons')
        icons = cache_icons(next((work/'cache').glob('*.cache')), output)
        print('Capturing 72 cases, two samples per case...', flush=True)
        log = run_game(target, rom, game, work/'cache', 'pass-1', True)
        first = capture_pass(log, game)
        for path in game.glob('shot_*.bmp'):
            path.unlink()
        print('Verifying a second independent render pass...', flush=True)
        second = capture_pass(run_game(target, rom, game, work/'cache', 'pass-2', True), game)
        for case in cases():
            if first[case['id']] != second[case['id']]:
                raise ValueError('Independent render passes differ: '+case['file'])
        rows = [dict(case, **asset(case['file'], RECIPE['width'], RECIPE['height'], 3,
                                 first[case['id']], output)) for case in cases()]
        renderer = {}
        for line in (log + (work/'pass-1.stdout.log').read_text(errors='replace')).splitlines():
            match = re.search(r'\bGL:\s*(version|vendor|renderer):\s*(.*)', line, re.I)
            if match:
                renderer[match[1].lower()] = match[2].strip()
        if set(renderer) != {'version', 'vendor', 'renderer'}:
            raise ValueError('Renderer identity missing from capture logs')
        manifest = dict(schema=1, recipe=RECIPE, inputs_sha256=fingerprint,
                        rom_sha256=digest(rom.read_bytes()), renderer=renderer,
                        repeat_verified=True, independent_pass_verified=True, cases=rows, icons=icons)
        if source_fingerprint(root) != fingerprint:
            raise ValueError('Rendering inputs changed during capture; rerun generation')
        (output/'manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True)+'\n', encoding='utf-8', newline='\n')
        (output/'index.html').write_text(page(manifest), encoding='utf-8', newline='\n')
        destination = root/OUTPUT
        before = json.loads((destination/'manifest.json').read_text()) if (destination/'manifest.json').exists() else {}
        old = {row['file']: row['pixels_sha256'] for row in before.get('cases', [])+before.get('icons', [])}
        changed = [row['file'] for row in rows+icons if old.get(row['file']) != row['pixels_sha256']]
        # Publish only after both complete passes and icon repeat checks succeed.
        destination.mkdir(parents=True, exist_ok=True)
        expected = {p.relative_to(output) for p in output.rglob('*') if p.is_file()}
        for relative in expected:
            path = destination/relative
            path.parent.mkdir(parents=True, exist_ok=True)
            data = (output/relative).read_bytes()
            if not path.exists() or path.read_bytes() != data:
                path.write_bytes(data)
        for path in destination.rglob('*'):
            if path.is_file() and path.relative_to(destination) not in expected:
                raise ValueError('Unexpected gallery file; remove deliberately: '+str(path))
        verify(root)
        print(f'Gallery ready: {destination / "index.html"}\nChanged image cells: {len(changed)}', flush=True)
        for name in changed:
            print('  '+name)
    finally:
        target.unlink(missing_ok=True)


def install_hook(root=ROOT):
    result = subprocess.run(['git', '-C', str(root), 'config', '--get', 'core.hooksPath'], capture_output=True, text=True)
    if result.returncode == 0 and result.stdout.strip() != '.githooks':
        raise ValueError('Existing core.hooksPath is configured; integrate the gallery hook there instead of replacing it')
    native = root/'.git/hooks/pre-commit'
    if result.returncode and native.exists():
        raise ValueError('An existing pre-commit hook needs integration; it has not been overwritten')
    subprocess.run(['git', '-C', str(root), 'config', 'midas.galleryPython', sys.executable], check=True)
    subprocess.run(['git', '-C', str(root), 'config', 'core.hooksPath', '.githooks'], check=True)
    print('Installed gallery pre-commit hook for this clone.')


def pre_commit(root=ROOT):
    if source_fingerprint(root) != source_fingerprint(root, True):
        raise ValueError('Rendering inputs have unstaged changes. Stage those inputs first; the hook will not capture a different working tree than the commit.')
    try:
        verify(root, True)
        print('Hand gallery is fresh and verified for staged rendering inputs.')
        return
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        print('Gallery check: '+str(error), flush=True)
    try:
        verify(root)
    except (ValueError, OSError, subprocess.CalledProcessError):
        generate(root)
    raise ValueError('Fresh gallery is in review/hand-gallery. Review the images, stage that directory, and commit again. Nothing was automatically staged.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=('generate', 'verify', 'install-hook', 'pre-commit'))
    parser.add_argument('--index', action='store_true', help='Verify the staged gallery and staged inputs')
    args = parser.parse_args()
    try:
        if args.command == 'generate':
            generate()
        elif args.command == 'verify':
            verify(staged=args.index)
            print('Gallery verified: 72 matrix cells, 35 icons, input/pixel/file hashes and generated HTML.')
        elif args.command == 'install-hook':
            install_hook()
        else:
            pre_commit()
    except (ValueError, OSError, KeyError, IndexError, StopIteration, subprocess.SubprocessError) as error:
        print('Hand gallery: '+str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
