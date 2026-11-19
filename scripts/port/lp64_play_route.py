"""Routes: the recorded pad 0 of lp64_play.py's `input` set (LP64-005 Task 8).

A route is written as a GE_INPUTPLAY file of `P TICK BUTTONS STICK_X STICK_Y` lines (port/src/
inputrec.c; hexadecimal, the stick as an unsigned byte). A headless build replays it by the count
of its pad reads, holding each line's state until the next line, so every press is followed by a
line that lets go.

A route is either fixed ticks or a list of steps, each "wait for this line, then press". The ticks
of steps are found by running, not guessed (build_presses): a recording run with the presses found
so far shows the tick of the line a step waits for, through the `ge: play: tick N` line the play
log (port/src/port_playlog.c) prints before each event.
"""
from dataclasses import dataclass
import re
from typing import Callable

# include/PR/os.h: the buttons the routes press.
START_BUTTON = 0x1000
A_BUTTON = 0x8000
Z_TRIG = 0x2000
# The ticks a press is held; the menus and the watch act on the tick a button goes down.
PRESS_TICKS = 5

# port/src/port_playlog.c, portPlayLog: "ge: play: tick %u\nge: play: %s %d\n".
PLAY_TICK = re.compile(r'ge: play: tick (\d+)$')
PLAY_EVENT = re.compile(r'ge: play: [a-z-]+ -?\d+$')
PLAY_MARK = 'ge: play:'


def play_line(event: str, value: int | None = None) -> re.Pattern:
    """The play log's line for EVENT with VALUE (any value when None)."""
    number = r'-?\d+' if value is None else str(value)
    return re.compile(rf'ge: play: {re.escape(event)} {number}$')


@dataclass(frozen=True)
class Press:
    """Pad 0 held at BUTTONS and the stick for HOLD ticks, then let go."""
    buttons: int
    stick_x: int = 0
    stick_y: int = 0
    hold: int = PRESS_TICKS


@dataclass(frozen=True)
class Step:
    """PRESS, DELAY ticks after the first WAIT line at or after the previous press's tick; with no
    WAIT, DELAY ticks after the previous press (or tick 0). LABEL names WAIT in a failure."""
    press: Press
    delay: int
    wait: re.Pattern | None = None
    label: str = ''


@dataclass(frozen=True)
class Route:
    """Fixed PRESSES at their ticks, or STEPS whose ticks a recording run finds."""
    presses: tuple[tuple[int, Press], ...] = ()
    steps: tuple[Step, ...] = ()


class RouteError(Exception):
    """A step's line never appeared in its recording run."""


def states(presses: list[tuple[int, Press]] | tuple[tuple[int, Press], ...]) -> list[tuple[int, int, int, int]]:
    """The pad states of PRESSES as (tick, buttons, stick_x, stick_y): each press at its tick and
    neutral when it is let go, unless the next press starts on that tick or before."""
    ordered = sorted(presses, key=lambda item: item[0])
    out = []
    for index, (tick, press) in enumerate(ordered):
        out.append((tick, press.buttons, press.stick_x, press.stick_y))
        release = tick + press.hold
        if index + 1 >= len(ordered) or ordered[index + 1][0] > release:
            out.append((release, 0, 0, 0))
    return out


def route_text(presses: list[tuple[int, Press]] | tuple[tuple[int, Press], ...]) -> str:
    """The GE_INPUTPLAY file of PRESSES."""
    return ''.join(f'P {tick} {buttons:x} {stick_x & 0xFF:x} {stick_y & 0xFF:x}\n'
                   for tick, buttons, stick_x, stick_y in states(presses))


def event_tick(lines, wait: re.Pattern, after: int) -> int | None:
    """The tick of the first line of LINES matching WAIT whose `ge: play: tick` line is at or after
    AFTER, or None."""
    tick = None
    for line in lines:
        line = line.rstrip('\n')
        match = PLAY_TICK.search(line)
        if match is not None:
            tick = int(match.group(1))
            continue
        if tick is not None and tick >= after and wait.search(line):
            return tick
    return None


def build_presses(steps: tuple[Step, ...],
                  find_tick: Callable[[str, Step, int, int], int | None]) -> list[tuple[int, Press]]:
    """The presses of STEPS at the ticks found by running. FIND_TICK(route, step, after, index) runs
    the game with ROUTE (the presses found so far) and gives the tick of STEP's line at or after
    AFTER, or None; a step whose line never appears raises RouteError."""
    presses: list[tuple[int, Press]] = []
    last = 0
    for index, step in enumerate(steps):
        base = last
        if step.wait is not None:
            found = find_tick(route_text(presses), step, last, index)
            if found is None:
                raise RouteError(f'route step {index + 1}: {step.label or step.wait.pattern} '
                                 f'did not appear at or after tick {last}')
            base = found
        last = base + step.delay
        presses.append((last, step.press))
    return presses
