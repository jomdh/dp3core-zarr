// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joaquin Martinez
//
// bytes, then optional zstd. Sharding and blosc are not this chain.
#ifndef DP3CORE_ZARR_CODEC_CHAIN_H_
#define DP3CORE_ZARR_CODEC_CHAIN_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dp3core/zarr/zarr_store.h"

namespace dp3core::zarr {

/// One step of a standard chain. `level` and `checksum` belong to zstd.
struct CodecStep {
  std::string name;
  int level = 0;
  bool checksum = false;
};

std::size_t ElementSize(ElementType type);

/// Fills @p count elements with 0, or false for bool.
void FillElements(ElementType type, std::size_t count, void* out);

/// Encodes @p count C-order elements. The chain is bytes, or bytes then zstd.
std::vector<std::byte> EncodeStandardChunk(
    ElementType type, std::size_t count, const void* data,
    const std::vector<CodecStep>& codecs);

/// Decodes a chunk written by EncodeStandardChunk into @p out.
void DecodeStandardChunk(ElementType type, std::size_t count,
                         const std::vector<std::byte>& encoded,
                         const std::vector<CodecStep>& codecs, void* out);

/// Undoes zstd when the chain has it. The result is the bytes payload.
std::vector<std::byte> DecodeBytePayload(
    std::size_t raw_size, const std::vector<std::byte>& encoded,
    const std::vector<CodecStep>& codecs);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_CODEC_CHAIN_H_
