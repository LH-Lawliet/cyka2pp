#!/usr/bin/env python3
"""Compare cyka2pp JSON against awpy (and optional demoinfocs) on corpus demos.

Usage:
  python3 scripts/compare_analyzers.py [--bin ./build/cyka2pp] [--maps-dir $HOME/cs2-maps-tri]

Prints a markdown table to stdout. Exits 1 if a hard mismatch is found on a
demo that all parsers completed (map name or kill-count delta > 15%).
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cyka_json(bin_path: Path, demo: Path, maps: Path | None) -> dict | None:
    cmd = [str(bin_path), "analyze", str(demo), "--format", "json", "--minify"]
    if maps and maps.exists():
        cmd += ["--maps-dir", str(maps)]
    try:
        p = subprocess.run(cmd, check=True, capture_output=True, text=True, timeout=600)
    except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired) as exc:
        print(f"cyka2pp failed on {demo.name}: {exc}", file=sys.stderr)
        return None
    return json.loads(p.stdout)


def awpy_stats(demo: Path) -> dict | None:
    try:
        from awpy import Demo  # type: ignore
    except ImportError:
        return None
    try:
        dem = Demo(path=str(demo))
        dem.parse()
    except Exception as exc:  # noqa: BLE001
        print(f"awpy failed on {demo.name}: {exc}", file=sys.stderr)
        return None
    header = getattr(dem, "header", None) or {}
    map_name = header.get("map_name") or header.get("mapName") or ""
    kills = dem.kills
    n_kills = len(kills) if kills is not None else -1
    rounds = dem.rounds
    n_rounds = len(rounds) if rounds is not None else -1
    return {"map": map_name, "kills": n_kills, "rounds": n_rounds, "parser": "awpy"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=str(ROOT / "build" / "cyka2pp"))
    ap.add_argument("--maps-dir", default=str(ROOT.parent / "cs2-maps-tri"))
    args = ap.parse_args()
    bin_path = Path(args.bin)
    maps = Path(args.maps_dir)
    man = json.loads((ROOT / "testdata" / "corpus" / "manifest.json").read_text())
    print("| demo | kind | cyka map | cyka rounds | cyka kills | awpy map | awpy rounds | awpy kills |")
    print("|---|---|---|---|---|---|---|---|")
    hard_fail = False
    for d in man["demos"]:
        path = ROOT / "testdata" / "demos" / d["file"]
        if not path.exists():
            print(f"| {d['id']} | {d.get('kind','')} | *missing demo* | | | | | |")
            continue
        cj = cyka_json(bin_path, path, maps)
        if cj is None:
            hard_fail = True
            continue
        ck = len(cj.get("kills") or [])
        cr = len(cj.get("rounds") or [])
        cm = cj.get("mapName") or ""
        aw = awpy_stats(path)
        if aw is None:
            print(f"| {d['id']} | {d.get('kind','')} | {cm} | {cr} | {ck} | *awpy not installed* | | |")
            continue
        print(
            f"| {d['id']} | {d.get('kind','')} | {cm} | {cr} | {ck} | {aw['map']} | {aw['rounds']} | {aw['kills']} |"
        )
        if aw["map"] and cm and aw["map"] != cm:
            print(f"mismatch map {d['id']}: cyka={cm} awpy={aw['map']}", file=sys.stderr)
            hard_fail = True
        if aw["kills"] > 0 and ck > 0:
            ratio = abs(aw["kills"] - ck) / max(aw["kills"], ck)
            if ratio > 0.15:
                print(
                    f"mismatch kills {d['id']}: cyka={ck} awpy={aw['kills']} delta={ratio:.0%}",
                    file=sys.stderr,
                )
                hard_fail = True
    return 1 if hard_fail else 0


if __name__ == "__main__":
    raise SystemExit(main())
