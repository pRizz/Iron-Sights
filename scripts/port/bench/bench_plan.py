"""Plan benchmark runs: settings, environment, routes, run order, host checks. Pure.

bench.py does the launching; everything here is a function of its arguments so
the ROM-free tests can check it.
"""
import csv
import json

GAME_PROCESSES = ('ironsights.exe', 'projectmidas.exe')
WEB_FRAME_LOG = '/tmp/bench-frames.csv'
WEB_ROUTE = '/tmp/bench-route.rec'
# Harness settings every run uses, whatever the preset: a window, no vsync and
# no frame cap, so the frame rate is the work and nothing else.
BASE_INI = ('[display]', 'fullscreen=window', 'vsync=0', 'maxfps=0')
# Replay counts ticks from process start, a few before the mission's own count.
ROUTE_MARGIN_TICKS = 600
BUTTON_FIRE = 0x2000  # Z trigger
# A stuck run is bounded by the runner's timeout; this only stops the pump.
FRAME_BACKSTOP = '2000000000'


def load_config(text):
    """Parse scenarios.json, filling each scenario from the defaults."""
    config = json.loads(text)
    if config.get('schema') != 1:
        raise ValueError('scenarios.json: unsupported schema')
    defaults = config['defaults']
    scenarios = {}
    for name, entry in config['scenarios'].items():
        scenario = dict(defaults)
        scenario.update(entry)
        scenario['name'] = name
        if ('stage' in scenario) == ('multiplayer' in scenario):
            raise ValueError(f'{name}: give either a stage or a multiplayer arena')
        if ('route' in scenario) == ('route_file' in scenario):
            raise ValueError(f'{name}: give either a route or a route_file')
        if 'route' in scenario:
            if scenario['route'] not in config['routes']:
                raise ValueError(f"{name}: unknown route {scenario['route']}")
            scenario['route'] = config['routes'][scenario['route']]
        scenarios[name] = scenario
    return {'scenarios': scenarios, 'presets': config['presets']}


def ini_text(preset_lines):
    """The run's midas.ini: harness settings, then the preset's overrides."""
    return '\n'.join(BASE_INI+tuple(preset_lines))+'\n'


def end_tick(scenario):
    return scenario['warmup_ticks']+scenario['ticks']


def common_env(scenario, rom, resolution):
    env = {
        'SDL_AUDIODRIVER': 'dummy',  # silent; the synthesiser still runs
        'GE_ROM': str(rom), 'GE_WINDOW': resolution, 'GE_NOINPUT': '1', 'GE_SEED': str(scenario['seed']),
        'GE_NOINISAVE': '1', 'GE_NOMSGBOX': '1', 'GE_FRAMES': FRAME_BACKSTOP,
    }
    if 'stage' in scenario:
        env.update(GE_STAGE=scenario['stage'], GE_STAGE_AFTER='1', GE_SKIPINTRO='1')
    else:
        arena = scenario['multiplayer']
        env.update(GE_MP=arena['arena'], GE_MPPLAYERS=str(arena['players']))
    if scenario.get('cheats'):
        env['GE_CHEATS'] = scenario['cheats']
    return env


def run_env(scenario, rom, resolution, log_path, route):
    """Environment of a measured run: fixed tick, unpaced, replaying the route.

    Replay drives player 1 only. The game's own scripted drivers (GE_DRIVE,
    GE_MPDRIVE) count controller polls, which outrun game ticks in an unpaced
    run, so they would give different input every time.
    """
    env = common_env(scenario, rom, resolution)
    env.update(GE_BENCH=str(log_path), GE_BENCH_END=str(end_tick(scenario)),
               GE_FIXEDTICK='1', GE_NOSLEEP='1', GE_TICKHZ='0', GE_INPUTPLAY=str(route))
    return env


def pad_at(route, tick):
    """(buttons, stick_y) the route holds at a replay tick."""
    buttons = 0
    maybe_fire = route.get('fire')
    if maybe_fire is not None and tick >= maybe_fire['from']:
        if (tick-maybe_fire['from']) % maybe_fire['every'] < maybe_fire['hold']:
            buttons = BUTTON_FIRE
    return buttons, route['forward']


def yaw_at(route, tick):
    """Degrees the camera turns at a replay tick: a straight leg, then a turn."""
    leg = route['straight_ticks']+route['turn_ticks']
    index, phase = divmod(tick, leg)
    if phase < route['straight_ticks']:
        return 0.0
    turns = route['turn_degrees']
    return turns[index % len(turns)]/route['turn_ticks']


def synth_route(route, ticks):
    """A GE_INPUTPLAY file for a route description: walk, turn, fire.

    Pad records are written where the pad changes (replay holds the last one)
    and end with a neutral pad; a mouse-look record carries each turning tick's
    yaw in degrees. Every value is a function of the tick, so a route is the
    same input for any build on any machine.
    """
    lines, last_pad = [], None
    for tick in range(1, ticks+1):
        buttons, stick_y = pad_at(route, tick)
        pad = f'{buttons:x} 0 {stick_y & 0xFF:x}'
        if pad != last_pad:
            lines.append(f'P {tick} {pad}')
            last_pad = pad
        yaw = yaw_at(route, tick)
        if yaw != 0.0:
            lines.append(f'M {tick} 1 0 0 {yaw:.6g} 0')
    lines.append(f'P {ticks+1} 0 0 0')
    return '\n'.join(lines)+'\n'


def route_text(scenario):
    """The scenario's generated route, long enough to cover the measured window."""
    return synth_route(scenario['route'], end_tick(scenario)+ROUTE_MARGIN_TICKS)


def web_runs(config, scenarios, presets, repeats):
    """The same runs for the browser build, as data its runner can replay.

    Paths are inside the page's in-memory file system. Settings the browser has
    no use for (the ROM path, the window size, the message-box and sound-device
    switches) are left out; its runner supplies the cartridge itself.
    """
    runs = []
    for preset in presets:
        for repeat, name, _ in run_order(['browser'], scenarios, repeats):
            scenario = config['scenarios'][name]
            if 'route_file' in scenario:
                raise ValueError(f'{name}: the browser runner takes generated routes only')
            env = run_env(scenario, '', '', WEB_FRAME_LOG, WEB_ROUTE)
            for key in ('GE_ROM', 'GE_WINDOW', 'GE_NOMSGBOX', 'SDL_AUDIODRIVER'):
                del env[key]
            runs.append({'preset': preset, 'scenario': name, 'repeat': repeat, 'env': env,
                         'ini': ini_text(config['presets'][preset]), 'route': route_text(scenario),
                         'frame_log': WEB_FRAME_LOG, 'route_path': WEB_ROUTE,
                         'first_tick': scenario['warmup_ticks'], 'end_tick': end_tick(scenario)})
    return runs


def web_log_name(run):
    """File the browser runner saves a run's frame log under."""
    return f"{run['preset']}-{run['scenario']}-{run['repeat']}.csv"


def other_games(tasklist_csv, own_pid):
    """Game processes in `tasklist /fo csv /nh` output, other than our own run.

    Another session's audit or the player's game takes the GPU and skews a run.
    """
    found = []
    for row in csv.reader(tasklist_csv.splitlines()):
        if len(row) >= 2 and row[0].lower() in GAME_PROCESSES and row[1] != str(own_pid):
            found.append(row[0])
    return found


def busy_fraction(before, after):
    """Share of all processor time in use between two (idle, kernel, user) samples.

    Windows counts idle time inside kernel time.
    """
    idle = after[0]-before[0]
    total = (after[1]-before[1])+(after[2]-before[2])
    if total <= 0:
        return None
    return max(0.0, 1.0-idle/total)


def run_order(builds, scenarios, repeats):
    """Every (repeat, scenario, build), builds interleaved so drift in the
    machine's speed over a session lands on all of them alike."""
    return [(repeat, scenario, build)
            for repeat in range(1, repeats+1) for scenario in scenarios for build in builds]
