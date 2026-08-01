#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
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
