"""A survey aid for the overlay lister: scalar-pointer casts stored in a local or a member.

`u16 *p = (u16 *) anim; ... p[2]` reads a record by position, but the read through `p` comes later,
in any form, and has no text that ties it to the cast, so it cannot be a site with a stable identity
and `check` cannot hold it. `lp64_overlays.py list --locals` prints each such store once instead,
`file:function:lhs=cast operand` with its line, for a review batch to read where the stored pointer goes.
It is not part of `check`. A store of a cast of `NULL` or `0` is not printed; whether the cast's operand
is itself scalar cannot be seen from the text, so every other store is.
"""
from dataclasses import dataclass

from lp64_overlays_kinds import SCALAR_CAST, is_cast, operand_end, postfix_start, previous_token
from lp64_overlays_source import normalize, prepare


@dataclass(frozen=True)
class Store:
    """One stored cast; `key` is `file:function:text`, `line` for display."""
    path: str
    function: str
    text: str
    line: int

    @property
    def key(self) -> str:
        return f'{self.path}:{self.function}:{self.text}'


def locals_in(path: str, text: str) -> list[Store]:
    """Return every assignment or initialization of a scalar-pointer cast in one file's compiled code."""
    prepared = prepare(text)
    code, found = prepared.code, []
    for cast in SCALAR_CAST.finditer(code):
        if previous_token(code, cast.start()) != '=' or not is_cast(code, cast.start(), cast.end()):
            continue
        equals = code.rindex('=', 0, cast.start())
        if code[equals - 1:equals] in ('=', '!', '<', '>'):
            continue
        end = operand_end(code, cast.end())
        if code[cast.end():end].strip() in ('NULL', '0'):
            continue
        target = normalize(code[postfix_start(code, equals):equals])
        found.append(Store(path, prepared.owner(cast.start()), f'{target}={normalize(code[cast.start():end])}',
                           prepared.line(cast.start())))
    return found
