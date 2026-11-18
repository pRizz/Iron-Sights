"""Find the `stored` sites of one prepared C file: a scalar-pointer cast whose value is stored.

See `find_stored` for the forms taken and lp64_overlays_kinds.py's docstring for the kind. Patterns on C
text, not a C parser: a store reached through a compound literal, `(T){ (u8 *) x }`, is not taken.
"""
import re

from lp64_overlays_kinds import (ANY_POINTER_CAST, CAST_PREFIX_WORDS, INT_CAST, SCALAR_CAST, is_cast, matching,
                                 opening, postfix_start, previous_token, unary_star)
from lp64_overlays_source import KEYWORDS, normalize

# The start of a declaration: specifiers, a type name, then a declarator's stars and name.
DECLARATION_START = re.compile(r'(?:(?:static|const|volatile|extern|register|unsigned|signed|struct|union|enum)\s+)*'
                               r'[A-Za-z_]\w*(?:\s*\*|\s)[\s*]*[A-Za-z_]')


def compound_start(code: str, equals: int):
    """Return where the assignment operator ending in the `=` at `equals` starts (`+=`, `<<=`), or None
    when that `=` belongs to a comparison (`==`, `!=`, `<=`, `>=`)."""
    if code[equals - 2:equals] in ('<<', '>>'):
        return equals - 2
    if code[equals - 1:equals] in ('=', '!', '<', '>'):
        return None
    return equals - 1 if code[equals - 1:equals] in tuple('+-*/%&|^') else equals


def find_stored(code: str):
    """Yield (start, text) for each assignment or initialization whose stored value is a scalar-pointer
    cast: directly, through grouping parentheses, as a conditional arm, as the last operand of a comma
    expression, as an operand of a binary `+`, or as an element of an initializer list. One site per
    assignment, at its first such cast; the text is `target=value` with the whole value."""
    seen = set()
    for cast in SCALAR_CAST.finditer(code):
        if not is_cast(code, cast.start(), cast.end()):
            continue
        store = store_of(code, cast.start())
        if store is None or store[0] in seen:
            continue
        equals, operator, value_start, initializer = store
        end = matching(code, value_start - 1) - 1 if initializer else value_end(code, value_start)
        value = code[value_start:end].strip()
        if value in ('NULL', '0') or code[cast.end():value_end(code, cast.end())].strip() in ('NULL', '0'):
            continue
        if not initializer and conditional_test(code, value_start, end, cast.start()):
            continue
        seen.add(equals)
        target = normalize(code[target_start(code, operator):operator])
        text = normalize(code[value_start - 1:end + 1] if initializer else value)
        yield cast.start(), f'{target}{code[operator:equals]}={text}'


def store_of(code: str, index: int):
    """Return (equals, operator, value_start, initializer) of the assignment or initializer whose stored
    value holds the cast at `index` by the forms `find_stored` names, or None."""
    position = index
    while True:
        cursor = before(code, position)
        if cursor < 0:
            return None
        char = code[cursor]
        if char == '=':
            operator = compound_start(code, cursor) if code[cursor + 1:cursor + 2] != '=' else None
            if operator is None:
                return None
            return cursor, operator, cursor + 1, False
        if char == '(':
            if not grouping(code, cursor) or followed_by_postfix(code, cursor):
                return None
            position = cursor
        elif char in '?:,' or (char == '+' and binary(code, cursor)):
            stop = skip_back(code, matching_question(code, cursor) if char == ':' else cursor)
            if stop is None:
                return None
            if code[stop] == '{':
                return initializer_of(code, stop)
            if char == ',' and code[stop] != '(':
                return None
            position = stop + 1
        elif char == '{':
            return initializer_of(code, cursor)
        else:
            return None


def initializer_of(code: str, brace: int):
    """Return the store of the initializer list opening at `brace`, or None for a block or a compound
    literal; a nested list is an element of its enclosing list."""
    cursor = before(code, brace)
    if cursor >= 0 and code[cursor] == '=' and code[cursor + 1:cursor + 2] != '=' \
            and compound_start(code, cursor) == cursor:
        return cursor, cursor, brace + 1, True
    if cursor >= 0 and code[cursor] in ',{':
        outer = skip_back(code, cursor) if code[cursor] == ',' else cursor
        if outer is not None and code[outer] == '{':
            store = initializer_of(code, outer)
            return None if store is None else (store[0], store[1], brace + 1, True)
    return None


def before(code: str, index: int) -> int:
    """Return the index of the last non-space character before `index`, or -1."""
    cursor = index - 1
    while cursor >= 0 and code[cursor].isspace():
        cursor -= 1
    return cursor


def skip_back(code: str, index: int):
    """From the operator at `index`, walk back over its left operand at bracket depth 0 and return the
    index of what bounds it: an unmatched `(` or `{`, an assignment `=`, `,`, `?` or `:`; None at `;`,
    `}` or the start."""
    depth, cursor = 0, index - 1
    while cursor >= 0:
        char = code[cursor]
        if char in ')]}':
            if char == '}' and depth == 0:
                return None
            depth += 1
        elif char in '([{':
            if depth == 0:
                return cursor if char != '[' else None
            depth -= 1
        elif depth == 0:
            if char == ';':
                return None
            if char in ',?:':
                return cursor
            if char == '=' and code[cursor + 1:cursor + 2] != '=' and compound_start(code, cursor) is not None:
                return cursor
        cursor -= 1
    return None


def matching_question(code: str, colon: int) -> int:
    """Return the index of the `?` whose `:` is at `colon` (nested conditionals counted)."""
    depth, cursor = 0, colon - 1
    while cursor >= 0:
        char = code[cursor]
        if char in ')]}':
            cursor = opening(code, cursor + 1) - 1
            continue
        if char in '([{;':
            return colon
        if char == ':':
            depth += 1
        elif char == '?':
            if depth == 0:
                return cursor
            depth -= 1
        cursor -= 1
    return colon


def grouping(code: str, paren: int) -> bool:
    """Return whether the `(` at `paren` groups an expression (not a call, a cast or `sizeof`)."""
    word = previous_token(code, paren)
    if word == ')' or word == ']' or (word[:1].isalnum() or word[:1] == '_') and word not in CAST_PREFIX_WORDS:
        return False
    group = code[paren:matching(code, paren)]
    return not any(pattern.fullmatch(group) for pattern in (INT_CAST, SCALAR_CAST, ANY_POINTER_CAST))


def followed_by_postfix(code: str, paren: int) -> bool:
    """Return whether the group opening at `paren` is indexed, called or reached through `.` or `->`."""
    after = matching(code, paren)
    while after < len(code) and code[after].isspace():
        after += 1
    return code[after:after + 1] in ('[', '(', '.') or code.startswith('->', after)


def binary(code: str, plus: int) -> bool:
    """Return whether the `+` at `plus` adds two operands (not unary, not `++` or `+=`)."""
    if code[plus + 1:plus + 2] in ('+', '=') or code[plus - 1:plus] == '+':
        return False
    word = previous_token(code, plus)
    return word in (')', ']') or (word[:1].isalnum() or word[:1] == '_') and word not in KEYWORDS


def conditional_test(code: str, start: int, end: int, cast: int) -> bool:
    """Return whether the cast at `cast` is (in) the test of a conditional at depth 0 of code[start:end]."""
    depth = 0
    for position in range(start, end):
        char = code[position]
        if char in '([{':
            depth += 1
        elif char in ')]}':
            depth -= 1
        elif char == '?' and depth == 0:
            return cast < position
    return False


def target_start(code: str, operator: int) -> int:
    """Return where the assigned target ending at `operator` starts, keeping leading dereferences
    (`*p = ...`); the `*` of a declarator (`u8 **p = ...`) is not a dereference."""
    start = postfix_start(code, operator)
    stars = start
    while True:
        cursor = before(code, stars)
        if cursor < 0 or code[cursor] != '*':
            break
        stars = cursor
    if stars == start:
        return start
    cursor = before(code, stars)
    if cursor >= 0 and code[cursor] == ',' and declaration_comma(code, cursor):
        return start
    return stars if unary_star(code, stars) else start


def declaration_comma(code: str, comma: int) -> bool:
    """Return whether the `,` at `comma` separates two declarators of one declaration
    (`u8 *a = 0, *b = ...`, also in a `for` header), not two operands of an expression."""
    depth, cursor = 0, comma - 1
    while cursor >= 0:
        char = code[cursor]
        if char in ')]}':
            if char == '}' and depth == 0:
                break
            depth += 1
        elif char in '([{':
            if depth == 0:
                if char == '(' and previous_token(code, cursor) != 'for':
                    return False
                break
            depth -= 1
        elif char == ';' and depth == 0:
            break
        cursor -= 1
    statement = code[cursor + 1:comma].lstrip()
    first = statement.split(None, 1)[0] if statement else ''
    return first not in ('return', 'case', 'goto', 'else', 'do') and DECLARATION_START.match(statement) is not None


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
