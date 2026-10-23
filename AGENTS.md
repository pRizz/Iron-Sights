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
  document their generator and regeneration instructions instead.

Before handing off work or preparing a commit/PR, compare the changes with the
inventory and ensure every changed area is covered. Updating the inventory is
part of completing the requested work, not a separate optional follow-up.
