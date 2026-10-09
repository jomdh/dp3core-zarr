// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/zarr/codec_chain.h"

#include <complex>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <zstd.h>

namespace dp3core::zarr {
namespace {

struct ZstdContext {
  ZstdContext() : context_(ZSTD_createCCtx()) {
    if (context_ == nullptr) {
      throw std::runtime_error("Cannot create a zstd context");
    }
  }
  ~ZstdContext() { ZSTD_freeCCtx(context_); }
  ZstdContext(const ZstdContext&) = delete;
  ZstdContext& operator=(const ZstdContext&) = delete;
  ZSTD_CCtx* get() const { return context_; }

 private:
  ZSTD_CCtx* context_;
};

void CheckChain(const std::vector<CodecStep>& codecs) {
  if (codecs.empty() || codecs[0].name != "bytes") {
    throw std::runtime_error("The standard chain starts with bytes");
  }
  if (codecs.size() > 2 ||
      (codecs.size() == 2 && codecs[1].name != "zstd")) {
    throw std::runtime_error(
        "The standard chain is bytes or bytes then zstd");
  }
}

void WriteLe32(std::uint32_t value, std::byte* out) {
  out[0] = static_cast<std::byte>(value & 0xFF);
  out[1] = static_cast<std::byte>((value >> 8) & 0xFF);
  out[2] = static_cast<std::byte>((value >> 16) & 0xFF);
  out[3] = static_cast<std::byte>((value >> 24) & 0xFF);
}

void WriteLe64(std::uint64_t value, std::byte* out) {
  for (int shift = 0; shift < 64; shift += 8) {
    out[shift / 8] =
        static_cast<std::byte>((value >> shift) & 0xFF);
  }
}

std::uint32_t ReadLe32(const std::byte* in) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<std::uint32_t>(std::to_integer<unsigned char>(
                 in[i]))
             << (8 * i);
  }
  return value;
}

std::uint64_t ReadLe64(const std::byte* in) {
  std::uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(
                 in[i]))
             << (8 * i);
  }
  return value;
}

std::uint32_t FloatBits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof bits);
  return bits;
}

float FloatFromBits(std::uint32_t bits) {
  float value = 0;
  std::memcpy(&value, &bits, sizeof value);
  return value;
}

std::uint64_t DoubleBits(double value) {
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof bits);
  return bits;
}

double DoubleFromBits(std::uint64_t bits) {
  double value = 0;
  std::memcpy(&value, &bits, sizeof value);
  return value;
}

std::vector<std::byte> Pack(ElementType type, std::size_t count,
                            const void* data) {
  const std::size_t width = ElementSize(type);
  std::vector<std::byte> bytes(count * width);
  for (std::size_t i = 0; i < count; ++i) {
    std::byte* out = bytes.data() + i * width;
    switch (type) {
      case ElementType::kBool: {
        const auto* values = static_cast<const bool*>(data);
        out[0] = static_cast<std::byte>(values[i] ? 1 : 0);
        break;
      }
      case ElementType::kFloat32: {
        const auto* values = static_cast<const float*>(data);
        WriteLe32(FloatBits(values[i]), out);
        break;
      }
      case ElementType::kFloat64: {
        const auto* values = static_cast<const double*>(data);
        WriteLe64(DoubleBits(values[i]), out);
        break;
      }
      case ElementType::kComplex64: {
        const auto* values =
            static_cast<const std::complex<float>*>(data);
        WriteLe32(FloatBits(values[i].real()), out);
        WriteLe32(FloatBits(values[i].imag()), out + 4);
        break;
      }
      case ElementType::kInt64: {
        const auto* values = static_cast<const std::int64_t*>(data);
        WriteLe64(static_cast<std::uint64_t>(values[i]), out);
        break;
      }
    }
  }
  return bytes;
}

void Unpack(ElementType type, const std::vector<std::byte>& bytes,
            std::size_t count, void* out) {
  const std::size_t width = ElementSize(type);
  if (bytes.size() != count * width) {
    throw std::runtime_error("Decoded chunk has the wrong size");
  }
  for (std::size_t i = 0; i < count; ++i) {
    const std::byte* in = bytes.data() + i * width;
    switch (type) {
      case ElementType::kBool: {
        auto* values = static_cast<bool*>(out);
        values[i] = in[0] != std::byte{0};
        break;
      }
      case ElementType::kFloat32: {
        auto* values = static_cast<float*>(out);
        values[i] = FloatFromBits(ReadLe32(in));
        break;
      }
      case ElementType::kFloat64: {
        auto* values = static_cast<double*>(out);
        values[i] = DoubleFromBits(ReadLe64(in));
        break;
      }
      case ElementType::kComplex64: {
        auto* values = static_cast<std::complex<float>*>(out);
        values[i] = {FloatFromBits(ReadLe32(in)),
                     FloatFromBits(ReadLe32(in + 4))};
        break;
      }
      case ElementType::kInt64: {
        auto* values = static_cast<std::int64_t*>(out);
        values[i] = static_cast<std::int64_t>(ReadLe64(in));
        break;
      }
    }
  }
}

std::vector<std::byte> Compress(const std::vector<std::byte>& raw,
                                int level, bool checksum) {
  ZstdContext context;
  const std::size_t bound = ZSTD_compressBound(raw.size());
  std::vector<std::byte> compressed(bound);
  const std::size_t level_status = ZSTD_CCtx_setParameter(
      context.get(), ZSTD_c_compressionLevel, level);
  const std::size_t checksum_status = ZSTD_CCtx_setParameter(
      context.get(), ZSTD_c_checksumFlag, checksum ? 1 : 0);
  if (ZSTD_isError(level_status) || ZSTD_isError(checksum_status)) {
    throw std::runtime_error("Cannot set zstd parameters");
  }
  const std::size_t written = ZSTD_compress2(
      context.get(), compressed.data(), compressed.size(), raw.data(),
      raw.size());
  if (ZSTD_isError(written)) {
    throw std::runtime_error(std::string("zstd compress failed: ") +
                             ZSTD_getErrorName(written));
  }
  compressed.resize(written);
  return compressed;
}

std::vector<std::byte> Decompress(const std::vector<std::byte>& encoded,
                                  std::size_t raw_size) {
  std::vector<std::byte> raw(raw_size);
  const std::size_t written =
      ZSTD_decompress(raw.data(), raw.size(), encoded.data(),
                      encoded.size());
  if (ZSTD_isError(written) || written != raw_size) {
    throw std::runtime_error("zstd decompress failed");
  }
  return raw;
}

}  // namespace

std::size_t ElementSize(ElementType type) {
  switch (type) {
    case ElementType::kBool:
      return 1;
    case ElementType::kFloat32:
      return 4;
    case ElementType::kFloat64:
    case ElementType::kInt64:
    case ElementType::kComplex64:
      return 8;
  }
  throw std::logic_error("Unknown element type");
}

void FillElements(ElementType type, std::size_t count, void* out) {
  std::memset(out, 0, count * ElementSize(type));
}

std::vector<std::byte> EncodeStandardChunk(
    ElementType type, std::size_t count, const void* data,
    const std::vector<CodecStep>& codecs) {
  CheckChain(codecs);
  std::vector<std::byte> bytes = Pack(type, count, data);
  if (codecs.size() == 2) {
    bytes = Compress(bytes, codecs[1].level, codecs[1].checksum);
  }
  return bytes;
}

void DecodeStandardChunk(ElementType type, std::size_t count,
                         const std::vector<std::byte>& encoded,
                         const std::vector<CodecStep>& codecs, void* out) {
  const std::vector<std::byte> bytes = DecodeBytePayload(
      count * ElementSize(type), encoded, codecs);
  Unpack(type, bytes, count, out);
}

std::vector<std::byte> DecodeBytePayload(
    std::size_t raw_size, const std::vector<std::byte>& encoded,
    const std::vector<CodecStep>& codecs) {
  CheckChain(codecs);
  if (codecs.size() == 2) return Decompress(encoded, raw_size);
  if (encoded.size() != raw_size) {
    throw std::runtime_error("Decoded chunk has the wrong size");
  }
  return encoded;
}

}  // namespace dp3core::zarr
