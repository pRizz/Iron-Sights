"""Check the benchmark suite's pure parts: metrics, run planning, the report."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent/'bench'))

import bench_metrics as metrics  # noqa: E402
import bench_plan as plan  # noqa: E402
import bench_report as report  # noqa: E402
import stock  # noqa: E402

HEADER = '# ge-bench 1 tickhz=0 render_thread=1 fixedtick=1 nosleep=1 reason=end\n'+','.join(metrics.COLUMNS)+'\n'


def frame_line(interval, tick, stage=33, draws=100, run=0, swap=0, rooms=0):
    return f'{interval},{run},{swap},0,400,70,0,{draws},{stage},{tick},{rooms},0,0,0,0\n'


def log_text(intervals, first_tick=0, load_us=120000):
    text = HEADER+frame_line(5000, 10, stage=metrics.TITLE_STAGE)+frame_line(load_us, first_tick, rooms=90000)
    for offset, interval in enumerate(intervals, start=1):
        text += frame_line(interval, first_tick+offset, run=interval//2, swap=interval//10)
    return text


class MetricsTests(unittest.TestCase):
    def test_window_is_chosen_by_tick_and_skips_the_title(self):
        # Arrange
        _, frames = metrics.parse_log(log_text([1000]*10))

        # Act
        window = metrics.select_window(frames, 3, 8)

        # Assert
        self.assertEqual([frame.tick for frame in window], [3, 4, 5, 6, 7])

    def test_average_and_lows_follow_the_frame_times(self):
        # Arrange: 99 frames of 2 ms and one of 10 ms
        text = log_text([2000]*99+[10000])

        # Act
        result = metrics.run_metrics(text, 1, 101)

        # Assert
        self.assertEqual(result['frames'], 100)
        self.assertAlmostEqual(result['avg_fps'], 100*1e6/(99*2000+10000))
        self.assertAlmostEqual(result['low_1pct_fps'], 100.0)
        self.assertEqual(result['frame_ms_p50'], 2.0)
        self.assertEqual(result['frame_ms_max'], 10.0)
        self.assertEqual(result['over_8ms'], 1)
        self.assertEqual(result['over_16ms'], 0)

    def test_render_time_excludes_the_swap(self):
        # Arrange
        text = log_text([4000]*20)

        # Act
        result = metrics.run_metrics(text, 1, 21)

        # Assert: run is half the interval, swap a tenth
        self.assertAlmostEqual(result['render_ms'], 1.6)
        self.assertAlmostEqual(result['swap_ms'], 0.4)

    def test_game_time_excludes_the_renderer_when_it_shares_the_thread(self):
        # Arrange: every frame logs 400 us of game stretch and interval/2 of renderer
        threaded = log_text([400]*20)
        shared = threaded.replace('render_thread=1', 'render_thread=0')

        # Act
        on = metrics.run_metrics(threaded, 1, 21)
        off = metrics.run_metrics(shared, 1, 21)

        # Assert: 400 us of stretch, 200 us of it the renderer
        self.assertAlmostEqual(on['sim_ms'], 0.4)
        self.assertAlmostEqual(off['sim_ms'], 0.2)
        self.assertAlmostEqual(off['work_ms'], off['render_ms']+0.2)

    def test_load_is_the_long_frame_before_the_window(self):
        # Arrange
        text = log_text([2000]*20, load_us=250000)

        # Act
        result = metrics.run_metrics(text, 5, 21)

        # Assert
        self.assertEqual(result['load_ms'], 250.0)
        self.assertEqual(result['load_events_ms']['rooms'], 90.0)

    def test_a_foreign_file_is_rejected(self):
        with self.assertRaises(ValueError):
            metrics.parse_log('interval,run\n1,2\n')

    def test_an_empty_window_is_an_error(self):
        with self.assertRaises(ValueError):
            metrics.run_metrics(log_text([1000]*5), 500, 600)

    def test_summary_takes_the_median_and_keeps_the_range(self):
        # Arrange
        runs = [{'avg_fps': 100.0, 'header': {}}, {'avg_fps': 300.0, 'header': {}}, {'avg_fps': 110.0, 'header': {}}]

        # Act
        summary = metrics.summarize(runs)

        # Assert
        self.assertEqual(summary['avg_fps'], {'median': 110.0, 'min': 100.0, 'max': 300.0})
        self.assertEqual(summary['runs'], 3)
        self.assertNotIn('header', summary)


class PlanTests(unittest.TestCase):
    ROUTE = {'forward': 70, 'straight_ticks': 4, 'turn_ticks': 2, 'turn_degrees': [90, -30],
             'fire': {'from': 3, 'every': 4, 'hold': 2}}
    CONFIG = ('{"schema": 1, "defaults": {"seed": 1, "warmup_ticks": 300, "ticks": 3600},'
              ' "routes": {"walk": {"forward": 70, "straight_ticks": 150, "turn_ticks": 45, "turn_degrees": [80]}},'
              ' "scenarios": {"dam": {"stage": "dam", "cheats": "invuln", "route": "walk"},'
              ' "mp": {"multiplayer": {"arena": "temple", "players": 4}, "route": "walk", "ticks": 600}},'
              ' "presets": {"default": [], "matched": ["msaa=1"]}}')

    def test_scenarios_take_the_defaults_and_their_route(self):
        config = plan.load_config(self.CONFIG)

        self.assertEqual(config['scenarios']['dam']['ticks'], 3600)
        self.assertEqual(config['scenarios']['dam']['route']['turn_ticks'], 45)
        self.assertEqual(config['scenarios']['mp']['ticks'], 600)
        self.assertEqual(plan.end_tick(config['scenarios']['mp']), 900)

    def test_a_scenario_needs_exactly_one_kind(self):
        with self.assertRaises(ValueError):
            plan.load_config(self.CONFIG.replace('"stage": "dam", ', ''))

    def test_a_scenario_needs_a_known_route(self):
        with self.assertRaises(ValueError):
            plan.load_config(self.CONFIG.replace('"route": "walk"', '"route": "swim"'))
        with self.assertRaises(ValueError):
            plan.load_config(self.CONFIG.replace(', "route": "walk"', ''))

    def test_measured_runs_are_fixed_tick_unpaced_and_silent(self):
        scenario = plan.load_config(self.CONFIG)['scenarios']['dam']

        env = plan.run_env(scenario, 'rom.z64', '1920x1080', 'frames.csv', 'dam.rec')

        self.assertEqual((env['GE_FIXEDTICK'], env['GE_NOSLEEP'], env['GE_TICKHZ']), ('1', '1', '0'))
        self.assertEqual(env['SDL_AUDIODRIVER'], 'dummy')
        self.assertEqual(env['GE_BENCH_END'], '3900')
        self.assertEqual(env['GE_INPUTPLAY'], 'dam.rec')
        self.assertEqual(env['GE_CHEATS'], 'invuln')

    def test_multiplayer_replays_player_one_and_leaves_the_scripted_drive_off(self):
        scenario = plan.load_config(self.CONFIG)['scenarios']['mp']

        env = plan.run_env(scenario, 'rom.z64', '1920x1080', 'frames.csv', 'mp.rec')

        self.assertEqual((env['GE_MP'], env['GE_MPPLAYERS'], env['GE_INPUTPLAY']), ('temple', '4', 'mp.rec'))
        self.assertNotIn('GE_STAGE', env)
        self.assertNotIn('GE_MPDRIVE', env)

    def test_preset_lines_follow_the_harness_settings(self):
        text = plan.ini_text(['msaa=1'])

        self.assertEqual(text, '[display]\nfullscreen=window\nvsync=0\nmaxfps=0\nmsaa=1\n')

    def test_route_walks_turns_fires_and_ends_neutral(self):
        # Act: legs of 4 straight ticks then 2 turning; fire 2 ticks of every 4 from tick 3
        route = plan.synth_route(self.ROUTE, 8)

        # Assert
        self.assertEqual(route.splitlines(), [
            'P 1 0 0 46', 'P 3 2000 0 46', 'M 4 1 0 0 45 0', 'P 5 0 0 46', 'M 5 1 0 0 45 0',
            'P 7 2000 0 46', 'P 9 0 0 0'])

    def test_route_turns_cycle_through_the_list(self):
        # Legs are 6 ticks: ticks 10-11 are the second leg's turn, 16-17 the third's.
        self.assertEqual(plan.yaw_at(self.ROUTE, 10), -15.0)
        self.assertEqual(plan.yaw_at(self.ROUTE, 16), 45.0)
        self.assertEqual(plan.yaw_at(self.ROUTE, 12), 0.0)

    def test_a_backward_stick_is_written_as_a_byte(self):
        route = plan.synth_route(dict(self.ROUTE, forward=-70, fire=None), 2)

        self.assertEqual(route.splitlines()[0], 'P 1 0 0 ba')

    def test_route_covers_the_measured_window(self):
        scenario = plan.load_config(self.CONFIG)['scenarios']['dam']

        last = plan.route_text(scenario).splitlines()[-1]

        self.assertEqual(last, f'P {3900+plan.ROUTE_MARGIN_TICKS+1} 0 0 0')

    def test_other_game_processes_are_found_and_our_own_run_is_not(self):
        listing = ('"System","4","Services","0","144 K"\n"IronSights.exe","100","Console","1","9 K"\n'
                   '"ProjectMidas.exe","200","Console","1","9 K"\n"python.exe","300","Console","1","9 K"\n')

        self.assertEqual(plan.other_games(listing, 100), ['ProjectMidas.exe'])
        self.assertEqual(plan.other_games(listing, None), ['IronSights.exe', 'ProjectMidas.exe'])
        self.assertEqual(plan.other_games('', 100), [])

    def test_busy_share_counts_idle_inside_kernel_time(self):
        # 100 units of processor time passed, 75 of them idle
        self.assertAlmostEqual(plan.busy_fraction((0, 0, 0), (75, 80, 20)), 0.25)
        self.assertIsNone(plan.busy_fraction((5, 5, 5), (5, 5, 5)))

    def test_browser_runs_carry_their_input_and_settings_as_data(self):
        config = plan.load_config(self.CONFIG)

        runs = plan.web_runs(config, ['dam'], ['matched'], 2)

        self.assertEqual([run['repeat'] for run in runs], [1, 2])
        self.assertEqual(runs[0]['env']['GE_INPUTPLAY'], plan.WEB_ROUTE)
        self.assertEqual(runs[0]['env']['GE_BENCH'], plan.WEB_FRAME_LOG)
        self.assertEqual((runs[0]['env']['GE_FIXEDTICK'], runs[0]['env']['GE_TICKHZ']), ('1', '0'))
        self.assertNotIn('GE_ROM', runs[0]['env'])
        self.assertIn('msaa=1', runs[0]['ini'])
        self.assertTrue(runs[0]['route'].startswith('P 1 0 0 46'))
        self.assertEqual((runs[0]['first_tick'], runs[0]['end_tick']), (300, 3900))
        self.assertEqual(plan.web_log_name(runs[1]), 'matched-dam-2.csv')

    def test_builds_alternate_within_a_repeat(self):
        order = plan.run_order(['fork', 'stock'], ['dam'], 2)

        self.assertEqual(order, [(1, 'dam', 'fork'), (1, 'dam', 'stock'), (2, 'dam', 'fork'), (2, 'dam', 'stock')])


class ReportTests(unittest.TestCase):
    @staticmethod
    def summary(fps, low, high):
        value = {'median': fps, 'min': low, 'max': high}
        flat = {'median': 1.0, 'min': 1.0, 'max': 1.0}
        result = {key: flat for key, _, _, _ in report.TABLE}
        result.update(avg_fps=value, runs=3)
        return result

    def results(self, other):
        return {'created': 'now', 'resolution': '1920x1080', 'visible': True,
                'host': {'cpu': 'cpu', 'gpu': 'gpu', 'os': 'os'},
                'builds': [{'label': 'stock', 'identity': 'a', 'sha256': 'f'*64},
                           {'label': 'fork', 'identity': 'b', 'sha256': 'e'*64}],
                'summary': {'default': {'dam': {'stock': self.summary(400.0, 390.0, 410.0), 'fork': other}}}}

    def test_a_change_beyond_the_noise_is_bold(self):
        text = report.render(self.results(self.summary(200.0, 195.0, 205.0)), 'stock')

        self.assertIn('| fork | 200 (**-50%**)', text)
        self.assertIn('| stock | 400 |', text)

    def test_unequal_work_between_repeats_is_called_out(self):
        other = dict(self.summary(200.0, 195.0, 205.0), draws={'median': 100.0, 'min': 90.0, 'max': 100.0})

        text = report.render(self.results(other), 'stock')

        self.assertIn('The repeats of fork did not draw the same amount', text)

    def test_equal_work_between_repeats_is_not_called_out(self):
        other = dict(self.summary(200.0, 195.0, 205.0), draws={'median': 100.0, 'min': 99.9, 'max': 100.0})

        self.assertNotIn('did not draw', report.render(self.results(other), 'stock'))

    def test_overlapped_runs_and_machine_load_are_reported(self):
        results = self.results(self.summary(200.0, 195.0, 205.0))
        results['runs'] = [{'disturbed': True, 'cpu_busy': 0.6}, {'disturbed': False, 'cpu_busy': 0.2},
                           {'disturbed': False, 'cpu_busy': None}]

        text = report.render(results, 'stock')

        self.assertIn('**1 of 3 runs overlapped another game process**', text)
        self.assertIn('median 60%, highest 60%', text)

    def test_a_quiet_session_reports_no_overlap(self):
        results = self.results(self.summary(200.0, 195.0, 205.0))
        results['runs'] = [{'disturbed': False, 'cpu_busy': 0.2}]

        self.assertNotIn('overlapped', report.render(results, 'stock'))

    def test_notes_are_listed_and_a_build_without_a_digest_omits_it(self):
        results = self.results(self.summary(200.0, 195.0, 205.0))
        results['notes'] = ['Browser build. Compare Render ms']
        results['builds'][1]['sha256'] = ''

        text = report.render(results, 'stock')

        self.assertIn('- Browser build. Compare Render ms', text)
        self.assertIn('- `fork`: b\n', text)
        self.assertIn('- `stock`: a (sha256 ffffffffffff)', text)

    def test_a_browser_session_names_its_window_as_headless(self):
        results = dict(self.results(self.summary(200.0, 195.0, 205.0)), target='browser', visible=False)

        self.assertIn('- Window: 1920x1080, headless browser', report.render(results, 'stock'))

    def test_a_single_run_is_never_bold(self):
        other = dict(self.summary(200.0, 200.0, 200.0), runs=1)

        text = report.render(self.results(other), 'stock')

        self.assertIn('| fork | 200 (-50%)', text)

    def test_a_change_within_the_noise_is_plain(self):
        text = report.render(self.results(self.summary(392.0, 300.0, 420.0)), 'stock')

        self.assertIn('| fork | 392 (-2%)', text)


class StockTests(unittest.TestCase):
    def test_hook_goes_after_its_anchor_once(self):
        text = 'before\nANCHOR\nafter\n'

        once = stock.insert_after(text, 'ANCHOR\n', 'HOOK\n', 'HOOK', 'file')
        twice = stock.insert_after(once, 'ANCHOR\n', 'HOOK\n', 'HOOK', 'file')

        self.assertEqual(once, 'before\nANCHOR\nHOOK\nafter\n')
        self.assertEqual(twice, once)

    def test_a_missing_anchor_stops_the_build(self):
        with self.assertRaises(ValueError):
            stock.insert_after('nothing here\n', 'ANCHOR\n', 'HOOK\n', 'HOOK', 'file')

    def test_a_commit_that_has_the_harness_keeps_its_own(self):
        # This checkout already calls the logger and returns early under fixed tick.
        hitchlog = (stock.REPO/'port'/'src'/'hitchlog.c').read_text(encoding='utf-8')
        timing = (stock.REPO/'src'/'game'/'frametiming.c').read_text(encoding='utf-8')

        self.assertEqual(stock.insert_after(hitchlog, stock.HITCH_ANCHOR, stock.HITCH_HOOK, stock.HITCH_CALL, 'h'),
                         hitchlog)
        self.assertEqual(stock.insert_after(timing, stock.TICK_ANCHOR, stock.TICK_HOOK, stock.TICK_PRESENT, 't'),
                         timing)

    def test_the_fork_hook_matches_the_one_given_to_stock(self):
        # The stock build must call the logger exactly as the fork does.
        hitchlog = (stock.REPO/'port'/'src'/'hitchlog.c').read_text(encoding='utf-8')

        self.assertIn(stock.HITCH_ANCHOR, hitchlog)
        self.assertIn(stock.HITCH_CALL, hitchlog)
        self.assertIn(stock.HITCH_CALL, stock.HITCH_HOOK)

    def test_the_baseline_is_the_one_the_inventory_names(self):
        inventory = (stock.REPO/'FORK_CHANGES.md').read_text(encoding='utf-8')

        self.assertIn(stock.BASELINE_REF, inventory.split('**Previous baseline:**')[0])


if __name__ == '__main__':
    unittest.main()
