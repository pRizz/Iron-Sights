"""The cases of lp64_play.py's sets: what each runs and the lines its log must hold (LP64-005 Task 1).

A case names its launch variables, its frame count (GE_FRAMES), the exit status it expects and the
lines that prove its subject ran (Required). The judge (lp64_play_judge.py) fails a case whose log
lacks one of them, so a run that reached its frame limit having done nothing does not pass.

The measured figures below (each demo's end tick, each tour's rooms) are constants with the date
and commit of their measurement; a run that gives another figure fails, and the figure is changed
only deliberately, with the new measurement recorded in FORK_CHANGES.md (FORK-089).
"""
from dataclasses import dataclass
import re

# ---- the lines a case can require, each quoted from the source that prints it ----------------
# src/game/ramromreplay.c, replay_recorded_ramrom_at_address:
#   "ge: demo start: stage=%d diff=%d pads=%d totaltime=%d vsyncs (~%ds)\n"
DEMO_START = re.compile(r'ge: demo start: ')
# src/game/ramromreplay.c, port_demo_exit_log (once a playback, whatever ended it):
#   "ge: demo exit: %s at tick %d, gametime %d vsyncs (~%ds), detail %x/%x\n"
DEMO_EXIT = re.compile(r'ge: demo exit: ')
# The same line with the reason iterate_ramrom_entries_handle_camera_out gives when the
# recording's time runs out.
DEMO_EXIT_TOTALTIME = re.compile(r'ge: demo exit: totaltime end \(timer/limit\) at tick (\d+),')
# Any other demo exit (a checksum or seed mismatch, a clean end, an input press): headless, no
# input can end a demo, so every playback should end by its time.
DEMO_EXIT_OTHER = re.compile(r'ge: demo exit: (?!totaltime end \(timer/limit\) at tick )')
# src/game/front.c, portDirectLaunchDemoTick, right after the demo's start line:
#   "ge: GE_DEMO direct playback: %s (index %d)\n"
DEMO_PLAYBACK = 'ge: GE_DEMO direct playback: {name} (index '
# port/src/pcsched.c, once GE_FRAMES frames have run (the pattern is lp64_boot.py's):
#   "ge: frame limit reached: frames=%u gfxtasks=%u\n"
FRAME_LIMIT = re.compile(r'ge: frame limit reached: frames=(\d+)')
# src/game/front.c, portDirectLaunchTick: "ge: GE_STAGE direct launch: levelid=%d diff=%d\n"
STAGE_LAUNCH = re.compile(r'ge: GE_STAGE direct launch: levelid=')
# src/game/front.c, portDirectLaunchMpTick:
#   "ge: GE_MP direct launch: arena=%d levelid=%d scenario=%d players=%d\n"
MP_LAUNCH = re.compile(r'ge: GE_MP direct launch: arena=')
# src/game/bg.c, load_bg_file, at both widths: "ge: load_bg_file levelid=%d stan=%s ..."; the
# stage's background has loaded, which the launch line alone (printed when the menu changes)
# does not show.
BG_LOADED = re.compile(r'ge: load_bg_file levelid=')
# src/game/front.c, portMenuHarnessTick: "ge: GE_MENU harness: menu=%d\n"
MENU_HARNESS = re.compile(r'ge: GE_MENU harness: menu=')
# port/src/port_load_tour.c, tourFinish: "ge: loadtour: done, %d of %d rooms%s\n"
TOUR_DONE = re.compile(r'ge: loadtour: done, (\d+) of \d+ rooms')
# port/src/port_soak.c, portSoakNextStage: "ge: soak: complete after %d laps\n"
SOAK_COMPLETE = re.compile(r'ge: soak: complete after 1 laps')
# port/src/sysstubs.c, sysFatalError, prefixes "ge: FATAL: " to the message.
FATAL = re.compile(r'ge: FATAL: ')


@dataclass(frozen=True)
class Required:
    """A line the log must hold COUNT times. With AT_LEAST or EQUALS, the pattern's first group is
    a number that must be at least, or exactly, that value in one of the matching lines. NEAR, when
    the line is missing, names the nearest line the log does hold (another demo exit, another
    FATAL), so the failure shows what happened instead."""
    label: str
    pattern: re.Pattern
    count: int = 1
    at_least: int | None = None
    equals: int | None = None
    near: re.Pattern | None = None
    # Only the lines of the log's first playback count: from its first `ge: demo start:` line up
    # to its second. After a demo ends the attract loop starts another, chosen at random, whose
    # exit line could otherwise stand in for the first one's.
    first_playback: bool = False


@dataclass(frozen=True)
class Case:
    """One run: NAME (also its log's file name), the SET it belongs to, its own variables, its
    frame count, the status it must exit with and the lines it must log."""
    name: str
    set_name: str
    env: tuple[tuple[str, str], ...]
    frames: int
    required: tuple[Required, ...]
    status: int = 0
    # Lines that fail the case wherever they appear.
    forbidden: tuple[re.Pattern, ...] = ()


def frame_limit(frames: int) -> Required:
    return Required(f'ge: frame limit reached: frames={frames}', FRAME_LIMIT, equals=frames)


LAUNCHED_STAGE = (Required('ge: GE_STAGE direct launch:', STAGE_LAUNCH),
                  Required('ge: load_bg_file levelid=', BG_LOADED))
LAUNCHED_ARENA = (Required('ge: GE_MP direct launch:', MP_LAUNCH),
                  Required('ge: load_bg_file levelid=', BG_LOADED))

# ---- demos ------------------------------------------------------------------------------------
# The tick at which each demo ends with "totaltime end (timer/limit)", measured on 2026-11-17 at
# 98a76dd (plan "What was measured"): the i686 build and the 64-bit build, plain and sanitized
# (aarch64), end each demo at the same tick. ramrom_table order (src/game/front.c, GE_DEMO).
DEMO_END_TICKS = {
    'dam1': 438, 'dam2': 445, 'facility1': 410, 'facility2': 538, 'facility3': 380,
    'runway1': 388, 'runway2': 388, 'bunker1': 482, 'bunker2': 771, 'silo1': 501,
    'silo2': 503, 'frigate1': 328, 'frigate2': 384, 'train': 464,
}
DEMOS = tuple(DEMO_END_TICKS)
DEMO_FRAMES = 3000


def demo_case(name: str) -> Case:
    return Case(name, 'demos', (('GE_DEMO', name), ('GE_DEMODIAG', '1')), DEMO_FRAMES, (
        Required('ge: demo start:', DEMO_START),
        *demo_playback_required(name),
        frame_limit(DEMO_FRAMES)))


def demo_playback_required(name: str) -> tuple[Required, ...]:
    """What shows that NAME's own first playback reached its end: its direct-playback line and
    its first exit line, at the measured tick, both before any second demo starts. The `demos`
    case and lp64_play_traces.py both judge a demo's log by these."""
    tick = DEMO_END_TICKS[name]
    playback = DEMO_PLAYBACK.format(name=name)
    return (Required(playback, re.compile(re.escape(playback)), first_playback=True),
            Required(f'ge: demo exit: totaltime end (timer/limit) at tick {tick}', DEMO_EXIT_TOTALTIME,
                     equals=tick, near=DEMO_EXIT, first_playback=True))


# ---- stages -----------------------------------------------------------------------------------
# audit_buffers.py's MISSIONS, in the game's order (the names GE_STAGE takes, front.c).
MISSIONS = ('dam', 'facility', 'runway', 'surface', 'bunker1', 'silo', 'frigate',
            'surface2', 'bunker2', 'statue', 'archives', 'streets', 'depot',
            'train', 'jungle', 'control', 'caverns', 'cradle', 'aztec', 'egypt')
# The names GE_MP takes (src/game/front.c, portDirectLaunchMpTick's table).
ARENAS = ('temple', 'complex', 'caves', 'library', 'basement', 'stack', 'facility', 'bunker',
          'archives', 'caverns', 'egypt', 'statue', 'cradle')
STAGE_FRAMES = 1800


def solo_case(name: str, stage: str) -> Case:
    env = (('GE_STAGE', stage), ('GE_STAGE_AFTER', '1'), ('GE_CHEATS', 'invuln,invis'),
           ('GE_LP64_CENSUS', '1'))
    return Case(name, 'stages', env, STAGE_FRAMES, LAUNCHED_STAGE + (frame_limit(STAGE_FRAMES),))


def arena_case(arena: str, players: int, scenario: int | None) -> Case:
    env = (('GE_MP', arena), ('GE_MPPLAYERS', str(players)))
    name = f'mp-{arena}-{players}p'
    if scenario is not None:
        env += (('GE_MPSCENARIO', str(scenario)),)
        name += f'-s{scenario}'
    return Case(name, 'stages', env, STAGE_FRAMES, LAUNCHED_ARENA + (frame_limit(STAGE_FRAMES),))


def stage_cases() -> list[Case]:
    cases = [solo_case(f'stage-{name}', name) for name in MISSIONS + ('cuba',)]
    cases += [arena_case(arena, players, None) for arena in ARENAS for players in (2, 4)]
    cases += [arena_case('temple', 4, scenario) for scenario in range(1, 8)]
    return cases


# ---- stops ------------------------------------------------------------------------------------
# Stage numbers whose files the ROM does not hold; a 64-bit build stops on each by name with
# status 1, measured on 2026-11-17 at 98a76dd. The setup files are named by port/src/romdata.c's
# romdataRequireServed ("The %s file %s is not served from the ROM; ..."), the backgrounds by
# port/src/romfile_bg.c ("The bg file %s is not served from the ROM; ..."); the level ids'
# background names are src/game/bg.c's levelinfotable.
STOPS = (
    ('citadel', 'The setup file UsetupcatZ is not served from the ROM'),
    ('complex', 'The setup file UsetuprefZ is not served from the ROM'),
    ('temple', 'The setup file UsetupdishZ is not served from the ROM'),
    ('42', 'The bg file bg/bg_sho_all_p.seg is not served from the ROM'),
    ('44', 'The bg file bg/bg_eld_all_p.seg is not served from the ROM'),
    ('47', 'The bg file bg/bg_lue_all_p.seg is not served from the ROM'),
    ('49', 'The bg file bg/bg_rit_all_p.seg is not served from the ROM'),
    ('51', 'The bg file bg/bg_ear_all_p.seg is not served from the ROM'),
    ('52', 'The bg file bg/bg_lee_all_p.seg is not served from the ROM'),
    ('53', 'The bg file bg/bg_lip_all_p.seg is not served from the ROM'),
    ('55', 'The bg file bg/bg_wax_all_p.seg is not served from the ROM'),
    ('56', 'The bg file bg/bg_pam_all_p.seg is not served from the ROM'),
)
STOP_STATUS = 1


def stop_case(stage: str, message: str) -> Case:
    line = 'ge: FATAL: ' + message
    return Case(f'stop-{stage}', 'stops', (('GE_STAGE', stage), ('GE_STAGE_AFTER', '1')), STAGE_FRAMES,
                (Required(line, re.compile(re.escape(line)), near=FATAL),), status=STOP_STATUS)


# ---- menus ------------------------------------------------------------------------------------
# The GE_MENU screens (src/game/front.c, portMenuHarnessTick).
MENUS = ('files', 'select', 'options', 'gameplay', 'control', 'display', 'credits')
MENU_FRAMES = 900


def menu_case(menu: str) -> Case:
    return Case(f'menu-{menu}', 'menus', (('GE_MENU', menu),), MENU_FRAMES,
                (Required('ge: GE_MENU harness: menu=', MENU_HARNESS), frame_limit(MENU_FRAMES)))


# ---- tours ------------------------------------------------------------------------------------
# Rooms visited by GE_LOADTOUR=24, measured on 2026-11-17 at 98a76dd (of the rooms listed: dam 56,
# facility 65, runway 11, surface 35, bunker1 24, silo 65, frigate 42, surface2 37, bunker2 46,
# statue 25, archives 59, streets 54, depot 36, train 50, jungle 44, control 66, caverns 46,
# cradle 29, aztec 55, egypt 26; the arenas, two players, visit all of theirs). Fifteen missions
# end early "(the mission ended there)"; Task 7 finds out why before these become the floor.
TOUR_ROOMS = {
    'dam': 54, 'facility': 50, 'runway': 11, 'surface': 11, 'bunker1': 21, 'silo': 64,
    # surface2's floor of 0: that tour ends at once, so the case proves only the launch, the
    # background and the done line, until a later task (Task 7) finds why.
    'frigate': 35, 'surface2': 0, 'bunker2': 24, 'statue': 6, 'archives': 55, 'streets': 54,
    'depot': 33, 'train': 50, 'jungle': 43, 'control': 57, 'caverns': 45, 'cradle': 28,
    'aztec': 55, 'egypt': 26,
}
TOUR_ARENA_ROOMS = {'temple': 9, 'complex': 23, 'caves': 10, 'library': 28, 'basement': 28, 'stack': 28}
TOUR_FRAMES = 16000
TOUR_ENV = (('GE_LOADTOUR', '24'), ('GE_CHEATS', 'invuln,invis'))


def tour_required(rooms: int) -> tuple[Required, ...]:
    return (Required(f'ge: loadtour: done, at least {rooms} rooms', TOUR_DONE, at_least=rooms),)


def tour_cases() -> list[Case]:
    cases = [Case(f'tour-{name}', 'tours', (('GE_STAGE', name), ('GE_STAGE_AFTER', '1')) + TOUR_ENV,
                  TOUR_FRAMES, LAUNCHED_STAGE + tour_required(rooms))
             for name, rooms in TOUR_ROOMS.items()]
    cases += [Case(f'tour-{arena}-2p', 'tours', (('GE_MP', arena), ('GE_MPPLAYERS', '2')) + TOUR_ENV,
                   TOUR_FRAMES, LAUNCHED_ARENA + tour_required(rooms))
              for arena, rooms in TOUR_ARENA_ROOMS.items()]
    return cases


# ---- the attract loop and long runs -----------------------------------------------------------
ATTRACT_FRAMES = 30000
ALL_CHEATS = 'invuln,invis,guns,turbo,rockets,linemode,enemyff,doors,dualzmg'  # src/game/lv.c


def attract_cases() -> list[Case]:
    limit = frame_limit(ATTRACT_FRAMES)
    return [
        Case('attract', 'attract', (), ATTRACT_FRAMES, (
            Required('ge: demo start: (twice)', DEMO_START, count=2),
            Required('ge: demo exit: totaltime end (timer/limit) (twice)', DEMO_EXIT_TOTALTIME, count=2),
            limit), forbidden=(DEMO_EXIT_OTHER,)),
        Case('soak', 'attract', (('GE_SOAK', 'all:600:1'),), ATTRACT_FRAMES,
             (Required('ge: soak: complete after 1 laps', SOAK_COMPLETE),)),
        Case('death', 'attract', (('GE_MP', 'temple'), ('GE_MPPLAYERS', '4'), ('GE_KILLBOND', '600')),
             ATTRACT_FRAMES, LAUNCHED_ARENA + (limit,)),
        Case('cheats', 'attract', (('GE_STAGE', 'facility'), ('GE_STAGE_AFTER', '1'),
                                   ('GE_CHEATS', ALL_CHEATS)), ATTRACT_FRAMES, LAUNCHED_STAGE + (limit,)),
    ]


SETS = {
    'demos': lambda: [demo_case(name) for name in DEMOS],
    'stages': stage_cases,
    'stops': lambda: [stop_case(stage, message) for stage, message in STOPS],
    'menus': lambda: [menu_case(menu) for menu in MENUS],
    'tours': tour_cases,
    'attract': attract_cases,
}


# The prefixes that keep case names unique where a demo and a mission share a name. `--only`
# also takes a name without its prefix when that selects exactly one case of the sets.
CASE_PREFIXES = ('stage-', 'mp-', 'stop-', 'menu-', 'tour-')


def bare_name(name: str) -> str:
    return next((name[len(prefix):] for prefix in CASE_PREFIXES if name.startswith(prefix)), name)


def select_cases(cases: list[Case], wanted: list[str]) -> list[Case]:
    """The cases of CASES that WANTED names, each by its full name or, when that selects exactly
    one case, by its name without the prefix. Raises ValueError for a name that selects none, or
    more than one (naming them)."""
    chosen = []
    for name in wanted:
        matches = [case for case in cases if name in (case.name, bare_name(case.name))]
        if not matches:
            raise ValueError(f'--only names no case of the sets: {name}')
        if len(matches) > 1:
            raise ValueError(f'--only {name} names more than one case: '
                             + ', '.join(case.name for case in matches))
        chosen.append(matches[0])
    return [case for case in cases if case in chosen]


def cases_of(set_names: list[str]) -> list[Case]:
    """The cases of SET_NAMES in order; raises ValueError naming an unknown set."""
    if not set_names:
        raise ValueError(f'--set names no set; the sets are {", ".join(SETS)}')
    unknown = [name for name in set_names if name not in SETS]
    if unknown:
        raise ValueError(f'unknown set {", ".join(unknown)}; the sets are {", ".join(SETS)}')
    return [case for name in set_names for case in SETS[name]()]
