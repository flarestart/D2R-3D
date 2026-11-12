"""Inspect native automap candidates in a memory image (RVA-indexed, not a PE file).
Run from the project root: python D2R-3D-master/tools/inspect_automap.py build/runtime.bin
Requires capstone (the workspace tools-python directory is supported).
These are analysis candidates, not verified hook addresses.
"""
import argparse
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools-python"))
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

SITES = {
    "ui_command_consumer": (0x94D050, 0x6D0),
    "ui_quad_vertices": (0x94DC70, 0x1D3),
    "ui_quad_draw": (0x94C510, 0x253),
    "ui_command_append": (0x51F8B0, 0x180),
    "map_list": (0xB6450, 0x410),
    "map_unit": (0xB72B0, 0x250),
    "map_marker": (0xB71A0, 0x110),
    "map_point": (0xB0C00, 0x180),
    "unit_point": (0xB0FB0, 0x180),
    "sprite_dispatch": (0x65A4A0, 0x230),
    "hd_sprite": (0x5E0AB0, 0x7C0),
}

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("image", type=Path)
    ap.add_argument("--output", type=Path, required=True)
    args = ap.parse_args()
    data = args.image.read_bytes()
    if data[:2] != b"MZ":
        raise SystemExit("Expected an RVA-indexed memory image with a PE header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    stamp = struct.unpack_from("<I", data, pe + 8)[0]
    if stamp != 0x6A6B96A4:
        raise SystemExit(f"Unsupported timestamp: {stamp:#x}")
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    lines = [f"Image: {args.image}; timestamp: {stamp:#x}",
             "Candidates only. A memory dump can contain unreadable pages or duplicate code."]
    for name, (rva, size) in SITES.items():
        lines.append(f"\n{name}: RVA {rva:#x}")
        lines.extend(f"{i.address:08x}  {i.mnemonic:10} {i.op_str}"
                     for i in md.disasm(data[rva:rva+size], rva))
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(args.output)

if __name__ == "__main__":
    main()
