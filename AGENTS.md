# Repository instructions

These instructions apply throughout this repository.

## Maintain the fork inventory

Whenever you add, modify, remove, or replace something in this repository,
update [FORK_CHANGES.md](FORK_CHANGES.md) as part of the same change. This applies
to gameplay, rendering, settings, fixes, documentation, assets and asset
generation, tests, tooling, build configuration, and repository rules.

- Read the relevant inventory entries before changing an existing feature.
- Update an existing entry when refining or fixing that feature. Add a new,
  stable `FORK-NNN` entry for a separate addition, using the next unused number.
- Describe the current behavior, why it differs from the inherited upstream
  baseline, relevant settings/defaults, main source paths, and validation.
- Include documentation-only and maintenance changes. Inventory bookkeeping
  itself can update its existing maintenance entry; it needs no recursive entry.
- Record removals and replacements, including what superseded an earlier
  implementation. Do not leave removed behavior described as current.
- Distinguish inherited upstream behavior from fork work. If an upstream merge
  changes ownership or makes an entry obsolete, explain that in the inventory.
  Change the comparison baseline only deliberately, with its old and new refs
  documented; do not silently reset it to the latest upstream commit.
- Keep entries factual. Do not invent commit IDs, claim unrun checks passed, or
  describe local work as released or pushed. A Git commit reference is useful
  evidence when available, but is not required before committing the change.
- Record implementation and workflow changes, rather than each local build,
  capture, or regeneration run. Keep ROM-derived images, packs, models and other
  generated game data out of Git, as required by CONTRIBUTING.md and .gitignore;
  document their generator and regeneration instructions instead. The user has
  explicitly authorized the rendered HTML/PNG reference gallery under
  `review/hand-gallery`; this exception excludes ROMs, extracted models/textures,
  raw capture files, executable copies and binary caches.

Before handing off work or preparing a commit/PR, compare the changes with the
inventory and ensure every changed area is covered. Updating the inventory is
part of completing the requested work, not a separate optional follow-up.

## First-person hand and arm invariants

Before changing hands, arms, wardrobe materials or their render paths, read
[the arm rendering contracts](scripts/port/arm_rendering.md). Preserve native
pistol hand vertices, UVs, colors, retained triangles and original material/render
setup unless the user explicitly requests a hand redesign. Fit the added forearm
to that protected hand. Rifle forearm bulk must not alter the grip hand, wrist
join or watch. Keep stock hand rendering separate from generated-arm shading.
Keep native pistol hands and added sleeves in the same weapon depth layer;
preserving hand data alone does not prevent skin from painting over a cuff.
Match generated wrist skin to the native weapon's UVs and vertex shade; change
only skin-owned attributes, preserving clothing/watch mapping and geometry.

Run the ROM-free wardrobe and ADS regressions for these changes. When the local
ROM is available, compare native pistols with the stock audit reference and
review intermediate ADS poses, both hands and affected wardrobes. Generated
reference images stay local unless published by the versioned review-gallery
generator. Deliberate contract changes must
update the guide, regression checks and fork inventory together.

## Render review gallery

Keep `review/hand-gallery` fresh for changes to rendering inputs. Use
`python scripts/port/hand_gallery.py generate`, review changed images, and stage
the gallery alongside its source changes. `verify` checks freshness and image
integrity without a ROM. Install the clone's pre-commit hook with `install-hook`;
the hook never stages files automatically and rejects partially staged render
inputs. See `scripts/port/hand_gallery.md` for the capture and hook workflow.

## Local build artifacts

Do not keep previous executables as rebuild backups. The user accepts losing
save-state compatibility when the executable changes. Obsolete local game
executables and suspend images associated with those old builds may be purged
without another confirmation; there is no need to archive them first.

Keep the current main executable. Match suspend images to obsolete executable
build fingerprints before purging them; leave unknown or current-build images
alone. This permission concerns build artifacts and suspend images, not normal
campaign saves (`Data/Saves`), profiles, settings, source files or other user data.
Audit executables are temporary: remove the copied executable when the audit
finishes, while keeping useful diagnostic logs and captures.
