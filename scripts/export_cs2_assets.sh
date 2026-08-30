#!/usr/bin/env bash
# Export CS2 map collision (.tri), player models+anims, and weapon worldmodels
# into a private asset directory you choose. Never write Valve binaries into
# the cyka2pp git tree — pass an absolute path outside this repo.
#
# Usage:
#   ./scripts/export_cs2_assets.sh /path/to/cs2-maps-tri
#
# Requires: Source2Viewer-CLI on PATH (or SOURCE2VIEWER_CLI), a CS2 install,
# optional demolens for DLT1 map .tri files. See README “Local CS2 asset folder”.
set -euo pipefail

CS2_ROOT="${CS2_ROOT:-$HOME/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive}"
CSGO="$CS2_ROOT/game/csgo"
if [[ $# -lt 1 ]]; then
  echo "usage: $0 /path/to/asset-root" >&2
  echo "  Create a private folder (e.g. \$HOME/cs2-maps-tri) outside cyka2pp." >&2
  echo "  Do not export into this repository." >&2
  exit 2
fi
OUT="$1"
CLI="${SOURCE2VIEWER_CLI:-Source2Viewer-CLI}"

if ! command -v "$CLI" >/dev/null 2>&1; then
  echo "error: $CLI not found on PATH (install Source 2 Viewer CLI)" >&2
  exit 1
fi

# Refuse exporting into the cyka2pp source tree.
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_ABS="$(mkdir -p "$OUT" && cd "$OUT" && pwd)"
case "$OUT_ABS" in
  "$REPO_ROOT"|"$REPO_ROOT"/*)
    echo "error: refusing to write Valve assets under cyka2pp ($OUT_ABS)" >&2
    echo "       pick a path outside the repo (e.g. \$HOME/cs2-maps-tri)." >&2
    exit 1
    ;;
esac

ANIMS='animation/anims/world/rifle/_default_rifle/idle_rifle,animation/anims/world/rifle/_default_rifle/run_n_rifle,animation/anims/world/rifle/_default_rifle/idle_crouch_rifle,animation/anims/world/rifle/_default_rifle/crouch_n_rifle'

mkdir -p "$OUT/players" "$OUT/maps"

echo "== players (CT SAS + T Phoenix, 4 locomotion clips) =="
"$CLI" -i "$CSGO/pak01_dir.vpk" \
  -f "agents/models/ctm_sas/ctm_sas.vmdl_c" \
  -o "$OUT/players/ct_sas.glb" -d \
  --gltf_export_format glb \
  --gltf_export_animations \
  --gltf_animation_list "$ANIMS" \
  --gltf_export_materials

"$CLI" -i "$CSGO/pak01_dir.vpk" \
  -f "agents/models/tm_phoenix/tm_phoenix.vmdl_c" \
  -o "$OUT/players/t_phoenix.glb" -d \
  --gltf_export_format glb \
  --gltf_export_animations \
  --gltf_animation_list "$ANIMS" \
  --gltf_export_materials

# Keep PNGs next to glbs or nest them
mkdir -p "$OUT/players/textures"
shopt -s nullglob
mv -n "$OUT/players"/*.png "$OUT/players/textures/" 2>/dev/null || true

echo "== maps (DLT1 .tri via demolens extract-map, if installed) =="
if command -v demolens >/dev/null 2>&1; then
  for map in de_mirage de_inferno de_nuke de_dust2 de_ancient de_anubis de_overpass de_vertigo de_train; do
    if [[ -f "$CSGO/maps/${map}.vpk" ]]; then
      echo "  $map"
      demolens extract-map --cs2 "$CS2_ROOT" --map "$map" --out-dir "$OUT/maps" \
        --vrf "$CLI" || true
      # demolens writes <map>.tri; cyka2pp expects <map>.tri in --maps-dir root
      if [[ -f "$OUT/maps/${map}.tri" ]]; then
        ln -sfn "maps/${map}.tri" "$OUT/${map}.tri"
      fi
    fi
  done
else
  echo "demolens not on PATH — map .tri files unchanged."
  echo "Install demolens or export world_physics with Source2Viewer-CLI and convert (see README)."
fi

echo "done → $OUT"
ls -lh "$OUT/players"/*.glb


echo "== weapons (worldmodels for POV attach) =="
mkdir -p "$OUT/weapons"
declare -A WPN=(
  [ak47]=weapons/models/ak47/weapon_rif_ak47.vmdl_c
  [awp]=weapons/models/awp/weapon_snip_awp.vmdl_c
  [deagle]=weapons/models/deagle/weapon_pist_deagle.vmdl_c
  [m4a4]=weapons/models/m4a4/weapon_rif_m4a4.vmdl_c
  [m4a1]=weapons/models/m4a1_silencer/weapon_rif_m4a1_silencer.vmdl_c
  [glock]=weapons/models/glock18/weapon_pist_glock18.vmdl_c
)
for slug in "${!WPN[@]}"; do
  echo "  $slug"
  "$CLI" -i "$CSGO/pak01_dir.vpk" -f "${WPN[$slug]}" -o "$OUT/weapons/${slug}.glb" -d \
    --gltf_export_format glb --gltf_export_materials || true
done
mkdir -p "$OUT/weapons/textures"
mv -n "$OUT/weapons"/*.png "$OUT/weapons/textures/" 2>/dev/null || true
rm -f "$OUT/weapons"/*_physics.glb
ls -lh "$OUT/weapons"/*.glb
