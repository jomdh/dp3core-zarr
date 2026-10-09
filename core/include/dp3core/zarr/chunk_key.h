// SPDX-License-Identifier: GPL-3.0-only
#ifndef DP3CORE_ZARR_CHUNK_KEY_H_
#define DP3CORE_ZARR_CHUNK_KEY_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "dp3core/zarr/zarr_store.h"

namespace dp3core::zarr {

/// zarr.json of @p node. Node "" is the root document.
inline std::filesystem::path MetadataPath(const std::filesystem::path& store,
                                          std::string_view node) {
  return store / std::string(node) / kZarrJsonName;
}

/// Chunk file c/i/j/... under @p array. The separator is '/'.
inline std::filesystem::path ChunkPath(
    const std::filesystem::path& store, std::string_view array,
    const std::vector<std::int64_t>& chunk_index) {
  std::filesystem::path path = store / std::string(array) / "c";
  for (const std::int64_t index : chunk_index) {
    path /= std::to_string(index);
  }
  return path;
}

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_CHUNK_KEY_H_
