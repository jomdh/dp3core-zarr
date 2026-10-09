// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joaquin Martinez
//
// One blosc configuration: lz4, level 5, no shuffle.
#ifndef DP3CORE_ZARR_BLOSC_LZ4_H_
#define DP3CORE_ZARR_BLOSC_LZ4_H_

#include <cstddef>
#include <vector>

namespace dp3core::zarr {

inline constexpr int kBloscLz4Level = 5;
// c-blosc BLOSC_MAX_TYPESIZE. Shuffle is off, so this is only a bound.
inline constexpr int kBloscMaxTypesize = 255;

/// Compresses @p raw with blosc_compress. The compressor is lz4.
std::vector<std::byte> EncodeBloscLz4(const std::vector<std::byte>& raw,
                                      int typesize);

/// Decompresses a frame written by EncodeBloscLz4 into @p raw_size bytes.
std::vector<std::byte> DecodeBloscLz4(const std::vector<std::byte>& encoded,
                                      std::size_t raw_size);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_BLOSC_LZ4_H_
