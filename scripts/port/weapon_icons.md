# Weapon HUD image generation

Windows player builds generate transparent 512 x 160 icons from the actual
ROM-loaded weapon models on first launch. The native helper uses the game's
renderer, fixed camera/poses and repeated black/white captures. It recovers
alpha and downsamples with premultiplied area sampling. No Python, compiler,
network access or rebuild is needed by players.

A progress window shows 35 images; Esc skips preparation. The per-user SDL
application-data folder contains `weapon-icons-v6-<ROM fingerprint>.cache` and
`last-preparation.log`. About shows readiness. Cache validation checks the ROM,
generator version, dimensions, complete slot coverage, exact lengths and CRC.
Invalid caches regenerate; errors or skipping fall back to weapon names.
The helper uses neutral settings and temporary program files, leaves campaign
saves/settings alone, and removes its temporary executable on exit. Unicode
paths are supported. Linux can read this cache format; automatic generation
currently targets Windows. Bump `PORT_ICON_CACHE_VERSION` when rendering changes
need new images. Version 2 introduced the authored pause-watch forearm meshes
and UVs in place of estimated sleeve coverage. Version 4 supersedes version 3
arm-body thickening: it fits the wrist to the retained native hand independently
of arm length, blends the cuff/forearm profile and refits the watch. Version 5
lightens the watch model's exposed-skin materials to match the native hand,
preserving sleeves, watch materials and texture detail. Version 6 measures the
actual shared wrist seam and stitches forearm edge triangles directly to the
native chopping hand's polygon outline, closing gaps throughout the animation.
It also closes the open clothing rim around the exposed wrist. Generated ROM-derived
data must stay out of Git.

The 38 indexed slots include aliases and empty entries, 29 weapon images and
six wardrobe-specific unarmed images. The HUD chooses the current mission's
outfit for unarmed and caches GPU textures for visible icons; ordinary gameplay
does not rerender weapon models for the HUD. Cross-GPU byte identity is not
promised; repeated captures must match exactly on the active rendering setup.

## Optional developer PNG exports

The existing Python exporter remains useful for inspecting models and making
preview sheets. Its legacy RGBA packet/header outputs are diagnostic assets;
normal player builds now use the native runtime cache instead of embedding them.
Do not rebuild expecting these exports to replace the native cache.

From the repository root, with a GE_VIDEO build, your `baserom.u.z64`, Python,
Pillow and NumPy available:

```powershell
python -m pip install Pillow==12.3.0 numpy==2.3.5
python scripts/port/generate_weapon_icons.py --exe build_win/ProjectMidas.exe
```

The images, packet pack and manifest remain ignored. Commit generator and
renderer code, not ROM-derived outputs. Delete the matching native cache to
regenerate player icons with a changed exporter.

The exporter opens an isolated copy of the game in an ignored `build_win`
directory and leaves player settings and saves alone. The temporary copied executable is
removed after capture; diagnostic logs and images remain. It uses a fixed near-side
camera, the watch inventory's model origins, fixed part poses and metallic
reflection vectors, no scene fog,
and no screen overlays. Every model is captured twice on black and twice on
white at 2048 x 1536. Black/white matte recovery creates transparency before
Lanczos downsampling and consistent padding. Equal repeated pixels are required;
missing models or models touching the image border are generation errors.

`preview.png` shows every output on a dark background. `manifest.json` records
the ROM/exporter hashes, source pixel hashes, image hashes, capture dimensions
and processing versions. The RGB pixels retain original weapon colours, so gold
and silver weapons have their own images. Unarmed uses the actual hand/forearm model with the same wardrobe treatment as
first-person chopping. Six additional unarmed icons (IDs 32..37: blue, boiler,
rolled jungle, snow, tuxedo and Connery) follow the current player's live mission
cuff; Folder resolves the selected Bond just as the watch does. The complete
pack has 35 images and 38 indexed slots including aliases/unused entries.
This supersedes the earlier embedded 32-slot bootstrap pack.

To inspect a single model or reprocess a complete capture set:

```powershell
python scripts/port/generate_weapon_icons.py --item 8
python scripts/port/generate_weapon_icons.py --captures build_win/weapon-icons-EXAMPLE
```

For repeatable generation use the same ROM, executable, graphics driver/backend,
capture size and Python package versions. The pixel equality checks verify
repeatability on that rendering setup; byte identity across GPU vendors is not
promised. Processing and RGBA packing are deterministic for identical inputs.
Generation uses the existing OpenGL renderer; normal gameplay only draws a
cached texture for each visible icon and performs no additional model rendering.
