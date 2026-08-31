#!/usr/bin/env python3
"""Extract SprayPoint tables from spray_*.cpp into little-endian float pair binaries."""

from __future__ import annotations

import json
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "src" / "csdata"
GEN = ROOT / "generated"

TABLE_RE = re.compile(
    r"constexpr\s+std::array<SprayPoint,\s*(\d+)>\s+(\w+)\s*=\s*\{(.*?)\};",
    re.DOTALL,
)
POINT_RE = re.compile(
    r"\.delta_x\s*=\s*([^,]+?),\s*\.delta_y\s*=\s*([^}]+)\}",
    re.DOTALL,
)


def eval_float(text: str) -> float:
    text = text.strip()
    if text == "-std::numbers::e":
        return -math.e
    if text == "std::numbers::e":
        return math.e
    return float(text)


def main() -> None:
    GEN.mkdir(exist_ok=True)
    manifest: dict[str, dict[str, object]] = {}
    for cpp in sorted(ROOT.glob("spray_*.cpp")):
        text = cpp.read_text()
        for match in TABLE_RE.finditer(text):
            count = int(match.group(1))
            name = match.group(2)
            body = match.group(3)
            raw_points = POINT_RE.findall(body)
            if len(raw_points) != count:
                raise SystemExit(
                    f"{cpp.name} {name}: expected {count} points, got {len(raw_points)}"
                )
            pairs = [(eval_float(x), eval_float(y)) for x, y in raw_points]
            data = b"".join(struct.pack("<dd", dx, dy) for dx, dy in pairs)
            out = GEN / f"{name.lower()}.bin"
            out.write_bytes(data)
            manifest[name] = {"file": out.name, "count": count, "source": cpp.name}
            print(f"{name}: {count} points -> {out.name} ({len(data)} bytes)")

    (GEN / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Total tables: {len(manifest)}")


if __name__ == "__main__":
    main()
