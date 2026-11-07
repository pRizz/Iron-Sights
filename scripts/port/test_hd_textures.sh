#!/usr/bin/env bash
# ROM-free tests of HD texture packs (FORK-053): the pack index and the
# download engine. Run from any
# directory; the goldeneye Docker image and CI's Ubuntu runner have the tools.
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-hd-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
python3 scripts/port/make_hd_texture_fixture.py "$work/pack"
"${CC:-gcc}" -c port/fast3d/third_party/puff/puff.c -o "$work/puff.o"
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I. -Iport/fast3d \
    scripts/port/test_hd_textures.cpp port/fast3d/hd_texture_pack.cpp port/fast3d/hd_texture_format.cpp \
    port/fast3d/png_decode.cpp "$work/puff.o" -o "$work/test"
"$work/test" "$work/pack"
# The download engine with a fake transport (no network).
"${CXX:-g++}" -std=c++20 -Wall -Wextra -Werror -I. -Iport/fast3d \
    scripts/port/test_hd_download.cpp port/fast3d/hd_download_engine.cpp -o "$work/test_download"
"$work/test_download" "$work/download"
# The Close HD Matches list (only while close_hd_matches is on) and the
# reviewed alias lists name different native palettes of in-scope keys.
python3 - <<'PY'
import json
def keys(name):
    aliases = json.load(open(f'port/fast3d/texture_packs/{name}.json'))['aliases']
    return [(a['pack_key'][0], a['native_palette']) for a in aliases]
close = keys('hd_level_close_aliases')
reviewed = set(keys('hd_level_native_aliases') + keys('hd_dam_native_aliases'))
scope = {t.split('/')[-1].split('#')[1].upper() for t in json.load(open('port/fast3d/texture_packs/hd_level_sources.json'))['textures']}
assert close and len(set(close)) == len(close) and not set(close) & reviewed
assert all(f'{texture:08X}' in scope for texture, _ in close)
print(f'hd aliases: {len(reviewed)} reviewed, {len(close)} close matches')
# Effect remaps: one image per native texture, fire frames cropped (never
# masked), smoke masked; the files they name are in the effect folders.
remaps = json.load(open('port/fast3d/texture_packs/hd_effect_remaps.json'))['remaps']
native = [tuple(r['native_key']) for r in remaps]
assert remaps and len(set(native)) == len(native)
for r in remaps:
    cropped = 'keep_rows' in r
    assert cropped != r.get('masked', False)
    assert not cropped or 0 < r['keep_rows'] < r['of_rows']
    assert f"#{r['pack_key'][0]:08X}#{r['pack_key'][2]}#{r['pack_key'][3]}_" in r['source']
    assert r['source'].startswith('GOLDENEYE/Effects/')
effects = json.load(open('port/fast3d/texture_packs/hd_effect_sources.json'))['textures']
assert len(set(effects)) == len(effects) and all(t.startswith('GOLDENEYE/') for t in effects)
print(f'hd effects: {len(effects)} by key, {len(remaps)} remapped')
PY
