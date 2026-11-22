"""The 38 cases of lp64_equiv.py, the runs LP64-006 compares between builds (Task 5).

    demos   the 14 demos, paced by their recordings as lp64_play.py runs them (never in lockstep)
    solo    the 20 missions and Cuba in lockstep, with GE_CHEATS=invuln and the generated route
            (forward and sidestep legs: a lockstep route never turns, so the demos are the
            cases that turn)
    mp      Temple with four players in lockstep, the same route for pad 0
    menus   `walk`: from boot through the front end to Dam, the watch's five tabs, the abort and
            back to the title, in lockstep
    reload  `dam-twice`: Dam, the watch's abort, Dam again from the front end and the generated
            route in the second load, in lockstep
    saves   (Task 13) runs that start from a saves folder (`run --saves DIR`); none yet

Every case but a demo requires `ge: lockstep: on`. A case with the generated route requires
SHOT_FLOOR shots or more and `moved` (player 0's own counters, port/src/port_playlog.c,
GE_PLAYLOG=1), except as the tables below say. A guard's hit holds the trigger off while the damage display runs (LP64-006 Task 1), so the solo
routes run with `invuln`.

`walk` and `dam-twice` are routes of steps (lp64_play_route.py): their ticks were found by
recording runs in lockstep and are pinned below with the date and commit of the measurement;
`stepped(name)` gives a case's steps again, to measure them anew (lp64_play_run.route_file builds
the presses). A pinned tick that no longer lands fails the case by its ordered lines.
"""
import re

from lp64_play_cases import (DEMOS, LAUNCHED_STAGE, MISSIONS, Case, Ordered, Required, demo_case, frame_limit,
                             launched_arena)
from lp64_play_input import ABORT_STEPS, FRONT_END_MENUS, LEVEL_DAM, LEVEL_TITLE, STEP_DELAY, ordered_play
from lp64_play_route import A_BUTTON, START_BUTTON, Z_TRIG, Press, Route, Step, play_line

# include/PR/os.h: C-left and C-right (CONT_C, CONT_F), the watch's tabs in `walk`.
L_CBUTTONS = 0x0002
R_CBUTTONS = 0x0001
# port/src/port_lockstep.c, once when GE_LOCKSTEP is set: "ge: lockstep: on\n".
LOCKSTEP_ON = Required('ge: lockstep: on', re.compile(r'^ge: lockstep: on$'))
# port_playlog.c logs `shots N` on each change of player 0's total shot count.
SHOTS = re.compile(r'ge: play: shots (\d+)$')
MOVED = Required('ge: play: moved N (Bond walked)', play_line('moved'))
PLAY_ENV = (('GE_PLAYLOG', '1'),)

# ---- the generated route ----------------------------------------------------------------------
# scripts/port/bench/scenarios.json's `fight` route made of pad records only. A headless replay acts
# on `P` records alone (port/src/inputrec.c), and nothing in one turns Bond: the stick's X axis
# sidesteps under the default control style and C-left does nothing (measured on cmp-a64, Dam, ticks
# 1300 to 1600: stick X at -80 moved Bond sideways, C-left left him still, his facing stayed 90.3).
# So a lockstep route keeps one facing for its whole run; turning would need an `M` record or a
# look-stick field that headless replay acts on, which is game C in input recording (controller
# ruling 2 as replaced, LP64-006 Task 5). The route: the stick forward for STRAIGHT_TICKS, then the
# stick's X axis at SIDE_STICK_X for SIDE_TICKS, to the left where the benchmark's turn list turns
# left (positive) and to the right where it turns right, stick Y released; Z_TRIG held FIRE_HOLD of
# every FIRE_EVERY ticks from FIRE_FROM.
FORWARD = 70
# Full deflection, inside the N64 stick's 85 and the game's assertion at 100 (bondview2.c).
SIDE_STICK_X = 80
STRAIGHT_TICKS = 150
SIDE_TICKS = 45
# The benchmark's turn list in degrees: only each sign is used, as a sidestep's direction.
SIDE_DIRECTIONS = (80, -50, 110, -140, 60, 170, -95, 35)
FIRE_FROM = 400
FIRE_EVERY = 120
FIRE_HOLD = 30
ROUTE_TICKS = 3900


def pad_at(tick: int) -> tuple[int, int, int]:
    """(buttons, stick_x, stick_y) the generated route holds at TICK."""
    index, phase = divmod(tick, STRAIGHT_TICKS + SIDE_TICKS)
    if phase < STRAIGHT_TICKS:
        stick_x, stick_y = 0, FORWARD
    else:
        stick_x, stick_y = (-SIDE_STICK_X if SIDE_DIRECTIONS[index % len(SIDE_DIRECTIONS)] > 0 else SIDE_STICK_X), 0
    fire = tick >= FIRE_FROM and (tick - FIRE_FROM) % FIRE_EVERY < FIRE_HOLD
    return (Z_TRIG if fire else 0), stick_x, stick_y


def route_presses(ticks: int = ROUTE_TICKS, start: int = 0) -> tuple[tuple[int, Press], ...]:
    """The generated route's first TICKS ticks as presses, each a run of one pad state, shifted to
    start at tick START."""
    presses = []
    run_start, state = 0, pad_at(0)
    for tick in range(1, ticks + 1):
        now = pad_at(tick) if tick < ticks else None
        if now != state:
            buttons, stick_x, stick_y = state
            presses.append((start + run_start, Press(buttons, stick_x, stick_y, hold=tick - run_start)))
            run_start, state = tick, now
    return tuple(presses)


# ---- what a route with fire must show ---------------------------------------------------------
# At least SHOT_FLOOR shots (half the route's 30 trigger windows) and `moved`. The plan's 30 was
# arithmetic: most missions start with a pistol, one shot a window, and the opening takes the first
# windows (LP64-006 Task 5, controller ruling 1). Each case's count, measured on cmp-a64 on
# 2026-11-21 at 1c777fc with this route, is recorded here as a figure, not judged for equality: a
# count that differs between builds is for `compare` to find.
SHOT_FLOOR = 15
MEASURED_SHOTS = {
    'stage-dam': 28, 'stage-facility': 29, 'stage-runway': 29, 'stage-surface': 28, 'stage-bunker1': 29,
    'stage-silo': 28, 'stage-frigate': 20, 'stage-surface2': 29, 'stage-statue': 29, 'stage-streets': 29,
    'stage-depot': 29, 'stage-train': 29, 'stage-jungle': 29, 'stage-control': 29, 'stage-caverns': 100,
    'stage-cradle': 29, 'stage-aztec': 29, 'stage-egypt': 29, 'dam-twice': 28,
}
# Armed cases whose measured count is under SHOT_FLOOR: their own floor, never 0, with the reason
# their log shows.
LOW_FLOORS: dict[str, tuple[int, str]] = {}
# Cases that start unarmed (`ge: play: weapon 1`, the fists, whose punches the shot count does not
# count): `moved` only.
UNARMED = {
    'stage-bunker2': 'its only weapon line is `ge: play: weapon 1`: Bond starts with his fists',
    'stage-archives': 'its only weapon line is `ge: play: weapon 1`: Bond starts with his fists',
    'temple-4p': 'its only weapon line is `ge: play: weapon 1`: the arena starts everyone with fists',
}
# Cases with no player control: the stage load and frame limit only.
NO_CONTROL = {'stage-cuba': 'no player control (no `moved`, no weapon in its log)'}


def fired(name: str) -> tuple[Required, ...]:
    """What NAME's route with fire must show: `moved` and its shots, or less as the tables say."""
    if name in NO_CONTROL:
        return ()
    if name in UNARMED:
        return (MOVED,)
    floor = LOW_FLOORS.get(name, (SHOT_FLOOR, ''))[0]
    return (MOVED, Required(f'ge: play: shots N (at least {floor})', SHOTS, at_least=floor))


def solo_case(stage: str) -> Case:
    env = (('GE_STAGE', stage), ('GE_STAGE_AFTER', '1'), ('GE_CHEATS', 'invuln')) + PLAY_ENV
    name = f'stage-{stage}'
    required = (LOCKSTEP_ON,) + LAUNCHED_STAGE + fired(name) + (frame_limit(ROUTE_TICKS),)
    return Case(name, 'solo', env, ROUTE_TICKS, required, route=Route(presses=route_presses()), lockstep=True)


def mp_case() -> Case:
    env = (('GE_MP', 'temple'), ('GE_MPPLAYERS', '4')) + PLAY_ENV
    required = (LOCKSTEP_ON,) + launched_arena(4) + fired('temple-4p') + (frame_limit(ROUTE_TICKS),)
    return Case('temple-4p', 'mp', env, ROUTE_TICKS, required, route=Route(presses=route_presses()),
                lockstep=True)


# ---- walk: the front end, the watch and the abort ------------------------------------------------
# The front end from boot to Dam (lp64_play_input.FRONT_END_MENUS), then the watch opened after the
# mission's opening (the abort case's START), its tabs one way and back by C-right and C-left, each
# press after the tab before it is drawn (`watch-page N`), and the abort from the first tab
# (lp64_play_input.ABORT_STEPS, after its START).
WATCH_TABS = 5
TAB_DELAY = 30


def tab_step(button: int, page: int) -> Step:
    return Step(Press(button), TAB_DELAY, play_line('watch-page', page), f'ge: play: watch-page {page}')


WALK_STEPS = (
    tuple(Step(Press(START_BUTTON), STEP_DELAY, play_line('menu', menu), f'ge: play: menu {menu}')
          for menu in FRONT_END_MENUS)
    + ABORT_STEPS[:1]
    + tuple(tab_step(R_CBUTTONS, page) for page in range(WATCH_TABS - 1))
    + tuple(tab_step(L_CBUTTONS, page) for page in range(WATCH_TABS - 1, 0, -1))
    + (Step(Press(A_BUTTON), TAB_DELAY, play_line('watch-page', 0), 'ge: play: watch-page 0'),)
    + ABORT_STEPS[2:])
WALK_PAGES = tuple(range(WATCH_TABS)) + tuple(range(WATCH_TABS - 2, -1, -1))
# Measured on 2026-11-21 at 1c777fc on cmp-a64 in lockstep (lp64_play_run.route_file on
# stepped('walk')); WALK_FRAMES leaves the last START's menu line 139 ticks to appear.
WALK_TICKS = (305, 365, 425, 485, 580, 640, 700, 760, 820, 2020, 2189, 2248, 2307, 2366, 2425, 2484, 2543, 2602,
              2661, 2701, 2741, 2861)
WALK_FRAMES = 3000


def walk_required() -> tuple[Required | Ordered, ...]:
    order = ordered_play(*[('menu', menu) for menu in FRONT_END_MENUS], ('menu', 11), ('stage-load', LEVEL_DAM),
                         *[('watch-page', page) for page in WALK_PAGES], ('stage-unload', LEVEL_DAM),
                         ('stage-load', LEVEL_TITLE), ('menu', None))
    return (LOCKSTEP_ON, order, frame_limit(WALK_FRAMES))


def walk_case(maybe_route: Route | None = None) -> Case:
    route = pinned(WALK_STEPS, WALK_TICKS) if maybe_route is None else maybe_route
    return Case('walk', 'menus', PLAY_ENV, WALK_FRAMES, walk_required(), route=route, lockstep=True)


# ---- dam-twice: Dam, the abort, Dam again and the route ----------------------------------------
# Dam launched directly, the watch's abort (lp64_play_input.ABORT_STEPS, ending with a START at the
# title), then START on each front-end menu it passes to Dam again (`menu 13`, then `menu 10`), and
# the generated route from the second load's `stage-load 33` on. The last step presses nothing: its
# tick is that load's, where the route starts.
DAM_AGAIN_MENUS = (13, 10)
DAM_TWICE_STEPS = (ABORT_STEPS
                   + tuple(Step(Press(START_BUTTON), STEP_DELAY, play_line('menu', menu), f'ge: play: menu {menu}')
                           for menu in DAM_AGAIN_MENUS)
                   + (Step(Press(0, hold=1), 0, play_line('stage-load', LEVEL_DAM), f'ge: play: stage-load {LEVEL_DAM}'),))
# Measured on 2026-11-21 at 1c777fc on cmp-a64 in lockstep (lp64_play_run.route_file on
# stepped('dam-twice')): the second load at tick 1675, so the route's ROUTE_TICKS end at 5575.
DAM_TWICE_TICKS = (1200, 1350, 1390, 1430, 1550, 1610, 1670, 1675)
DAM_TWICE_FRAMES = DAM_TWICE_TICKS[-1] + ROUTE_TICKS


def dam_twice_route(ticks: tuple[int, ...]) -> Route:
    """The pinned presses before the second load, then the generated route from its tick."""
    *before, second_load = pinned(DAM_TWICE_STEPS, ticks).presses
    return Route(presses=tuple(before) + route_presses(start=second_load[0]))


def dam_twice_case(maybe_route: Route | None = None) -> Case:
    env = (('GE_STAGE', 'dam'), ('GE_STAGE_AFTER', '1'), ('GE_CHEATS', 'invuln')) + PLAY_ENV
    order = ordered_play(('stage-load', LEVEL_DAM), ('stage-unload', LEVEL_DAM), ('stage-load', LEVEL_TITLE),
                         *[('menu', menu) for menu in DAM_AGAIN_MENUS], ('stage-load', LEVEL_DAM))
    required = (LOCKSTEP_ON, order) + fired('dam-twice') + (frame_limit(DAM_TWICE_FRAMES),)
    route = dam_twice_route(DAM_TWICE_TICKS) if maybe_route is None else maybe_route
    return Case('dam-twice', 'reload', env, DAM_TWICE_FRAMES, required, route=route, lockstep=True)


def pinned(steps: tuple[Step, ...], ticks: tuple[int, ...]) -> Route:
    """STEPS' presses at their pinned TICKS."""
    if len(steps) != len(ticks):
        raise ValueError(f'{len(ticks)} pinned ticks for {len(steps)} steps')
    return Route(presses=tuple((tick, step.press) for tick, step in zip(ticks, steps)))


def stepped(name: str) -> Case:
    """The case NAME (`walk` or `dam-twice`) with its route of steps in place of the pinned ticks,
    to measure them anew."""
    if name == 'walk':
        return walk_case(Route(steps=WALK_STEPS))
    return dam_twice_case(Route(steps=DAM_TWICE_STEPS))


SETS = {
    'demos': lambda: [demo_case(name) for name in DEMOS],
    'solo': lambda: [solo_case(stage) for stage in MISSIONS + ('cuba',)],
    'mp': lambda: [mp_case()],
    'menus': lambda: [walk_case()],
    'reload': lambda: [dam_twice_case()],
    'saves': lambda: [],
}
# The sets whose cases start from `run --saves DIR` (Task 13).
SAVES_SETS = ('saves',)


def cases_of(set_names: list[str]) -> list[Case]:
    """The cases of SET_NAMES in order; raises ValueError naming an unknown set, or none."""
    unknown = [name for name in set_names if name not in SETS]
    if unknown or not set_names:
        raise ValueError(f'unknown set {", ".join(unknown) or "(none)"}; the sets are {", ".join(SETS)}')
    return [case for name in set_names for case in SETS[name]()]


def is_demo(name: str) -> bool:
    """Whether the case NAME plays a demo (its `sim` stream is its playback, lp64_equiv_compare.py)."""
    return name in DEMOS
