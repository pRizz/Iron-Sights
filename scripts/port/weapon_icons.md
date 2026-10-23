# Weapon HUD image generation

These transparent 512 x 160 PNGs are renders of the game's actual ROM-loaded
weapon models, including their original textures and material variants. The
runtime uses `weapon_icons.rle`, an RGBA packet pack embedded in the executable;
the PNGs, preview sheet and manifest are inspectable generation outputs.

From the repository root, with a GE_VIDEO build, your `baserom.u.z64`, Python,
Pillow and NumPy available:

```powershell
python -m pip install Pillow==12.3.0 numpy==2.3.5
python scripts/port/generate_weapon_icons.py --exe build_win/ProjectMidas.exe
```

Rebuild the game after generation. CMake tracks the pack and generated header,
so normal play needs no extra icon files beside the executable. A fresh checkout
can build the exporter without these generated files; icon mode uses weapon
names until generation and the following rebuild are complete. The images,
pack and manifest remain ignored, consistent with the repository's policy for
ROM-derived assets. Commit the generator and renderer code, not its outputs.

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
Regenerate the entire pack with the updated exporter, then rebuild; older
32-slot packs temporarily fall back to their original unarmed icon.

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
