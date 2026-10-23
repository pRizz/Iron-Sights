"""Register game-owned writable sections, never CRT/SDL/GL state.

Append the exact-build layout to the executable. PE and ELF loaders ignore the
overlay; no extra sidecar is needed when the user copies their executable.
"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import subprocess

MAGIC = b'MIDASLAY'
FOOTER = b'MIDASEND'
# Host queues/timing/input and persistent profile/settings are deliberately
# outside the world image. They continue to belong to the running process.
EXCLUDE = {'boss.c', 'init.c', 'joy.c', 'sched.c', 'pi.c', 'rom.c', 'ramrom.c',
           'vi.c', 'rsp.c', 'deb.c', 'debugmenu.c', 'file2.c', 'options.c',
           'frametiming.c', 'speedgraphrenderer.c', 'token.c'}
PORT = {'n64_random.c', 'segments.c', 'animdata.c', 'romdata.c', 'audio_stubs.c',
        'romfile_bg.c'}  # converted-ROM cache must travel with the ROM bytes
SKIP_SYMBOL = re.compile(r'^(?:_?)(?:saved(?:count)?\.|pristine\.|wd_log\.|s_RoomAudit)')


def regions(map_text, source_root, symbols):
    names = {p.name for p in (source_root/'src').glob('*.c')}
    names |= {p.name for p in (source_root/'src/game').glob('*.c')}
    names -= EXCLUDE
    names |= PORT
    names |= {p.name for p in (source_root/'assets').glob('*.c')}
    entries = []
    pattern = re.compile(r'^ (?:\.data|\.bss|COMMON)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+(.+)$')
    # Static cache pointers point to host allocations, not to mission objects.
    # nm includes local symbols; use their next symbol/section end as bounds.
    syms = sorted((int(m[1], 16), m[2]) for line in symbols.splitlines()
                  if (m := re.match(r'^([0-9a-fA-F]+)\s+[bBdD]\s+(\S+)$', line)))
    for line in map_text.splitlines():
        m = pattern.match(line)
        if not m:
            continue
        start, size = int(m[1], 16), int(m[2], 16)
        owner = m[3].strip()
        obj = re.search(r'(?:\(|/)([^/()]+)\.c\.(?:obj|o)\)?$', owner)
        asset = 'asset_obj/' in owner or '/assets/' in owner
        if not size or not (asset or obj and obj[1]+'.c' in names):
            continue
        inside = [(addr, name) for addr, name in syms if start <= addr < start+size]
        cut = start
        for i, (addr, name) in enumerate(inside):
            if not SKIP_SYMBOL.match(name):
                continue
            end = next((a for a, _ in inside[i+1:] if a > addr), start+size)
            if cut < addr:
                entries.append((cut, addr-cut))
            cut = max(cut, end)
        if cut < start+size:
            entries.append((cut, start+size-cut))
    merged = []
    for addr, size in sorted(entries):
        if merged and addr <= merged[-1][0]+merged[-1][1]:
            prev, length = merged[-1]
            merged[-1] = (prev, max(prev+length, addr+size)-prev)
        else:
            merged.append((addr, size))
    if len(merged) < 50 or sum(size for _, size in merged) < 100000:
        raise ValueError('Incomplete state layout: refusing to enable snapshots')
    return merged


def embed(data, chunks):
    if len(data) >= 12 and data[-12:-4] == FOOTER:
        old_size, = struct.unpack('<I', data[-4:])
        if old_size < 44 or old_size > len(data)-12 or data[-12-old_size:-4-old_size] != MAGIC:
            raise ValueError('Invalid prior state-layout footer')
        data = data[:-12-old_size]
    layout = MAGIC+hashlib.sha256(data).digest()+struct.pack('<I', len(chunks))
    layout += b''.join(struct.pack('<II', *chunk) for chunk in chunks)
    return data+layout+FOOTER+struct.pack('<I', len(layout))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', type=Path, required=True)
    ap.add_argument('--map', type=Path, required=True)
    ap.add_argument('--root', type=Path, required=True)
    ap.add_argument('--nm', required=True)
    args = ap.parse_args()
    data = args.exe.read_bytes()
    symbols = subprocess.check_output([args.nm, '-n', str(args.exe)], text=True)
    chunks = regions(args.map.read_text(), args.root, symbols)
    args.exe.write_bytes(embed(data, chunks))
    print(f'State layout: {len(chunks)} regions, {sum(n for _, n in chunks)} owned bytes')


if __name__ == '__main__':
    main()
