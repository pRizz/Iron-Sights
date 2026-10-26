# Bright Builds Rules in Iron Sights

The managed standards start at [standards/index.md](../../standards/index.md).
Read `AGENTS.md`, `AGENTS.bright-builds.md`, `standards-overrides.md`, then the
task-relevant standards. Source revision, managed paths and installation mode
are recorded in [the audit trail](../../bright-builds-rules.audit.md).

The adoption preserves existing gameplay, arm/render contracts, asset rules,
the fork inventory requirement and the gallery hook. The previous PR template
is preserved in [the project checklist](project_pr_checklist.md); use it with
the new managed PR template and `CONTRIBUTING.md`. `README.md` has canonical
uppercase spelling so managed updates also work on case-sensitive checkouts.

## Checks and dependencies

Install a stable Bun globally using the [official instructions](https://bun.sh/docs/installation).
For an existing npm-managed global installation:

```powershell
npm install --global bun@latest
bun --version
bun scripts/bright-builds-check.ts all
```

Bun is for development checks. The game remains a CMake C/C++ project; this
installation does not create a package manifest, lockfile or node_modules in
the repository. The managed CI workflow pins its own upstream-supported Bun
version. The current clone's `.githooks/pre-commit` runs the Bun checker before
the existing Python staged-gallery check. Other clones enable the hook using
`python scripts/port/hand_gallery.py install-hook` after installing Bun.

The initial file-size check found 195 existing findings at `cdd6b5f`.
`.bright-builds-rules-checks.tsv` contains 96 exact-file and five generated/vendor
directory exceptions, each with a reason. These preserve existing layouts while
keeping new source files checked. See `standards-overrides.md`; this is an
adoption baseline, not a completed whole-repository standards audit. Remove
exceptions as their files are modularized or no longer need them.

## Manual updates

Auto-update is disabled because every repository modification must update
`FORK_CHANGES.md` and pass local review. The checks workflow is enabled; the
unattended update workflow and credential requirements are not installed.

Run the official manager from Git Bash at the repository root:

```bash
curl -fsSL https://raw.githubusercontent.com/bright-builds-llc/bright-builds-rules/main/scripts/manage-downstream.sh -o build_win/bright-builds-manage.sh
bash build_win/bright-builds-manage.sh status --repo-root "$PWD"
bash build_win/bright-builds-manage.sh update --repo-root "$PWD" --ref main --auto-update disabled
```

Inspect a downloaded manager before running it. Use `update` only for a clean
managed install. If status reports drift, review the specific managed paths;
keep project customization in local guidance/overrides, not managed files.
Review the diff and recorded exact source commit, update `FORK_CHANGES.md`, and
run the standards and gallery checks before committing. Do not reset this fork's
upstream comparison baseline during a standards update.

The installer uses a pinned Markdown formatter for temporary managed candidates.
On Windows, a compatible isolated environment can be prepared without adding
global Python packages (use the Python interpreter used by the gallery tools):

```powershell
python -m venv build_win/bright-builds-format-env
./build_win/bright-builds-format-env/Scripts/python.exe -m pip install mdformat==1.0.0 mdformat-frontmatter==2.1.2 mdformat-gfm==1.0.0
```

Expose that formatter to the manager in Git Bash:

```bash
export PATH="$PWD/build_win/bright-builds-format-env/Scripts:$PATH"
```

Keep the repository's Markdown dialect and existing formatter configuration.
The installer formats only temporary managed candidates. New user-owned rules
documentation uses the same GFM/frontmatter formatter with `--wrap keep` and LF
endings. For preserved legacy `AGENTS.md`, `CONTRIBUTING.md`, README and inventory
content, check the scoped diff, local links and managed-block integrity; do not
reformat whole legacy files as an installation side effect.

The managed rules and checker retain their upstream MIT license in
`tools/BRIGHT_BUILDS_RULES_LICENSE.txt`. That license does not change the game's
or ROM-derived artwork's licensing. Local backup-first install snapshots stay
ignored under `.bright-builds-rules-backups/`.
