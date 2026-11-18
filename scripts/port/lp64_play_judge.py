"""Judge one lp64_play.py run by what its log shows (LP64-005 Task 1).

A case fails on a status other than the one it expects, a timeout, any sanitizer marker,
`ge: fatal signal`, `ASSERTION FAULT`, a failing `ge: lp64:` line (lp64_boot.py's default-deny
rule) and on each line it requires that the log lacks. The markers and the `ge: lp64:` rule are
lp64_boot.py's own, so the two tools cannot drift apart.
"""
from lp64_boot import FATAL_SIGNAL, SANITIZER_MARKERS, failing_lp64_lines
from lp64_play_cases import DEMO_START, Case, Required

# include/assert.h and the game's own asserts: "\n--- ASSERTION FAULT - %s - %s, line %d\n\n".
ASSERTION_FAULT = 'ASSERTION FAULT'


def first_line_with(lines: list[str], markers: tuple[str, ...]) -> str | None:
    return next((line for line in lines if any(marker in line for marker in markers)), None)


def first_playback(lines: list[str]) -> list[str]:
    """The lines of the log's first demo playback: from its first `ge: demo start:` line up to
    its second (all the rest when there is no second); none when no demo started."""
    starts = [index for index, line in enumerate(lines) if DEMO_START.search(line)]
    if not starts:
        return []
    return lines[starts[0]:starts[1]] if len(starts) > 1 else lines[starts[0]:]


def requirement_problem(lines: list[str], required: Required) -> str | None:
    """Why LINES do not satisfy REQUIRED, or None when they do."""
    if required.first_playback:
        lines = first_playback(lines)
    matches = [match for match in map(required.pattern.search, lines) if match is not None]
    if len(matches) < required.count:
        if not matches and required.near is not None:
            near = next((line for line in lines if required.near.search(line)), None)
            if near is not None:
                return f'missing line: {required.label}; the log has: {near.strip()}'
        if required.count > 1:
            return f'missing line: {required.label} ({len(matches)} of {required.count})'
        return f'missing line: {required.label}'
    values = [int(match.group(1)) for match in matches] if (required.at_least is not None
                                                            or required.equals is not None) else []
    if required.equals is not None and required.equals not in values:
        return f'{matches[0].group(0)}: expected {required.equals}'
    if required.at_least is not None and max(values) < required.at_least:
        best = matches[values.index(max(values))].group(0)
        return f'{best}: expected at least {required.at_least}'
    return None


def problems(case: Case, status: int | None, text: str, timeout: float) -> list[str]:
    """Every way the run of CASE differs from what it must do, in the order the first is reported:
    the status (None for a timeout, negative for a signal), then the forbidden lines, then the
    required ones. Empty when the case passes."""
    lines = text.splitlines()
    found = []
    if status is None:
        last = lines[-1].strip() if lines else '(empty log)'
        found.append(f'timed out after {timeout:.0f} s; last line: {last}')
    elif status < 0:
        found.append(f'killed by signal {-status}')
    elif status != case.status:
        found.append(f'exit status {status}, expected {case.status}')
    for markers in (SANITIZER_MARKERS, (FATAL_SIGNAL,), (ASSERTION_FAULT,)):
        line = first_line_with(lines, markers)
        if line is not None:
            found.append(line.strip())
    for pattern in case.forbidden:
        line = next((line for line in lines if pattern.search(line)), None)
        if line is not None:
            found.append(line.strip())
    refusals = failing_lp64_lines(lines)
    if refusals:
        found.append(refusals[0].strip())
    for required in case.required:
        problem = requirement_problem(lines, required)
        if problem is not None:
            found.append(problem)
    return found
