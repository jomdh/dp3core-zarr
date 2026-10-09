// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/zarr/blosc_lz4.h"

#include <stdexcept>

#include <blosc.h>

namespace dp3core::zarr {
namespace {

void PrepareBloscLz4() {
  static const int selected = [] {
    blosc_init();
    return blosc_set_compressor("lz4");
  }();
  if (selected < 0) {
    throw std::runtime_error("Cannot select the lz4 compressor");
  }
}

}  // namespace

std::vector<std::byte> EncodeBloscLz4(const std::vector<std::byte>& raw,
                                      int typesize) {
  if (typesize < 1 || typesize > kBloscMaxTypesize) {
    throw std::runtime_error("The blosc typesize is out of range");
  }
  PrepareBloscLz4();
  if (raw.empty()) return {};
  if (raw.size() > static_cast<std::size_t>(BLOSC_MAX_BUFFERSIZE)) {
    throw std::runtime_error("The blosc input is too large");
  }
  std::vector<std::byte> compressed(
      raw.size() + static_cast<std::size_t>(BLOSC_MAX_OVERHEAD));
  const int written = blosc_compress(
      kBloscLz4Level, BLOSC_NOSHUFFLE, static_cast<std::size_t>(typesize),
      raw.size(), raw.data(), compressed.data(), compressed.size());
  if (written <= 0) {
    throw std::runtime_error("blosc compress failed");
  }
  compressed.resize(static_cast<std::size_t>(written));
  return compressed;
}

std::vector<std::byte> DecodeBloscLz4(const std::vector<std::byte>& encoded,
                                      std::size_t raw_size) {
  PrepareBloscLz4();
  if (raw_size == 0) {
    if (!encoded.empty()) {
      throw std::runtime_error("blosc decompress failed");
    }
    return {};
  }
  std::vector<std::byte> raw(raw_size);
  const int written =
      blosc_decompress_ctx(encoded.data(), raw.data(), raw.size(), 1);
  if (written <= 0 || static_cast<std::size_t>(written) != raw_size) {
    throw std::runtime_error("blosc decompress failed");
  }
  return raw;
}

}  // namespace dp3core::zarr
