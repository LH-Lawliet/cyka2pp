# cyka2pp

**cyka** + **CS2** + **C++** (`pp`) — the CS2 demo analyzer for
[cykaslayer.com](https://cykaslayer.com).

I aggregated several open-source CS2 parsers/analyzers into **one** translated
C++ project (with Cursor’s help) so the site has a single binary to call:
parse a `.dem`, compute scoreboard / aim / highlights, emit JSON for the
existing TypeScript services (or a human-readable table for local debugging).

I chose C++ because that is what I know well, and because extending the
pipeline (new tags, metrics, mesh LOS, etc.) is faster for me in this language
than maintaining a multi-repo Go/Python/TS stack.

## Features

| Area | What you get |
|------|----------------|
| **Demo parse** | PBDEMS2 stream, snappy frames, game events, string tables, Source 2 entity / sendtable decode, per-tick player poses |
| **Match model** | Roster (incl. disconnects), rounds / scores / half scores, kills, damages, flashes, bomb, utility |
| **Scoreboard** | K/A/D, ADR, HS%, KAST, HLTV-style rating (Rtg), trades |
| **Clutches** | Once-per-round 1vN detection + win rates |
| **Aim (needs `--maps-dir`)** | Median time-to-damage (per-tick LOS), accuracy, spray, counter-strafe %, spotted accuracy, crosshair placement, round swing, kill `ttd_ms` |
| **Highlights** | Clip windows + emoji kill tags (see below) |
| **Output** | ANSI table and/or JSON |

Mesh LOS uses DLT1 `.tri` maps + BVH raycasts (batched / multi-threaded).

## Requirements

```bash
sudo pacman -S cmake ninja nlohmann-json snappy clang
```

## Build

```bash
cd ~/demo_go/cyka2pp
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI

```bash
# Table (default on a TTY). Needs meshes for TTD / LOS aim columns.
./build/cyka2pp analyze ../dotdem/3835689269611987518.dem \
  --maps-dir ../cs2-maps-tri \
  --format table

# All table sections
./build/cyka2pp analyze path/to/demo.dem \
  --maps-dir ../cs2-maps-tri \
  --format table --sections all

# Subset: scoreboard,clutches,aim,highlights,rounds,kills
./build/cyka2pp analyze path/to/demo.dem --format table \
  --sections scoreboard,highlights

# JSON to stdout (default when not a TTY)
./build/cyka2pp analyze path/to/demo.dem \
  --maps-dir ../cs2-maps-tri \
  --format json --minify

# JSON to a file (implies JSON if --format omitted)
./build/cyka2pp analyze path/to/demo.dem \
  --maps-dir ../cs2-maps-tri \
  --out match.json --minify

# Limit highlights to specific players
./build/cyka2pp analyze path/to/demo.dem \
  --steam-id 7656119… --steam-id 7656119…
```

`--format table` with `--out` prints the table and still writes JSON.

## Kill tag system

Each kill can collect zero or more emoji tags (joined for display / JSON).
Event flags come from the demo; pose / shot / TTD tags need samples (and mesh
for ⚡).

| Tag | Meaning | Rule (short) |
|-----|---------|----------------|
| 🎯 | Headshot | `is_headshot` |
| 🧱 | Wallbang | `penetrated_objects > 0` |
| 💨 | Through smoke | `is_through_smoke` |
| 🙈 | Blind kill | killer flashed |
| 🦅 | Airborne | killer airborne near kill tick |
| 🔭 | No-scope | `is_no_scope` |
| ☝️ | One-tap | HS with exactly one shot in the last ~1 s (excl. snipers/shotguns) |
| 💫 | Flick | large yaw change in ~0.1 s before the kill |
| 🥶 | Cold blood / last bullet | ammo model hits 0 on the kill shot |
| 🚑 | 1 HP | killer health == 1 near kill |
| ✈️ | Long range | distance &gt; 30 m (50 m if scoped) |
| 🎳 | Collateral | ≥2 kills same tick / same killer |
| ⚡ | Fast TTD | kill `ttd_ms` &lt; 100 |

Implementation: `src/highlights/tags.cpp` + `tag_rules.cpp`.

## Layout

Translation units aim to stay under ~200 lines.

```
include/cyka/     public API (analyze, models, options)
src/demo/         PBDEMS2 + entities + match builder
src/geom/         DLT1 mesh + BVH
src/metrics/      KAST / HLTV ratings, clutches, swing
src/aim/          samples, batched LOS, TTD / spray / CS / …
src/highlights/   clip windows + tags
src/io/           JSON
src/render/       ANSI tables
tests/            unit + golden demo checks
```

## Sources (all MIT — this project is MIT)

Algorithms and semantics were reimplemented / adapted from:

- [demoinfocs-golang](https://github.com/markus-wa/demoinfocs-golang) — demo / sendtable / entity decode semantics (C++ port under `src/demo/ent`)
- [demolens](https://github.com/f-gillmann/demolens) — DLT1 mesh, BVH occlusion, spray tables, aim metric ideas
- [cs-demo-analyzer](https://github.com/akiver/cs-demo-analyzer) — scoreboard / export-oriented formulas cross-check
- [awpy](https://github.com/pnxenopoulos/awpy) — parsing / stats cross-check

This tree does **not** vendor those repos; it is a single controlled C++ surface for cykaslayer.

## License

MIT — see [LICENSE](LICENSE).
