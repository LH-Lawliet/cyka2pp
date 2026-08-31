#pragma once

#include "cyka/demo/ent/context.hpp"

#include <cstdint>
#include <span>

namespace cyka::demo::ent {

/// CDemoStringTables / DEM_FullPacket.string_table: pull `instancebaseline`
/// entries (key = server-class id, value = baseline field data).
void ingestBaselineTables(std::span<const std::uint8_t> body, EntityContext& ctx);

/// svc_CreateStringTable. Handles only `instancebaseline`; returns the table
/// name so the caller can track table ids for later updates.
[[nodiscard]] std::string onCreateStringTable(std::span<const std::uint8_t> msg,
                                              EntityContext& ctx);

/// svc_UpdateStringTable for a table previously seen as `instancebaseline`.
void onUpdateStringTable(std::span<const std::uint8_t> msg, EntityContext& ctx);

} // namespace cyka::demo::ent
