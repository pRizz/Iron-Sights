"""Check the benchmark profilers' pure parts: sample files, naming, tables."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent/'bench'))

import bench_profile as native  # noqa: E402
import bench_profile_web as web  # noqa: E402

NM = '''10001000 T _main
10002000 t _lvlRender
10003000 T _portPumpEvents
10004000 T _joyPoll
10005000 D _someData
'''
# Two wake-ups 1 ms apart in the window; thread 7 ran in both, thread 9 in one.
PROFILE = '''# ge-profile 1 hz=1000
M 10000000 100000 C:\\game\\IronSights.exe
M 70000000 10000 C:\\Windows\\driver.dll
S 7 90 5 1 100 10002010 10001010
S 7 33 300 10 5000 10002010 10001010
S 7 33 301 11 6000 70000100 10002020 10001010
S 9 33 301 11 6000 10004004 10003004 10001010
S 7 33 9000 99 99000 10002010 10001010
'''


def namer():
    modules, _ = native.parse_profile(PROFILE)
    return native.Namer(modules, native.parse_symbols(NM), 'IronSights.exe')


class NativeProfileTests(unittest.TestCase):
    def test_samples_and_modules_are_parsed(self):
        modules, samples = native.parse_profile(PROFILE)

        self.assertEqual(modules[1], (0x70000000, 0x10000, 'C:\\Windows\\driver.dll'))
        self.assertEqual(len(samples), 5)
        self.assertEqual(samples[2].addresses, (0x70000100, 0x10002020, 0x10001010))
        self.assertEqual((samples[2].thread, samples[2].stage, samples[2].tick, samples[2].wake), (7, 33, 301, 11))

    def test_a_foreign_file_is_rejected(self):
        with self.assertRaises(ValueError):
            native.parse_profile('S 1 2 3\n')

    def test_only_functions_are_symbols(self):
        symbols = native.parse_symbols(NM)

        self.assertEqual([name for _, name in symbols], ['_main', '_lvlRender', '_portPumpEvents', '_joyPoll'])

    def test_addresses_take_function_or_module_names(self):
        names = namer()

        self.assertEqual(names.name(0x10002abc), '_lvlRender')
        self.assertEqual(names.name(0x70000100), '[driver.dll]')
        self.assertIsNone(names.name(0x50000000))

    def test_a_64_bit_exe_loaded_elsewhere_is_rebased_to_its_preferred_base(self):
        # Arrange: x86_64 names (no underscore) at the preferred base 0x140000000; ASLR loaded the
        # exe at 0x7ff600000000
        nm = '0000000140001000 T main\n0000000140002000 T lvlRender\n'
        modules = [(0x7FF600000000, 0x100000, 'C:\\game\\IronSights.exe')]
        names = native.Namer(modules, native.parse_symbols(nm), 'IronSights.exe', 0x140000000)

        # Act / Assert
        self.assertEqual(names.name(0x7FF600002ABC), 'lvlRender')
        self.assertEqual(names.name(0x7FF600001010), 'main')

    def test_the_pe_header_gives_machine_and_preferred_base(self):
        # Arrange: a PE32+ header with ImageBase 0x140000000, and a PE32 one with 0x10000000
        def header(machine, magic, base_offset, base, width):
            data = bytearray(512)
            data[0x3C:0x40] = (0x80).to_bytes(4, 'little')
            data[0x80:0x84] = b'PE\0\0'
            data[0x84:0x86] = machine.to_bytes(2, 'little')
            data[0x98:0x9A] = magic.to_bytes(2, 'little')
            data[0x98+base_offset:0x98+base_offset+width] = base.to_bytes(width, 'little')
            return bytes(data)

        # Act / Assert
        self.assertEqual(native.pe_header(header(0x8664, 0x20B, 24, 0x140000000, 8)), (0x8664, 0x140000000))
        self.assertEqual(native.pe_header(header(0x14C, 0x10B, 28, 0x10000000, 4)), (0x14C, 0x10000000))

    def test_32_bit_names_lose_their_underscore_and_64_bit_names_keep_theirs(self):
        symbols = [(1, '_main'), (2, '__ZL3fooi'), (3, 'plain')]

        self.assertEqual(native.without_underscore(symbols, 0x14C), [(1, 'main'), (2, '_ZL3fooi'), (3, 'plain')])
        self.assertEqual(native.without_underscore(symbols, 0x8664), symbols)

    def test_the_pump_is_set_aside_under_either_spelling(self):
        self.assertIn('portPumpEvents', native.PUMPS)
        self.assertIn('_portPumpEvents', native.PUMPS)

    def test_window_is_chosen_by_tick_and_skips_the_title(self):
        _, samples = native.parse_profile(PROFILE)

        self.assertEqual([sample.tick for sample in native.window_samples(samples, 300, 3900)], [300, 301, 301])

    def test_time_is_a_share_of_the_window_per_frame(self):
        # Arrange: a window of 2 wake-ups and 1 ms, measured over 2 frames
        _, samples = native.parse_profile(PROFILE)

        # Act
        threads = native.aggregate(samples, namer(), 300, 302)

        # Assert: thread 7 ran in both wake-ups, so it cost the whole 1 ms over 2 frames
        game = threads[0]
        self.assertEqual(game['root'], '_main')
        self.assertAlmostEqual(game['ms'], 0.5)
        self.assertAlmostEqual(game['self']['_lvlRender'], 0.25)
        self.assertAlmostEqual(game['self']['[driver.dll]'], 0.25)
        self.assertAlmostEqual(game['inclusive']['_lvlRender'], 0.5)
        self.assertAlmostEqual(game['inclusive']['_main'], 0.5)

    def test_time_in_the_pump_is_set_aside(self):
        _, samples = native.parse_profile(PROFILE)

        threads = native.aggregate(samples, namer(), 300, 302)

        polling = threads[1]
        self.assertAlmostEqual(polling['pump_ms'], 0.25)
        self.assertAlmostEqual(polling['ms'], 0.0)
        self.assertNotIn('_joyPoll', polling['self'])

    def test_differences_are_ranked_by_size(self):
        lines = native.render_difference('stock', {'_a': 1.0, '_b': 0.2}, 'fork', {'_a': 1.1, '_b': 0.9, '_c': 0.3}, 2)

        self.assertIn('| +0.700 | 0.200 | 0.900 | `_b` |', lines)
        self.assertIn('| +0.300 | 0.000 | 0.300 | `_c` |', lines)
        self.assertFalse(any('`_a`' in line for line in lines))


class WebProfileTests(unittest.TestCase):
    PROFILE = {
        'midasFrames': 2,
        'nodes': [
            {'id': 1, 'callFrame': {'functionName': '(root)', 'url': ''}, 'children': [2, 5]},
            {'id': 2, 'callFrame': {'functionName': 'gfx_run', 'url': 'wasm://wasm/abc'}, 'children': [3, 4]},
            {'id': 3, 'callFrame': {'functionName': 'gfx_sp_tri1', 'url': 'wasm://wasm/abc'}},
            {'id': 4, 'callFrame': {'functionName': 'drawArrays', 'url': 'http://host/runtime/x/ProjectMidas.mjs'}},
            {'id': 5, 'callFrame': {'functionName': '(idle)', 'url': ''}},
        ],
        'samples': [3, 3, 4, 2, 5],
        'timeDeltas': [1000, 1000, 2000, 500, 9000],
    }

    def test_time_is_per_frame_and_idle_is_left_out(self):
        self_ms, inclusive_ms, total = web.aggregate(self.PROFILE)

        self.assertAlmostEqual(self_ms['gfx_sp_tri1'], 1.0)
        self.assertAlmostEqual(self_ms['drawArrays [ProjectMidas.mjs]'], 1.0)
        self.assertAlmostEqual(inclusive_ms['gfx_run'], 2.25)
        self.assertAlmostEqual(total, 2.25)

    def test_a_profile_without_a_frame_count_is_rejected(self):
        with self.assertRaises(ValueError):
            web.aggregate(dict(self.PROFILE, midasFrames=0))

    def test_tables_leave_out_the_root_and_idle(self):
        self_ms, inclusive_ms, total = web.aggregate(self.PROFILE)

        text = '\n'.join(web.render('web', self_ms, inclusive_ms, total, 10))

        self.assertIn('## web: 2.25 ms a frame', text)
        self.assertNotIn('(idle)', text)
        self.assertNotIn('(root)', text)


if __name__ == '__main__':
    unittest.main()
