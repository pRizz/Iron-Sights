# Contributing to Project Midas

## Project Midas ADS fork: change inventory

For work in this fork, update [FORK_CHANGES.md](FORK_CHANGES.md) in the same change
whenever you add, modify, remove or replace repository content. This includes
code, settings, fixes, documentation, tooling, tests, build configuration and
repository rules. Update an existing entry for refinements, or add a stable
`FORK-NNN` entry for a new area. Describe behavior/defaults, the difference from
the inherited upstream baseline, main source paths and validation. Record
replacements and removals so the inventory stays accurate. Generated ROM-derived
outputs stay untracked; inventory their generator and workflow instead. The
explicit exception is the rendered review gallery under `review/hand-gallery`:
its HTML, manifest and generated PNG references are versioned for visual review.
This exception does not allow ROMs, extracted models/textures or binary caches.

The repository-wide agent instructions are in [AGENTS.md](AGENTS.md). The guidance
below is inherited from upstream; its upstream project name and roadmap describe
that project rather than additions made by this fork.

This fork also adopts [Bright Builds Rules](standards/index.md), with deliberate
local exceptions in [standards-overrides.md](standards-overrides.md). Run
`bun scripts/bright-builds-check.ts all` for development standards checks; the
existing pre-commit hook also checks staged render references. Use the managed
PR template with [the preserved project checklist](scripts/port/project_pr_checklist.md).
See [installation and maintenance](scripts/port/bright_builds_rules.md).

For authorized work in this fork, the default delivery is a validated commit
pushed directly to `main`. Do not create a feature branch or pull request by
default. Use a PR when the maintainer asks or an external requirement needs one;
the template and preserved checklist apply in that case. Keep normal checks and
fork inventory updates, preserve unrelated work and respect branch protections.

Project Midas is a PC port of GE built with care: matching the original game where it matters, with every change tested in game. Contributions are welcome when they meet that bar. Please contribute here rather than starting a fork, so the work stays in one place.

## Already in progress

These are being worked on now. If you want to help with one, open an issue first so effort isn't duplicated.

| Area | Status |
| --- | --- |
| Co-op | In development |
| Netplay | In development |
| Mod loading (xdelta patches) | In development |
| Lua modding | Planned |

## Small changes: Lua mods

Once Lua modding is developed and released, minor changes (tweaks to weapons, rules, HUD or other gameplay details) should be made as Lua mods rather than pull requests. That keeps the port itself stable and lets players choose the changes they want. Until then, open an issue to discuss a small change before sending a pull request.

## What a good pull request looks like

1. **One focused change.** Fix one bug or add one feature per PR. Don't mix in unrelated cleanup.
2. **Builds on both platforms.** CI builds Linux and Windows; it must pass.
3. **Tested in game.** Say what you tested and on which level or menu. "It compiles" is not enough.
4. **Matches the code around it.** Keep the decompiled names. Keep comments short: what the code does and why, in at most two plain sentences.
5. **No game assets.** Never commit ROM data or extracted assets. Rendered PNG
   references in `review/hand-gallery` are the fork's limited review exception.

## AI-assisted contributions

AI tools are fine to use for code. Low-effort AI output is not.

- **Code only.** AI-generated images, textures, models, sound or music will never be accepted. Art and audio must be made by people.
- Say in the PR that AI was used (the PR template has a box for it).
- You must understand every line you submit and be able to answer questions about it.
- You must have run and tested the change yourself.
- Large unreviewed or untested AI changes will be closed without review.

## Forks

The code is MIT licensed, so you are free to fork it. If you do, please:

- give your fork its own name, not "Project Midas",
- link back to this repository,
- send fixes back here as pull requests so everyone benefits.

## License

By contributing you agree that your contribution is licensed under the MIT license in [LICENSE](LICENSE). That license covers the port code only; see its "Scope" section.

<!-- bright-builds-rules-contributing:begin -->

# Bright Builds Contribution Defaults

This managed block is owned upstream by `bright-builds-rules`. If this block needs a fix, open an upstream PR or issue instead of editing the managed text in a downstream repo. Keep repo-local contribution guidance outside this managed block.

## Default contribution expectations

- Treat `AGENTS.md` as the entrypoint for repo-local instructions, not the complete Bright Builds Rules spec.
- Before plan, review, implementation, or audit work, read local `AGENTS.md`, `AGENTS.bright-builds.md`, `standards-overrides.md` when present, and the local managed standards pages relevant to the task; if that has not happened yet, stop and load them before continuing.
- Follow the local `AGENTS.md`.
- Use the local managed standards pages as the canonical reference, with source provenance recorded in `AGENTS.bright-builds.md` and `bright-builds-rules.audit.md`.
- Prefer simple, root-cause fixes over broad rewrites.
- Document repo-specific exceptions in `standards-overrides.md`.

## Code expectations

- Keep business logic in a functional core when practical.
- Prefer early returns and shallow control flow.
- Prefix internal nullable or optional names with `maybe`, including functions, bindings, and internal fields, and use `MaybeX` aliases only when they materially clarify a repeated nullable surface.
- Split oversized functions and files into sensible units.
- Do not hide substantial foreign-language logic inside strings; keep workflow and automation config thin, move scripts, queries, and similar artifacts into repo-owned or language-aware files, make checked-in scripts rerunnable when sensible, and have them leave breadcrumb-heavy logs and summaries in a repo-defined gitignored location.
- Parse boundary input into domain types when that removes repeated validation.
- Apply any relevant language-specific guidance from the local managed standards.

## Verification expectations

- Before substantive implementation work, sync first: fetch remote state before editing; if the current branch tracks an upstream and the worktree is clean, prefer rebasing onto the latest upstream or the repo's equivalent sync path, such as `git pull --rebase` when local guidance uses it; if a worktree starts detached, assume the repo default branch, often `main`; resolve any sync conflicts before proceeding, then run the repo's normal bootstrap or dependency-sync step when dependencies or tools may be stale.
- Before formatting Markdown, inspect repo-local guidance and formatter configuration, require the configured syntax extensions, preserve existing configuration, use repo-owned setup or migration commands when provided, and run check mode before an authorized, scoped write. Never fall back to bare `mdformat` when required plugins are unavailable.
- Before committing, run the relevant repo-native verification steps for the changed paths, including repository-compatible Markdown or shell formatter checks when supported tools are already available and local guidance does not define a clearer workflow, and do not commit if they fail.
- Prefer a repo-owned verify/check/validate/ci command when it exists over reconstructing tool commands by hand.
- Run `bun scripts/bright-builds-check.ts all` when the managed starter checker is installed. Use `.bright-builds-rules-checks.tsv` only for deliberate exact-file exceptions or trailing-slash `file-lengths` directory exceptions with a required reason; the file is user-owned and must not be generated by Bright Builds.
- Heavy integration, end-to-end, or external-service suites may stay pre-push or CI-only when local guidance or `standards-overrides.md` documents that choice.
- If hooks appear to own verification here and the local workflow is unclear, clarify whether the repo expects hooks, manual checks, or both.

## Test expectations

- Unit test pure code and business logic.
- Keep each unit test focused on one concept.
- Use explicit Arrange, Act, Assert sections unless the structure is truly obvious.

## Pull request expectations

- Explain the behavior change, not just the code movement.
- Call out any new exceptions to the standards.
- When plan, review, or audit work relied on these standards, briefly name the local guidance, sidecar, overrides, or standards pages that materially informed the work.
- Include verification evidence for the changed paths.
- Note any residual risks or follow-up work.

<!-- bright-builds-rules-contributing:end -->
