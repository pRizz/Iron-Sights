"""Check that every GL state query in the renderer is answered from memory.

A state query makes the driver, and in a browser the GPU process, finish all
queued work before it answers (FORK-087). The state shadow
(port/fast3d/gfx_gl_shadow.inl, FORK-088) answers the ones it tracks without
asking. This test fails when the renderer queries a piece of state the shadow
does not track, unless the query is listed below as one made at start-up only.
"""
import re
import unittest
from pathlib import Path

FAST3D = Path(__file__).resolve().parents[2]/'port'/'fast3d'
SHADOW = FAST3D/'gfx_gl_shadow.inl'
# Asked once, never per frame.
STARTUP_ONLY = {
    'GL_SAMPLES': "the window's multisample count, asked once",
    'GL_SAMPLE_BUFFERS': 'whether the window is multisampled, asked once',
    'GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT': 'a driver limit read at start-up',
}
QUERY = re.compile(r'\bgl(?:GetIntegerv|GetBooleanv|GetFloatv|IsEnabled)\(\s*(GL_[A-Z0-9_]+)')


def tracked_names():
    """State names the shadow answers: its `case GL_X:` labels and `== GL_X` tests."""
    text = SHADOW.read_text(encoding='utf-8')
    return set(re.findall(r'case (GL_[A-Z0-9_]+):', text)) | set(re.findall(r'pname == (GL_[A-Z0-9_]+)', text))


def queried_names():
    """{state name: [file:line, ...]} for every query in the renderer's sources."""
    found = {}
    for path in sorted(FAST3D.glob('*.inl'))+[FAST3D/'gfx_opengl.cpp']:
        if path == SHADOW:
            continue
        for number, line in enumerate(path.read_text(encoding='utf-8', errors='replace').splitlines(), start=1):
            for name in QUERY.findall(line):
                found.setdefault(name, []).append(f'{path.name}:{number}')
    return found


class GlQueryTests(unittest.TestCase):
    def test_every_queried_state_is_tracked_or_start_up_only(self):
        tracked = tracked_names()

        untracked = {name: places for name, places in queried_names().items()
                     if name not in tracked and name not in STARTUP_ONLY}

        self.assertEqual(untracked, {}, 'add these to gfx_gl_shadow.inl (setter and query), or cache the value '
                                        'when it is set; a per-frame driver query stalls the frame')

    def test_the_renderer_does_query_tracked_state(self):
        # Guards the scan itself: if this finds nothing, the pattern has rotted.
        names = queried_names()

        self.assertIn('GL_CURRENT_PROGRAM', names)
        self.assertIn('GL_BLEND', names)

    def test_start_up_exceptions_are_still_in_use(self):
        names = queried_names()

        self.assertEqual([name for name in STARTUP_ONLY if name not in names], [])


if __name__ == '__main__':
    unittest.main()
