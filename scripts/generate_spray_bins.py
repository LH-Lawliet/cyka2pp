#!/usr/bin/env python3
"""Refresh testdata manifest counts from src/csdata/generated/*.bin."""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "src" / "csdata" / "generated"
CPP_BY_PREFIX = {
    "bl_": "spray_base_lmg.cpp",
    "br_": "spray_base_rifles.cpp",
    "bs_": "spray_base_smg.cpp",
    "nosil_": "spray_nosil.cpp",
    "scoped_": "spray_scoped.cpp",
}


def source_for(name: str) -> str:
    for prefix, src in CPP_BY_PREFIX.items():
        if name.startswith(prefix):
            return src
    return "spray_tables.cpp"


def main() -> None:
    manifest: dict[str, dict[str, object]] = {}
    for path in sorted(GEN.glob("*.bin")):
        key = path.stem.upper()
        count = path.stat().st_size // 16
        manifest[key] = {"file": path.name, "count": count, "source": source_for(path.name)}
        print(f"{key}: {count} points")
    (GEN / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Total tables: {len(manifest)}")


if __name__ == "__main__":
    main()
