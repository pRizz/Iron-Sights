"""The record types the `record` kind names, and the `run` sites of a header.

`record_types` reads the record headers (`src/bondtypes.h`, `src/game/*.h`, `include/PR/libaudio.h`) as
a 64-bit PC build compiles them and collects the struct and union typedef names (`typedef struct X {...}
Name;`, `typedef struct X Name;`, `typedef Name Other;` of one of them) and tags (`struct X {`). A
typedef of a pointer (`} Name, *NamePtr;`) is not collected, so a cast through it is not seen.

`runs_in` finds, in each struct or union body of a header, a member that is not a pointer, whose type is
`Gfx`, `Model` or a record type, and that is followed directly by one or more filler members (named
`field_...`, `pad...` or `unk...`, and not themselves of such a type, which ends the run and starts
its own). Code may write a larger object through the first member's address
into the room the fillers give, so the room must hold it at both widths. The site's function is the
struct's name (its typedef name, else its tag, else the enclosing struct's), its text the member's
declaration and the number of fillers that follow it: a change to either is a new site.
"""
import re

from lp64_overlays_kinds import RecordTypes, Site, matching
from lp64_overlays_source import IDENT, normalize, prepare

RUN_TYPES = {'Gfx', 'Model'}
FILLER = re.compile(r'^(field_|pad|unk)')
QUALIFIERS = {'const', 'volatile', 'struct', 'union', 'enum', 'unsigned', 'signed', 'static', 'extern'}


def record_types(headers: dict[str, str]) -> RecordTypes:
    """Return the struct and union names the given record headers declare (see the module's docstring)."""
    typedefs, tags, aliases = set(), set(), []
    for text in headers.values():
        code = prepare(text).code
        for match in re.finditer(r'\b(struct|union)\s+(\w+)\s*\{', code):
            tags.add(match.group(2))
        for match in re.finditer(r'\btypedef\s+(?:struct|union)\s*\w*\s*\{', code):
            close = matching(code, match.end() - 1)
            declarators = code[close:code.find(';', close)]
            typedefs.update(name.strip() for name in declarators.split(',')
                            if '*' not in name and IDENT.fullmatch(name.strip()))
        for match in re.finditer(r'\btypedef\s+(?:struct|union)\s+(\w+)\s+(\w+)\s*;', code):
            typedefs.add(match.group(2))
        aliases += re.findall(r'\btypedef\s+(\w+)\s+(\w+)\s*;', code)
    changed = True
    while changed:
        changed = False
        for old, new in aliases:
            if old in typedefs and new not in typedefs:
                typedefs.add(new)
                changed = True
    return RecordTypes(frozenset(typedefs), frozenset(tags))


def members(code: str, start: int, end: int) -> list[str]:
    """Return the member declarations of the body code[start:end], a nested body counted as one."""
    found, depth, current = [], 0, start
    for index in range(start, end):
        char = code[index]
        if char in '{(':
            depth += 1
        elif char in '})':
            depth -= 1
        elif char == ';' and depth == 0:
            found.append(code[current:index].strip())
            current = index + 1
    return [member for member in found if member]


def declared(member: str) -> tuple[list[str], str, bool]:
    """Return (names declared, type name, is a pointer) for one member declaration."""
    if '{' in member or '(' in member:
        return [], '', True
    member = re.sub(r':\s*\w+\s*$', '', member)
    parts = [re.sub(r'\[[^\]]*\]', '', part) for part in member.split(',')]
    words = [word for word in IDENT.findall(parts[0]) if word not in QUALIFIERS]
    names = [IDENT.findall(part)[-1] for part in parts if IDENT.findall(part)]
    type_name = words[-2] if len(words) >= 2 else ''
    return names, type_name, '*' in member


def is_run_head(member: str, types: RecordTypes) -> bool:
    names, type_name, pointer = declared(member)
    if pointer or len(names) != 1:
        return False
    tagged = re.match(r'\s*(?:const\s+)?(struct|union)\s+(\w+)', member)
    if tagged:
        return tagged.group(2) in types.tags
    return type_name in RUN_TYPES or type_name in types.typedefs


def struct_name(code: str, match: re.Match, close: int, enclosing: str) -> str:
    """Return a struct's typedef name, else its tag, else the enclosing struct's name."""
    if code[max(0, match.start() - 64):match.start()].rstrip().endswith('typedef'):
        after = re.match(r'\s*(\w+)', code[close:])
        if after:
            return after.group(1)
    return match.group(2) or enclosing or '-'


def runs_in(path: str, text: str, types: RecordTypes) -> list[Site]:
    """Return the `run` sites of one header (see the module's docstring)."""
    prepared = prepare(text)
    code, sites, stack = prepared.code, [], []
    for match in re.finditer(r'\b(struct|union)\s*(\w*)\s*\{', code):
        open_brace = match.end() - 1
        close = matching(code, open_brace)
        while stack and stack[-1][0] <= match.start():
            stack.pop()
        name = struct_name(code, match, close, stack[-1][1] if stack else '')
        stack.append((close, name))
        body = members(code, open_brace + 1, close - 1)
        for index, member in enumerate(body):
            if not is_run_head(member, types):
                continue
            fillers = 0
            for follower in body[index + 1:]:
                names = declared(follower)[0]
                if not names or not all(FILLER.match(n) for n in names) or is_run_head(follower, types):
                    break
                fillers += len(names)
            if fillers:
                offset = code.find(member, open_brace)
                sites.append(Site(path, name, 'run', f'{normalize(member)}; +{fillers} fillers',
                                  prepared.line(offset)))
    return sites
