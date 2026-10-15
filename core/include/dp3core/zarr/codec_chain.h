// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joaquin Martinez
//
// bytes, then optional zstd or the one accepted blosc configuration.
#ifndef DP3CORE_ZARR_CODEC_CHAIN_H_
#define DP3CORE_ZARR_CODEC_CHAIN_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "dp3core/zarr/zarr_store.h"

namespace dp3core::zarr {

/// One step of a standard chain. zstd uses `level` and `checksum`.
/// blosc uses `level` and `typesize`.
struct CodecStep {
  std::string name;
  int level = 0;
  bool checksum = false;
  int typesize = 1;
};

std::size_t ElementSize(ElementType type);

/// Fills @p count elements with 0, or false for bool.
void FillElements(ElementType type, std::size_t count, void* out);

/// Encodes @p count C-order elements. The chain is bytes, bytes then
/// zstd, or bytes then blosc.
std::vector<std::byte> EncodeStandardChunk(
    ElementType type, std::size_t count, const void* data,
    const std::vector<CodecStep>& codecs);

/// Decodes a chunk written by EncodeStandardChunk into @p out.
void DecodeStandardChunk(ElementType type, std::size_t count,
                         const std::vector<std::byte>& encoded,
                         const std::vector<CodecStep>& codecs, void* out);

/// Encodes @p count native elements. The host is little-endian, so
/// the memory bytes are the little-endian payload.
std::vector<std::byte> EncodeNativeChunk(
    ElementType type, std::size_t count, const void* data,
    const std::vector<CodecStep>& codecs);

/// Decodes a little-endian chunk into @p out. The host is little-endian,
/// so the payload bytes are the native elements.
void DecodeNativeChunk(ElementType type, std::size_t count,
                       const std::vector<std::byte>& encoded,
                       const std::vector<CodecStep>& codecs, void* out);

/// Undoes zstd or blosc when the chain has it. The result is the
/// bytes payload.
std::vector<std::byte> DecodeBytePayload(
    std::size_t raw_size, const std::vector<std::byte>& encoded,
    const std::vector<CodecStep>& codecs);

/// DecodeBytePayload for a payload of @p full_size bytes, or of
/// @p short_size bytes. A Zarr v3 chunk is always full-size. This
/// library wrote an edge chunk clipped to the array shape before
/// 2026-10-15, and that payload is @p short_size. The result has the
/// size that was found.
std::vector<std::byte> DecodeBytePayloadEither(
    std::size_t full_size, std::size_t short_size,
    const std::vector<std::byte>& encoded,
    const std::vector<CodecStep>& codecs);

/// DecodeStandardChunk for @p full_count elements, or @p short_count
/// (see DecodeBytePayloadEither). Returns the count decoded.
std::size_t DecodeStandardChunkEither(ElementType type,
                                      std::size_t full_count,
                                      std::size_t short_count,
                                      const std::vector<std::byte>& encoded,
                                      const std::vector<CodecStep>& codecs,
                                      void* out);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_CODEC_CHAIN_H_
