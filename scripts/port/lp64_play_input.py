"""The `input` set of lp64_play.py: runs that need a button (LP64-005 Task 8).

Each case replays a route (lp64_play_route.py) as pad 0 of a headless build, with GE_PLAYLOG=1,
and requires the lines that show it reached what the button is for: the pause, each watch tab
(`watch-page N`, the tab drawn once the watch is open), a walk with the trigger held (`moved` and
`shots 1`, from player 0's own counters), the front end from boot to Dam, the watch's abort, the
briefing and a multiplayer respawn. None of those lines appears without the route's presses.

The pause has no `ge: play:` event: the only code that sets it is in src/game/bondview2.c (strict
tier), so the pause cases read GE_PAUSEDBG's existing line there, printed on each change of the
watch's animation state: 5 is the watch menu up with the game paused, and 0 after it is the watch
put away. The ticks below were measured on 2026-11-18 (FORK-089): the swirl that follows each
mission's overview ends before tick 1200 in Dam, Facility and Train (Dam's at about 1076 fields
with no button pressed, after the 482 of its overview), and a START at 600 lands in Dam's swirl and
skips it instead of pausing.
"""
import re

from lp64_play_cases import (LAUNCHED_ARENA, LAUNCHED_STAGE, MENU_HARNESS, ARENAS, Case, Ordered,
                             Required, frame_limit)
from lp64_play_route import (A_BUTTON, START_BUTTON, Z_TRIG, Press, Route, Step, play_line)

PLAY_ENV = (('GE_PLAYLOG', '1'),)
SOLO_CHEATS = (('GE_STAGE_AFTER', '1'), ('GE_CHEATS', 'invuln,invis'))
# src/game/bondview2.c, portPauseBoxTick, with GE_PAUSEDBG=1:
#   "pausebox: STATE %d -> %d at frame %u raw=%.3f eased=%.3f pausing=%d\n"
PAUSE_ON = re.compile(r'pausebox: STATE \d+ -> 5 at frame ')
PAUSE_OFF = re.compile(r'pausebox: STATE \d+ -> 0 at frame ')
PAUSE_ON_STEP = ('pausebox: STATE N -> 5 (the watch menu up, the game paused)', PAUSE_ON)
PAUSE_OFF_STEP = ('pausebox: STATE N -> 0 (the watch put away)', PAUSE_OFF)
# src/game/bondview2.c, mp_respawn_handler: "ge: mp spawn p%d pad=%d (%s) stanroom=%d pos=..."; the
# first spawn prints bondview_r.c's "ge: mp spawn p%d startpad=" instead.
RESPAWN = re.compile(r'ge: mp spawn p0 pad=')
RESPAWN_STEP = ('ge: mp spawn p0 pad= (the respawn)', RESPAWN)

# port/src/port_playlog.c, from player 0's own counters: the distance walked first reaching
# PORT_PLAY_MOVED_UNITS, and the total shot count's first change.
MOVED = play_line('moved')
FIRST_SHOT = play_line('shots', 1)

PAUSE_TICK = 1200
UNPAUSE_TICK = 1800
PAUSE_FRAMES = 2400
WATCH_FRAMES = 1800
# The watch's tabs, by GE_WATCHPAGE's number (src/game/options.h, WATCH_INDEX_*).
WATCH_TABS = ('status', 'inventory', 'control', 'options', 'briefing')
WALK_MISSIONS = ('dam', 'facility', 'runway')
WALK_FRAMES = 1800
WALK_TICKS = 300
FIRE_TICKS = 60
# Forward, inside the N64 stick's 85 and the game's assertion at 100 (bondview2.c).
WALK_STICK_Y = 70
FRONT_END_FRAMES = 2400
# The front end from boot, as a press of START each passes: the logos (1 Nintendo, 2 Rare, 3 the
# eye intro, 4 the title), 5 the file select (folder 1), 6 mode select (solo), 7 mission select
# (Dam), 8 difficulty (Agent) and 10 the briefing, which starts the mission (src/bondconstants.h,
# MENU). The legal screen (0) is set at boot without frontChangeMenu and takes no button the first
# time; it moves on to 1 by its timer.
FRONT_END_MENUS = (1, 2, 3, 4, 5, 6, 7, 8, 10)
STEP_DELAY = 60
LEVEL_DAM = 33
LEVEL_TITLE = 90
# The watch's abort, on its first tab: START opens the watch; A takes the abort line; the stick
# right picks CONFIRM; A confirms (src/game/options.c, watch_screen0_navigation and
# draw_abort_cancel_confirm). The watch prints no play event, so the presses in it are timed from
# the START. The abort loads the title stage back with the front end's next menu already chosen
# (it was set when the mission started), so a START there gives the menu line after the unload.
ABORT_START_DELAY = PAUSE_TICK - 5
ABORT_STEPS = (
    Step(Press(START_BUTTON), ABORT_START_DELAY, play_line('stage-load', LEVEL_DAM), 'ge: play: stage-load 33'),
    Step(Press(A_BUTTON), 150),
    Step(Press(0, stick_x=70, hold=20), 40),
    Step(Press(A_BUTTON), 40),
    Step(Press(START_BUTTON), 120, play_line('stage-load', LEVEL_TITLE), 'ge: play: stage-load 90'),
)
ABORT_FRAMES = 2400
BRIEFING_FRAMES = 900
RESPAWN_FRAMES = 1800
RESPAWN_PRESSES = tuple(range(900, 1501, 60))
# GE_PLAYKILL=N (port/src/port_playlog.c) kills player 0 on his N-th pass of the frame's player
# loop and logs `ge: play: kill N` when that left him dead. The brief's GE_KILLBOND=600
# (bondview2.c) counts both players' MoveBond calls and kills only when the 600th is player 0's; the frame takes its players in a
# shuffled order, so in a two-player run that kills in some runs and not others (measured
# 2026-11-18: the same arena respawned in one run and not the next).
PLAYKILL = 600


def press_at(*ticks: int, press: Press = Press(START_BUTTON)) -> Route:
    return Route(presses=tuple((tick, press) for tick in ticks))


def walk_fire_route(frames: int) -> Route:
    """Stick forward WALK_TICKS, then Z held FIRE_TICKS with the stick let go, repeated."""
    presses = []
    for start in range(0, frames, WALK_TICKS + FIRE_TICKS):
        presses.append((start, Press(0, stick_y=WALK_STICK_Y, hold=WALK_TICKS)))
        presses.append((start + WALK_TICKS, Press(Z_TRIG, hold=FIRE_TICKS)))
    return Route(presses=tuple(presses))


def ordered_play(*pairs: tuple[str, int | None]) -> Ordered:
    return Ordered(tuple((f'ge: play: {event} {"N" if value is None else value}', play_line(event, value))
                         for event, value in pairs))


def pause_case(mission: str) -> Case:
    env = (('GE_STAGE', mission),) + SOLO_CHEATS + PLAY_ENV + (('GE_PAUSEDBG', '1'),)
    return Case(f'pause-{mission}', 'input', env, PAUSE_FRAMES,
                LAUNCHED_STAGE + (Ordered((PAUSE_ON_STEP, PAUSE_OFF_STEP)), frame_limit(PAUSE_FRAMES)),
                route=press_at(PAUSE_TICK, UNPAUSE_TICK))


def watch_case(number: int, tab: str) -> Case:
    env = (('GE_STAGE', 'dam'),) + SOLO_CHEATS + PLAY_ENV + (('GE_PAUSEDBG', '1'), ('GE_WATCHPAGE', str(number)))
    page = (f'ge: play: watch-page {number} (the {tab} tab drawn, the watch open)', play_line('watch-page', number))
    return Case(f'watch-{tab}', 'input', env, WATCH_FRAMES,
                LAUNCHED_STAGE + (Ordered((PAUSE_ON_STEP, page)), frame_limit(WATCH_FRAMES)),
                route=press_at(PAUSE_TICK))


def walk_fire_case(mission: str) -> Case:
    env = (('GE_STAGE', mission),) + SOLO_CHEATS + PLAY_ENV
    required = LAUNCHED_STAGE + (Required('ge: play: moved N (Bond walked)', MOVED),
                                 Required('ge: play: shots 1 (Bond fired)', FIRST_SHOT),
                                 frame_limit(WALK_FRAMES))
    return Case(f'walk-fire-{mission}', 'input', env, WALK_FRAMES, required, route=walk_fire_route(WALK_FRAMES))


def front_end_case() -> Case:
    steps = tuple(Step(Press(START_BUTTON), STEP_DELAY, play_line('menu', menu), f'ge: play: menu {menu}')
                  for menu in FRONT_END_MENUS)
    required = ordered_play(*[('menu', menu) for menu in FRONT_END_MENUS], ('menu', 11),
                            ('stage-load', LEVEL_DAM))
    return Case('front-end', 'input', PLAY_ENV, FRONT_END_FRAMES, (required,), route=Route(steps=steps))


def abort_case() -> Case:
    env = (('GE_STAGE', 'dam'),) + SOLO_CHEATS + PLAY_ENV
    return Case('abort', 'input', env, ABORT_FRAMES,
                (ordered_play(('stage-load', LEVEL_DAM), ('stage-unload', LEVEL_DAM), ('stage-load', LEVEL_TITLE),
                              ('menu', None)),),
                route=Route(steps=ABORT_STEPS))


def briefing_case() -> Case:
    env = (('GE_MENU', 'briefing'),) + PLAY_ENV
    required = (Required('ge: GE_MENU harness: menu=', MENU_HARNESS), ordered_play(('menu', 10)),
                frame_limit(BRIEFING_FRAMES))
    return Case('briefing', 'input', env, BRIEFING_FRAMES, required, route=Route())


def respawn_case(arena: str) -> Case:
    env = (('GE_MP', arena), ('GE_MPPLAYERS', '2'), ('GE_PLAYKILL', str(PLAYKILL))) + PLAY_ENV
    kill = (f'ge: play: kill {PLAYKILL}', play_line('kill', PLAYKILL))
    required = LAUNCHED_ARENA + (Ordered((kill, RESPAWN_STEP)), frame_limit(RESPAWN_FRAMES))
    return Case(f'respawn-{arena}', 'input', env, RESPAWN_FRAMES, required, route=press_at(*RESPAWN_PRESSES))


def input_cases() -> list[Case]:
    cases = [pause_case(mission) for mission in ('dam', 'facility', 'train')]
    cases += [watch_case(number, tab) for number, tab in enumerate(WATCH_TABS)]
    cases += [walk_fire_case(mission) for mission in WALK_MISSIONS]
    cases += [front_end_case(), abort_case(), briefing_case()]
    cases += [respawn_case(arena) for arena in ARENAS]
    return cases
