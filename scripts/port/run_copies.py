"""One copy of an executable for many concurrent runs, each placed in its own folder by a hard link.

The play tool (lp64_play_run.py) and the mission smoke (audit_buffers.py) start each case from a
copy of the executable in the case's own temporary folder, from several threads at once (the game
reads midas.ini from, and writes files beside, its executable). Copying there per case raced: while
one thread held its copy open for writing, another thread forked to start its case, and the forked
child kept a duplicate of that descriptor until its own exec (close-on-exec closes it only then).
The first thread's exec of its finished copy then failed with ETXTBSY ("Text file busy"), seen in
LP64-005 Task 10's step 8 and in CI on 4a9b541.

SharedExecutable copies the executable once, before any case starts, and links that closed copy into
each case's folder: placing it opens no file for writing, so no fork can inherit a writer.

On Windows the game needs SDL2.dll beside it (WIN64-002): each of RUNTIME_LIBRARIES that sits beside
the executable is copied with it and linked beside each placed copy.
"""
import os
from pathlib import Path
import shutil
import tempfile

# The libraries a Windows build loads from its own folder (lp64_boot.py copies the same one).
RUNTIME_LIBRARIES = ('SDL2.dll',)


class SharedExecutable:
    """A context manager holding one closed copy of EXECUTABLE in a private temporary folder; place()
    hard-links it into a case's folder under the executable's own name."""

    def __init__(self, executable: Path):
        self.executable = executable
        self._folder = tempfile.TemporaryDirectory(prefix='midas-run-copy-')
        self.copy = Path(self._folder.name)/executable.name
        self.libraries = [executable.parent/name for name in RUNTIME_LIBRARIES
                          if (executable.parent/name).is_file()]
        try:
            shutil.copy2(executable, self.copy)
            for library in self.libraries:
                shutil.copy2(library, Path(self._folder.name)/library.name)
        except BaseException:
            self._folder.cleanup()
            raise

    def place(self, folder: Path) -> Path:
        """Links the copy into FOLDER and gives the link's path. Raises OSError naming both paths
        when the link cannot be made (FOLDER on another file system than the temporary folder):
        copying instead would bring the race back."""
        child = folder/self.executable.name
        try:
            os.link(self.copy, child)
            for library in self.libraries:
                os.link(self.copy.parent/library.name, folder/library.name)
        except OSError as error:
            raise OSError(error.errno, f'cannot link the executable copy {self.copy} into {folder}: '
                                       f'{error.strerror}; a per-case copy would race with concurrent '
                                       'starts (Text file busy)') from error
        return child

    def close(self) -> None:
        self._folder.cleanup()

    def __enter__(self) -> 'SharedExecutable':
        return self

    def __exit__(self, *exc) -> None:
        self.close()
