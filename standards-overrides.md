# Standards Overrides

These are deliberate Iron Sights decisions alongside the managed corpus.
Owner: Iron Sights maintainers. Review date: 2026-10-24.

## Existing source size and native interfaces

Standard: `standards/core/code-shape.md` (file/function size and naming).

This adoption preserves the existing C/C++ engine, decompiled game interfaces,
ABI layouts and generated/vendor sources. It does not rename upstream symbols,
convert engine state to a different programming model, or split unrelated game
functions to install development rules. Apply the standards to new work and
review refactoring opportunities when touching existing code, with native tests
and render verification where relevant.

The initial checker found 195 over-limit files at `cdd6b5f`. The user-owned
`.bright-builds-rules-checks.tsv` records 96 exact-file exceptions and five
generated/vendor directories (`assets/`, `port/fast3d/glad/`, `port/generated/`,
`tools/gzipsrc/`, `tools/ido5.3_recomp/`). The latter preserve generated data and
third-party layout. These are adoption exceptions, not evidence of a full
standards audit or a reason to grow large files casually. New files outside
those generated/vendor directories remain subject to the 628-line check.
Prune exceptions when files are split or no longer need them; keep the managed
checker unchanged.

## Verification and generated references

Standard: `standards/core/verification.md`.

The existing pre-commit hook owns both Bun standards checks and staged gallery
freshness. Manual runs for installation, debugging and review are permitted;
the hook may rerun them when committing without another confirmation. The
gallery hook regenerates only when rendering inputs change, stops for review
and staging, and rejects partially staged render inputs. Rules/docs maintenance
does not require rendering or unrelated gameplay tests. Rendering changes still
follow `scripts/port/arm_rendering.md` and `scripts/port/hand_gallery.md`.

Markdown installation preserves legacy user-authored regions. The scoped
formatter and preservation-check contract is documented in
`scripts/port/bright_builds_rules.md`; adoption does not authorize a whole-file
formatting migration of the README, contribution guide, inventory or AGENTS.

## Manual managed-file updates

Standard: Bright Builds adoption/update workflow.

Auto-update is explicitly disabled. Every repository change must update
`FORK_CHANGES.md`, and rendering references must match staged inputs. Unattended
managed-file commits do not maintain those local contracts. Use the documented
manual status/update flow, review changes, update the inventory and run checks.

## Direct-to-main delivery

Standard: inherited contribution/PR guidance and repository delivery workflow.

The owner requested direct-to-main delivery on 2026-10-25. For authorized work
in this fork, sync first, implement and validate, then commit and push directly
to `main`. Feature branches and PRs are not the default. Use them when explicitly
requested or needed for an external requirement, including upstream contribution
or branch protection. The managed PR template and preserved checklist apply only
when a PR is actually needed. Keep normal verification, inventory, provenance and
deployment checks; preserve unrelated work and do not bypass protections or
force-push. This preference does not change the managed upstream files.

## Identity placement and optional skills

Standard: the sidecar's owner-specific skill default and frontend identity guidance.

The installer-managed OpenLinks README badge is the identity placement for this
adoption. Keep Iron Sights, its download link and upstream/game attribution
primary. Do not add profile branding to the HUD, watch or other in-game surfaces
as a side effect of standards maintenance. The `openlinks-identity-presence`
skill may be used when available for an explicitly requested identity task;
its absence does not block ordinary code/docs maintenance or require global
skill installation. Preserve existing About/build-provenance behavior.

## Immersive browser entrypoint

Standard: `standards/core/frontend-ui.md` (normal visible product chrome).

The user explicitly requested a canvas-only initial room on 2026-10-24. The
retained HTML launcher, source, provenance, maintainer link and model credits
remain accessible through F2 and an in-scene controls affordance rather than a
permanent initial header/footer. Keep accessible controls and error recovery.
See `scripts/port/web_room_plan.md` for the accepted experience and milestones.

On 2026-10-27 the user clarified that Import Save and Export Save belong in
GoldenEye's four-campaign Files screen, alongside Copy, Erase, Options and Exit
Game, as two direct buttons in a two-row/three-column web action area. This
supersedes the earlier room toolbar interpretation; the opening room returns to
its canvas-only presentation. Source, provenance and credits remain available
through F2. Import confirms the full backup and stages it for a fresh runtime;
the live game's cached EEPROM is never replaced.
