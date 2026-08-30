#!/usr/bin/env python3
"""Download corpus .dem files listed in testdata/corpus/manifest.json."""

from __future__ import annotations

import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "testdata" / "corpus" / "manifest.json"
DEST = ROOT / "testdata" / "demos"


def fetch(url: str, dest: Path) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    part = dest.with_suffix(dest.suffix + ".part")
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Mozilla/5.0 (compatible; cyka2pp-corpus/1.0)",
            "Accept": "*/*",
        },
    )
    for attempt in range(5):
        try:
            with urllib.request.urlopen(req, timeout=300) as r, open(part, "wb") as out:
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    out.write(chunk)
        except Exception as exc:  # noqa: BLE001
            print(f"  fail {url}: {exc}", file=sys.stderr)
            if part.exists():
                part.unlink()
            return False
        if part.exists() and part.stat().st_size > 1024:
            part.replace(dest)
            print(f"  ok {dest.name} ({dest.stat().st_size} bytes)")
            return True
        print(f"  retry {attempt + 1} {url} (got {part.stat().st_size if part.exists() else 0} B)")
        if part.exists():
            part.unlink()
        import time

        time.sleep(3)
    return False


def main() -> int:
    data = json.loads(MANIFEST.read_text())
    DEST.mkdir(parents=True, exist_ok=True)
    n_ok = 0
    n_skip = 0
    n_fail = 0
    for demo in data["demos"]:
        dest = DEST / demo["file"]
        if dest.exists() and dest.stat().st_size > 1024:
            print(f"skip {dest.name} (already present)")
            n_skip += 1
            n_ok += 1
            continue
        print(f"get {demo['id']} → {dest.name}")
        done = False
        for url in demo.get("urls") or []:
            if fetch(url, dest):
                done = True
                break
        if done:
            n_ok += 1
        else:
            n_fail += 1
    print(f"done ok={n_ok} skipped_existing={n_skip} failed={n_fail}")
    return 0 if n_fail == 0 or n_ok > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
