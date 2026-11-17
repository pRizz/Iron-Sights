"""ROM-free tests for lp64_census.py: sample smoke logs written here, compared and listed."""
import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('lp64_census', Path(__file__).with_name('lp64_census.py'))
census = importlib.util.module_from_spec(spec)
spec.loader.exec_module(census)

TEXT = 'ge: romdata: text LdamE     36 slots from the ROM\n'
BG = 'ge: romdata: bg bg/bg_dam_all_p.seg  converted (139 rooms, 389 vis cmds, 160 portals)\n'
STAN = 'ge: romdata: stan  Tbg_dam_all_p_stanZ converted (2755 tiles, 5 rooms, 89000 bytes)\n'
SETUP = 'ge: romdata: setup UsetupdamZ       converted (9565 propdef words, 203 intro words, 367 pads)\n'
MODEL = 'ge: romdata: model CheadmarkZ       converted (6 nodes, 126 verts, 4 DLs)\n'
GUARD = 'ge: romdata: model Cgreatguard2Z    converted (79 nodes, 900 verts, 25 DLs)\n'
NOISE = 'ge: romdata: seg sfxctl             -> ROM (12345 bytes)\nge: frame limit reached: frames=600\n'
WIDE_ONLY = 'ge: lp64: bg tables bg/bg_dam_all_p.seg: 139 room records, 195 portal entries, 8688 bytes\n'
FULL = TEXT + BG + STAN + SETUP + MODEL + GUARD


def write_smoke(folder: Path, logs: dict[str, str]) -> None:
    """One log a mission of the smoke: LOGS's text for the missions it names, FULL for the rest."""
    folder.mkdir(parents=True, exist_ok=True)
    for mission in census.MISSIONS:
        (folder / f'{mission}.log').write_text(logs.get(mission, FULL), encoding='utf-8')


def run(*argv: str) -> tuple[int, str]:
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        status = census.main(list(argv))
    return status, out.getvalue()


class Compare(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def compare(self, logs32: dict[str, str], logs64: dict[str, str]) -> tuple[int, str]:
        write_smoke(self.root / '32', logs32)
        write_smoke(self.root / '64', logs64)
        return run('compare', str(self.root / '32'), str(self.root / '64'))

    def test_equal_sets_pass(self):
        # Arrange: the 64-bit log has the lines in another order, other padding and lines only it prints
        reordered = GUARD + MODEL + SETUP.replace('       ', ' ') + STAN + BG + TEXT + NOISE + WIDE_ONLY

        # Act
        status, out = self.compare({}, {'dam': reordered})

        # Assert
        self.assertEqual((status, 'dam: 6 lines equal' in out), (0, True), out)

    def test_a_missing_model_fails_and_names_it(self):
        # Act
        status, out = self.compare({}, {'egypt': FULL.replace(GUARD, '')})

        # Assert
        self.assertEqual((status, 'egypt: only in the 32-bit log: model Cgreatguard2Z converted' in out), (1, True),
                         out)

    def test_a_count_that_differs_fails(self):
        # Act
        status, out = self.compare({}, {'silo': FULL.replace('126 verts', '127 verts')})

        # Assert
        self.assertEqual((status, 'silo: only in the 32-bit log: model CheadmarkZ converted (6 nodes, 126 verts'
                          in out), (1, True), out)

    def test_a_model_loaded_twice_at_one_width_fails(self):
        # Act
        status, out = self.compare({}, {'dam': FULL + MODEL})

        # Assert
        self.assertEqual((status, 'dam: only in the 64-bit log: model CheadmarkZ' in out), (1, True), out)

    def test_a_mission_that_loads_no_model_fails_even_when_equal(self):
        # Arrange
        no_models = TEXT + BG + STAN + SETUP

        # Act
        status, out = self.compare({'train': no_models}, {'train': no_models})

        # Assert
        self.assertEqual((status, 'train: no model line' in out), (1, True), out)

    def test_a_missing_log_fails(self):
        # Arrange
        write_smoke(self.root / '32', {})
        write_smoke(self.root / '64', {})
        (self.root / '64' / 'cradle.log').unlink()

        # Act
        status, out = run('compare', str(self.root / '32'), str(self.root / '64'))

        # Assert
        self.assertEqual((status, 'cradle: no log' in out), (1, True), out)


class Types(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self):
        self.temporary.cleanup()

    def test_lists_loaded_and_unloaded_kinds(self):
        # Arrange
        lines = ('ge: census: model CheadmarkZ opcodes 1=1 4=3 23=1\n'
                 'ge: census: setup UsetupdamZ props 1=10 3=40 48=1\n')
        write_smoke(self.root, {'dam': FULL + lines})

        # Act
        status, out = run('types', str(self.root))

        # Assert
        self.assertEqual((status, '4 DL: 3 in 1 load' in out, '1 DOOR: 10 in 1 load' in out,
                          'opcodes no mission loaded: 0 NULL, 2 GROUP,' in out,
                          'types no mission loaded: 0 NOTHING, 2 DOOR_SCALE,' in out), (0, True, True, True, True), out)

    def test_an_opcode_beyond_the_table_is_unknown(self):
        # Arrange
        write_smoke(self.root, {'dam': FULL + 'ge: census: model CheadmarkZ opcodes 1=1 31=2\n'})

        # Act
        status, out = run('types', str(self.root))

        # Assert
        self.assertEqual((status, '31 unknown: 2 in 1 load' in out), (0, True), out)

    def test_no_census_lines_fails(self):
        # Arrange
        write_smoke(self.root, {})

        # Act
        status, out = run('types', str(self.root))

        # Assert
        self.assertEqual((status, 'no census line' in out), (1, True), out)


if __name__ == '__main__':
    unittest.main()
