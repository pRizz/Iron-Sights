"""Prepare a C file for the overlay lister: blank what a 64-bit PC build does not compile, name owners.

`prepare(text)` returns the file's code with comments, string and character literals, preprocessor
lines and every line a 64-bit PC build leaves out blanked to spaces (offsets and line numbers are kept),
the `#define` bodies it does compile, and the owner of every offset: the function it is in, or the
object a file-scope initializer defines.

Preprocessor regions: a condition is evaluated with three values. `TARGET_N64`, `!IS_64_BIT`, `0` and
the symbols of `UNDEFINED_ON_PC` and `UNDEFINED_AT_64` are false; `IS_64_BIT`, `1`, `__SIZEOF_POINTER__ == 8` and the symbols
of `DEFINED_ON_PC` (CMakeLists.txt's `add_compile_definitions`) are true; every other symbol is unknown,
and a branch whose condition is unknown is kept, together with its `#else`. `&&`, `||`, `!` and
parentheses combine them (false && unknown is false, true || unknown is true). A region nested in one
left out is left out. `#include`, `#undef` and the like are blanked; `#define` lines inside a region
left out are not read.
"""
import re

# Symbols CMakeLists.txt defines for every PC build (its add_compile_definitions list).
DEFINED_ON_PC = ('VERSION_US', 'LANG_US', 'REFRESH_NTSC', 'LEFTOVERDEBUG', 'LEFTOVERSPECTRUM', 'BUGFIX_R0',
                 'NONMATCHING', 'AVOID_UB', '_LANGUAGE_C')
# Symbols no PC build defines (not in CMakeLists.txt or cmake/, no `#define` in the tree), so code under
# them is dead on PC. test_lp64_overlays.py checks the build files never name one.
UNDEFINED_ON_PC = ('TARGET_N64', 'VERSION_EU', 'VERSION_JP', 'DEBUG', 'BUGFIX_R1', 'REFRESH_PAL',
                   'AUD_PROFILE', 'ENABLE_USB', 'AIPARSE', 'XBLADEBUG', 'LIBDRAGON')
# Never defined in a 64-bit build: CMakeLists.txt refuses GE_POINTER_BITS=64 on Windows and with
# Emscripten, and only cmake/emscripten.cmake defines GE_WEB. The 32-bit builds do define them.
UNDEFINED_AT_64 = ('_WIN32', '__EMSCRIPTEN__', 'GE_WEB')
TRUE_ATOMS = {'IS_64_BIT', '1'}

DIRECTIVE = re.compile(r'^[ \t]*#[ \t]*(\w*)(.*)$')
IDENT = re.compile(r'[A-Za-z_]\w*')
KEYWORDS = {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'do', 'else', 'case', '__attribute__',
            'typeof', '__typeof__', 'defined', 'alignof', '_Alignof', 'asm', '__asm__'}


class SourceError(ValueError):
    """A file the lister cannot read safely (unbalanced braces after its regions are applied)."""


def blank_comments_and_literals(text: str) -> str:
    """Return `text` with comments and string/character literals replaced by spaces (newlines kept)."""
    out = list(text)
    index, length = 0, len(text)

    def blank(start, end):
        for position in range(start, end):
            # A backslash ending a line splices lines before comments are removed (C's phase 2).
            if out[position] != '\n' and not text.startswith('\\\n', position):
                out[position] = ' '

    while index < length:
        char = text[index]
        if text.startswith('/*', index):
            end = text.find('*/', index + 2)
            end = length if end < 0 else end + 2
            blank(index, end)
            index = end
        elif text.startswith('//', index):
            end = text.find('\n', index)
            end = length if end < 0 else end
            blank(index, end)
            index = end
        elif char in '"\'':
            end = index + 1
            while end < length and text[end] != char and text[end] != '\n':
                end += 2 if text[end] == '\\' else 1
            blank(index + 1, min(end, length))
            index = end + 1
        else:
            index += 1
    return ''.join(out)


def _tokens(expression: str) -> list[str]:
    return re.findall(r'defined|\w+|&&|\|\||==|!=|>=|<=|[!()<>]', expression)


def evaluate(expression: str):
    """Return True, False or None (unknown) for an `#if` expression, as a 64-bit PC build sees it."""
    tokens = _tokens(expression)
    position = 0

    def peek():
        return tokens[position] if position < len(tokens) else None

    def take():
        nonlocal position
        position += 1
        return tokens[position - 1]

    def atom():
        token = take() if peek() else None
        if token == '!':
            value = atom()
            return None if value is None else not value
        if token == '(':
            value = disjunction()
            if peek() == ')':
                take()
            return value
        if token == 'defined':
            parenthesised = peek() == '('
            if parenthesised:
                take()
            name = take() if peek() else ''
            if parenthesised and peek() == ')':
                take()
            return symbol(name)
        if token is None:
            return None
        if peek() in ('==', '!=', '>=', '<=', '>', '<'):
            operator, right = take(), take() if peek() else ''
            return compare(token, operator, right)
        return literal(token)

    def conjunction():
        value = atom()
        while peek() == '&&':
            take()
            right = atom()
            value = False if value is False or right is False else (None if None in (value, right) else True)
        return value

    def disjunction():
        value = conjunction()
        while peek() == '||':
            take()
            right = conjunction()
            value = True if value is True or right is True else (None if None in (value, right) else False)
        return value

    return disjunction()


def symbol(name: str):
    """Return whether a 64-bit PC build defines `name`: True, False or None (unknown)."""
    if name in UNDEFINED_ON_PC or name in UNDEFINED_AT_64:
        return False
    if name in DEFINED_ON_PC or name == 'IS_64_BIT':
        return True
    return None


def literal(token: str):
    if token in TRUE_ATOMS:
        return True
    if token == '0' or token in UNDEFINED_ON_PC or token in UNDEFINED_AT_64:
        return False
    return None


def compare(left: str, operator: str, right: str):
    if left == '__SIZEOF_POINTER__' and right.isdigit():
        return {'==': int(right) == 8, '!=': int(right) != 8, '>=': 8 >= int(right),
                '<=': 8 <= int(right), '>': 8 > int(right), '<': 8 < int(right)}[operator]
    return None


def logical_lines(text: str):
    """Yield (first line index, joined text, line count) with backslash continuations joined."""
    lines = text.split('\n')
    index = 0
    while index < len(lines):
        start, joined = index, lines[index]
        while joined.endswith('\\') and index + 1 < len(lines):
            index += 1
            joined = joined[:-1] + ' ' + lines[index]
        yield start, joined, index - start + 1
        index += 1


class Prepared:
    """A file's compiled code, its compiled macro bodies and the owner of each offset."""

    def __init__(self, code: str, macros: list[tuple[str, int, str]]):
        self.code = code
        self.macros = macros
        self.spans = owner_spans(code)

    def owner(self, offset: int) -> str:
        for start, end, name in self.spans:
            if start <= offset < end:
                return name
        return '-'

    def line(self, offset: int) -> int:
        return self.code.count('\n', 0, offset) + 1


def prepare(text: str) -> Prepared:
    """Return `text` prepared for the finders; see the module's docstring."""
    clean = blank_comments_and_literals(text)
    lines = clean.split('\n')
    offsets = [0]
    for line in lines:
        offsets.append(offsets[-1] + len(line) + 1)
    keep = [True] * len(lines)
    macros = []
    stack = []  # entries: [state of this branch, a branch surely taken, a branch of unknown state seen]
    for start, joined, count in logical_lines(clean):
        match = DIRECTIVE.match(joined)
        active = all(entry[0] is not False for entry in stack)
        if not match:
            if not active:
                keep[start:start + count] = [False] * count
            continue
        keep[start:start + count] = [False] * count
        name, rest = match.group(1), match.group(2)
        if name in ('if', 'ifdef', 'ifndef'):
            value = {'if': evaluate, 'ifdef': symbol,
                     'ifndef': lambda s: None if symbol(s) is None else not symbol(s)}[name](rest.strip())
            stack.append([value, value is True, value is None])
        elif name in ('elif', 'else') and stack:
            entry = stack[-1]
            raw = False if entry[1] else (True if name == 'else' else evaluate(rest))
            # After a branch of unknown state, a true one may not be the one taken, but it closes the rest.
            value = None if entry[2] and raw is True else raw
            entry[0], entry[1], entry[2] = value, entry[1] or raw is True, entry[2] or value is None
        elif name == 'endif' and stack:
            stack.pop()
        elif name == 'define' and active:
            definition = re.match(r'\s*([A-Za-z_]\w*)(\([^)]*\))?(.*)$', rest)
            if definition:
                body_offset = offsets[start] + joined.find(rest) + definition.start(3)
                macros.append((definition.group(1), body_offset, definition.group(3)))
    code = '\n'.join(line if kept else ' ' * len(line) for line, kept in zip(lines, keep))
    return Prepared(code, macros)


def _declared_name(segment: str) -> str:
    """Return the name a file-scope declaration defines: the last identifier before `=` or the end."""
    head = segment.split('=', 1)[0]
    head = re.sub(r'\[[^\]]*\]', '', head)
    names = [name for name in IDENT.findall(head) if name not in KEYWORDS]
    return names[-1] if names else '-'


def _function_name(header: str):
    """Return the function a definition header names, or None when the header defines no function."""
    depth, candidates = 0, []
    for match in re.finditer(r'[()]|[A-Za-z_]\w*(?=\s*\()', header):
        token = match.group()
        if token == '(':
            depth += 1
        elif token == ')':
            depth -= 1
        elif token not in KEYWORDS:
            candidates.append((depth, token))
    top = [name for depth_, name in candidates if depth_ == 0]
    if top:
        return top[-1]
    return candidates[-1][1] if candidates else None


def _top_level_owner(header: str) -> str:
    """Return the owner of the braces that follow `header` (text since the previous top-level `}`)."""
    segment = header.rsplit(';', 1)[-1]
    if re.match(r'\s*(typedef\s+)?(struct|union|enum)\b', segment) and '(' not in segment:
        tag = re.match(r'\s*(?:typedef\s+)?(?:struct|union|enum)\s*(\w*)', segment).group(1)
        return tag or '-'
    if '=' in segment:
        return _declared_name(segment)
    return _function_name(segment if '(' in segment else header) or '-'


def owner_spans(code: str) -> list[tuple[int, int, str]]:
    """Return (start, end, owner) for each top-level brace body and file-scope statement of `code`."""
    spans, depth = [], 0
    header_start = statement_start = 0
    body_start, body_owner = 0, '-'
    for match in re.finditer(r'[{};]', code):
        char, position = match.group(), match.start()
        if char == '{':
            if depth == 0:
                body_start, body_owner = position, _top_level_owner(code[header_start:position])
            depth += 1
        elif char == '}':
            depth -= 1
            if depth < 0:
                raise SourceError(f'unbalanced braces: a closing brace at line {code.count(chr(10), 0, position) + 1}')
            if depth == 0:
                spans.append((body_start, position + 1, body_owner))
                header_start = position + 1
                if not re.search(r'[=]|\b(struct|union|enum)\b', code[statement_start:body_start]):
                    statement_start = position + 1
        elif depth == 0:
            spans.append((statement_start, position + 1, _declared_name(code[statement_start:position])))
            statement_start = position + 1  # header_start stays: a K&R definition declares before its brace
    if depth != 0:
        raise SourceError(f'unbalanced braces: {depth} still open at the end of the file')
    return spans


def normalize(text: str) -> str:
    """Return `text` with whitespace removed, except one space between two identifier characters."""
    collapsed = re.sub(r'\s+', ' ', text.strip())
    return re.sub(r' (?=\W)|(?<=\W) ', '', collapsed)
