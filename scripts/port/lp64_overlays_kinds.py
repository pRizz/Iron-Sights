"""Find the `index`, `offset`, `opaque`, `record` and `stored` sites of one prepared C file.

These are patterns on C text after `lp64_overlays_source.prepare`, not a C parser:

- `index`: `(` CAST EXPR `)` `[` INDEX `]` (also through redundant parentheses, `(((u16 *) h))[2]`), where CAST is a cast to a pointer to a scalar type (`u8` to
  `u64`, `s8` to `s64`, `f32`, `f64`, `int`, `char`, `short`, `long`, `float`, `double`, the `stdint.h`
  and pointer-width names), or to a pointer to a pointer (`void **`, `u32 **`), with any index.
- `offset`: a unary `*` applied to an expression that reads by position through a pointer cast
  (`positional`): after redundant parentheses and a chain of pointer and integer casts are unwrapped,
  a pointer cast of a sum or difference, `*(T *) ((u8 *) x + K)`, `*((u16 *) (uintptr_t) (t - 8))`;
  a sum or difference whose left operand is a pointer cast, `*((s32 *) x + 2)`; or a pointer cast of
  an address, `*(u16 *) &src[2]`, `*(s32 *) &door->maxFrac`; or a pointer cast of any other operand, an
  offset of zero that reads the first bytes, `*(s32 *) node->Data`. T is any pointer type and K any operand,
  constant or computed (`+ offset` is a read by position too). A `*` after `return`, `case`, `else`, `do`,
  the `)` of an `if`, `while`, `for` or `switch` condition, or the `)` of a cast (`(s32) *(u16 *) p`; a
  one-name group `(x)` counts as a cast, see `cast_group_before`) is a dereference.
- `opaque`: a member named `pad`, `pad_...`, `padN...`, `padding...` or `unk...` (not `pads`), reached by `.` or `->`, indexed by an
  integer literal: `x->act_bytes.padding[84]`. The text includes the chain of postfix operators before it.
- `record`: a cast to a pointer (or pointer to pointer) to a struct or union type that `RecordTypes`
  holds, with its operand: `(ChrRecord *) prop->voidp`. A cast of `NULL` or `0` is not listed.
- `stored`: a scalar-pointer cast (as `index` has it) stored by `=` or a compound assignment in a
  local, a member or any other target, `u16 *p = (u16 *) anim;`, its text `target=value` with the
  whole value stored, the cast and anything added to it (`p=(u16*)anim`, `q=(u8*)p+size`). What is later done through the stored pointer (`p[2]`) has no text tied to the
  cast, so a change to those uses changes no key: the verdict's reason names the uses that were read.
  A store of a cast of `NULL` or `0` is not listed.

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
STATEMENT_WORDS = {'else', 'do'}
CONDITION_WORDS = {'if', 'while', 'for', 'switch'}
INT_CAST = re.compile(rf'\(\s*{QUALIFIER}(?:{SCALAR})\s*\)')
NAME_GROUP = re.compile(r'\(\s*(?:(?:const|volatile)\s+)*[A-Za-z_]\w*\s*\)')


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


def cast_group_before(code: str, index: int) -> bool:
    """Return whether the `)` just before `index` closes a cast: a scalar, pointer or one-name type group.

    A one-name group, `(x)`, may be a typedef name or a parenthesised variable, which text cannot tell
    apart; it counts as a cast, so a doubtful `(x) * (T *) p` is listed (a false site costs one review,
    a missed read costs silence). A group after an identifier (`sizeof (u32)`, a call) or after `]` is not
    a cast; after `)` it is one only when that group is a cast too (`(s32)(u32) *p`)."""
    close = index - 1
    while close >= 0 and code[close].isspace():
        close -= 1
    start = opening(code, close + 1)
    group = code[start:close + 1]
    if not any(pattern.fullmatch(group) for pattern in (INT_CAST, SCALAR_CAST, ANY_POINTER_CAST, NAME_GROUP)):
        return False
    before = previous_token(code, start)
    if before == ')':
        return cast_group_before(code, start)
    return not (before == ']' or (before[:1].isalnum() or before[:1] == '_') and before not in CAST_PREFIX_WORDS)


def unary_star(code: str, index: int) -> bool:
    """Return whether the `*` at `index` is a dereference rather than a multiplication.

    After an identifier, `)` or `]` it multiplies, except after `return`/`case`, after `else`/`do`, after
    the `)` that closes an `if`, `while`, `for` or `switch` condition, where a statement starts, and after
    the `)` of a cast (`cast_group_before`), whose operand it starts."""
    before = previous_token(code, index)
    if before in CAST_PREFIX_WORDS or before in STATEMENT_WORDS:
        return True
    if before == ')':
        close = index - 1
        while code[close].isspace():
            close -= 1
        if previous_token(code, opening(code, close + 1)) in CONDITION_WORDS:
            return True
        return cast_group_before(code, index)
    return not (before[:1].isalnum() or before[:1] == '_' or before == ']')


def first_additive(code: str, start: int, end: int) -> int:
    """Return the index of the first binary `+` or `-` of code[start:end] outside brackets, or -1."""
    depth, previous = 0, ''
    for index in range(start, end):
        char = code[index]
        if char in '([':
            depth += 1
        elif char in ')]':
            depth -= 1
        elif char in '+-' and depth == 0 and previous and previous not in '(+-*/=<>!&|,~?:' \
                and code[index + 1:index + 2] not in ('>', char) and code[index - 1:index] != char:
            return index
        if not char.isspace():
            previous = char
    return -1


def top_level_additive(code: str, start: int, end: int) -> bool:
    """Return whether code[start:end] has a binary `+` or `-` outside any brackets."""
    return first_additive(code, start, end) >= 0


def unwrap(text: str) -> str:
    """Return `text` stripped of whitespace and of parentheses that enclose all of it."""
    text = text.strip()
    while text.startswith('(') and matching(text, 0) == len(text):
        text = text[1:-1].strip()
    return text


def left_operand(text: str) -> str:
    """Return the text before the first binary `+` or `-` outside brackets (all of `text` if none)."""
    index = first_additive(text, 0, len(text))
    return text if index < 0 else text[:index]


def positional(text: str, casted: bool = False) -> bool:
    """Return whether dereferencing `text` reads memory by position through a pointer cast.

    Redundant parentheses and chains of casts (pointer or integer, `(u16 *) (uintptr_t)`) are unwrapped. It
    is positional when, under a pointer cast, the rest is a sum or difference (constant or computed) or the
    address of an object (`&x[2]`, `&p->member`), or any other operand (an offset of zero, `*(s32 *) p`);
    or when a sum's left operand is a pointer cast."""
    text = unwrap(text)
    cast = ANY_POINTER_CAST.match(text) or SCALAR_CAST.match(text) or INT_CAST.match(text)
    if cast:
        pointer = cast.re is not INT_CAST
        return positional(text[cast.end():], casted or pointer)
    if casted and text.startswith('&'):
        return True
    if top_level_additive(text, 0, len(text)):
        if casted:
            return True
        left = unwrap(left_operand(text))
        return bool(ANY_POINTER_CAST.match(left) or SCALAR_CAST.match(left))
    return casted and bool(text)


def find_index(code: str):
    for cast in SCALAR_CAST.finditer(code):
        outer = cast.start() - 1
        while outer >= 0 and code[outer].isspace():
            outer -= 1
        if outer < 0 or code[outer] != '(' or not is_cast(code, cast.start(), cast.end()):
            continue
        while True:  # climb parentheses that only wrap this group: (((u16 *) h))[2]
            close = matching(code, outer)
            after = close
            while after < len(code) and code[after].isspace():
                after += 1
            if after < len(code) and code[after] == '[':
                yield outer, matching(code, after)
                break
            parent = outer - 1
            while parent >= 0 and code[parent].isspace():
                parent -= 1
            if parent < 0 or code[parent] != '(' or code[close:matching(code, parent) - 1].strip():
                break
            outer = parent


def find_offset(code: str):
    for star in re.finditer(r'\*\s*\(', code):
        if not unary_star(code, star.start()):
            continue
        end = operand_end(code, star.start() + 1)
        if positional(code[star.start() + 1:end]):
            yield star.start(), end


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


def compound_start(code: str, equals: int):
    """Return where the assignment operator ending in the `=` at `equals` starts (`+=`, `<<=`), or None
    when that `=` belongs to a comparison (`==`, `!=`, `<=`, `>=`)."""
    if code[equals - 2:equals] in ('<<', '>>'):
        return equals - 2
    if code[equals - 1:equals] in ('=', '!', '<', '>'):
        return None
    return equals - 1 if code[equals - 1:equals] in tuple('+-*/%&|^') else equals


def find_stored(code: str):
    """Yield (start, text) for each assignment or initialization of a scalar-pointer cast."""
    for cast in SCALAR_CAST.finditer(code):
        if previous_token(code, cast.start()) != '=' or not is_cast(code, cast.start(), cast.end()):
            continue
        equals = code.rindex('=', 0, cast.start())
        operator = compound_start(code, equals)
        if operator is None:
            continue
        end = value_end(code, cast.end())
        if code[cast.end():end].strip() in ('NULL', '0'):
            continue
        target = normalize(code[postfix_start(code, operator):operator])
        yield cast.start(), f'{target}{code[operator:equals]}={normalize(code[cast.start():end])}'


def value_end(code: str, index: int) -> int:
    """Return the end of the assigned value starting at `index`: the `;`, the `,` or the unmatched
    closing bracket that ends it, so an offset added after the cast (`(u8 *) p + size`) is in the text."""
    depth = 0
    for position in range(index, len(code)):
        char = code[position]
        if char in '([{':
            depth += 1
        elif char in ')]}':
            if depth == 0:
                return position
            depth -= 1
        elif char in ';,' and depth == 0:
            return position
    return len(code)


def texts(code: str, spans):
    for start, end in spans:
        yield start, normalize(code[start:end])


def sites_in_code(path: str, code: str, owner, line, types: RecordTypes, base: int = 0) -> list[Site]:
    """Return the sites of `code`, owned by `owner(offset)`; offsets are shifted by `base`."""
    finders = (('index', texts(code, find_index(code))), ('offset', texts(code, find_offset(code))),
               ('opaque', texts(code, find_opaque(code))), ('record', texts(code, find_record(code, types))),
               ('stored', find_stored(code)))
    found = sorted((start, kind, text) for kind, found_texts in finders for start, text in found_texts)
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
