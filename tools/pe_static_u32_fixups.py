#!/usr/bin/env python3
"""Canonicalize tagged 2xu32 static initializers in a fixed-base PE32+ image."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


PE32_PLUS_MAGIC = 0x20B
AMD64_MACHINE = 0x8664
IMAGE_DIRECTORY_ENTRY_BASERELOC = 5
IMAGE_REL_BASED_ABSOLUTE = 0
IMAGE_REL_BASED_DIR64 = 10
IMAGE_FILE_RELOCS_STRIPPED = 0x0001
STATIC_U32_TAG = 0x80000000


class FixupError(RuntimeError):
    pass


def u16(data: bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def u64(data: bytearray, offset: int) -> int:
    return struct.unpack_from("<Q", data, offset)[0]


def checked_range(data: bytearray, offset: int, size: int, label: str) -> None:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise FixupError(f"{label} lies outside the PE file")


def canonicalize(input_path: Path, output_path: Path, expected_image_base: int) -> int:
    data = bytearray(input_path.read_bytes())
    checked_range(data, 0, 0x40, "DOS header")
    if data[:2] != b"MZ":
        raise FixupError("input is not an MZ executable")

    pe_offset = u32(data, 0x3C)
    checked_range(data, pe_offset, 24, "PE header")
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise FixupError("input is not a PE executable")

    coff = pe_offset + 4
    if u16(data, coff) != AMD64_MACHINE:
        raise FixupError("only AMD64 PE images are supported")
    section_count = u16(data, coff + 2)
    optional_size = u16(data, coff + 16)
    optional = coff + 20
    checked_range(data, optional, optional_size, "optional header")
    if u16(data, optional) != PE32_PLUS_MAGIC:
        raise FixupError("only PE32+ images are supported")

    image_base = u64(data, optional + 24)
    if image_base != expected_image_base:
        raise FixupError(
            f"image base is 0x{image_base:x}, expected fixed base 0x{expected_image_base:x}"
        )
    size_of_headers = u32(data, optional + 60)
    number_of_directories = u32(data, optional + 108)
    if number_of_directories <= IMAGE_DIRECTORY_ENTRY_BASERELOC:
        raise FixupError("PE image has no base-relocation directory entry")

    directory = optional + 112 + IMAGE_DIRECTORY_ENTRY_BASERELOC * 8
    checked_range(data, directory, 8, "base-relocation directory entry")
    reloc_rva = u32(data, directory)
    reloc_size = u32(data, directory + 4)
    if reloc_rva == 0 or reloc_size == 0:
        raise FixupError("PE image has no base relocations to canonicalize")

    sections: list[tuple[int, int, int, int]] = []
    section_table = optional + optional_size
    checked_range(data, section_table, section_count * 40, "section table")
    for index in range(section_count):
        section = section_table + index * 40
        virtual_size = u32(data, section + 8)
        virtual_address = u32(data, section + 12)
        raw_size = u32(data, section + 16)
        raw_offset = u32(data, section + 20)
        sections.append((virtual_address, max(virtual_size, raw_size), raw_offset, raw_size))

    def rva_to_file_offset(rva: int, size: int, label: str) -> int:
        if rva < size_of_headers:
            checked_range(data, rva, size, label)
            return rva
        for virtual_address, span, raw_offset, raw_size in sections:
            if virtual_address <= rva and rva + size <= virtual_address + span:
                delta = rva - virtual_address
                if delta + size > raw_size:
                    break
                result = raw_offset + delta
                checked_range(data, result, size, label)
                return result
        raise FixupError(f"cannot map {label} RVA 0x{rva:x} to file data")

    reloc_offset = rva_to_file_offset(reloc_rva, reloc_size, "base-relocation directory")
    reloc_end = reloc_offset + reloc_size
    cursor = reloc_offset
    fixed = 0
    dir64 = 0

    while cursor < reloc_end:
        checked_range(data, cursor, 8, "base-relocation block")
        page_rva = u32(data, cursor)
        block_size = u32(data, cursor + 4)
        if block_size < 8 or (block_size & 1) != 0 or cursor + block_size > reloc_end:
            raise FixupError("malformed base-relocation block")

        entry = cursor + 8
        block_end = cursor + block_size
        while entry < block_end:
            encoded_entry = u16(data, entry)
            relocation_type = encoded_entry >> 12
            page_offset = encoded_entry & 0x0FFF
            if relocation_type == IMAGE_REL_BASED_DIR64:
                dir64 += 1
                target_rva = page_rva + page_offset
                target = rva_to_file_offset(target_rva, 8, "DIR64 target")
                encoded_pair = u64(data, target)
                tagged_first = encoded_pair >> 32

                # A normal pointer in the required low image has a zero high
                # dword.  A protocol initializer always has the tagged first
                # word there; 0x80 is reserved as a command opcode so the tag
                # cannot wrap a valid first word to zero.
                if tagged_first != 0:
                    first = (tagged_first - STATIC_U32_TAG) & 0xFFFFFFFF
                    second = encoded_pair & 0xFFFFFFFF
                    if (first >> 24) == 0x80:
                        raise FixupError(
                            f"reserved opcode 0x80 at RVA 0x{target_rva:x}"
                        )
                    struct.pack_into("<II", data, target, first, second)
                    struct.pack_into(
                        "<H",
                        data,
                        entry,
                        (IMAGE_REL_BASED_ABSOLUTE << 12) | page_offset,
                    )
                    fixed += 1
            elif relocation_type != IMAGE_REL_BASED_ABSOLUTE:
                raise FixupError(
                    f"unsupported base-relocation type {relocation_type} at file offset 0x{entry:x}"
                )
            entry += 2
        cursor = block_end

    if cursor != reloc_end:
        raise FixupError("base-relocation directory has trailing partial data")
    if fixed == 0:
        raise FixupError("no tagged static-u32 initializers were found")

    # The rewritten values are 32-bit absolutes.  A fixed base is therefore
    # part of their ABI; make that explicit to the Windows loader.
    struct.pack_into("<II", data, directory, 0, 0)
    struct.pack_into("<H", data, coff + 18, u16(data, coff + 18) | IMAGE_FILE_RELOCS_STRIPPED)
    struct.pack_into("<I", data, optional + 64, 0)  # stale PE checksum

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data)
    print(
        f"canonicalized {fixed} tagged static-u32 pairs "
        f"({dir64} DIR64 entries), fixed image base 0x{image_base:x}"
    )
    return fixed


def parse_int(value: str) -> int:
    return int(value, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--image-base", required=True, type=parse_int)
    args = parser.parse_args()

    try:
        canonicalize(args.input, args.output, args.image_base)
    except (FixupError, OSError, struct.error) as error:
        print(f"pe_static_u32_fixups: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
