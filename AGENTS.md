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

The user may play/debug the main Windows executable during development. Before
building or replacing an executable, check running game processes and their
resolved executable paths. Use a separate CMake binary directory and isolated
runtime directory when a build or audit could collide with a running instance
(for example `build_win/hd-prototype`). Keep the main executable, settings,
saves, logs and crash output untouched. Do not stop a user's game to make a
build succeed. Audit only task-owned processes and remove only their temporary
executable copies after they exit. This also applies to gallery regeneration;
pass its isolated executable rather than rebuilding an active player binary.

## Repo-Local Guidance

- Minimize changes in upstream-owned files. Put new fork behavior in focused
  files under existing fork/platform directories, connected through small,
  explicit hooks. Preserve upstream layout and names; avoid unrelated formatting
  and broad refactors. Extract existing fork code when the boundary is clear and
  verification can establish equivalent behavior. Do not force separation that
  adds needless indirection or weakens rendering/ABI contracts. Document shared
  hooks and overlapping upstream behavior in `FORK_CHANGES.md` for later merges.
- Default delivery is direct to `main`: sync remote state, complete the requested
  work, run the relevant checks, then commit and push to `main`. Do not create a
  feature branch or pull request by default. Use a PR only when the user requests
  one or an external requirement makes it necessary, such as an upstream
  contribution or branch protection. Preserve unrelated work and never bypass
  protections or force-push to satisfy this preference.
- Web target work follows the accepted [plan](scripts/port/web_target_plan.md) and
  [milestone progress](scripts/port/web_target_progress.md). Read both before
  resuming; update only the relevant stable milestone block with changes,
  verification evidence, blockers and the next action before handing off.
- The immersive browser room follows its accepted
  [experience plan](scripts/port/web_room_plan.md) and WEB-007 through WEB-010 in
  the same progress file. Keep the game canvas/storage lifecycle intact; record
  model licenses and distinguish procedural stand-ins from approved final props.
- This is a C/C++ CMake game port. Bun runs development standards checks and the
  browser tooling subpackage under `web/`. Use `bun install --frozen-lockfile`
  inside `web/` for its tests/audit tooling; the repository root has no Bun package.
- `.githooks/pre-commit` runs `bun scripts/bright-builds-check.ts all`, then staged gallery verification. Manual runs for installation, debugging or review are allowed; a commit hook rerun needs no separate confirmation.
- Source changes use the relevant `scripts/port/test_*.sh` checks. Wardrobe/render changes require the arm contracts and gallery workflow above. Documentation/rules-only changes need no game rebuild or render regeneration.
- Existing long files have a documented adoption baseline in `.bright-builds-rules-checks.tsv`; new source files remain checked. See `standards-overrides.md` before changing an exempt file.
- When a PR is needed, use the managed PR template together with [the preserved project checklist](scripts/port/project_pr_checklist.md), `CONTRIBUTING.md` and the fork inventory requirement.
- Standards maintenance and Windows setup: [Bright Builds workflow](scripts/port/bright_builds_rules.md). Managed file updates are reviewed manually and update `FORK_CHANGES.md`; unattended updates are disabled.

<!-- bright-builds-rules-managed:begin -->

# Bright Builds Rules

`AGENTS.md` is the entrypoint for repo-local instructions, not the complete Bright Builds Rules specification.

This managed block is owned upstream by `bright-builds-rules`. If this block needs a fix, open an upstream PR or issue instead of editing the managed text in a downstream repo. Keep downstream-specific instructions outside this managed block.

Before plan, review, implementation, or audit work:

1. Read the repo-local instructions in `AGENTS.md`, including any `## Repo-Local Guidance` section and any instructions outside this managed block.
1. Read `AGENTS.bright-builds.md`.
1. Read `standards-overrides.md` when present.
1. Read the local managed standards pages under `standards/` relevant to the task.
1. If you have not done that yet, stop and load those sources before continuing.

Use this routing map when deciding what to load next:

- For repo-specific commands, prerequisites, generated-file ownership, CI-only suites, or recurring workflow facts, use the local `AGENTS.md`, especially `## Repo-Local Guidance`.
- For the Bright Builds default workflow and high-signal cross-cutting rules used in most tasks, use `AGENTS.bright-builds.md`.
- For deliberate repo-specific exceptions to the Bright Builds defaults, use `standards-overrides.md`.
- To choose the right managed standards page, start with the local Bright Builds entrypoint `standards/index.md`.
- For business-logic structure, domain modeling, and functional-core versus imperative-shell decisions, use the managed standards page `standards/core/architecture.md`.
- For control flow, naming, function/file size, and readability rules, use the managed standards page `standards/core/code-shape.md`.
- For frontend visual defaults, theme defaults, and dark-mode decisions, use the managed standards page `standards/core/frontend-ui.md`.
- For sync, bootstrap, and pre-commit verification rules, use the managed standards page `standards/core/verification.md`.
- For the managed starter checks, run `bun scripts/bright-builds-check.ts all`; use its `--help` output for check-specific commands, exact-file exceptions, and trailing-slash `file-lengths` directory exceptions.
- For unit-test expectations, use the managed standards page `standards/core/testing.md`.
- For Rust or TypeScript/JavaScript-specific rules, use the matching managed standards page under `standards/languages/`.
- For TypeScript/JavaScript frontend framework and UI-library defaults, use `standards/languages/typescript-javascript.md`.
- Keep recurring repo-specific workflow facts, commands, and links in a `## Repo-Local Guidance` section elsewhere in this file.
- Record deliberate repo-specific exceptions and override decisions in `standards-overrides.md`.
- If instructions elsewhere in `AGENTS.md` conflict with `AGENTS.bright-builds.md`, follow the repo-local instructions and treat them as an explicit local exception.

<!-- bright-builds-rules-managed:end -->
