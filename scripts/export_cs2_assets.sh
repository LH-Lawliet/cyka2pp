#!/usr/bin/env bash
# Export CS2 map collision (.tri), player models+anims, and weapon worldmodels
# into the sibling maps tree (default: ../cs2-maps-tri). Never write Valve
# binaries into the cyka2pp repo.
# Requires: Source2Viewer-CLI on PATH (or SOURCE2VIEWER_CLI), CS2 install, optional demolens.
set -euo pipefail

CS2_ROOT="${CS2_ROOT:-$HOME/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive}"
CSGO="$CS2_ROOT/game/csgo"
OUT="${1:-$(cd "$(dirname "$0")/../.." && pwd)/cs2-maps-tri}"
CLI="${SOURCE2VIEWER_CLI:-Source2Viewer-CLI}"

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
