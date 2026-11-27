#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work=$(mktemp -d /tmp/midas-harness-widths.XXXXXX)
trap 'rm -rf "$work"' EXIT

# A stand-in compiler: FAKE_M32=reject refuses -m32, FAKE_MACHINE is what
# -dumpmachine prints (x86_64-linux-gnu unless set), FAKE_POINTER_BYTES is what
# its preprocessor reports as __SIZEOF_POINTER__, FAKE_DM_FAIL=1 makes the
# macro probe fail with a message and FAKE_DUMPMACHINE_FAIL=1 makes
# -dumpmachine fail with one.
cat >"$work/gcc" <<'FAKE'
#!/usr/bin/env bash
for arg in "$@"; do
    if [[ $arg == -m32 && ${FAKE_M32:-accept} == reject ]]; then exit 1; fi
    if [[ $arg == -dumpmachine && -n ${FAKE_DUMPMACHINE_FAIL:-} ]]; then echo "fake: no machine" >&2; exit 1; fi
    if [[ $arg == -dumpmachine ]]; then echo "${FAKE_MACHINE:-x86_64-linux-gnu}"; exit 0; fi
    if [[ $arg == -dM && -n ${FAKE_DM_FAIL:-} ]]; then echo "fake: cannot run cc1" >&2; exit 1; fi
    if [[ $arg == -dM ]]; then
        echo "#define __SIZEOF_POINTER__ ${FAKE_POINTER_BYTES:-8}"
        # A real compiler prints hundreds of macros after this one.
        if [[ -n ${FAKE_NOISY:-} ]]; then yes '#define FILLER 1' | head -n 20000 || exit $?; fi
    fi
done
exit 0
FAKE
chmod +x "$work/gcc"

failures=0
expect() {
    local name=$1 expected=$2 actual=$3
    if [[ $actual == "$expected" ]]; then
        echo "ok: $name"
    else
        echo "FAIL: $name: expected [$expected], got [$actual]"
        failures=$((failures + 1))
    fi
}

check_widths() {
    local name=$1 expected=$2
    shift 2
    local actual
    actual=$(env PATH="$work:$PATH" CC=gcc "$@" \
        bash -c 'set -euo pipefail; source scripts/port/harness_widths.sh && harness_widths' 2>/dev/null | tr '\n' ' ' || true)
    expect "$name" "$expected" "$actual"
}

# Arrange / Act / Assert for each case: the environment, the call, the printed widths.
check_widths "an aarch64 compiler without -m32 runs 64 only" "64 " FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=8
check_widths "a compiler with -m32 on a 64-bit host runs 32 then 64" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8
check_widths "a 32-bit host runs 32 only" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=4
check_widths "MIDAS_HARNESS_WIDTHS=32 restricts to 32" "32 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32
check_widths "MIDAS_HARNESS_WIDTHS=64 restricts to 64" "64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=64
check_widths "a long macro list does not hide 64 under pipefail" "32 64 " FAKE_M32=accept FAKE_POINTER_BYTES=8 FAKE_NOISY=1
check_widths "a request for an unavailable width prints nothing" "" FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32
check_widths "an x86_64 compiler without -m32 fails and prints no width" "" FAKE_M32=reject FAKE_POINTER_BYTES=8
check_widths "an x86_64 compiler without -m32 runs 64 when restricted to it" "64 " FAKE_M32=reject FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=64
# Apple clang accepts -m32 for arm64 and can build nothing with it (MAC-001).
check_widths "an Apple compiler that accepts -m32 runs 64 only" "64 " FAKE_M32=accept FAKE_MACHINE=arm64-apple-darwin27.2.0 FAKE_POINTER_BYTES=8
check_widths "an Apple compiler asked only for 32 prints nothing" "" FAKE_M32=accept FAKE_MACHINE=arm64-apple-darwin27.2.0 FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS=32

# Runs harness_widths with a stand-in compiler and prints its exit status and stderr on one line.
widths_failure() {
    local status=0 err
    err=$(env PATH="$work:$PATH" CC=gcc "$@" \
        bash -c 'set -euo pipefail; source scripts/port/harness_widths.sh && harness_widths' 2>&1 >/dev/null) || status=$?
    echo "status=$status $err"
}
expect_failure() {
    local name=$1 pattern=$2
    shift 2
    local actual
    actual=$(widths_failure "$@")
    if [[ $actual == status=[1-9]*"$pattern"* ]]; then
        echo "ok: $name"
    else
        echo "FAIL: $name: expected a failure mentioning [$pattern], got [$actual]"
        failures=$((failures + 1))
    fi
}

# Arrange / Act / Assert: the environment, the call, the failure and what it says.
expect_failure "no usable width fails" "cannot build any" FAKE_M32=reject FAKE_MACHINE=aarch64-linux-gnu FAKE_POINTER_BYTES=4
expect_failure "an x86_64 compiler without -m32 names the missing 32-bit toolchain" "32-bit toolchain is missing" FAKE_M32=reject FAKE_POINTER_BYTES=8
expect_failure "an i686 compiler without -m32 names the missing 32-bit toolchain" "32-bit toolchain is missing" FAKE_M32=reject FAKE_MACHINE=i686-linux-gnu FAKE_POINTER_BYTES=4
expect_failure "the missing toolchain message says how to restrict the run" "MIDAS_HARNESS_WIDTHS=64" FAKE_M32=reject FAKE_POINTER_BYTES=8
expect_failure "an unknown width is refused" "unknown width '16'" FAKE_M32=accept FAKE_POINTER_BYTES=8 MIDAS_HARNESS_WIDTHS="16"
expect_failure "a failing macro probe fails and shows the compiler's message" "fake: cannot run cc1" FAKE_M32=accept FAKE_POINTER_BYTES=8 FAKE_DM_FAIL=1
expect_failure "a failing -dumpmachine fails and shows the compiler's message" "fake: no machine" FAKE_M32=reject FAKE_POINTER_BYTES=8 FAKE_DUMPMACHINE_FAIL=1

# Arrange
source scripts/port/harness_widths.sh
# Act and Assert: at the close of LP64-003 no harness half is pending, so every
# harness runs at both widths (the plan's Review Focus 6).
expect "the shipped HARNESS_PENDING is empty" "" "${HARNESS_PENDING[*]}"
# Arrange
HARNESS_PENDING=("test_wardrobe 64" "x 32")
# Act and Assert
if harness_pending test_wardrobe 64; then echo "ok: test_wardrobe 64 is pending"; else echo "FAIL: test_wardrobe 64 is pending"; failures=$((failures + 1)); fi
if harness_pending test_dyn 64; then echo "FAIL: test_dyn 64 is not pending"; failures=$((failures + 1)); else echo "ok: test_dyn 64 is not pending"; fi
if harness_pending test_wardrobe 32; then echo "FAIL: test_wardrobe 32 is not pending"; failures=$((failures + 1)); else echo "ok: test_wardrobe 32 is not pending"; fi

expect "harness_flags 32 prints -m32" "-m32" "$(harness_flags 32)"
expect "harness_flags 32 adds the harness's own flags" "-m32 -msse2 -mfpmath=sse" "$(harness_flags 32 -msse2 -mfpmath=sse)"
expect "harness_flags 64 prints nothing" "" "$(harness_flags 64 -msse2 -mfpmath=sse)"

# A loop over "$(harness_widths)" runs zero times when the helper fails, and the
# harness passes having run nothing: a harness takes widths=$(harness_widths) on
# its own line so set -e stops it. Arrange / Act: the harnesses that loop directly.
direct_loops=$(grep -lE 'in[[:space:]]+"?\$\(harness_widths\)' scripts/port/*.sh | tr '\n' ' ' || true)
# Assert
expect "no harness loops directly over \$(harness_widths)" "" "$direct_loops"

# The list check (LP64-006 Task 12). Every scripts/port/test_*.sh that compiles
# code under src/ or port/ runs it at both widths, and every harness runs in CI;
# a harness that cannot is on one of these lists as "NAME: REASON".
#
# Harnesses that compile src/ or port/ code without harness_widths.sh.
HARNESS_WIDTH_EXEMPT=(
    "test_pointer_bits: reads the flags CMake records for each width; it compiles only canaries"
    "test_web_touch: compiles port/web/touch.c with emcc, the browser's compiler"
    "test_web_menu_pointer: tests port/web/menu_pointer.h, which only the browser build compiles"
    "test_headless_configure: compiles one file of CMake's default (32-bit) headless tree; CI's build/lp64/gate configures the 64-bit tree"
    "test_enhanced_fonts: port/fast3d C++ that includes no header from src/ or include/: no game record"
    "test_hd_textures: port/fast3d C++ that includes no header from src/ or include/: no game record"
    "test_open_fonts: port/fast3d C++ that includes no header from src/ or include/: no game record"
    "test_room_seams: port/fast3d/gfx_seam_grow.h, float and fixed-width integer maths with no pointer or record"
)
# Harnesses that no workflow runs.
HARNESS_WORKFLOW_EXEMPT=(
    "test_macos_host: runs only on macOS; named in the macOS job from MAC-001 Task 10"
)

# Prints FILE without its comment lines.
harness_code() {
    grep -v '^[[:space:]]*#' "$1" || [[ $? -eq 1 ]]
}

# True when the harness calls a compiler (or CMake) on code under src/ or
# port/: named on its own lines, or included by a driver it compiles.
compiles_game_code() {
    local root=$1 harness=$2 code driver drivers
    code=$(harness_code "$harness")
    if ! grep -qE '\$\{?(CC|CXX|cc|compiler)\b|(^|[[:space:]"(])(gcc|g\+\+|cc|c\+\+|clang|emcc|cmake)[[:space:]]' <<<"$code"; then
        return 1
    fi
    if grep -qE '(^|[^[:alnum:]_/.])(src|port)/|-I(src|port)\b' <<<"$code"; then
        return 0
    fi
    drivers=$(grep -oE 'scripts/port/[[:alnum:]_]+\.(c|cpp)' <<<"$code" || [[ $? -eq 1 ]])
    for driver in $drivers; do
        if [[ -f $root/$driver ]] && grep -qE '#include[[:space:]]+"(\.\./)*(src|port)/' "$root/$driver"; then
            return 0
        fi
    done
    return 1
}

# True when the harness runs its body at every width harness_widths.sh offers,
# or at the compiler's own width and then at 32 (build_and_run, then with -m32).
runs_both_widths() {
    local code
    code=$(harness_code "$1")
    if grep -qE '^[[:space:]]*source scripts/port/harness_widths\.sh' <<<"$code" &&
        grep -qF '$(harness_widths)' <<<"$code"; then
        return 0
    fi
    grep -qE '^[[:space:]]*build_and_run[[:space:]]*$' <<<"$code" &&
        grep -qE '^[[:space:]]*build_and_run -m32' <<<"$code"
}

# Prints the reason NAME has on the list named LIST, or fails when it has none.
exempt_reason() {
    local -n list=$1
    local entry
    for entry in "${list[@]}"; do
        if [[ ${entry%%:*} == "$2" ]]; then
            echo "${entry#*: }"
            return 0
        fi
    done
    return 1
}

# Prints one line for each problem in the harnesses and workflows under ROOT,
# and nothing when every harness is covered.
harness_list_problems() {
    local root=$1 harness name list entry lines
    local -a harnesses workflows
    local -A named=()
    shopt -s nullglob
    harnesses=("$root"/scripts/port/test_*.sh)
    workflows=("$root"/.github/workflows/*.yml)
    shopt -u nullglob
    if [[ ${#workflows[@]} -eq 0 ]]; then
        echo "no workflow in $root/.github/workflows"
        return 0
    fi
    for harness in "${harnesses[@]}"; do
        name=$(basename "$harness" .sh)
        # A --web line runs the browser build only, not the harness's widths.
        lines=$(grep -hF "scripts/port/$name.sh" "${workflows[@]}" || [[ $? -eq 1 ]])
        # A commented-out line runs nothing.
        lines=$(grep -vE '^[[:space:]]*#' <<<"$lines" || [[ $? -eq 1 ]])
        if grep -vqF "scripts/port/$name.sh --web" <<<"$lines" && [[ -n $lines ]]; then
            named[$name]=1
        fi
    done
    for harness in "${harnesses[@]}"; do
        name=$(basename "$harness" .sh)
        if compiles_game_code "$root" "$harness" && ! runs_both_widths "$harness" &&
            ! exempt_reason HARNESS_WIDTH_EXEMPT "$name" >/dev/null; then
            echo "$name.sh compiles src/ or port/ code at one width: source harness_widths.sh, or add it to HARNESS_WIDTH_EXEMPT with its reason"
        fi
        if [[ -z ${named[$name]:-} ]] && ! run_by_named "$root" "$name" named &&
            ! exempt_reason HARNESS_WORKFLOW_EXEMPT "$name" >/dev/null; then
            echo "$name.sh is run by no workflow: name it in .github/workflows, or add it to HARNESS_WORKFLOW_EXEMPT with its reason"
        fi
    done
    for list in HARNESS_WIDTH_EXEMPT HARNESS_WORKFLOW_EXEMPT; do
        local -n entries=$list
        for entry in "${entries[@]}"; do
            if [[ ! -f $root/scripts/port/${entry%%:*}.sh ]]; then
                echo "$list names ${entry%%:*}, which is not a harness in scripts/port"
            fi
            if [[ $entry != *': '?* ]]; then
                echo "$list entry '$entry' gives no reason"
            fi
        done
        unset -n entries
    done
    return 0
}

# True when a harness that a workflow names runs scripts/port/NAME.sh itself.
run_by_named() {
    local root=$1 name=$2 parent code
    local -n by_workflow=$3
    for parent in "${!by_workflow[@]}"; do
        code=$(harness_code "$root/scripts/port/$parent.sh")
        if grep -qE "^[[:space:]]*(bash )?scripts/port/$name\.sh\b" <<<"$code"; then
            return 0
        fi
    done
    return 1
}

# Arrange / Act: the repository's own harnesses and workflows.
list_problems=$(harness_list_problems .)
# Assert
expect "every harness runs at both widths and in a workflow, or is listed" "" "$list_problems"

# Fixture trees for the list check: a scripts/port folder and one workflow.
new_fixture() {
    local root=$work/fixture-$1
    mkdir -p "$root/scripts/port" "$root/.github/workflows"
    : >"$root/.github/workflows/ci.yml"
    echo "$root"
}
# Writes ROOT/scripts/port/NAME.sh holding BODY; with "named", the workflow runs it.
write_harness() {
    local root=$1 name=$2 body=$3 maybe_named=${4:-}
    printf '#!/usr/bin/env bash\nset -euo pipefail\n%s\n' "$body" >"$root/scripts/port/$name.sh"
    if [[ $maybe_named == named ]]; then
        echo "          bash scripts/port/$name.sh" >>"$root/.github/workflows/ci.yml"
    fi
}
one_width='"${CC:-gcc}" -Iport/src scripts/port/test_x.c -o "$bin"'
both_widths='source scripts/port/harness_widths.sh
widths=$(harness_widths)
for width in $widths; do "${CC:-gcc}" $(harness_flags "$width") -Iport/src scripts/port/test_x.c -o "$bin"; done'
own_then_32='build_and_run() { "${CC:-gcc}" "$@" -Isrc scripts/port/test_x.c -o "$bin"; }
build_and_run
build_and_run -m32 -msse2 -mfpmath=sse'
one_message="test_one.sh compiles src/ or port/ code at one width: source harness_widths.sh, or add it to HARNESS_WIDTH_EXEMPT with its reason"
# Runs the list check on ROOT with only the list entries given (none if empty).
fixture_problems() {
    local root=$1 width_entry=${2:-} workflow_entry=${3:-}
    HARNESS_WIDTH_EXEMPT=()
    HARNESS_WORKFLOW_EXEMPT=()
    if [[ -n $width_entry ]]; then HARNESS_WIDTH_EXEMPT=("$width_entry"); fi
    if [[ -n $workflow_entry ]]; then HARNESS_WORKFLOW_EXEMPT=("$workflow_entry"); fi
    harness_list_problems "$root"
}
unnamed_message="test_both.sh is run by no workflow: name it in .github/workflows, or add it to HARNESS_WORKFLOW_EXEMPT with its reason"

# Arrange
root=$(new_fixture one-width)
write_harness "$root" test_one "$one_width" named
# Act and Assert
expect "a harness at one width fails the list check" "$one_message" "$(fixture_problems "$root")"
# Act and Assert
expect "a listed harness at one width passes" "" \
    "$(fixture_problems "$root" "test_one: a reason")"

# Arrange
root=$(new_fixture unnamed)
write_harness "$root" test_both "$both_widths"
# Act and Assert
expect "a harness in no workflow fails the list check" "$unnamed_message" "$(fixture_problems "$root")"
# Act and Assert
expect "a listed harness in no workflow passes" "" \
    "$(fixture_problems "$root" "" "test_both: a reason")"

# Arrange
root=$(new_fixture web-only)
write_harness "$root" test_both "$both_widths"
echo "          docker run image bash scripts/port/test_both.sh --web" >>"$root/.github/workflows/ci.yml"
# Act and Assert
expect "a harness named only with --web is run by no workflow" "$unnamed_message" "$(fixture_problems "$root")"

# Arrange
root=$(new_fixture commented)
write_harness "$root" test_both "$both_widths"
echo "          # bash scripts/port/test_both.sh" >>"$root/.github/workflows/ci.yml"
# Act and Assert
expect "a harness named only on a commented-out line is run by no workflow" "$unnamed_message" \
    "$(fixture_problems "$root")"

# Arrange
root=$(new_fixture child)
write_harness "$root" test_parent "bash scripts/port/test_both.sh" named
write_harness "$root" test_both "$both_widths"
# Act and Assert
expect "a harness run by a harness a workflow names passes" "" "$(fixture_problems "$root")"

# Arrange
root=$(new_fixture own-then-32)
write_harness "$root" test_own "$own_then_32" named
write_harness "$root" test_tool 'python3 scripts/port/tool.py port/src' named
# Act and Assert
expect "own width then 32, and a harness that compiles nothing, pass" "" "$(fixture_problems "$root")"

# Arrange
root=$(new_fixture driver)
write_harness "$root" test_one '"${CC:-gcc}" scripts/port/test_one.c -o "$bin"' named
echo '#include "../../port/src/video.c"' >"$root/scripts/port/test_one.c"
# Act and Assert
expect "a driver that includes port/ code counts as compiling it" "$one_message" "$(fixture_problems "$root")"

# Arrange
root=$(new_fixture stale)
# Act and Assert
expect "a list entry naming a missing harness fails" \
    "HARNESS_WORKFLOW_EXEMPT names test_gone, which is not a harness in scripts/port" \
    "$(fixture_problems "$root" "" "test_gone: removed")"
# Arrange
write_harness "$root" test_both "$both_widths" named
# Act and Assert
expect "a list entry without a reason fails" "HARNESS_WIDTH_EXEMPT entry 'test_both' gives no reason" \
    "$(fixture_problems "$root" "test_both")"

if [[ $failures -ne 0 ]]; then
    echo "test_harness_widths: $failures failed"
    exit 1
fi
echo "test_harness_widths: PASS"
