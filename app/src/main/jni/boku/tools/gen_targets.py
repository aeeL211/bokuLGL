#!/usr/bin/env python3
"""Turn targets.def into an encrypted target table.

The shipped library must not contain the class and method names in plaintext,
so the strings are packed into one blob and XORed with a key derived from the
build seed. targets.cpp decrypts the blob at startup with the same seed, so
every build ships a different table and no plaintext name is stored.
"""

import re
import sys

MASK = (1 << 64) - 1
MIX_A = 0xBF58476D1CE4E5B9
MIX_B = 0x94D049BB133111EB
GOLDEN = 0x9E3779B97F4A7C15
PARAM_NONE = 0xFFFF


def mix(value):
    value = ((value ^ (value >> 30)) * MIX_A) & MASK
    value = ((value ^ (value >> 27)) * MIX_B) & MASK
    return (value ^ (value >> 31)) & MASK


def key_byte(seed, index):
    value = (seed ^ (index * GOLDEN)) & MASK
    return mix(value) & 0xFF


def parse(path):
    rows = []
    with open(path, "r", encoding="utf-8") as source:
        for number, line in enumerate(source, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("|")
            if len(parts) != 5:
                raise SystemExit(f"{path}:{number}: expected 5 fields, got {len(parts)}")
            target_id, cls, name, argc, param = parts
            if not re.fullmatch(r"T_[A-Z0-9_]+", target_id):
                raise SystemExit(f"{path}:{number}: bad target id {target_id!r}")
            if not cls or not name:
                raise SystemExit(f"{path}:{number}: empty class or method")
            if not argc.isdigit() or not 0 <= int(argc) <= 255:
                raise SystemExit(f"{path}:{number}: bad argc {argc!r}")
            rows.append((target_id, cls, name, int(argc), param))
    if not rows:
        raise SystemExit(f"{path}: no targets")
    seen = set()
    for row in rows:
        if row[0] in seen:
            raise SystemExit(f"{path}: duplicate target id {row[0]}")
        seen.add(row[0])
    return rows


def build_blob(rows):
    blob = bytearray()
    data = []
    for _, cls, name, argc, param in rows:
        cls_off = len(blob)
        blob += cls.encode("utf-8") + b"\0"
        name_off = len(blob)
        blob += name.encode("utf-8") + b"\0"
        if param:
            param_off = len(blob)
            blob += param.encode("utf-8") + b"\0"
        else:
            param_off = PARAM_NONE
        data.append((cls_off, name_off, param_off, argc))
    if len(blob) >= PARAM_NONE:
        raise SystemExit("target blob is too large for 16-bit offsets")
    return blob, data


def emit_header(path, rows, blob, seed):
    lines = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "// Generated from targets.def; edit that file, not this one.",
        "enum TargetId {",
    ]
    for target_id, _, _, _, _ in rows:
        lines.append(f"    {target_id},")
    lines += [
        "    T_COUNT,",
        "};",
        "",
        "struct TargetData {",
        "    uint16_t cls;",
        "    uint16_t name;",
        "    uint16_t param;",
        "    uint8_t argc;",
        "    uint8_t reserved;",
        "};",
        "",
        f"constexpr uint32_t kTargetBlobSize = {len(blob)};",
        f"constexpr uint64_t kBuildSeed = 0x{seed:016x}ULL;",
        "constexpr uint16_t kNoParam = 0xFFFF;",
        "",
        "extern const uint8_t kTargetBlob[];",
        "extern const TargetData kTargetData[T_COUNT];",
        "",
    ]
    with open(path, "w", encoding="utf-8") as out:
        out.write("\n".join(lines))


def emit_source(path, blob, data, seed):
    cipher = bytearray(len(blob))
    for index, byte in enumerate(blob):
        cipher[index] = byte ^ key_byte(seed, index)

    lines = [
        '#include "targets_generated.h"',
        "",
        "const uint8_t kTargetBlob[kTargetBlobSize] = {",
    ]
    for start in range(0, len(cipher), 16):
        chunk = ", ".join(f"0x{byte:02x}" for byte in cipher[start : start + 16])
        lines.append(f"    {chunk},")
    lines += [
        "};",
        "",
        "const TargetData kTargetData[T_COUNT] = {",
    ]
    for cls_off, name_off, param_off, argc in data:
        lines.append(f"    {{{cls_off}, {name_off}, {param_off}, {argc}, 0}},")
    lines += [
        "};",
        "",
    ]
    with open(path, "w", encoding="utf-8") as out:
        out.write("\n".join(lines))


def main():
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: gen_targets.py <targets.def> <header-out> <source-out> <seed>"
        )
    def_path, header_path, source_path, seed_text = sys.argv[1:]
    try:
        seed = int(seed_text, 0)
    except ValueError:
        raise SystemExit(f"bad build seed {seed_text!r}")
    if not 0 <= seed <= MASK:
        raise SystemExit("build seed does not fit in 64 bits")
    rows = parse(def_path)
    blob, data = build_blob(rows)
    emit_header(header_path, rows, blob, seed)
    emit_source(source_path, blob, data, seed)


if __name__ == "__main__":
    main()
