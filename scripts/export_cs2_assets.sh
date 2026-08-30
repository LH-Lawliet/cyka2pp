#!/usr/bin/env bash
# Export CS2 map collision (.tri), player models+anims, and weapon worldmodels
# into a private asset directory you choose. Never write Valve binaries into
# the cyka2pp git tree — pass an absolute path outside this repo.
#
# Geometry only (no materials/textures) — cyka2pp raycasts meshes, it does not
# sample textures.
#
# Usage:
#   ./scripts/export_cs2_assets.sh /path/to/cs2-maps-tri
#   ./scripts/export_cs2_assets.sh /path/to/cs2-maps-tri weapons   # weapons only
#
# Requires: Source2Viewer-CLI on PATH (or SOURCE2VIEWER_CLI), a CS2 install,
# optional demolens for DLT1 map .tri files. See README “Local CS2 asset folder”.
set -euo pipefail

CS2_ROOT="${CS2_ROOT:-$HOME/.local/share/Steam/steamapps/common/Counter-Strike Global Offensive}"
CSGO="$CS2_ROOT/game/csgo"
if [[ $# -lt 1 ]]; then
  echo "usage: $0 /path/to/asset-root [all|players|weapons|maps]" >&2
  echo "  Create a private folder (e.g. \$HOME/cs2-maps-tri) outside cyka2pp." >&2
  echo "  Do not export into this repository." >&2
  exit 2
fi
OUT="$1"
WHAT="${2:-all}"
CLI="${SOURCE2VIEWER_CLI:-Source2Viewer-CLI}"
if [[ ! -x "$CLI" ]] && ! command -v "$CLI" >/dev/null 2>&1; then
  if [[ -x /tmp/s2v/Source2Viewer-CLI ]]; then
    CLI=/tmp/s2v/Source2Viewer-CLI
  else
    echo "error: Source2Viewer-CLI not found (set SOURCE2VIEWER_CLI)" >&2
    exit 1
  fi
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

# Mesh/skin only — no PNGs, no material pack.
GLTF_GEO=(--gltf_export_format glb)

ANIMS='animation/anims/world/rifle/_default_rifle/idle_rifle,animation/anims/world/rifle/_default_rifle/run_n_rifle,animation/anims/world/rifle/_default_rifle/idle_crouch_rifle,animation/anims/world/rifle/_default_rifle/crouch_n_rifle'

export_players() {
  mkdir -p "$OUT/players"
  echo "== players (CT SAS + T Phoenix, 4 locomotion clips, geometry only) =="
  "$CLI" -i "$CSGO/pak01_dir.vpk" \
    -f "agents/models/ctm_sas/ctm_sas.vmdl_c" \
    -o "$OUT/players/ct_sas.glb" -d \
    "${GLTF_GEO[@]}" \
    --gltf_export_animations \
    --gltf_animation_list "$ANIMS"

  "$CLI" -i "$CSGO/pak01_dir.vpk" \
    -f "agents/models/tm_phoenix/tm_phoenix.vmdl_c" \
    -o "$OUT/players/t_phoenix.glb" -d \
    "${GLTF_GEO[@]}" \
    --gltf_export_animations \
    --gltf_animation_list "$ANIMS"

  rm -f "$OUT/players"/*.png "$OUT/players"/*_physics.glb
  find "$OUT/players" -type d -name textures -exec rm -rf {} + 2>/dev/null || true
  ls -lh "$OUT/players"/*.glb
}

export_weapons() {
  mkdir -p "$OUT/weapons"
  echo "== weapons (worldmodels, geometry only) =="
  # slug → pak path (third-person / world models)
  declare -A WPN=(
    # rifles
    [ak47]=weapons/models/ak47/weapon_rif_ak47.vmdl_c
    [m4a4]=weapons/models/m4a4/weapon_rif_m4a4.vmdl_c
    [m4a1]=weapons/models/m4a1_silencer/weapon_rif_m4a1_silencer.vmdl_c
    [famas]=weapons/models/famas/weapon_rif_famas.vmdl_c
    [galilar]=weapons/models/galilar/weapon_rif_galilar.vmdl_c
    [aug]=weapons/models/aug/weapon_rif_aug.vmdl_c
    [sg556]=weapons/models/sg556/weapon_rif_sg556.vmdl_c
    # snipers
    [awp]=weapons/models/awp/weapon_snip_awp.vmdl_c
    [ssg08]=weapons/models/ssg08/weapon_snip_ssg08.vmdl_c
    [scar20]=weapons/models/scar20/weapon_snip_scar20.vmdl_c
    [g3sg1]=weapons/models/g3sg1/weapon_snip_g3sg1.vmdl_c
    # pistols
    [deagle]=weapons/models/deagle/weapon_pist_deagle.vmdl_c
    [glock]=weapons/models/glock18/weapon_pist_glock18.vmdl_c
    [usp]=weapons/models/usp_silencer/weapon_pist_usp_silencer.vmdl_c
    [hkp2000]=weapons/models/hkp2000/weapon_pist_hkp2000.vmdl_c
    [p250]=weapons/models/p250/weapon_pist_p250.vmdl_c
    [fiveseven]=weapons/models/fiveseven/weapon_pist_fiveseven.vmdl_c
    [tec9]=weapons/models/tec9/weapon_pist_tec9.vmdl_c
    [cz75a]=weapons/models/cz75a/weapon_pist_cz75a.vmdl_c
    [elite]=weapons/models/elite/weapon_pist_elite.vmdl_c
    [revolver]=weapons/models/revolver/weapon_pist_revolver.vmdl_c
    # smgs
    [mp9]=weapons/models/mp9/weapon_smg_mp9.vmdl_c
    [mac10]=weapons/models/mac10/weapon_smg_mac10.vmdl_c
    [mp7]=weapons/models/mp7/weapon_smg_mp7.vmdl_c
    [ump45]=weapons/models/ump45/weapon_smg_ump45.vmdl_c
    [p90]=weapons/models/p90/weapon_smg_p90.vmdl_c
    [bizon]=weapons/models/bizon/weapon_smg_bizon.vmdl_c
    [mp5sd]=weapons/models/mp5sd/weapon_smg_mp5sd.vmdl_c
    # shotguns / heavy
    [nova]=weapons/models/nova/weapon_shot_nova.vmdl_c
    [xm1014]=weapons/models/xm1014/weapon_shot_xm1014.vmdl_c
    [mag7]=weapons/models/mag7/weapon_shot_mag7.vmdl_c
    [sawedoff]=weapons/models/sawedoff/weapon_shot_sawedoff.vmdl_c
    [m249]=weapons/models/m249/weapon_mach_m249.vmdl_c
    [negev]=weapons/models/negev/weapon_mach_negev.vmdl_c
  )
  local ok=0
  local fail=0
  # stable order
  for slug in $(printf '%s\n' "${!WPN[@]}" | sort); do
    echo "  $slug"
    if "$CLI" -i "$CSGO/pak01_dir.vpk" -f "${WPN[$slug]}" -o "$OUT/weapons/${slug}.glb" -d \
         "${GLTF_GEO[@]}" 2>/tmp/cyka-wpn-export.err; then
      ok=$((ok + 1))
    else
      echo "    skip (export failed — see path ${WPN[$slug]})"
      fail=$((fail + 1))
      rm -f "$OUT/weapons/${slug}.glb"
    fi
  done
  rm -f "$OUT/weapons"/*.png "$OUT/weapons"/*_physics.glb
  find "$OUT/weapons" -type d -name textures -exec rm -rf {} + 2>/dev/null || true
  echo "weapons: $ok ok, $fail failed"
  ls -lh "$OUT/weapons"/*.glb 2>/dev/null || true
}

export_maps() {
  mkdir -p "$OUT/maps"
  echo "== maps (DLT1 .tri via demolens extract-map, if installed) =="
  if command -v demolens >/dev/null 2>&1; then
    for map in de_mirage de_inferno de_nuke de_dust2 de_ancient de_anubis de_overpass de_vertigo de_train; do
      if [[ -f "$CSGO/maps/${map}.vpk" ]]; then
        echo "  $map"
        demolens extract-map --cs2 "$CS2_ROOT" --map "$map" --out-dir "$OUT/maps" \
          --vrf "$CLI" || true
        if [[ -f "$OUT/maps/${map}.tri" ]]; then
          ln -sfn "maps/${map}.tri" "$OUT/${map}.tri"
        fi
      fi
    done
  else
    echo "demolens not on PATH — map .tri files unchanged."
    echo "Install demolens or export world_physics with Source2Viewer-CLI and convert (see README)."
  fi
}

case "$WHAT" in
  all)
    export_players
    export_weapons
    export_maps
    ;;
  players) export_players ;;
  weapons) export_weapons ;;
  maps) export_maps ;;
  *)
    echo "unknown mode: $WHAT (want all|players|weapons|maps)" >&2
    exit 2
    ;;
esac

echo "done → $OUT"
