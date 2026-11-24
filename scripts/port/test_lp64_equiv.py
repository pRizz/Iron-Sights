"""ROM-free tests of lp64_equiv.py's run (LP64-006 Tasks 5 and 9) through stand-in executables
(lp64_equiv_testkit.py). --video and sensitivity: test_lp64_equiv_video.py; the cases and profiles:
test_lp64_equiv_cases.py; compare: test_lp64_equiv_compare.py."""
import hashlib
import json
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import lp64_equiv  # noqa: E402
import lp64_play_run  # noqa: E402
from lp64_equiv_testkit import (COMPARE_LIBM, COMPARE_ON, DAM1_GOOD, DAM_GOOD, LOCKSTEP_ON, SHOTS,  # noqa: E402
                                StandIns, demo_sim, sim)
from lp64_play_cases import Case, frame_limit  # noqa: E402
from lp64_play_route import START_BUTTON, Press, Route, Step, play_line  # noqa: E402
from run_copies import SharedExecutable  # noqa: E402


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


    def test_run_records_its_env_commit_and_executable(self):
        # Arrange
        exe = self.fake(DAM_GOOD)

        # Act
        status, out = self.run_dam(exe, '--env', 'GE_SIMTRACE_DETAIL=1-2')
        record = json.loads((self.output/lp64_equiv.RECORD).read_text())

        # Assert
        self.assertEqual(status, 0, out)
        self.assertEqual(record['env'], {'GE_SIMTRACE_DETAIL': '1-2'})
        self.assertEqual(record['executable_sha256'], hashlib.sha256(exe.read_bytes()).hexdigest())
        self.assertIn('commit', record)
        self.assertIn('worktree_changed', record)


if __name__ == '__main__':
    unittest.main()
