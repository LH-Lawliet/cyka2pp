#!/usr/bin/env python3
"""Extend demolens spray bins to full magazine using community 30+ shot shapes.

Source of the extra bullets: ArtanisInc/Artanis-RCS patterns/*.csv (mouse-pixel
increments, same family as the widely copied AHK/Logitech 30-shot AK table).
We keep our CS2 degree samples for the overlapping prefix and only append the
fitted tail so the gold path stays in pattern-space degrees.
"""

from __future__ import annotations

import struct
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GEN = ROOT / "src" / "csdata" / "generated"
BASE = "https://raw.githubusercontent.com/ArtanisInc/Artanis-RCS/main/patterns"

# bin, artanis csv, magazine capacity
TABLES = [
    ("br_0.bin", "ak47.csv", 30),
    ("br_1.bin", "aug.csv", 30),
    ("br_2.bin", "famas.csv", 25),
    ("br_3.bin", "galil.csv", 35),
    ("br_4.bin", "m4a1.csv", 20),
    ("br_5.bin", "m4a4.csv", 30),
    ("br_6.bin", "sg553.csv", 30),
    ("bs_0.bin", "mac10.csv", 30),
    ("bs_1.bin", "mp5sd.csv", 30),
    ("bs_2.bin", "mp7.csv", 30),
    ("bs_3.bin", "mp9.csv", 30),
    ("bs_4.bin", "p90.csv", 50),
    ("bs_5.bin", "bizon.csv", 64),
    ("bs_6.bin", "ump45.csv", 25),
    ("bl_0.bin", "m249.csv", 100),
    ("bl_1.bin", "negev.csv", 150),
    ("nosil_0.bin", "m4a1.csv", 20),
    ("scoped_0.bin", "aug.csv", 30),
    ("scoped_1.bin", "sg553.csv", 30),
]

MAX_FIT_ERR = 0.55  # deg; skip tail if the pixel table is a different shape


def load_bin(path: Path) -> list[tuple[float, float]]:
    data = path.read_bytes()
    return [struct.unpack_from("<dd", data, idx * 16) for idx in range(len(data) // 16)]


def save_bin(path: Path, points: list[tuple[float, float]]) -> None:
    path.write_bytes(b"".join(struct.pack("<dd", dx, dy) for dx, dy in points))


def fetch_csv(name: str) -> str:
    req = urllib.request.Request(
        f"{BASE}/{name}",
        headers={"User-Agent": "cyka2pp-spray-extend/1.0"},
    )
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read().decode()


def parse_artanis(text: str) -> list[tuple[float, float]]:
    rows: list[tuple[float, float, float]] = []
    for line in text.splitlines():
        line = line.strip().lstrip("\ufeff")
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 2:
            continue
        rows.append((float(parts[0]), float(parts[1]), float(parts[2]) if len(parts) > 2 else 0.0))
    if not rows:
        return []
    # First row is often "0,0,<mag>" metadata, not a shot.
    if rows[0][0] == 0.0 and rows[0][1] == 0.0 and rows[0][2] >= 10:
        mag_hint = int(rows[0][2])
        rows = rows[1:]
    else:
        mag_hint = 0
    cum: list[tuple[float, float]] = []
    pos_x = 0.0
    pos_y = 0.0
    for dx, dy, _delay in rows:
        pos_x += dx
        pos_y += dy
        cum.append((pos_x, pos_y))
    # CSVs are often mag-1 increments after the header (missing the last bullet).
    while mag_hint > 0 and len(cum) < mag_hint and len(cum) >= 2:
        prev_x, prev_y = cum[-2]
        last_x, last_y = cum[-1]
        cum.append((last_x + (last_x - prev_x), last_y + (last_y - prev_y)))
    return cum


def fit_and_extend(
    ours: list[tuple[float, float]], community: list[tuple[float, float]], mag: int
) -> list[tuple[float, float]] | None:
    overlap = min(len(ours), len(community), mag)
    if overlap < 8:
        return None
    num_x = den_x = num_y = den_y = 0.0
    for idx in range(overlap):
        our_x, our_y = ours[idx]
        com_x, com_y = community[idx]
        # Community tables are compensation (pull); ours are pattern/punch.
        num_x += com_x * (-our_x)
        den_x += com_x * com_x
        num_y += com_y * (-our_y)
        den_y += com_y * com_y
    if den_x < 1e-9 or den_y < 1e-9:
        return None
    scale_x = num_x / den_x
    scale_y = num_y / den_y
    err = 0.0
    for idx in range(overlap):
        our_x, our_y = ours[idx]
        com_x, com_y = community[idx]
        pred_x = com_x * scale_x
        pred_y = com_y * scale_y
        err += ((pred_x + our_x) ** 2 + (pred_y + our_y) ** 2) ** 0.5
    mean_err = err / overlap
    if mean_err > MAX_FIT_ERR:
        return None
    out = list(ours)
    want = min(mag, len(community))
    for idx in range(len(ours), want):
        com_x, com_y = community[idx]
        # Back to punch/pattern space.
        out.append((-com_x * scale_x, -com_y * scale_y))
    return out


def main() -> int:
    GEN.mkdir(parents=True, exist_ok=True)
    cache: dict[str, list[tuple[float, float]]] = {}
    for bin_name, csv_name, mag in TABLES:
        path = GEN / bin_name
        if not path.exists():
            print(f"skip missing {bin_name}")
            continue
        ours = load_bin(path)
        if csv_name not in cache:
            cache[csv_name] = parse_artanis(fetch_csv(csv_name))
        community = list(cache[csv_name])
        while len(community) < mag and len(community) >= 2:
            prev_x, prev_y = community[-2]
            last_x, last_y = community[-1]
            community.append((last_x + (last_x - prev_x), last_y + (last_y - prev_y)))
        extended = fit_and_extend(ours, community, mag)
        if extended is None or len(extended) <= len(ours):
            print(f"{bin_name}: keep {len(ours)} (no reliable tail)")
            continue
        save_bin(path, extended)
        print(f"{bin_name}: {len(ours)} -> {len(extended)} (mag {mag})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
