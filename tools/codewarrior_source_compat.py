#!/usr/bin/env python3
"""Generate GCC-compatible views of legacy CodeWarrior source files.

The adapter is intentionally source-agnostic.  It reconciles linkage forms
accepted by CodeWarrior, expands implementation fragments while adapting them,
and fixes a few historical preprocessing/declaration forms without changing
the original source tree.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Iterable, NamedTuple


FUNCTION_DECLARATION = re.compile(
    r"(?m)^[ \t]*(?:extern[ \t]+)?"
    r"(?!static\b|typedef\b|#)"
    r"[^;{}\n]*?[ \t*](?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"[ \t]*\([^;{}]*\)[ \t]*;"
)
VARIABLE_DECLARATION = re.compile(
    r"(?m)^[ \t]*extern[ \t]+"
    r"[^;(){}\n]*?[ \t*](?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?:[ \t]*\[[^\]\n]*\])*[ \t]*;"
)
C_FRAGMENT_INCLUDE = re.compile(
    r'(?m)^[ \t]*#include[ \t]+"(?P<path>[^"\n]+\.c_inc)"[ \t]*$'
)
STATIC_FUNCTION_DEFINITION = re.compile(
    r"(?m)^(?P<indent>[ \t]*)(?P<storage>static)(?P<rest>[ \t]+"
    r"[^;{}\n]*?[ \t*](?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"[ \t]*\([^;{}]*\)[ \t\r\n]*\{)"
)
EXTERN_OBJECT_DECLARATION = re.compile(
    r"(?m)^(?P<indent>[ \t]*)(?P<storage>extern)(?P<rest>[ \t]+"
    r"(?P<prefix>[^;(),={}\n]*?[ \t*])"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?P<arrays>(?:[ \t]*\[[^\]\n]*\])*)[ \t]*;)"
)
STATIC_OBJECT_DEFINITION = re.compile(
    r"(?m)^(?P<indent>[ \t]*)(?P<storage>static)(?P<rest>[ \t]+"
    r"(?P<prefix>[^;(),={}\n]*?[ \t*])"
    r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
    r"(?P<arrays>(?:[ \t]*\[[^\]\n]*\])*)[ \t]*=)"
)

BYTE_SCALAR_TYPE = r"(?:u8|char|signed[ \t]+char|unsigned[ \t]+char)"
BYTE_POINTER_QUALIFIER = r"(?:const|volatile|immut)"
BYTE_POINTER_TYPE = (
    rf"(?:{BYTE_POINTER_QUALIFIER}[ \t]+)*"
    rf"{BYTE_SCALAR_TYPE}"
    rf"(?:[ \t]+{BYTE_POINTER_QUALIFIER})*[ \t]*\*[ \t]*"
    rf"(?:{BYTE_POINTER_QUALIFIER}[ \t]*)*"
)
BYTE_POINTER_DECLARATION = re.compile(
    rf"(?<![A-Za-z0-9_])(?P<type>{BYTE_POINTER_TYPE})"
    rf"(?P<name>[A-Za-z_][A-Za-z0-9_]*)[ \t]*(?P<tail>=|;)"
)
BIG_ENDIAN_BYTE_VIEW_DECLARATION = re.compile(
    rf"(?<![A-Za-z0-9_])(?P<type>{BYTE_POINTER_TYPE})"
    rf"(?P<name>[A-Za-z_][A-Za-z0-9_]*)[ \t\r\n]*=[ \t\r\n]*"
    rf"reinterpret_cast[ \t\r\n]*<[ \t\r\n]*"
    rf"(?P<cast_type>{BYTE_POINTER_TYPE})>[ \t\r\n]*"
    rf"\([ \t\r\n]*&[ \t\r\n]*"
    rf"(?P<scalar>[A-Za-z_][A-Za-z0-9_]*"
    rf"(?:[ \t\r\n]*(?:\.|->)[ \t\r\n]*[A-Za-z_][A-Za-z0-9_]*)*)"
    rf"[ \t\r\n]*\)[ \t\r\n]*;"
)


class BytePointerDeclaration(NamedTuple):
    name: str
    start: int
    end: int
    scope_depth: int
    scope_end: int
    scalar: str | None = None


def external_symbol_names(include_roots: Iterable[Path]) -> tuple[set[str], set[str], list[Path]]:
    functions: set[str] = set()
    variables: set[str] = set()
    headers: list[Path] = []

    for include_root in include_roots:
        for header in sorted(include_root.rglob("*.h")):
            if not header.is_file():
                continue
            headers.append(header.resolve())
            text = header.read_text(encoding="utf-8", errors="replace")
            functions.update(match.group("name") for match in FUNCTION_DECLARATION.finditer(text))
            variables.update(match.group("name") for match in VARIABLE_DECLARATION.finditer(text))

    return functions, variables, headers


def expand_c_inc_files(
    text: str,
    including: Path,
    search_roots: list[Path],
    dependencies: set[Path],
) -> str:
    """Inline legacy C fragments so compatibility rewrites cover them too."""

    def expand(match: re.Match[str]) -> str:
        reference = Path(match.group("path"))
        candidates = [including.parent / reference]
        candidates.extend(root / reference for root in search_roots)
        fragment = next((candidate.resolve() for candidate in candidates if candidate.is_file()), None)
        if fragment is None:
            return match.group(0)

        dependencies.add(fragment)
        fragment_text = fragment.read_text(encoding="utf-8", errors="replace")
        fragment_text = expand_c_inc_files(fragment_text, fragment, search_roots, dependencies)
        return_line = text.count("\n", 0, match.start()) + 2
        fragment_name = fragment.as_posix().replace('"', '\\"')
        including_name = including.as_posix().replace('"', '\\"')
        return (
            f'#line 1 "{fragment_name}"\n'
            f"{fragment_text}\n"
            f'#line {return_line} "{including_name}"'
        )

    return C_FRAGMENT_INCLUDE.sub(expand, text)


def mask_non_code(text: str) -> str:
    """Mask comments, literals, and preprocessor directives without moving offsets."""

    masked = list(text)
    state = "code"
    at_line_start = True
    index = 0

    def hide(position: int) -> None:
        if text[position] not in "\r\n":
            masked[position] = " "

    while index < len(text):
        character = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""

        if state == "line-comment":
            hide(index)
            if character == "\n":
                state = "code"
                at_line_start = True
            index += 1
            continue

        if state == "block-comment":
            hide(index)
            if character == "*" and following == "/":
                hide(index + 1)
                index += 2
                state = "code"
                continue
            if character == "\n":
                at_line_start = True
            index += 1
            continue

        if state in {"string", "character"}:
            hide(index)
            terminator = '"' if state == "string" else "'"
            if character == "\\" and following:
                hide(index + 1)
                if following == "\n":
                    at_line_start = True
                index += 2
                continue
            if character == terminator:
                state = "code"
            elif character == "\n":
                at_line_start = True
            index += 1
            continue

        if state == "directive":
            hide(index)
            if character == "\n":
                if index == 0 or text[index - 1] != "\\":
                    state = "code"
                at_line_start = True
            index += 1
            continue

        if at_line_start:
            if character in " \t\r":
                index += 1
                continue
            if character == "\n":
                index += 1
                continue
            if character == "#":
                hide(index)
                state = "directive"
                index += 1
                continue

        if character == "/" and following == "/":
            hide(index)
            hide(index + 1)
            state = "line-comment"
            index += 2
            continue
        if character == "/" and following == "*":
            hide(index)
            hide(index + 1)
            state = "block-comment"
            index += 2
            continue
        if character == '"':
            hide(index)
            state = "string"
            at_line_start = False
            index += 1
            continue
        if character == "'":
            hide(index)
            state = "character"
            at_line_start = False
            index += 1
            continue

        if character == "\n":
            at_line_start = True
        elif character not in " \t\r":
            at_line_start = False
        index += 1

    return "".join(masked)


def brace_depths(masked: str) -> list[int]:
    """Return the lexical brace depth immediately before each character."""

    depths = [0] * (len(masked) + 1)
    depth = 0
    for index, character in enumerate(masked):
        depths[index] = depth
        if character == "{":
            depth += 1
        elif character == "}" and depth > 0:
            depth -= 1
    depths[len(masked)] = depth
    return depths


def apply_replacements(text: str, replacements: Iterable[tuple[int, int, str]]) -> str:
    for start, end, replacement in sorted(replacements, reverse=True):
        text = text[:start] + replacement + text[end:]
    return text


def lexical_scope_end(
    masked: str,
    depths: list[int],
    position: int,
    scope_depth: int,
) -> int:
    """Find the closing brace for the scope containing ``position``."""

    if scope_depth == 0:
        return len(masked)
    for index in range(position, len(masked)):
        if masked[index] == "}" and depths[index] == scope_depth:
            return index
    return len(masked)


def matching_subscript_end(masked: str, opening: int) -> int | None:
    """Return the closing bracket for a subscript in already-masked code."""

    depth = 1
    for index in range(opening + 1, len(masked)):
        if masked[index] == "[":
            depth += 1
        elif masked[index] == "]":
            depth -= 1
            if depth == 0:
                return index
    return None


def is_adapted_byte_subscript(index_text: str, scalar: str) -> bool:
    """Recognize this pass's output so adapting a generated view is stable."""

    compact_index = re.sub(r"\s+", "", index_text)
    compact_scalar = re.sub(r"\s+", "", scalar)
    prefix = f"(sizeof({compact_scalar})-1)-("
    return compact_index.startswith(prefix) and compact_index.endswith(")")


def adapt_big_endian_scalar_byte_views(text: str) -> str:
    """Emulate CodeWarrior's big-endian byte view of local scalar objects.

    Legacy code commonly takes a byte pointer to a scalar and then addresses
    that scalar by byte index.  The index denotes a big-endian target-memory
    byte, not a serialized file byte, so the same source selects the opposite
    host-memory byte on a little-endian PC.  For explicitly recognizable byte
    views, rewrite ``bytes[index]`` as
    ``bytes[(sizeof(scalar) - 1) - (index)]`` until the declaration's lexical
    scope ends.

    This deliberately handles only declarations initialized directly from
    ``reinterpret_cast<byte-type*>(&scalar)``.  It does not infer provenance
    through assignments or function calls.
    """

    masked = mask_non_code(text)
    depths = brace_depths(masked)
    adapted_declarations: dict[tuple[int, str], tuple[int, str]] = {}
    for declaration in BIG_ENDIAN_BYTE_VIEW_DECLARATION.finditer(masked):
        key = (declaration.start(), declaration.group("name"))
        scalar = text[
            declaration.start("scalar") : declaration.end("scalar")
        ].strip()
        adapted_declarations[key] = (declaration.end(), scalar)

    if not adapted_declarations:
        return text

    declarations: list[BytePointerDeclaration] = []
    for declaration in BYTE_POINTER_DECLARATION.finditer(masked):
        name = declaration.group("name")
        key = (declaration.start(), name)
        adapted = adapted_declarations.get(key)
        declaration_end = adapted[0] if adapted is not None else declaration.end()
        scope_depth = depths[declaration.start()]
        declarations.append(
            BytePointerDeclaration(
                name=name,
                start=declaration.start(),
                end=declaration_end,
                scope_depth=scope_depth,
                scope_end=lexical_scope_end(
                    masked,
                    depths,
                    declaration_end,
                    scope_depth,
                ),
                scalar=adapted[1] if adapted is not None else None,
            )
        )

    names = sorted(
        {declaration.name for declaration in declarations},
        key=len,
        reverse=True,
    )
    if not names:
        return text
    subscript = re.compile(
        rf"(?<![A-Za-z0-9_.>:])(?P<name>{'|'.join(map(re.escape, names))})"
        rf"[ \t\r\n]*(?P<opening>\[)"
    )

    insertions: list[tuple[int, int, str]] = []
    for use in subscript.finditer(masked):
        use_start = use.start("name")
        visible = [
            declaration
            for declaration in declarations
            if declaration.name == use.group("name")
            and declaration.end <= use_start < declaration.scope_end
        ]
        if not visible:
            continue
        declaration = max(
            visible,
            key=lambda candidate: (candidate.scope_depth, candidate.start),
        )
        if declaration.scalar is None:
            # A nested ordinary byte pointer shadows an adapted outer view.
            continue

        opening = use.start("opening")
        closing = matching_subscript_end(masked, opening)
        if closing is None or closing >= declaration.scope_end:
            continue
        index_text = masked[opening + 1 : closing]
        if not index_text.strip() or is_adapted_byte_subscript(
            index_text,
            declaration.scalar,
        ):
            continue

        insertions.append(
            (
                opening + 1,
                opening + 1,
                f"(sizeof({declaration.scalar}) - 1) - (",
            )
        )
        insertions.append((closing, closing, ")"))

    return apply_replacements(text, insertions)


def adapt_same_tu_function_linkage(text: str) -> str:
    """Make same-TU prototypes local when a later file-scope definition is local."""

    masked = mask_non_code(text)
    depths = brace_depths(masked)
    definitions_by_name: dict[str, list[int]] = {}
    for definition in STATIC_FUNCTION_DEFINITION.finditer(masked):
        if depths[definition.start("storage")] == 0:
            definitions_by_name.setdefault(definition.group("name"), []).append(
                definition.start()
            )

    replacements: list[tuple[int, int, str]] = []
    for name, definition_offsets in definitions_by_name.items():
        prototype = re.compile(
            rf"(?m)^(?P<indent>[ \t]*)(?!static\b|typedef\b|#)"
            rf"(?P<decl>(?P<extern>extern[ \t]+)?[A-Za-z_][A-Za-z0-9_]*"
            rf"(?:[ \t*]+[A-Za-z_][A-Za-z0-9_]*)*[ \t*]+"
            rf"{re.escape(name)}[ \t]*\([^;{{}}]*\)[ \t]*;)"
        )
        first_definition = min(definition_offsets)
        for declaration in prototype.finditer(masked, 0, first_definition):
            if depths[declaration.start("decl")] != 0:
                continue
            extern = declaration.group("extern")
            if extern is not None:
                replacements.append(
                    (declaration.start("extern"), declaration.end("extern"), "static ")
                )
            else:
                replacements.append(
                    (declaration.start("decl"), declaration.start("decl"), "static ")
                )
    return apply_replacements(text, replacements)


def object_declarator_key(match: re.Match[str]) -> tuple[str, str, str]:
    return (
        match.group("name"),
        re.sub(r"\s+", "", match.group("prefix")),
        re.sub(r"\s+", "", match.group("arrays")),
    )


def adapt_same_tu_object_linkage(text: str) -> str:
    """Reconcile matching file-scope declarations without exporting definitions."""

    masked = mask_non_code(text)
    depths = brace_depths(masked)
    declarations: dict[tuple[str, str, str], list[re.Match[str]]] = {}
    for declaration in EXTERN_OBJECT_DECLARATION.finditer(masked):
        if depths[declaration.start("storage")] == 0:
            declarations.setdefault(object_declarator_key(declaration), []).append(declaration)

    replacements: list[tuple[int, int, str]] = []
    for definition in STATIC_OBJECT_DEFINITION.finditer(masked):
        if depths[definition.start("storage")] != 0:
            continue
        for declaration in declarations.get(object_declarator_key(definition), []):
            if declaration.start() >= definition.start():
                continue
            replacements.append(
                (declaration.start("storage"), declaration.end("storage"), "static")
            )
    return apply_replacements(text, replacements)


def adapt_legacy_c(text: str, external_functions: set[str], external_variables: set[str]) -> str:
    # Retain these arguments for compatibility with existing generators.  A
    # project-wide bare-name index is not sufficient evidence to change a
    # definition's linkage; only relationships visible in this TU are used.
    del external_functions, external_variables
    adapted = text

    # CodeWarrior accepted a leading token-paste operator; GCC does not.
    adapted = re.sub(r"(?<![A-Za-z0-9_])##", "", adapted)
    adapted = adapted.replace('extern "C" static ', "static ")

    # CodeWarrior accepted same-translation-unit linkage mismatches that GCC
    # rejects.  Preserve the definition's explicitly local linkage and narrow
    # only matching, earlier, file-scope declarations.
    adapted = adapt_same_tu_function_linkage(adapted)
    adapted = adapt_same_tu_object_linkage(adapted)

    # Historical implicit-int declaration forms used by several SDK-era
    # sources.
    adapted = re.sub(
        r"(?m)^(?P<indent>[ \t]*static[ \t]+)"
        r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)"
        r"(?P<array>[ \t]*\[[^\]\n]*\][ \t]*=)",
        r"\g<indent>int \g<name>\g<array>",
        adapted,
    )
    return adapted


def adapt_source(
    source: Path,
    search_roots: list[Path],
    external_functions: set[str],
    external_variables: set[str],
    *,
    big_endian_scalar_byte_views: bool = False,
) -> tuple[str, str, set[Path]]:
    source = source.resolve()
    dependencies = {source}
    original = source.read_text(encoding="utf-8", errors="replace")
    expanded = expand_c_inc_files(original, source, search_roots, dependencies)
    adapted = expanded

    if source.suffix.lower() == ".c":
        adapted = adapt_legacy_c(adapted, external_functions, external_variables)

    adapted = adapted.replace("va_end();", "va_end(vl);")
    adapted = re.sub(
        r"\(\(OSMessage\)(?P<value>[0-9]+)\)",
        r"((OSMessage)(uintptr_t)\g<value>)",
        adapted,
    )
    if big_endian_scalar_byte_views:
        adapted = adapt_big_endian_scalar_byte_views(adapted)
    return expanded, adapted, dependencies


def load_symbols(path: Path) -> tuple[set[str], set[str]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    return set(data["functions"]), set(data["variables"])


def dep_escape(path: Path | str) -> str:
    return str(path).replace("\\", "/").replace(" ", "\\ ").replace(":", "\\:")


def write_depfile(path: Path, target: Path, dependencies: Iterable[Path]) -> None:
    deps = " ".join(dep_escape(dep) for dep in sorted(set(dependencies)))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(f"{dep_escape(target)}: {deps}\n", encoding="utf-8")


def command_symbols(args: argparse.Namespace) -> int:
    functions, variables, headers = external_symbol_names(args.include_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps({"functions": sorted(functions), "variables": sorted(variables)}, indent=2),
        encoding="utf-8",
    )
    if args.depfile:
        write_depfile(args.depfile, args.output, headers)
    return 0


def source_search_roots(source_root: Path) -> list[Path]:
    source_root = source_root.resolve()
    return [source_root, source_root.parent, source_root.parent / "include"]


def command_adapt(args: argparse.Namespace) -> int:
    external_functions, external_variables = load_symbols(args.symbols)
    _, adapted, dependencies = adapt_source(
        args.input,
        source_search_roots(args.source_root),
        external_functions,
        external_variables,
        big_endian_scalar_byte_views=getattr(
            args,
            "big_endian_scalar_byte_views",
            False,
        ),
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    source_name = args.input.resolve().as_posix().replace('"', '\\"')
    args.output.write_text(f'#line 1 "{source_name}"\n{adapted}', encoding="utf-8")
    if args.depfile:
        write_depfile(args.depfile, args.output, dependencies | {args.symbols.resolve()})
    return 0


def split_manifest_sources(manifest: Path, prefixes: list[str]) -> list[str]:
    sources: list[str] = []
    for line in manifest.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line or line[0].isspace() or not line.endswith(":"):
            continue
        source = line[:-1]
        if Path(source).suffix.lower() not in {".c", ".cc", ".cpp", ".cxx"}:
            continue
        if prefixes and not any(source.startswith(prefix) for prefix in prefixes):
            continue
        sources.append(source)
    return sources


def command_discover_splits(args: argparse.Namespace) -> int:
    external_functions, external_variables, _ = external_symbol_names(args.include_root)
    search_roots = source_search_roots(args.source_root)
    for relative_source in split_manifest_sources(args.manifest, args.prefix):
        expanded, adapted, _ = adapt_source(
            args.source_root / relative_source,
            search_roots,
            external_functions,
            external_variables,
            big_endian_scalar_byte_views=getattr(
                args,
                "big_endian_scalar_byte_views",
                False,
            ),
        )
        # Expansion alone does not require a generated compatibility view;
        # the compiler already expands the same fragment.  Select only files
        # for which a compatibility rewrite changed the expanded source.
        if adapted != expanded:
            print(relative_source)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    symbols = subparsers.add_parser("symbols")
    symbols.add_argument("--include-root", type=Path, action="append", required=True)
    symbols.add_argument("--output", type=Path, required=True)
    symbols.add_argument("--depfile", type=Path)
    symbols.set_defaults(func=command_symbols)

    adapt = subparsers.add_parser("adapt")
    adapt.add_argument("--input", type=Path, required=True)
    adapt.add_argument("--output", type=Path, required=True)
    adapt.add_argument("--source-root", type=Path, required=True)
    adapt.add_argument("--symbols", type=Path, required=True)
    adapt.add_argument("--depfile", type=Path)
    adapt.add_argument("--big-endian-scalar-byte-views", action="store_true")
    adapt.set_defaults(func=command_adapt)

    discover = subparsers.add_parser("discover-splits")
    discover.add_argument("--manifest", type=Path, required=True)
    discover.add_argument("--source-root", type=Path, required=True)
    discover.add_argument("--include-root", type=Path, action="append", required=True)
    discover.add_argument("--prefix", action="append", default=[])
    discover.add_argument("--big-endian-scalar-byte-views", action="store_true")
    discover.set_defaults(func=command_discover_splits)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
