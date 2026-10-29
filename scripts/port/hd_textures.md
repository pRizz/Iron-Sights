# Native HD texture packs

This is a Dam environment prototype for the Windows/OpenGL player. Web and Xbox
graphics are unchanged. Original textures remain the default. In the frontend
or pause watch, open **Port Options > Texture Pack** and choose Original, HD or
4K. Missing packs and unmatched or invalid textures fall back to the originals.
The supplied HD pack covers Dam scenery, not a complete game-wide remaster.
Existing enhanced fonts remain independent; weapon and hand geometry is unchanged.

## Pack preparation and swapping

The executable reads `Data/TexturePacks/hd.istex` or `4k.istex` beside itself.
These are compressed sidecar files, not executable resources. Replace either
file to update artwork without recompiling. Changes are detected at a safe
frame boundary, approximately once per 60 rendered frames; choosing Original
and then HD/4K also reloads immediately. The owning render thread discards both
GPU textures and retained draws when the pack changes. Windows handles allow
atomic replacement while the old snapshot is being read.

Prepare the pinned source HD archive with Python and Pillow 12.0.0:

```powershell
python -m venv build_win/hd-tools
build_win/hd-tools/Scripts/python.exe -m pip install Pillow==12.0.0
build_win/hd-tools/Scripts/python.exe scripts/port/prepare_texture_pack.py --download --archive build_win/hd-research/ge007-hd.zip --output build_win/hd-prototype/Data/TexturePacks/hd.istex
```

For custom artwork, pass its ZIP through `--archive` without `--download`.
Keep the Rice filenames: `GOLDENEYE#TEXTURE#FORMAT#SIZE_all.png`, or
`GOLDENEYE#TEXTURE#FORMAT#SIZE#PALETTE_ciByRGBA.png`. Folder names do not affect
matching. The default Dam scope uses `hd_dam_sources.json`; `--scope all` imports
all supported keys. A prepared higher-resolution source archive can be written
to `4k.istex` using the same command. 4K artwork is not included in this prototype.
Palette wildcard `$` is supported. Separate `_rgb`/`_a` pairs and HTS caches are
outside the initial format. Conflicting images with the same key are rejected.

The generator atomically replaces the pack and writes a JSON provenance/analysis
report plus artwork and decoder notices beside it. Its report records archive,
PNG and decoded-image SHA256 values, dimensions, duplicate images, source PNG
bytes, decoded RGBA bytes and final compressed size. No source pixels are put
in Git. Native release packaging explicitly allowlists the prepared packs,
reports and notices; it never sweeps ROMs, user saves or unrelated archives.

## Identity and rendering

The native renderer computes Rice texture fingerprints from the original tile
data, reconstructing N64 word ordering and the alternate-row swaps omitted by
the PC inflater. CI identity also includes native palette content. The pack
preparer adds a reviewed stock-USA Dam crosswalk from native palette identities
to the source pack's GLideN64 identities (`hd_dam_native_aliases.json`). It never
selects arbitrary palette artwork from an index hash alone. Other palette
variants remain original until separately verified. Exact matches precede
explicit palette wildcards in the source artwork.
The naming reference is GLideN64's [replacement loader](https://github.com/gonetz/GLideN64/blob/master/src/GLideNHQ/TxHiResLoader.cpp)
and [Rice fingerprint definition](https://github.com/gonetz/GLideN64/blob/master/src/GLideNHQ/TxUtil.cpp).
Only the final RGBA upload is substituted. Native geometry, UVs, wrap/mirror,
combiner, alpha tests and mip/sampler rules retain their original ownership.
Unrecognized or incompatible tiles fall back rather than guessing.

`ISTEX001` stores a little-endian header, sorted 36-byte records and independently
DEFLATE-compressed PNG-filtered RGBA payloads. Records contain texture/palette
fingerprints, format/size, image width/height, offset/length and decoded Adler32.
Identical decoded images share one payload even when keys differ. PNG container
metadata is omitted, and rows flip losslessly to the native bottom-up convention.
Every RGBA pixel and alpha value is retained; near-duplicate
or visually similar textures are not deleted. Decode validates bounds, filters,
length and checksum with Mark Adler's bounded puff decoder. No runtime PNG DLL
or extraction directory is needed. CPU decoding holds one replacement at a time;
HD GPU cache retention has a byte budget in addition to the inherited count cap.

## Attribution

GoldenEye 007 HD is by **intermissionfb**, with font textures and technical
assistance by **GhostlyDark**. See the [pack page](https://evilgames.eu/texture-packs/ge007-hd.htm)
and [source repository](https://github.com/GhostlyDark/GoldenEye-007-HD).
The source HD release is v2025.12.30, SHA256
`a72d082e23b516f28f4868b0b32cbb676a5b0ce7f00e7eb79badf89f71ab90ba`.
The source archive supplies attribution but no explicit redistribution license;
the port's MIT license does not relicense artist artwork. The owner requested
bundling this compressed Dam subset. Preserve its separate notice/provenance.
About / Build Info also credits the artists and the decoder.

The unmodified puff 2.3 sources come from zlib v1.3.1, with their original
license retained in `port/fast3d/third_party/puff/puff.h` and shipped as a notice.

## Validation and isolated development

Build into a separate CMake binary directory while the main player is running.
Do not terminate a player process or replace its executable, settings or saves.
`audit_hd_textures.ps1 -Executable <isolated exe>` creates its own runtime folders
and compares seeded Original/HD Dam captures. `-Gameplay` skips the intro;
`-Reload` removes/restores only the audit's copied pack while rendering to verify
live fallback/reload and Windows file sharing. `-Menu` checks the Port Options
sheet. Captures, diagnostic texture dumps and copied artwork stay ignored.
Audit executables are removed after their task-owned processes finish.

ROM-free checks: `test_hd_textures.sh`, `test_texture_pack_preparation.py` and
`test_distribution.py`. Shared rendering changes also require wardrobe/ADS
regressions and the review gallery. Generate the latter with
`hand_gallery.py generate --executable <isolated exe>` so the player build is
untouched. Diagnostics `GE_HD_TRACE=1` log matches/misses; `GE_HD_DUMP=1` adds local
raw tile dumps only when tracing is active. These dumps must never be committed.
