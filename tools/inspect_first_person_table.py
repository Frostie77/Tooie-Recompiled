#!/usr/bin/env python3
"""Read only the first-person dispatch signature/table from a local decompressed ROM.

No ROM bytes beyond the 47 target offsets are printed. Nothing is modified.
Signature, table and whole-ROM hashes are checked against the user-supplied
Step 105 USA extraction. The full ROM is never copied into the repository.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROM_BASE = 0x1F4BE90
TABLE_OFFSET = 0xA494
FUNCTION_START, FUNCTION_END = 0x5994, 0x5F48
DISPATCH = (0x3C010001, 0x002F0821, 0x8C2FA494, 0x01E00008, 0)
ROM_SHA = 'ab20e28753259f3888ce0d318634cc5c6b0097c6e5ba185721e936fe6d3f3788'
TABLE_SHA = '57810e0d7399ae57ef9f0e261ea4c315875400ee67c8ff91174e20751fcd2cd3'


def inspect(path):
    data = Path(path).read_bytes()
    end = ROM_BASE + TABLE_OFFSET + 47*4
    if len(data) < end:
        raise ValueError("decompressed ROM is too short for the complete table")
    signature = struct.unpack_from(">5I", data, ROM_BASE + 0x59EC)
    if signature != DISPATCH:
        raise ValueError("bsfirstp dispatch signature differs; require the USA decompressed ROM")
    words = struct.unpack_from(">47I", data, ROM_BASE + TABLE_OFFSET)
    for i, word in enumerate(words):
        if word & 3 or not FUNCTION_START <= word < FUNCTION_END:
            raise ValueError(f"slot {i}: 0x{word:08X} is not an aligned intra-function offset")
    if words[46] != 0x5AE0:
        raise ValueError("slot 46 differs from the captured USA first-person target")
    table_sha = hashlib.sha256(data[ROM_BASE+TABLE_OFFSET:end]).hexdigest()
    if table_sha != TABLE_SHA:
        raise ValueError('complete USA table SHA-256 differs from Step 105')
    rom_sha = hashlib.sha256(data).hexdigest()
    return {
        "rom_bytes": len(data), "rom_sha256": rom_sha,
        "matches_step105_rom": len(data)==35098224 and rom_sha==ROM_SHA,
        "table_rom": f"0x{ROM_BASE+TABLE_OFFSET:08X}", "count": 47,
        "table_sha256": table_sha,
        "raw_offset_words": [f"{word:08X}" for word in words],
        "unique_offsets": sorted({f"0x{word:04X}" for word in words}),
        "scope": "Identity compared with user-supplied Step 105 USA decompression provenance.",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("decompressed_rom", type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.decompressed_rom)
        if not result['matches_step105_rom']:
            raise ValueError('complete decompressed ROM size/SHA-256 differs from Step 105')
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f"First-person table rejected: {error}\n")
