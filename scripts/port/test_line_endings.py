#!/usr/bin/env python3
"""The tracked files Git stores with CRLF (or mixed) line endings are exactly
the pinned ones (FORK-093, MAC-001's final review).

`.gitattributes` normalises text to LF, but files that came with CRLF from
upstream keep it in the index, and an upstream-owned file is never
reformatted (AGENTS.md). A tool that reads and rewrites a whole file, as
Python's `read_text` and `write_text` do, silently turns every line to LF:
`src/game/stan.c` lost 3,747 CRLF endings that way in one two-line edit, and
the diff then hid the edit among them. This fails when a pinned file no
longer has its endings, and when another file gains them.

A file normalised on purpose leaves the list in the same change, with the
inventory saying so. Needs a Git checkout.
"""
import pathlib
import subprocess
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]

# `git ls-files --eol`: what the index holds for each path.
PINNED = {
    'scripts/test_files_readme.md': 'crlf',
    'src/game/bondwalk2.c': 'mixed',
    'src/game/objecthandler_2.c': 'crlf',
    'src/game/stan.c': 'mixed',
    'src/libultra/libc/bcopy.s': 'crlf',
    'src/libultra/os/exception.s': 'crlf',
    'src/libultra/os/getcount.s': 'crlf',
    'src/libultra/os/getfpccsr.s': 'crlf',
    'src/libultra/os/gettlbhi.s': 'crlf',
    'src/libultra/os/invaldcache.s': 'crlf',
    'src/libultra/os/invalicache.s': 'crlf',
    'src/libultra/os/setfpccsr.s': 'crlf',
    'src/libultra/os/setintmask.s': 'crlf',
    'src/libultra/os/unmaptlb.s': 'crlf',
    'src/libultra/os/writebackdcache.s': 'crlf',
    'src/libultra/os/writebackdcacheall.s': 'crlf',
    'tools/ge-stats.py': 'crlf',
}


def index_endings(listing: str) -> dict[str, str]:
    """The paths of a `git ls-files --eol` listing whose index copy has CRLF
    or mixed endings, each with which."""
    found = {}
    for line in listing.splitlines():
        info, _, path = line.partition('\t')
        index = info.split()[0]
        if index in ('i/crlf', 'i/mixed'):
            found[path] = index[2:]
    return found


class LineEndings(unittest.TestCase):
    def test_a_listing_is_read_by_its_index_column(self):
        # Arrange
        listing = ('i/lf    w/lf    attr/text=auto eol=lf \ta.c\n'
                   'i/crlf  w/crlf  attr/text=auto eol=lf \tdir/b c.s\n'
                   'i/mixed w/lf    attr/text=auto eol=lf \tc.c\n'
                   'i/-text w/-text attr/-text            \td.png\n')
        # Act
        found = index_endings(listing)
        # Assert
        self.assertEqual(found, {'dir/b c.s': 'crlf', 'c.c': 'mixed'})

    def test_the_files_stored_with_crlf_are_the_pinned_ones(self):
        # Arrange, Act
        listing = subprocess.run(['git', '-C', str(ROOT), 'ls-files', '--eol'], check=True, capture_output=True,
                                 text=True).stdout
        found = index_endings(listing)
        # Assert
        lost = sorted(path for path in PINNED if found.get(path) != PINNED[path])
        gained = sorted(path for path in found if path not in PINNED)
        self.assertEqual(lost, [], 'these files no longer have the line endings they came with')
        self.assertEqual(gained, [], 'these files gained CRLF line endings')


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False).result.wasSuccessful() else 1)
