#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import struct
import tempfile
from pathlib import Path


TOOL_PATH = Path(__file__).parents[1] / "tools" / "pe_static_u32_fixups.py"
SPEC = importlib.util.spec_from_file_location("pe_static_u32_fixups", TOOL_PATH)
assert SPEC is not None and SPEC.loader is not None
FIXUPS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXUPS)

IMAGE_BASE = 0x10000000
PE_OFFSET = 0x80
COFF_OFFSET = PE_OFFSET + 4
OPTIONAL_OFFSET = COFF_OFFSET + 20
OPTIONAL_SIZE = 0xF0
DATA_RAW_OFFSET = 0x200
DATA_RVA = 0x1000
RELOC_RAW_OFFSET = 0x400
RELOC_RVA = 0x2000
STATIC_TAG = 0x80000000


def put_u16(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<H", data, offset, value)


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value)


def put_u64(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<Q", data, offset, value)


def read_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def tagged_pair(first: int, second: int) -> int:
    tagged_first = (first + STATIC_TAG) & 0xFFFFFFFF
    return (tagged_first << 32) | (second & 0xFFFFFFFF)


def build_pe(image_base: int = IMAGE_BASE) -> tuple[bytearray, list[tuple[int, int]]]:
    data = bytearray(0x600)
    data[:2] = b"MZ"
    put_u32(data, 0x3C, PE_OFFSET)
    data[PE_OFFSET : PE_OFFSET + 4] = b"PE\0\0"

    put_u16(data, COFF_OFFSET, FIXUPS.AMD64_MACHINE)
    put_u16(data, COFF_OFFSET + 2, 2)
    put_u16(data, COFF_OFFSET + 16, OPTIONAL_SIZE)
    put_u16(data, COFF_OFFSET + 18, 0x0026)

    put_u16(data, OPTIONAL_OFFSET, FIXUPS.PE32_PLUS_MAGIC)
    put_u64(data, OPTIONAL_OFFSET + 24, image_base)
    put_u32(data, OPTIONAL_OFFSET + 60, DATA_RAW_OFFSET)
    put_u32(data, OPTIONAL_OFFSET + 64, 0x12345678)
    put_u32(data, OPTIONAL_OFFSET + 108, 16)

    section_table = OPTIONAL_OFFSET + OPTIONAL_SIZE
    data[section_table : section_table + 8] = b".data\0\0\0"
    put_u32(data, section_table + 8, 0x200)
    put_u32(data, section_table + 12, DATA_RVA)
    put_u32(data, section_table + 16, 0x200)
    put_u32(data, section_table + 20, DATA_RAW_OFFSET)

    reloc_section = section_table + 40
    data[reloc_section : reloc_section + 8] = b".reloc\0\0"
    put_u32(data, reloc_section + 8, 0x200)
    put_u32(data, reloc_section + 12, RELOC_RVA)
    put_u32(data, reloc_section + 16, 0x200)
    put_u32(data, reloc_section + 20, RELOC_RAW_OFFSET)

    packets = [
        (0x01008010, 0x10007030),  # image pointer
        (0xDF000000, 0x00000000),  # zero payload
        (0xB8000000, 0xFFFFFFFF),  # signed -1 payload
        (0xFA001234, 0xF6789ABC),  # high-bit scalar payload
    ]
    for index, (first, second) in enumerate(packets):
        put_u64(data, DATA_RAW_OFFSET + index * 8, tagged_pair(first, second))

    # A normal low-image DIR64 pointer must not be mistaken for a tagged pair.
    put_u64(data, DATA_RAW_OFFSET + len(packets) * 8, 0x10005000)

    # A pointer-typed numeric wire constant is already emitted as canonical
    # words and deliberately has no relocation for the fixer to visit.
    typed_wire_offset = DATA_RAW_OFFSET + (len(packets) + 1) * 8
    put_u32(data, typed_wire_offset, 0xDA380003)
    put_u32(data, typed_wire_offset + 4, 0x0D000008)

    relocation_count = len(packets) + 1
    block_size = 8 + (relocation_count + 1) * 2
    put_u32(data, RELOC_RAW_OFFSET, DATA_RVA)
    put_u32(data, RELOC_RAW_OFFSET + 4, block_size)
    for index in range(relocation_count):
        put_u16(
            data,
            RELOC_RAW_OFFSET + 8 + index * 2,
            (FIXUPS.IMAGE_REL_BASED_DIR64 << 12) | (index * 8),
        )
    put_u16(
        data,
        RELOC_RAW_OFFSET + 8 + relocation_count * 2,
        FIXUPS.IMAGE_REL_BASED_ABSOLUTE << 12,
    )

    directory = OPTIONAL_OFFSET + 112 + FIXUPS.IMAGE_DIRECTORY_ENTRY_BASERELOC * 8
    put_u32(data, directory, RELOC_RVA)
    put_u32(data, directory + 4, block_size)
    return data, packets


def require_fixup_error(
    data: bytearray,
    expected_message: str,
    directory: Path,
    expected_image_base: int = IMAGE_BASE,
) -> None:
    source = directory / "invalid.exe"
    output = directory / "invalid.fixed.exe"
    source.write_bytes(data)
    try:
        FIXUPS.canonicalize(source, output, expected_image_base)
    except FIXUPS.FixupError as error:
        assert expected_message in str(error), str(error)
    else:
        raise AssertionError(f"expected FixupError containing {expected_message!r}")
    assert not output.exists()


with tempfile.TemporaryDirectory(prefix="porpoise-static-u32-") as temporary:
    directory = Path(temporary)
    source = directory / "input.exe"
    output = directory / "output.exe"
    original, packets = build_pe()
    source.write_bytes(original)

    fixed_count = FIXUPS.canonicalize(source, output, IMAGE_BASE)
    assert fixed_count == len(packets)
    fixed = output.read_bytes()

    for index, expected in enumerate(packets):
        actual = struct.unpack_from("<II", fixed, DATA_RAW_OFFSET + index * 8)
        assert actual == expected, (index, expected, actual)

    normal_pointer_offset = DATA_RAW_OFFSET + len(packets) * 8
    assert struct.unpack_from("<Q", fixed, normal_pointer_offset)[0] == 0x10005000

    typed_wire_offset = DATA_RAW_OFFSET + (len(packets) + 1) * 8
    assert struct.unpack_from("<II", fixed, typed_wire_offset) == (
        0xDA380003,
        0x0D000008,
    )

    relocation_directory = (
        OPTIONAL_OFFSET + 112 + FIXUPS.IMAGE_DIRECTORY_ENTRY_BASERELOC * 8
    )
    assert struct.unpack_from("<II", fixed, relocation_directory) == (0, 0)
    assert read_u16(fixed, COFF_OFFSET + 18) & FIXUPS.IMAGE_FILE_RELOCS_STRIPPED
    assert read_u32(fixed, OPTIONAL_OFFSET + 64) == 0

    wrong_base, _ = build_pe(image_base=0x20000000)
    require_fixup_error(wrong_base, "expected fixed base", directory)

    malformed, _ = build_pe()
    put_u32(malformed, RELOC_RAW_OFFSET + 4, 6)
    require_fixup_error(malformed, "malformed base-relocation block", directory)

    unsupported, _ = build_pe()
    put_u16(unsupported, RELOC_RAW_OFFSET + 8, (3 << 12) | 0)
    require_fixup_error(unsupported, "unsupported base-relocation type 3", directory)
