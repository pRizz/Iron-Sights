"""Check the late-load audit's pure parts: log parsing, the load split, findings, learning."""
import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent/'bench'))

import bench_metrics as metrics  # noqa: E402
import late_load_audit as audit  # noqa: E402
import late_load_report as report  # noqa: E402

HEADER = '# ge-bench 1 tickhz=0 render_thread=1 fixedtick=1 nosleep=1 reason=end\n'+','.join(metrics.COLUMNS)+'\n'


def frame_line(stage, interval, tick, uploads=0, shaders=0):
    return f'{interval},{interval},0,0,100,0,0,100,{stage},{tick},0,0,0,{shaders},{uploads}\n'


def frames(rows, stage=35):
    """Frame log rows of (interval, tick[, uploads[, shaders]]) in microseconds."""
    return metrics.parse_log(HEADER+''.join(frame_line(stage, *row) for row in rows))[1]


LATELOG = '''# ge-latelog 1: L <stage> <tick> <kind> <us> <what>; S <stage> <tick> = load ended
S 90 4
L 90 10 shaders 900 0000000010001000 10000001
S 35 5
L 35 40 uploads 6000 fmt 2 siz 1 256x256 name 0 us | hd 256x256 lookup 120 us
L 35 41 shaders 3100 020d020d818a818a 10000711
L 35 41 models 1500 CboilerbondZ
'''


class ParseTests(unittest.TestCase):
    def test_latelog_gives_load_ends_and_events(self):
        # Act
        loaded, events = report.parse_latelog(LATELOG)

        # Assert
        self.assertEqual(loaded, [(90, 4), (35, 5)])
        self.assertEqual([event.kind for event in events], ['shaders', 'uploads', 'shaders', 'models'])
        self.assertEqual(events[1].what, 'fmt 2 siz 1 256x256 name 0 us | hd 256x256 lookup 120 us')

    def test_an_old_load_line_without_a_tick_still_parses(self):
        # Act
        loaded, _ = report.parse_latelog('# ge-latelog 1\nS 33\n')

        # Assert
        self.assertEqual(loaded, [(33, None)])

    def test_other_files_are_refused(self):
        with self.assertRaises(ValueError):
            report.parse_latelog('# ge-bench 1\n')


class SummaryTests(unittest.TestCase):
    def test_the_hidden_warm_up_frames_count_as_the_load(self):
        # Arrange: three frames drawn before the first one shown at tick 5
        rows = [(700000, 2), (190000, 3), (9000, 4), (8000, 5), (30000, 40, 6000), (8000, 41, 0, 3100)]

        # Act
        loading, played = report.split_load(frames(rows), 5)

        # Assert
        self.assertEqual([frame.tick for frame in loading], [2, 3, 4])
        self.assertEqual([frame.tick for frame in played], [5, 40, 41])

    def test_with_no_load_tick_the_first_frame_is_the_load(self):
        # Act
        loading, played = report.split_load(frames([(700000, 2), (8000, 3)]), None)

        # Assert
        self.assertEqual(len(loading), 1)
        self.assertEqual(len(played), 1)

    def test_summary_counts_late_work_and_frames_over_budget(self):
        # Arrange
        _, events = report.parse_latelog(LATELOG)
        rows = [(700000, 2), (8000, 5), (30000, 40, 6000), (8000, 41, 0, 3100)]

        # Act
        summary = report.summarize_run('runway', 35, frames(rows), events, 4000, 5)

        # Assert
        self.assertAlmostEqual(summary['load_ms'], 700.0)
        self.assertEqual(summary['late_count'], 3)
        self.assertEqual(summary['by_kind']['shaders']['count'], 1)
        self.assertEqual(summary['frames_over_budget'], 1)
        self.assertAlmostEqual(summary['worst_load_ms'], 6.0)
        self.assertAlmostEqual(summary['worst_any_ms'], 30.0)
        self.assertEqual(summary['top'][0].kind, 'uploads')

    def test_check_names_frames_over_budget_and_late_shaders(self):
        # Arrange
        _, events = report.parse_latelog(LATELOG)
        summary = report.summarize_run('runway', 35, frames([(700000, 2), (30000, 40, 6000)]), events, 4000, 5)

        # Act
        problems = report.check([summary], 4.0)

        # Assert
        self.assertEqual(len(problems), 2)
        self.assertIn('frame(s) after the load', problems[0])
        self.assertIn('shader(s) compiled during play', problems[1])

    def test_a_clean_run_passes_the_check(self):
        # Arrange
        summary = report.summarize_run('dam', 33, frames([(500000, 2), (8000, 3)], stage=33), [], 4000, 3)

        # Act
        problems = report.check([summary], 4.0)

        # Assert
        self.assertEqual(problems, [])

    def test_late_shaders_are_listed_once_in_shaders_txt_format(self):
        # Arrange
        _, events = report.parse_latelog(LATELOG)
        runs = [{'events': events}, {'events': events}]

        # Act
        shaders = report.late_shaders(runs)

        # Assert
        self.assertEqual(shaders, ['0000000010001000 10000001', '020d020d818a818a 10000711'])

    def test_markdown_has_a_row_per_stage(self):
        # Arrange
        _, events = report.parse_latelog(LATELOG)
        summary = report.summarize_run('runway', 35, frames([(700000, 2), (30000, 40, 6000)]), events, 4000, 5)
        summary['rooms'] = '11 of 11'

        # Act
        text = report.markdown([summary], [], {'settings': 'HD texture pack', 'dwell': 24, 'budget_ms': 4.0})

        # Assert
        self.assertIn('| runway | 11 of 11 | 700 |', text)
        self.assertIn('## Shaders compiled during play', text)


class LearnTests(unittest.TestCase):
    def test_only_textures_from_other_folders_are_learned(self):
        # Arrange
        text = 'U 35 0A1B2C3D#2#1#00000000 1\nU 35 0e9fc07e#2#1#37e5c3e9 0\nU 33 11111111#4#1#00000000 0\n'

        # Act
        found = report.parse_used(text)

        # Assert
        self.assertEqual(found, {35: {'0E9FC07E#2#1#37E5C3E9'}, 33: {'11111111#4#1#00000000'}})

    def test_merge_replaces_audited_stages_and_keeps_others(self):
        # Arrange
        doc = {'about': 'x', 'stages': {'33': ['OLD'], '22': ['KEEP']}}

        # Act
        merged = report.merge_learned(doc, {33: {'B', 'A'}, 9: {'C'}})

        # Assert
        self.assertEqual(merged['about'], 'x')
        self.assertEqual(merged['stages'], {'9': ['C'], '22': ['KEEP'], '33': ['A', 'B']})
        self.assertEqual(list(merged['stages']), ['9', '22', '33'])


class ContainerOptionTests(unittest.TestCase):
    """The options that run the audit in a container (LP64-005 Task 14); without them a run is unchanged."""

    def setUp(self):
        self.folder = Path(tempfile.mkdtemp(prefix='late-load-test-'))
        self.exe = self.folder/'IronSights'
        self.exe.write_bytes(b'')
        self.addCleanup(shutil.rmtree, self.folder, ignore_errors=True)

    def options(self, *extra):
        return audit.parse_options(['--exe', str(self.exe), '--no-pack', '--out', str(self.folder/'out'), *extra])

    def test_without_a_video_driver_the_run_keeps_the_callers_display_and_home(self):
        # Arrange
        options = self.options()

        # Act
        env = audit.run_env('dam', self.folder/'rom', self.folder/'dam', options)

        # Assert
        self.assertEqual(env.get('SDL_VIDEODRIVER'), os.environ.get('SDL_VIDEODRIVER'))
        self.assertEqual(env.get('HOME'), os.environ.get('HOME'))
        self.assertNotIn('GE_ICON_CACHE_DIR', env)

    def test_a_video_driver_is_given_to_the_game(self):
        # Arrange
        options = self.options('--video-driver', 'offscreen')

        # Act
        env = audit.run_env('dam', self.folder/'rom', self.folder/'dam', options)

        # Assert
        self.assertEqual(env['SDL_VIDEODRIVER'], 'offscreen')

    def test_a_video_driver_gives_the_run_its_own_home_and_icon_cache(self):
        # Arrange
        options = self.options('--video-driver', 'offscreen')

        # Act
        env = audit.run_env('temple', self.folder/'rom', self.folder/'temple', options)

        # Assert
        self.assertEqual(env['HOME'], str(self.folder/'temple'/'home'))
        self.assertEqual(env['GE_ICON_CACHE_DIR'], str(self.folder/'temple'/'icons'))

    def test_an_env_option_still_wins_over_the_container_variables(self):
        # Arrange
        options = self.options('--video-driver', 'offscreen', '--env', 'SDL_VIDEODRIVER=x11')

        # Act
        env = audit.run_env('dam', self.folder/'rom', self.folder/'dam', options)

        # Assert
        self.assertEqual(env['SDL_VIDEODRIVER'], 'x11')

    def run_folder(self):
        folder = self.folder/'dam'
        for name in ('home', 'icons'):
            (folder/name).mkdir(parents=True)
        (folder/'midas.ini').write_text('[display]\n')
        return folder

    def test_a_run_with_no_usable_logs_leaves_no_home_or_icon_cache(self):
        # Arrange: the stage's run times out and leaves no logs (play and collect stand in for the game)
        def play(item, rom, folder, options):
            for name in ('home', 'icons'):
                (folder/name).mkdir(parents=True)
            (folder/'midas.ini').write_text('[display]\n')
            return 'timeout'
        for name, stand_in in (('find_rom', lambda: self.folder/'rom'), ('play', play),
                               ('collect', lambda item, folder, budget_us: None)):
            original = getattr(audit, name)
            setattr(audit, name, stand_in)
            self.addCleanup(setattr, audit, name, original)

        # Act
        with contextlib.redirect_stdout(io.StringIO()):
            audit.main(['--exe', str(self.exe), '--no-pack', '--out', str(self.folder/'out'), '--stage', 'dam',
                        '--video-driver', 'offscreen'])

        # Assert
        self.assertEqual(sorted(path.name for path in (self.folder/'out'/'dam').iterdir()), ['midas.ini'])

    def test_keep_leaves_a_run_folder_as_it_is(self):
        # Arrange
        folder = self.run_folder()
        options = self.options('--video-driver', 'offscreen', '--keep')

        # Act
        audit.clean_run(folder, options, usable=False)

        # Assert
        self.assertEqual(sorted(path.name for path in folder.iterdir()), ['home', 'icons', 'midas.ini'])

    def test_the_run_timeout_defaults_to_ten_minutes(self):
        # Act
        options = self.options()

        # Assert
        self.assertEqual(options.run_timeout, 600)

    def test_the_run_timeout_can_be_raised_for_an_emulated_host(self):
        # Act
        options = self.options('--run-timeout', '2400')

        # Assert
        self.assertEqual(options.run_timeout, 2400)


class NameComparisonTests(unittest.TestCase):
    """Two audits compared by stage, kind and name, without ticks or timings (LP64-006 Task 17)."""

    def setUp(self):
        self.folder = Path(tempfile.mkdtemp(prefix='late-compare-'))
        self.addCleanup(shutil.rmtree, self.folder, ignore_errors=True)

    def audit_folder(self, name, stages):
        """An audit folder holding STAGES: {stage name: late.txt text}."""
        for item, text in stages.items():
            (self.folder/name/item).mkdir(parents=True)
            (self.folder/name/item/'late.txt').write_text(text, encoding='utf-8')
        return self.folder/name

    def test_ticks_and_timings_are_left_out(self):
        # Arrange: the same events at other ticks, with other durations and an upload's other name and lookup times
        _, before = report.parse_latelog(LATELOG)
        _, after = report.parse_latelog(LATELOG.replace('L 35 40 uploads 6000 fmt 2 siz 1 256x256 name 0 us | hd '
                                                        '256x256 lookup 120 us',
                                                        'L 35 52 uploads 7100 fmt 2 siz 1 256x256 name 31 us | hd '
                                                        '256x256 lookup 9 us').replace('L 35 41 models 1500',
                                                                                       'L 35 60 models 2200'))

        # Act
        differences = report.compare_names(before, after)

        # Assert
        self.assertEqual(differences, [])

    def test_a_name_logged_a_different_number_of_times_is_named_with_both_counts(self):
        # Arrange
        _, before = report.parse_latelog(LATELOG)
        _, after = report.parse_latelog(LATELOG+'L 35 70 models 900 CboilerbondZ\nL 35 71 tex 80 2317\n')

        # Act
        differences = report.compare_names(before, after)

        # Assert
        self.assertEqual(differences, [((35, 'models', 'CboilerbondZ'), 1, 2), ((35, 'tex', '2317'), 0, 1)])

    def test_the_same_name_in_another_stage_is_another_event(self):
        # Arrange
        _, before = report.parse_latelog(LATELOG)
        _, after = report.parse_latelog(LATELOG.replace('L 90 10 shaders', 'L 35 10 shaders'))

        # Act
        differences = report.compare_names(before, after)

        # Assert
        self.assertEqual(differences, [((35, 'shaders', '0000000010001000 10000001'), 0, 1),
                                       ((90, 'shaders', '0000000010001000 10000001'), 1, 0)])

    def test_audits_with_the_same_events_pass(self):
        # Arrange
        before = self.audit_folder('before', {'dam': LATELOG, 'temple': LATELOG})
        after = self.audit_folder('after', {'dam': LATELOG.replace('L 35 41 models 1500', 'L 35 44 models 1800'),
                                            'temple': LATELOG})
        out = io.StringIO()

        # Act
        with contextlib.redirect_stdout(out):
            status = audit.main(['--compare', str(before), str(after)])

        # Assert
        self.assertEqual(status, 0)
        self.assertIn('late-load: OK: 2 of 2 stages logged the same late events by kind and name', out.getvalue())

    def test_audits_that_differ_fail_and_name_the_difference(self):
        # Arrange
        before = self.audit_folder('before', {'dam': LATELOG, 'temple': LATELOG})
        after = self.audit_folder('after', {'dam': LATELOG+'L 35 70 tex 80 2317\n', 'temple': LATELOG})
        out = io.StringIO()

        # Act
        with contextlib.redirect_stdout(out):
            status = audit.main(['--compare', str(before), str(after)])

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('late-load: dam: 4 late events against 5', out.getvalue())
        self.assertIn('late-load:   stage 35 tex 2317: 0 against 1', out.getvalue())
        self.assertIn('late-load: FAIL: 1 of 2 stages logged the same late events by kind and name', out.getvalue())

    def test_a_stage_in_one_audit_only_fails(self):
        # Arrange
        before = self.audit_folder('before', {'dam': LATELOG, 'temple': LATELOG})
        after = self.audit_folder('after', {'dam': LATELOG})
        out = io.StringIO()

        # Act
        with contextlib.redirect_stdout(out):
            status = audit.main(['--compare', str(before), str(after)])

        # Assert
        self.assertEqual(status, 1)
        self.assertIn('late-load: temple: no late.txt in', out.getvalue())
        self.assertIn('late-load: FAIL: 1 of 2 stages logged the same late events by kind and name', out.getvalue())


if __name__ == '__main__':
    unittest.main()
