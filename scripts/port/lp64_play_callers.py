"""Name the callers of a demo tick's random draws (LP64-005 Task 1).

With GE_RNGTRACE=T the game prints, for each random draw of ticks T-1 and T
(port/src/n64_random.c, randomGetNext):
    "ge: rng draw %d from %p\n"
with the draw's return address. The draws a tick makes come before its own `ge: demo tick N`
line (src/game/ramromreplay.c prints that line at the end of the tick), so each draw belongs to
the next tick line. A position-independent executable's addresses are made relative to its load
address, the `executable image` of the log's `ge: lp64: address space OK` line
(port/src/port_lp64.c). One is subtracted from each return address so that addr2line names the
call's line rather than the line after it.
"""
from pathlib import Path
import re
import subprocess

DRAW_LINE = re.compile(r'^ge: rng draw (\d+) from (0x[0-9a-fA-F]+)')
TICK_LINE = re.compile(r'^ge: demo tick (\d+) ')
IMAGE_ADDRESS = re.compile(r'ge: lp64: address space OK:.*\bexecutable image (0x[0-9a-fA-F]+)')
ELF_MAGIC = b'\x7fELF'
ELF_TYPE_OFFSET = 16
ET_DYN = 3
ROOT = Path(__file__).resolve().parents[2]


def is_position_independent(executable: Path) -> bool:
    """True for an ELF file of type ET_DYN (a PIE); raises ValueError for a file that is not ELF."""
    header = executable.read_bytes()[:ELF_TYPE_OFFSET + 2]
    if header[:4] != ELF_MAGIC or len(header) < ELF_TYPE_OFFSET + 2:
        raise ValueError(f'{executable} is not an ELF file')
    byteorder = 'little' if header[5] == 1 else 'big'
    return int.from_bytes(header[ELF_TYPE_OFFSET:ELF_TYPE_OFFSET + 2], byteorder) == ET_DYN


def draws_by_tick(text: str, tick: int) -> list[tuple[int, int]]:
    """The (tick, return address) of each draw of ticks TICK-1 and TICK in TEXT, in order."""
    pending: list[int] = []
    found: list[tuple[int, int]] = []
    for line in text.splitlines():
        line = line.strip()
        draw = DRAW_LINE.match(line)
        if draw is not None:
            pending.append(int(draw.group(2), 16))
            continue
        tick_line = TICK_LINE.match(line)
        if tick_line is not None:
            number = int(tick_line.group(1))
            if tick - 1 <= number <= tick:
                found += [(number, address) for address in pending]
            pending = []
    return found


def image_base(text: str) -> int:
    match = IMAGE_ADDRESS.search(text)
    if match is None:
        raise ValueError('the log has no "ge: lp64: address space OK" line with the executable image')
    return int(match.group(1), 16)


def short_path(location: str) -> str:
    """FILE:LINE with the checkout's root removed and addr2line's discriminator dropped."""
    location = location.split(' (discriminator')[0]
    for prefix in (str(ROOT) + '/', '/home/dev/project/'):
        if location.startswith(prefix):
            return location[len(prefix):]
    return location


def callers(log: Path, executable: Path, tick: int, addr2line: str = 'addr2line') -> list[str]:
    """`tick function file:line` for each draw of ticks TICK-1 and TICK in LOG."""
    text = log.read_text(errors='replace')
    draws = draws_by_tick(text, tick)
    if not draws:
        raise ValueError(f'{log} has no "ge: rng draw" lines for ticks {tick - 1} and {tick}')
    base = image_base(text) if is_position_independent(executable) else 0
    addresses = [hex(address - base - 1) for _, address in draws]
    output = subprocess.run([addr2line, '-f', '-e', str(executable), *addresses], check=True,
                            capture_output=True, text=True).stdout.splitlines()
    if len(output) != 2 * len(draws):
        raise ValueError(f'{addr2line} printed {len(output)} lines for {len(draws)} addresses')
    return [f'{number} {output[2 * index]} {short_path(output[2 * index + 1])}'
            for index, (number, _) in enumerate(draws)]
