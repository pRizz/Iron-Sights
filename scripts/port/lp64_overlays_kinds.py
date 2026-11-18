"""Find the `index`, `offset`, `opaque` and `record` sites of one prepared C file.

These are patterns on C text after `lp64_overlays_source.prepare`, not a C parser:

- `index`: `(` CAST EXPR `)` `[` INDEX `]`, where CAST is a cast to a pointer to a scalar type (`u8` to
  `u64`, `s8` to `s64`, `f32`, `f64`, `int`, `char`, `short`, `long`, `float`, `double`, the `stdint.h`
  and pointer-width names), or to a pointer to a pointer (`void **`, `u32 **`), with any index.
- `offset`: a unary `*` applied to a cast of a parenthesised sum or difference, `*(T *) ((u8 *) x + K)`,
  or to a parenthesised sum or difference whose left operand starts with a cast, `*((s32 *) x + 2)`;
  T is any pointer type, and K any operand, constant or not.
- `opaque`: a member named `pad`, `pad_...`, `padN...`, `padding...` or `unk...` (not `pads`), reached by `.` or `->`, indexed by an
  integer literal: `x->act_bytes.padding[84]`. The text includes the chain of postfix operators before it.
- `record`: a cast to a pointer (or pointer to pointer) to a struct or union type that `RecordTypes`
  holds, with its operand: `(ChrRecord *) prop->voidp`. A cast of `NULL` or `0` is not listed.

A parenthesised type counts as a cast only when it is followed by the start of an operand and is not
preceded by an identifier other than `return` or `case` (a call or a prototype) or by `)` (a declarator).
"""
from dataclasses import dataclass
import re

from lp64_overlays_source import KEYWORDS, Prepared, normalize

SCALAR = (r'(?:u|s)(?:8|16|32|64)|f32|f64|int|char|short|long|float|double|unsigned|signed|'
          r'u?int(?:8|16|32|64|ptr)_t|size_t|ssize_t|uintptr_t|intptr_t|ptrdiff_t|bool|_Bool|'
          r'romptr_t|vu32|vs32|vu16|vu8|uchar|ushort|uint|ulong')
QUALIFIER = r'(?:(?:const|volatile|unsigned|signed|long|short)\s+)*'
SCALAR_CAST = re.compile(rf'\(\s*{QUALIFIER}(?:(?:{SCALAR})\s*(?:const\s*|volatile\s*)?\*+|void\s*(?:const\s*)?\*\s*\*+)'
                         r'\s*(?:const\s*)?\)')
ANY_POINTER_CAST = re.compile(r'\(\s*(?:(?:const|volatile|unsigned|signed|struct|union)\s+)*[A-Za-z_]\w*\s*'
                              r'(?:const\s*|volatile\s*)?\*+\s*(?:const\s*)?\)')
RECORD_CAST = re.compile(r'\(\s*(?:(?:const|volatile)\s+)?(?:(struct|union)\s+)?([A-Za-z_]\w*)\s*'
                         r'(?:const\s*)?(\*{1,2})\s*(?:const\s*)?\)')
OPAQUE = re.compile(r'(?:->|\.)\s*(pad(?:ding\w*|[_\d]\w*)?|unk\w*)\s*\[\s*(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*\]')
OPERAND_START = re.compile(r'\s*[\w(&*\-!~+]')
CAST_PREFIX_WORDS = {'return', 'case'}


@dataclass(frozen=True)
class Site:
    """One site: `key` is `file:function:kind:text`; `line` is for display only."""
    path: str
    function: str
    kind: str
    text: str
    line: int

    @property
    def key(self) -> str:
        return f'{self.path}:{self.function}:{self.kind}:{self.text}'


@dataclass(frozen=True)
class RecordTypes:
    """The struct and union names declared in the record headers: typedef names and tags."""
    typedefs: frozenset
    tags: frozenset


def matching(code: str, start: int) -> int:
    """Return the index just past the bracket that closes the one at `start`."""
    pairs = {'(': ')', '[': ']', '{': '}'}
    opener, closer, depth = code[start], pairs[code[start]], 0
    for index in range(start, len(code)):
        depth += (code[index] == opener) - (code[index] == closer)
        if depth == 0:
            return index + 1
    return len(code)


def opening(code: str, end: int) -> int:
    """Return the index of the bracket that opens the one closing at `end - 1`."""
    pairs = {')': '(', ']': '[', '}': '{'}
    closer, opener, depth = code[end - 1], pairs[code[end - 1]], 0
    for index in range(end - 1, -1, -1):
        depth += (code[index] == closer) - (code[index] == opener)
        if depth == 0:
            return index
    return 0


def previous_token(code: str, index: int) -> str:
    """Return the identifier or the single character just before `index`, skipping spaces."""
    position = index - 1
    while position >= 0 and code[position].isspace():
        position -= 1
    if position < 0:
        return ''
    if code[position].isalnum() or code[position] == '_':
        start = position
        while start > 0 and (code[start - 1].isalnum() or code[start - 1] == '_'):
            start -= 1
        return code[start:position + 1]
    return code[position]


def is_cast(code: str, start: int, end: int) -> bool:
    """Return whether the parenthesised type at `start:end` is a cast (see the module's docstring)."""
    before = previous_token(code, start)
    if before == ')' or before == ']' or (before[:1].isalnum() or before[:1] == '_') and before not in CAST_PREFIX_WORDS:
        return False
    return bool(OPERAND_START.match(code, end))


def operand_end(code: str, index: int) -> int:
    """Return the end of the unary expression starting at `index` (prefix operators, primary, postfix)."""
    length = len(code)

    def skip(position):
        while position < length and code[position].isspace():
            position += 1
        return position

    position = skip(index)
    while position < length and code[position] in '&*-!~+':
        position = skip(position + 1)
    cast = ANY_POINTER_CAST.match(code, position) or SCALAR_CAST.match(code, position)
    if cast:
        return operand_end(code, cast.end())
    if position < length and code[position] == '(':
        position = matching(code, position)
    else:
        word = re.compile(r'[\w.]+').match(code, position)
        position = word.end() if word else position
    while True:
        after = skip(position)
        if code.startswith('->', after) or code.startswith('.', after):
            step = 2 if code.startswith('->', after) else 1
            word = re.compile(r'\s*\w+').match(code, after + step)
            if not word:
                return position
            position = word.end()
        elif after < length and code[after] in '[(':
            position = matching(code, after)
        else:
            return position


def postfix_start(code: str, end: int) -> int:
    """Return the start of the postfix chain (`a->b.c[2]`, `f(x)->y`, `(p)->q`) that ends at `end`."""
    def last(position):
        cursor = position - 1
        while cursor >= 0 and code[cursor].isspace():
            cursor -= 1
        return cursor

    position = end
    while True:
        cursor = last(position)
        if cursor < 0:
            return position
        if code[cursor] in ')]':
            position = opening(code, cursor + 1)
        elif code[cursor].isalnum() or code[cursor] == '_':
            word = previous_token(code, cursor + 1)
            if word in KEYWORDS:
                return position
            position = cursor + 1 - len(word)
        else:
            return position
        before = last(position)
        if before < 0:
            return position
        if code[before] == '.' or code[before - 1:before + 1] == '->':
            position = before if code[before] == '.' else before - 1
        elif code[position] == '[' and (code[before] in ')]' or code[before].isalnum() or code[before] == '_'):
            continue
        elif code[position] == '(' and (code[before].isalnum() or code[before] == '_') \
                and previous_token(code, before + 1) not in KEYWORDS:
            continue
        else:
            return position


def unary_star(code: str, index: int) -> bool:
    """Return whether the `*` at `index` is a dereference rather than a multiplication."""
    before = previous_token(code, index)
    return not (before[:1].isalnum() or before[:1] == '_' or before in (')', ']')) or before in CAST_PREFIX_WORDS


def top_level_additive(code: str, start: int, end: int) -> bool:
    """Return whether code[start:end] has a binary `+` or `-` outside any brackets."""
    depth, previous = 0, ''
    for index in range(start, end):
        char = code[index]
        if char in '([':
            depth += 1
        elif char in ')]':
            depth -= 1
        elif char in '+-' and depth == 0 and previous and previous not in '(+-*/=<>!&|,~?:' \
                and code[index + 1:index + 2] not in ('>', char) and code[index - 1:index] != char:
            return True
        if not char.isspace():
            previous = char
    return False


def find_index(code: str):
    for cast in SCALAR_CAST.finditer(code):
        outer = cast.start() - 1
        while outer >= 0 and code[outer].isspace():
            outer -= 1
        if outer < 0 or code[outer] != '(' or not is_cast(code, cast.start(), cast.end()):
            continue
        close = matching(code, outer)
        after = close
        while after < len(code) and code[after].isspace():
            after += 1
        if after < len(code) and code[after] == '[':
            yield outer, matching(code, after)


def find_offset(code: str):
    for star in re.finditer(r'\*\s*\(', code):
        if not unary_star(code, star.start()):
            continue
        paren = star.end() - 1
        cast = ANY_POINTER_CAST.match(code, paren)
        if cast and is_cast(code, cast.start(), cast.end()):
            group = cast.end()
            while group < len(code) and code[group].isspace():
                group += 1
            if group < len(code) and code[group] == '(':
                close = matching(code, group)
                if top_level_additive(code, group + 1, close - 1):
                    yield star.start(), close
            continue
        close = matching(code, paren)
        inner = paren + 1
        while inner < close and code[inner].isspace():
            inner += 1
        first = ANY_POINTER_CAST.match(code, inner)
        if first and is_cast(code, first.start(), first.end()) and top_level_additive(code, paren + 1, close - 1):
            yield star.start(), close


def find_opaque(code: str):
    for member in OPAQUE.finditer(code):
        yield postfix_start(code, member.start()), member.end()


def find_record(code: str, types: RecordTypes):
    for cast in RECORD_CAST.finditer(code):
        keyword, name = cast.group(1), cast.group(2)
        known = name in types.tags if keyword else name in types.typedefs
        if not known or not is_cast(code, cast.start(), cast.end()):
            continue
        end = operand_end(code, cast.end())
        if code[cast.end():end].strip() in ('NULL', '0'):
            continue
        yield cast.start(), end


def sites_in_code(path: str, code: str, owner, line, types: RecordTypes, base: int = 0) -> list[Site]:
    """Return the sites of `code`, owned by `owner(offset)`; offsets are shifted by `base`."""
    found = []
    finders = (('index', find_index(code)), ('offset', find_offset(code)), ('opaque', find_opaque(code)),
               ('record', find_record(code, types)))
    for kind, spans in finders:
        for start, end in spans:
            found.append((start, kind, normalize(code[start:end])))
    found.sort()
    return [Site(path, owner(base + start), kind, text, line(base + start)) for start, kind, text in found]


def sites_in_prepared(path: str, prepared: Prepared, types: RecordTypes) -> list[Site]:
    """Return the sites of a prepared file and of the macro bodies it compiles, numbered by occurrence."""
    sites = sites_in_code(path, prepared.code, prepared.owner, prepared.line, types)
    for name, offset, body in prepared.macros:
        sites += sites_in_code(path, body, lambda _offset, n=name: f'#define {n}', prepared.line, types,
                               base=offset)
    return number_occurrences(sites)


def number_occurrences(sites: list[Site]) -> list[Site]:
    """Append ` #N` to the text of the Nth occurrence (N >= 2) of one key within the file."""
    seen, numbered = {}, []
    for site in sorted(sites, key=lambda s: s.line):
        count = seen[site.key] = seen.get(site.key, 0) + 1
        numbered.append(site if count == 1 else Site(site.path, site.function, site.kind,
                                                      f'{site.text} #{count}', site.line))
    return numbered
