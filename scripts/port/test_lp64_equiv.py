"""ROM-free tests for lp64_equiv.py (LP64-006 Tasks 5 and 9): run and sensitivity through stand-in
executables (shell scripts that print a log, write the simulation trace GE_SIMTRACE names and exit),
as test_lp64_play.py does, and the cases and profiles as data. compare: test_lp64_equiv_compare.py."""
import contextlib
import io
import json
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402
import lp64_equiv_cases  # noqa: E402
import lp64_equiv_profile  # noqa: E402
import lp64_play_run  # noqa: E402
from lp64_play_cases import Case, frame_limit  # noqa: E402
from lp64_play_route import START_BUTTON, Z_TRIG, Press, Route, Step, play_line, route_text  # noqa: E402
from run_copies import SharedExecutable  # noqa: E402
import settings_placement  # noqa: E402

COMPARE_ON = 'ge: compare: on\n'
COMPARE_LIBM = 'ge: compare: on (libm)\n'
LOCKSTEP_ON = 'ge: lockstep: on\n'
DAM_LAUNCH = ('ge: GE_STAGE direct launch: levelid=33 diff=0\n'
              'ge: load_bg_file levelid=33 stan=Tbg_dam_all_p_stanZ file=0x1 record=0x2 first=0x3\n')
MOVED = 'ge: play: tick 600\nge: play: moved 102\n'
SHOTS = ''.join(f'ge: play: tick {640 + 120 * shot}\nge: play: shots {shot}\n' for shot in range(1, 31))
FRAMES = 'ge: frame limit reached: frames=3900 gfxtasks=3897\n'
DAM_GOOD = COMPARE_ON + LOCKSTEP_ON + DAM_LAUNCH + MOVED + SHOTS + FRAMES
DAM1_GOOD = (COMPARE_ON + 'ge: demo start: stage=33 diff=0 pads=2 totaltime=1200 vsyncs (~20s)\n'
             'ge: GE_DEMO direct playback: dam1 (index 0)\n'
             'ge: demo exit: totaltime end (timer/limit) at tick 438, gametime 1300 vsyncs (~21s), detail 514/514\n'
             'ge: frame limit reached: frames=3000 gfxtasks=1500\n')


AUDIO_LOCKED = 'ge: audio: locked to game fields\n'


def wav(*samples: tuple[int, int]) -> bytes:
    """A 22,050 Hz stereo recording of SAMPLES (left, right), as port_audio_capture.c writes one."""
    data = b''.join(struct.pack('<hh', left, right) for left, right in samples)
    return (b'RIFF' + struct.pack('<I', 36 + len(data)) + b'WAVEfmt '
            + struct.pack('<IHHIIHH', 16, 1, 2, 22050, 22050 * 4, 4, 16) + b'data' + struct.pack('<I', len(data))
            + data)


def sim_line(tick: int, seed: str = '0000000000000001') -> str:
    return (f'T n={tick} timer={tick} seed={seed} draws=0 players=1 '
            'p0=00000000,00000000,00000000,00000000,00000000 chrs=0:00000000 props=0:00000000 rooms=0:00000000\n')


def demo_sim(first_n: int, timer_shift: int = 0, demo_ticks: int = 3) -> str:
    """A paced demo's trace: lines before the replay from 1 to FIRST_N - 1, then DEMO_TICKS replay
    lines (demo=1, 2, ...), then one after it; every timer shifted by TIMER_SHIFT."""
    lines = [sim_line(n).replace(f'timer={n} ', f'timer={n + timer_shift} ') for n in range(1, first_n + demo_ticks + 1)]
    for tick in range(1, demo_ticks + 1):
        lines[first_n - 2 + tick] = lines[first_n - 2 + tick].replace('\n', f' demo={tick}\n')
    return ''.join(lines)


def sim(ticks: int = 3, seed_at: dict[int, str] | None = None) -> str:
    seeds = seed_at or {}
    return ''.join(sim_line(tick, seeds.get(tick, '0000000000000001')) for tick in range(1, ticks + 1))


class StandIns(unittest.TestCase):
    """A temporary folder with a stand-in ROM, stand-in executables and an output folder."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.folder = Path(self.tmp.name)
        self.rom = self.folder/'rom.z64'
        self.rom.write_bytes(b'not a rom')
        self.output = self.folder/'out'

    def canned(self, name: str, text: str) -> Path:
        path = self.folder/name
        path.write_text(text)
        return path

    def fake(self, log: str, trace: str = sim(), script: str = '') -> Path:
        """A stand-in executable: runs SCRIPT, writes TRACE to $GE_SIMTRACE, prints LOG, exits 0."""
        number = len(list(self.folder.iterdir()))
        log_path = self.canned(f'canned{number}.log', log)
        trace_path = self.canned(f'canned{number}.sim', trace)
        path = self.folder/f'IronSights{number}'
        path.write_text(f'#!/bin/sh\n{script}\n[ -n "$GE_SIMTRACE" ] && cp "{trace_path}" "$GE_SIMTRACE"\n'
                        f'cat "{log_path}" >&2\nexit 0\n')
        path.chmod(path.stat().st_mode | stat.S_IXUSR)
        return path

    def equiv(self, *argv: str) -> tuple[int, str]:
        out = io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            try:
                status = lp64_equiv.main(list(argv))
            except SystemExit as stop:
                status = stop.code
        return status, out.getvalue()

    def run_dam(self, executable: Path, *extra: str) -> tuple[int, str]:
        return self.equiv('run', '--rom', str(self.rom), '--set', 'solo', '--only', 'dam', '--jobs', '2', *extra,
                          '--output', str(self.output), str(executable))


class Run(StandIns):
    def test_run_fails_a_case_without_the_lockstep_line(self):
        # Arrange
        exe = self.fake(DAM_GOOD.replace(LOCKSTEP_ON, ''))

        # Act
        status, out = self.run_dam(exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (missing line: ge: lockstep: on)', out)

    def test_run_fails_a_route_that_fired_nothing(self):
        # Arrange
        exe = self.fake(DAM_GOOD.replace(SHOTS, ''))

        # Act
        status, out = self.run_dam(exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (missing line: ge: play: shots N (at least 15))', out)

    def test_a_good_case_ran_with_its_ticks_and_shots(self):
        # Arrange
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.run_dam(exe)

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('stage-dam: RAN (3 ticks, 30 shots)', out)
        self.assertIn('OK: 1 of 1 cases ran', out)

    def test_repeat_reports_a_case_that_differs_from_itself(self):
        # Arrange: the second run's trace (after the build check and run 1) has another seed at tick 2
        other = self.canned('other.sim', sim(seed_at={2: '00000000000000ff'}))
        counter = self.folder/'runs'
        log = self.canned('run2.log', DAM_GOOD)
        exe = self.fake(DAM_GOOD, script=f'echo x >> "{counter}"\n'
                                         f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then '
                                         f'cp "{other}" "$GE_SIMTRACE"; cat "{log}" >&2; exit 0; fi')

        # Act
        status, out = self.run_dam(exe, '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('OK: 1 of 1 cases ran', out)
        self.assertIn('FAIL: stage-dam sim differs from itself at 2', out)

    def run_demo(self, executable: Path, *extra: str) -> tuple[int, str]:
        return self.equiv('run', '--rom', str(self.rom), '--set', 'demos', '--only', 'dam1', '--jobs', '2', *extra,
                          '--output', str(self.output), str(executable))

    def test_run_fails_a_demo_without_its_playback(self):
        # Arrange
        exe = self.fake(DAM1_GOOD)

        # Act
        status, out = self.run_demo(exe)

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('dam1: FAIL (sim-demo unreadable:', out)
        self.assertIn('no line of a replay (demo=)', out)

    def test_a_demo_agrees_with_itself_whatever_the_pace(self):
        # Arrange: run 2 boots one tick later, with another timer, then replays the same
        other = self.canned('paced.sim', demo_sim(first_n=3, timer_shift=5))
        counter = self.folder/'runs'
        log = self.canned('run2.log', DAM1_GOOD)
        exe = self.fake(DAM1_GOOD, trace=demo_sim(first_n=2),
                        script=f'echo x >> "{counter}"\n'
                               f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then '
                               f'cp "{other}" "$GE_SIMTRACE"; cat "{log}" >&2; exit 0; fi')

        # Act
        status, out = self.run_demo(exe, '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: RAN (3 ticks, 0 shots)', out)
        self.assertIn('OK: 1 of 1 cases agree with themselves', out)

    def test_plain_check_refuses_a_libm_build(self):
        # Arrange
        exe = self.fake(DAM_GOOD.replace(COMPARE_ON, COMPARE_LIBM))

        # Act
        status, out = self.run_dam(exe)

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('ge: compare: on (libm)', out)
        self.assertIn('--maths-off', out)

    def test_maths_off_refuses_a_plain_build(self):
        # Arrange
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.run_dam(exe, '--maths-off')

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('lacks the line ge: compare: on (libm)', out)

    def test_saves_set_needs_saves_folder(self):
        # Arrange
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.equiv('run', '--rom', str(self.rom), '--set', 'saves', '--output', str(self.output),
                                 str(exe))

        # Assert
        self.assertEqual(status, 2, out)
        self.assertIn('the saves set needs --saves DIR', out)

    def test_saves_reach_the_recording_runs_of_a_route_of_steps(self):
        # Arrange: the stand-in shows the menu its step waits for only with the saves folder's file
        saves = self.folder/'saves'
        (saves/'Data'/'Saves').mkdir(parents=True)
        (saves/'Data'/'Saves'/'eeprom.bin').write_bytes(bytes(512))
        exe = self.fake('ge: frame limit reached: frames=60 gfxtasks=30\n',
                        script='[ -f Data/Saves/eeprom.bin ] && printf "ge: play: tick 5\\nge: play: menu 7\\n" >&2')
        step = Step(Press(START_BUTTON), 10, play_line('menu', 7), 'ge: play: menu 7')
        case = Case('saves-steps', 'saves', (), 60, (frame_limit(60),), route=Route(steps=(step,)))
        self.output.mkdir()

        # Act
        with SharedExecutable(exe) as shared:
            outcome = lp64_play_run.run_case(case, shared, self.rom, {}, self.output, maybe_saves=saves)

        # Assert
        self.assertEqual(outcome.problems, [])
        self.assertEqual(outcome.route_ticks, [15])

    def test_a_run_that_changes_a_saves_file_fails_naming_it(self):
        # Arrange: the stand-in rewrites the save it was given
        saves = self.folder/'saves'
        (saves/'Data'/'Saves').mkdir(parents=True)
        (saves/'Data'/'Saves'/'eeprom.bin').write_bytes(bytes(512))
        exe = self.fake('ge: frame limit reached: frames=60 gfxtasks=30\n',
                        script='printf x | dd of=Data/Saves/eeprom.bin bs=1 seek=7 conv=notrunc 2>/dev/null')
        case = Case('saves-write', 'saves', (), 60, (frame_limit(60),))
        self.output.mkdir()

        # Act
        with SharedExecutable(exe) as shared:
            outcome = lp64_play_run.run_case(case, shared, self.rom, {}, self.output, maybe_saves=saves)

        # Assert
        self.assertEqual(outcome.problems, ['saves: the run changed Data/Saves/eeprom.bin (first at byte 7)'])

    def test_env_refuses_runner_variables(self):
        # Arrange
        exe = self.fake(DAM_GOOD)
        refused = ('GE_LOCKSTEP', 'GE_FIXEDTICK', 'GE_NOSLEEP', 'GE_SIMTRACE', 'GE_GFXDIGEST',
                   'GE_GFXDIGEST_NODRAW', 'GE_AUDIO_LOCK', 'GE_AUDIO_CAPTURE', 'GE_SEED', 'GE_IRONSIGHTS_FONTS')

        # Act
        results = {name: self.run_dam(exe, '--env', f'{name}=1') for name in refused}

        # Assert
        for name, (status, out) in results.items():
            self.assertEqual(status, 2, f'{name}: {out}')
            self.assertIn(f'--env {name} is refused', out)

    def test_env_passes_the_detail_variables(self):
        # Arrange
        exe = self.fake(DAM_GOOD, script='[ -z "$GE_SIMTRACE" ] || { [ "$GE_SIMTRACE_DETAIL" = 1-2 ] '
                                         '&& [ "$GE_GFXDIGEST_DETAIL" = 3 ]; } || exit 9')

        # Act
        status, out = self.run_dam(exe, '--env', 'GE_SIMTRACE_DETAIL=1-2', '--env', 'GE_GFXDIGEST_DETAIL=3')

        # Assert
        self.assertEqual(status, 0, out)

    def test_run_records_each_case_verdict_after_the_agreement_check(self):
        # Arrange: run 2's trace (after the build check and run 1) has another seed at tick 2
        other = self.canned('other.sim', sim(seed_at={2: '00000000000000ff'}))
        counter = self.folder/'runs'
        log = self.canned('run2.log', DAM_GOOD)
        exe = self.fake(DAM_GOOD, script=f'echo x >> "{counter}"\n'
                                         f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then '
                                         f'cp "{other}" "$GE_SIMTRACE"; cat "{log}" >&2; exit 0; fi')

        # Act
        self.run_dam(exe, '--repeat', '2', '--jobs', '1')
        record = json.loads((self.output/lp64_equiv.RECORD).read_text())

        # Assert
        self.assertEqual(record['repeat'], 2)
        self.assertEqual(record['verdicts'], {'stage-dam': {'ran': True, 'agreed': False}})


class Video(StandIns):
    """`run --video` through a stand-in that writes the picture digest and the recording when named."""
    VIDEO_LOG = DAM_GOOD.replace(LOCKSTEP_ON, LOCKSTEP_ON + 'ge: video: window + GL context up\n' + AUDIO_LOCKED)
    GFX = 'F n=1 tick=1 draws=1 tris=2 vtx=3bee89fb tex=0:811c9dc5 state=0383debb post=e5d4f569\n'

    def capture(self, recording: bytes = wav((1, -1), (2, -2))) -> str:
        """A script line that writes RECORDING when the runner names a recording."""
        path = self.folder/f'canned{len(list(self.folder.iterdir()))}.wav'
        path.write_bytes(recording)
        return f'[ -n "$GE_AUDIO_CAPTURE" ] && cp "{path}" "$GE_AUDIO_CAPTURE"\n'

    def write_streams(self, recording: bytes = wav((1, -1), (2, -2))) -> str:
        """A script that writes the digest and RECORDING when the runner names them."""
        return f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX}" > "$GE_GFXDIGEST"\n' + self.capture(recording)

    def env_seen(self) -> Path:
        return self.folder/'env.txt'

    def video_fake(self, script: str | None = None, log: str = VIDEO_LOG) -> Path:
        """A stand-in that runs SCRIPT (by default write_streams') and, in a case's runs, saves its GE_
        variables and midas.ini."""
        keep = (f'[ -n "$GE_SIMTRACE" ] && {{ env | grep "^GE_" | sort > "{self.env_seen()}"; '
                f'cat midas.ini > "{self.folder/"ini.txt"}"; }}')
        return self.fake(log, script=f'{self.write_streams() if script is None else script}\n{keep}')

    def seen(self) -> dict[str, str]:
        return dict(line.split('=', 1) for line in self.env_seen().read_text().splitlines())

    def test_video_names_the_digest_and_recording_and_skips_drawing(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video')

        # Assert
        self.assertEqual(status, 0, out)
        env = self.seen()
        self.assertTrue(env['GE_GFXDIGEST'].endswith('/run1/stage-dam.gfx'))
        self.assertTrue(env['GE_AUDIO_CAPTURE'].endswith('/run1/stage-dam.wav'))
        self.assertEqual(env['GE_AUDIO_LOCK'], '1')
        self.assertEqual(env['GE_GFXDIGEST_NODRAW'], '1')

    def test_draw_leaves_drawing_on(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video', '--draw')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertNotIn('GE_GFXDIGEST_NODRAW', self.seen())

    def test_video_run_fails_without_its_digest_and_recording(self):
        # Act
        status, out = self.run_dam(self.video_fake(script=''), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (gfx: no stage-dam.gfx)', out)

    def test_video_run_fails_a_torn_digest(self):
        # Act
        script = (f'[ -n "$GE_GFXDIGEST" ] && printf "%s" "{self.GFX.strip()}" > "$GE_GFXDIGEST"\n'
                  + self.capture())
        status, out = self.run_dam(self.video_fake(script=script), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (gfx unreadable:', out)
        self.assertIn('the last line has no newline', out)

    def test_video_run_fails_a_recording_that_is_not_a_wav(self):
        # Act
        script = (f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX}" > "$GE_GFXDIGEST"\n'
                  '[ -n "$GE_AUDIO_CAPTURE" ] && printf wav > "$GE_AUDIO_CAPTURE"\n')
        status, out = self.run_dam(self.video_fake(script=script), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (snd unreadable:', out)

    def test_video_run_fails_without_the_sound_lock(self):
        # Act
        status, out = self.run_dam(self.video_fake(log=self.VIDEO_LOG.replace(AUDIO_LOCKED, '')), '--video')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('stage-dam: FAIL (missing line: ge: audio: locked to game fields)', out)

    def test_video_agreement_fails_a_recording_that_differs_from_itself(self):
        # Arrange: the second run of the case (the third start, after the build check) records another sample
        counter = self.folder/'runs'
        first, second = self.capture(wav((1, -1), (2, -2))), self.capture(wav((1, -1), (2, 9)))
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then\n{second}else\n{first}fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX.strip()}\\n" > "$GE_GFXDIGEST"\n')

        # Act
        status, out = self.run_dam(self.video_fake(script=script), '--video', '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: stage-dam snd differs from itself at 1 (R frame=0 field=1, run2)', out)

    def test_video_agreement_leaves_a_demo_recording_unjudged_and_says_why(self):
        # Arrange: a paced demo's recording differs between runs; its playback does not
        counter = self.folder/'runs'
        first, second = self.capture(wav((1, -1))), self.capture(wav((1, -1), (2, -2)))
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then\n{second}else\n{first}fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{self.GFX.strip()} demo=1\\n" > "$GE_GFXDIGEST"\n')
        exe = self.fake(DAM1_GOOD.replace(COMPARE_ON, COMPARE_ON + 'ge: video: window + GL context up\n' + AUDIO_LOCKED),
                        trace=demo_sim(2), script=script)

        # Act
        status, out = self.equiv('run', '--rom', str(self.rom), '--set', 'demos', '--only', 'dam1', '--video',
                                 '--repeat', '2', '--jobs', '1', '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('dam1: snd not judged: ' + lp64_equiv.DEMO_SND_REASON, out)
        self.assertIn('OK: 1 of 1 cases agree with themselves', out)

    def test_video_agreement_fails_a_digest_that_differs_from_itself(self):
        # Arrange: the second run of the case (the third start, after the build check) draws another frame
        counter = self.folder/'runs'
        other = self.GFX.replace('vtx=3bee89fb', 'vtx=00000000')
        script = (f'echo x >> "{counter}"\n'
                  f'if [ "$(wc -l < "{counter}")" -gt 2 ]; then g="{other}"; else g="{self.GFX}"; fi\n'
                  '[ -n "$GE_GFXDIGEST" ] && printf "$g" > "$GE_GFXDIGEST"\n'
                  + self.capture())

        # Act
        status, out = self.run_dam(self.video_fake(script=script), '--video', '--repeat', '2', '--jobs', '1')

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: stage-dam gfx differs from itself at 1 (vtx, run2)', out)

    def test_video_env_refuses_what_the_video_runner_sets(self):
        # Arrange
        exe = self.video_fake()

        # Act
        results = {name: self.run_dam(exe, '--video', '--env', f'{name}=1') for name in ('GE_SHOT', 'HOME')}

        # Assert
        for name, (status, out) in results.items():
            self.assertEqual(status, 2, f'{name}: {out}')
            self.assertIn(f'--env {name} is refused with --video', out)

    def test_all_profile_writes_its_ini_and_loads_its_fonts(self):
        # Act
        status, out = self.run_dam(self.video_fake(), '--video', '--profile', 'all')

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual((self.folder/'ini.txt').read_text(), lp64_equiv_profile.profile_ini('all'))
        self.assertEqual(self.seen()['GE_IRONSIGHTS_FONTS'], '1')


class Sensitivity(StandIns):
    def test_sensitivity_fails_when_two_seeds_agree(self):
        # Arrange: the trace ignores GE_SEED
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.equiv('sensitivity', '--rom', str(self.rom), '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 1, out)
        self.assertIn('FAIL: sim is the same between seeds', out)

    def test_sensitivity_passes_when_the_seed_changes_the_trace(self):
        # Arrange
        other = self.canned('other.sim', sim(seed_at={1: '0000000000000002'}))
        log = self.canned('seed2.log', DAM_GOOD)
        exe = self.fake(DAM_GOOD, script=f'if [ "$GE_SEED" = 2 ]; then cp "{other}" "$GE_SIMTRACE"; '
                                         f'cat "{log}" >&2; exit 0; fi')

        # Act
        status, out = self.equiv('sensitivity', '--rom', str(self.rom), '--output', str(self.output), str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: sim differs between seeds', out)

    def test_video_sensitivity_judges_sim_gfx_and_snd(self):
        # Arrange: GE_SEED=2 changes the trace, the digest and the recording
        other = self.canned('other.sim', sim(seed_at={1: '0000000000000002'}))
        gfx = Video.GFX
        log = self.canned('seed2.log', Video.VIDEO_LOG)
        one, two = self.canned('one.wav', ''), self.canned('two.wav', '')
        one.write_bytes(wav((1, -1)))
        two.write_bytes(wav((1, 5)))
        script = ('if [ "$GE_SEED" = 2 ]; then '
                  f'cp "{other}" "$GE_SIMTRACE"; printf "{gfx.replace("vtx=3bee89fb", "vtx=00000000")}" > "$GE_GFXDIGEST"; '
                  f'cp "{two}" "$GE_AUDIO_CAPTURE"; cat "{log}" >&2; exit 0; fi\n'
                  f'[ -n "$GE_GFXDIGEST" ] && printf "{gfx}" > "$GE_GFXDIGEST"\n'
                  f'[ -n "$GE_AUDIO_CAPTURE" ] && cp "{one}" "$GE_AUDIO_CAPTURE"\n')
        exe = self.fake(Video.VIDEO_LOG, script=script)

        # Act
        status, out = self.equiv('sensitivity', '--video', '--rom', str(self.rom), '--output', str(self.output),
                                 str(exe))

        # Assert
        self.assertEqual(status, 0, out)
        self.assertIn('OK: sim, gfx and snd differ between seeds', out)


class Cases(unittest.TestCase):
    def test_route_uses_pad_records_only(self):
        # Arrange
        case = lp64_equiv_cases.solo_case('dam')
        side = lp64_equiv_cases.STRAIGHT_TICKS  # the first side leg's first tick

        # Act
        lines = route_text(case.route.presses).splitlines()

        # Assert
        self.assertFalse([line for line in lines if not line.startswith('P ')], 'no M record')
        self.assertFalse([line for line in lines if int(line.split()[2], 16) & ~Z_TRIG], 'no C button')
        self.assertIn(f'P {side} 0 {-lp64_equiv_cases.SIDE_STICK_X & 0xFF:x} 0', lines)

    def test_unarmed_and_uncontrolled_cases_need_less_than_the_floor(self):
        # Act
        bunker2 = [item.label for item in lp64_equiv_cases.solo_case('bunker2').required]
        cuba = [item.label for item in lp64_equiv_cases.solo_case('cuba').required]
        dam = [item.label for item in lp64_equiv_cases.solo_case('dam').required]

        # Assert
        self.assertIn('ge: play: moved N (Bond walked)', bunker2)
        self.assertFalse([label for label in bunker2 + cuba if 'shots' in label])
        self.assertNotIn('ge: play: moved N (Bond walked)', cuba)
        self.assertIn(f'ge: play: shots N (at least {lp64_equiv_cases.SHOT_FLOOR})', dam)

    def test_no_armed_case_has_a_floor_of_zero(self):
        # Assert
        self.assertFalse([name for name, (floor, _) in lp64_equiv_cases.LOW_FLOORS.items() if floor < 1])

    def test_demo_cases_never_get_lockstep(self):
        # Arrange
        demos = lp64_equiv_cases.SETS['demos']()

        # Act
        envs = [lp64_play_run.case_environment(case, Path('rom'), {}, base={}) for case in demos]

        # Assert
        self.assertEqual(len(demos), 14)
        self.assertFalse([case.name for case in demos if case.lockstep])
        self.assertFalse([env for env in envs if 'GE_LOCKSTEP' in env])

    def test_there_are_38_cases(self):
        # Act
        cases = lp64_equiv.cases_of(['demos', 'solo', 'mp', 'menus', 'reload'])

        # Assert
        self.assertEqual(len(cases), 38)
        self.assertEqual(len({case.name for case in cases}), 38)

    def test_pinned_routes_match_their_steps(self):
        # Assert
        self.assertEqual(len(lp64_equiv_cases.WALK_TICKS), len(lp64_equiv_cases.WALK_STEPS))
        self.assertEqual(len(lp64_equiv_cases.DAM_TWICE_TICKS), len(lp64_equiv_cases.DAM_TWICE_STEPS))
        self.assertEqual(len(lp64_equiv_cases.SAVES_TICKS), len(lp64_equiv_cases.SAVES_STEPS))

    def test_the_saves_set_is_one_lockstep_case_outside_the_38(self):
        # Act
        cases = lp64_equiv.cases_of(['saves'])

        # Assert
        self.assertEqual([(case.name, case.lockstep) for case in cases], [('saves-depot', True)])
        self.assertNotIn('saves-depot', {case.name for case in lp64_equiv.cases_of(
            ['demos', 'solo', 'mp', 'menus', 'reload'])})


class Profiles(unittest.TestCase):
    def test_all_profile_covers_every_fork_key(self):
        # Arrange
        source = (settings_placement.ROOT/settings_placement.OPTIONS).read_text()
        fork = settings_placement.option_keys(source) - settings_placement.read_upstream()

        # Act
        covered = set(lp64_equiv_profile.ALL_ON) | set(lp64_equiv_profile.EXCLUDED)

        # Assert
        self.assertEqual(covered, fork)
        self.assertFalse(set(lp64_equiv_profile.ALL_ON) & set(lp64_equiv_profile.EXCLUDED))

    def test_all_profile_keeps_fonts_local_and_packs_out(self):
        # Act
        ini = lp64_equiv_profile.profile_ini('all')

        # Assert
        self.assertEqual(lp64_equiv_profile.ALL_ON['font_style'], 2)
        self.assertIn('texture_pack', lp64_equiv_profile.EXCLUDED)
        self.assertIn('font_style=2\n', ini)
        self.assertNotIn('texture_pack=', ini)

    def test_all_profile_lets_its_font_style_load(self):
        # Assert
        self.assertEqual(lp64_equiv_profile.profile_env('all'), {'GE_IRONSIGHTS_FONTS': '1'})
        self.assertEqual(lp64_equiv_profile.profile_env('default'), {})

    def test_default_profile_runs_windowed(self):
        # Assert
        self.assertEqual(lp64_equiv_profile.profile_ini('default'), '[display]\nfullscreen=windowed\n')


if __name__ == '__main__':
    unittest.main()
