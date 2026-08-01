#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import tempfile
from pathlib import Path


TOOL_PATH = Path(__file__).parents[1] / "tools" / "codewarrior_source_compat.py"
SPEC = importlib.util.spec_from_file_location("codewarrior_source_compat", TOOL_PATH)
assert SPEC is not None and SPEC.loader is not None
COMPAT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPAT)


def require_lines(actual: str, expected: list[str]) -> None:
    lines = [line.strip() for line in actual.splitlines() if line.strip()]
    assert lines == expected, f"expected {expected!r}, got {lines!r}"


require_lines(
    COMPAT.adapt_legacy_c(
        "void local_function(void);\n"
        "static void local_function(void) {}\n",
        set(),
        set(),
    ),
    [
        "static void local_function(void);",
        "static void local_function(void) {}",
    ],
)

require_lines(
    COMPAT.adapt_legacy_c(
        "void public_function(void);\n"
        "static void public_function(void) {}\n",
        {"public_function"},
        set(),
    ),
    [
        "static void public_function(void);",
        "static void public_function(void) {}",
    ],
)

# A same-named public declaration elsewhere is not evidence that this TU's
# explicitly local definition should be exported.
public_index_does_not_export = (
    "static void header_named_function(void) {}\n"
    "static int header_named_object = 1;\n"
)
assert (
    COMPAT.adapt_legacy_c(
        public_index_does_not_export,
        {"header_named_function"},
        {"header_named_object"},
    )
    == public_index_does_not_export
)

require_lines(
    COMPAT.adapt_legacy_c(
        "extern int local_object[];\n"
        "static int local_object[] = { 1 };\n",
        set(),
        set(),
    ),
    [
        "static int local_object[];",
        "static int local_object[] = { 1 };",
    ],
)

require_lines(
    COMPAT.adapt_legacy_c(
        "extern int public_object[];\n"
        "static int public_object[] = { 1 };\n",
        set(),
        {"public_object"},
    ),
    [
        "static int public_object[];",
        "static int public_object[] = { 1 };",
    ],
)

# Regression fixtures for local objects whose names also appear in unrelated
# public headers.  Their indentation is meaningful: each object has block
# scope and must retain static storage duration.
block_static_fixture = (
    "void initialize_actor(void)\n"
    "{\n"
    "    static ACTOR_PROFILE Dummy_Profile = { 0 };\n"
    "    static int kusa_group_tbl[] = { 1, 2 };\n"
    "    static rgba_t window_color = { 0 };\n"
    "}\n"
)
assert (
    COMPAT.adapt_legacy_c(
        block_static_fixture,
        set(),
        {"Dummy_Profile", "kusa_group_tbl", "window_color"},
    )
    == block_static_fixture
)

# Two unrelated TUs may intentionally use the same local symbol.  A global
# name index must not turn either definition into a public duplicate.
suisou_pos_tu_a = "static int suisou_pos[] = { 1 };\n"
suisou_pos_tu_b = "static float suisou_pos[] = { 2.0f };\n"
assert (
    COMPAT.adapt_legacy_c(suisou_pos_tu_a, set(), {"suisou_pos"})
    == suisou_pos_tu_a
)
assert (
    COMPAT.adapt_legacy_c(suisou_pos_tu_b, set(), {"suisou_pos"})
    == suisou_pos_tu_b
)

# The generated-model pattern is safe to narrow because the matching extern
# and initialized static array are both visible at file scope in the same TU.
model_array_fixture = (
    "extern Vtx act_m_abura_v[];\n"
    "static Vtx act_m_abura_v[] = { { 0 } };\n"
)
model_array_expected = (
    "static Vtx act_m_abura_v[];\n"
    "static Vtx act_m_abura_v[] = { { 0 } };\n"
)
assert (
    COMPAT.adapt_legacy_c(
        model_array_fixture,
        set(),
        {"act_m_abura_v"},
    )
    == model_array_expected
)

# A bare name match is insufficient when the declared types differ.
incompatible_object_fixture = (
    "extern int shared_name[];\n"
    "static float shared_name[] = { 1.0f };\n"
)
assert (
    COMPAT.adapt_legacy_c(incompatible_object_fixture, set(), set())
    == incompatible_object_fixture
)

# Title-specific implicit-int names are no longer rewritten by the generic
# adapter.
implicit_strip_fixture = "extern _strip(void*, void*, int);\n"
assert (
    COMPAT.adapt_legacy_c(implicit_strip_fixture, set(), set())
    == implicit_strip_fixture
)

# Re-adapting an already generated view must be stable.
assert (
    COMPAT.adapt_legacy_c(model_array_expected, set(), set())
    == model_array_expected
)


# Big-endian scalar byte views are opt-in because ordinary byte pointers refer
# to byte streams and must retain their native indexing.  The recognized
# CodeWarrior pattern takes a byte pointer directly to a scalar object.
byte_view_fixture = (
    "void decode(u32 word, int index)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    bytes[0] = bytes[3];\n"
    "    consume(bytes[index]);\n"
    "}\n"
)
byte_view_expected = (
    "void decode(u32 word, int index)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    bytes[(sizeof(word) - 1) - (0)] = "
    "bytes[(sizeof(word) - 1) - (3)];\n"
    "    consume(bytes[(sizeof(word) - 1) - (index)]);\n"
    "}\n"
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(byte_view_fixture)
    == byte_view_expected
)

# The same pattern commonly appears in inline header methods.  Header text has
# no special-case semantics: both directions of a scalar/string ID conversion
# must retain the target's byte order when included on a little-endian host.
inline_header_byte_view = (
    "class Tag {\n"
    "public:\n"
    "    void unpack(u32 id) {\n"
    "        mID = id;\n"
    "        char* bytes = reinterpret_cast<char*>(&mID);\n"
    "        for (int i = 0; i < 4; ++i) mText[i] = bytes[i];\n"
    "    }\n"
    "    void pack() {\n"
    "        char* bytes = reinterpret_cast<char*>(&mID);\n"
    "        for (int i = 0; i < 4; ++i) bytes[i] = mText[i];\n"
    "    }\n"
    "    u32 mID;\n"
    "    char mText[5];\n"
    "};\n"
)
inline_header_byte_view_expected = inline_header_byte_view.replace(
    "mText[i] = bytes[i]",
    "mText[i] = bytes[(sizeof(mID) - 1) - (i)]",
).replace(
    "bytes[i] = mText[i]",
    "bytes[(sizeof(mID) - 1) - (i)] = mText[i]",
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(inline_header_byte_view)
    == inline_header_byte_view_expected
)

# Qualifiers, both supported byte spellings, multiline casts, and member
# scalar expressions all retain their original declarations.
qualified_byte_views = (
    "void decode(State state, u32 word, int index)\n"
    "{\n"
    "    const char* stateBytes =\n"
    "        reinterpret_cast<const char*>(\n"
    "            &state.value);\n"
    "    immut char * wordBytes = reinterpret_cast<immut char *>(&word);\n"
    "    stateBytes[index] = wordBytes[2];\n"
    "}\n"
)
qualified_byte_views_expected = (
    "void decode(State state, u32 word, int index)\n"
    "{\n"
    "    const char* stateBytes =\n"
    "        reinterpret_cast<const char*>(\n"
    "            &state.value);\n"
    "    immut char * wordBytes = reinterpret_cast<immut char *>(&word);\n"
    "    stateBytes[(sizeof(state.value) - 1) - (index)] = "
    "wordBytes[(sizeof(word) - 1) - (2)];\n"
    "}\n"
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(qualified_byte_views)
    == qualified_byte_views_expected
)

# An ordinary nested byte pointer shadows an adapted outer view, while a
# nested recognized view gets its own scalar.  Uses outside the declaration's
# lexical scope are untouched.
scoped_byte_views = (
    "void decode(u32 outer, u32 inner, int index)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&outer);\n"
    "    bytes[0] = bytes[index];\n"
    "    {\n"
    "        char* bytes = acquire_bytes();\n"
    "        bytes[1] = 0;\n"
    "    }\n"
    "    {\n"
    "        char* bytes = reinterpret_cast<char*>(&inner);\n"
    "        bytes[index] = 1;\n"
    "    }\n"
    "    object.bytes[2] = bytes[3];\n"
    "}\n"
    "void separate(void) { bytes[0] = 2; }\n"
)
scoped_byte_views_expected = (
    "void decode(u32 outer, u32 inner, int index)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&outer);\n"
    "    bytes[(sizeof(outer) - 1) - (0)] = "
    "bytes[(sizeof(outer) - 1) - (index)];\n"
    "    {\n"
    "        char* bytes = acquire_bytes();\n"
    "        bytes[1] = 0;\n"
    "    }\n"
    "    {\n"
    "        char* bytes = reinterpret_cast<char*>(&inner);\n"
    "        bytes[(sizeof(inner) - 1) - (index)] = 1;\n"
    "    }\n"
    "    object.bytes[2] = bytes[(sizeof(outer) - 1) - (3)];\n"
    "}\n"
    "void separate(void) { bytes[0] = 2; }\n"
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(scoped_byte_views)
    == scoped_byte_views_expected
)

# Insertions, rather than overlapping replacements, preserve nested
# subscripts and reverse each target-byte lookup independently.
nested_subscripts = (
    "void decode(u32 word)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    consume(bytes[bytes[0]]);\n"
    "}\n"
)
nested_subscripts_expected = (
    "void decode(u32 word)\n"
    "{\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    consume(bytes[(sizeof(word) - 1) - "
    "(bytes[(sizeof(word) - 1) - (0)])]);\n"
    "}\n"
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(nested_subscripts)
    == nested_subscripts_expected
)

# The lexer mask prevents declarations and uses in comments, literals, and
# preprocessor directives from participating.  A real use with an intervening
# comment still transforms without changing the comment.
masked_byte_views = (
    "void decode(u32 word)\n"
    "{\n"
    "    // u8* ignored = reinterpret_cast<u8*>(&word); ignored[0] = 1;\n"
    "    const char* text = \"bytes[0]\";\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    /* bytes[1] remains documentation. */\n"
    "#define FIRST_BYTE bytes[2]\n"
    "    bytes /* target byte */ [3] = '[';\n"
    "}\n"
)
masked_byte_views_expected = (
    "void decode(u32 word)\n"
    "{\n"
    "    // u8* ignored = reinterpret_cast<u8*>(&word); ignored[0] = 1;\n"
    "    const char* text = \"bytes[0]\";\n"
    "    u8* bytes = reinterpret_cast<u8*>(&word);\n"
    "    /* bytes[1] remains documentation. */\n"
    "#define FIRST_BYTE bytes[2]\n"
    "    bytes /* target byte */ [(sizeof(word) - 1) - (3)] = '[';\n"
    "}\n"
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(masked_byte_views)
    == masked_byte_views_expected
)

# Applying the pass twice is stable, which matters when generated fragments
# are rediscovered and adapted again.
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(byte_view_expected)
    == byte_view_expected
)
assert (
    COMPAT.adapt_big_endian_scalar_byte_views(nested_subscripts_expected)
    == nested_subscripts_expected
)

# adapt_source preserves its historical default and enables the pass only via
# its keyword option.
with tempfile.TemporaryDirectory() as temporary_directory:
    source = Path(temporary_directory) / "byte_view.cpp"
    source.write_text(byte_view_fixture, encoding="utf-8")
    expanded, default_adapted, _ = COMPAT.adapt_source(
        source,
        [],
        set(),
        set(),
    )
    assert expanded == byte_view_fixture
    assert default_adapted == byte_view_fixture
    _, opted_in_adapted, _ = COMPAT.adapt_source(
        source,
        [],
        set(),
        set(),
        big_endian_scalar_byte_views=True,
    )
    assert opted_in_adapted == byte_view_expected

# Both CLI paths expose the same explicit opt-in.  Callers that omit it keep
# the previous behavior.
parser = COMPAT.build_parser()
adapt_args = parser.parse_args(
    [
        "adapt",
        "--input",
        "input.cpp",
        "--output",
        "output.cpp",
        "--source-root",
        ".",
        "--symbols",
        "symbols.json",
        "--big-endian-scalar-byte-views",
    ]
)
assert adapt_args.big_endian_scalar_byte_views is True
discover_args = parser.parse_args(
    [
        "discover-splits",
        "--manifest",
        "config.yml",
        "--source-root",
        ".",
        "--include-root",
        "include",
        "--big-endian-scalar-byte-views",
    ]
)
assert discover_args.big_endian_scalar_byte_views is True
default_adapt_args = parser.parse_args(
    [
        "adapt",
        "--input",
        "input.cpp",
        "--output",
        "output.cpp",
        "--source-root",
        ".",
        "--symbols",
        "symbols.json",
    ]
)
assert default_adapt_args.big_endian_scalar_byte_views is False
