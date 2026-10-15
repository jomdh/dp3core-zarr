// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
#include "slab.h"

#include <algorithm>
#include <bit>
#include <complex>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <utility>

#include "dp3core/dysco/codec.h"
#include "dp3core/zarr/array_document.h"
#include "dp3core/zarr/chunk_key.h"
#include "dp3core/zarr/blosc_lz4.h"
#include "dp3core/zarr/codec_chain.h"

namespace dp3core::zarr {
namespace {

struct Chain {
  bool dysco_weights = false;
  bool dysco_data = false;
  bool has_map = false;
  dp3core::dysco::AntennaMap map;
  std::vector<CodecStep> standard;
};

std::size_t Product(const std::vector<std::int64_t>& lengths) {
  std::size_t count = 1;
  for (const std::int64_t length : lengths) {
    if (length < 0) {
      throw std::runtime_error("A dimension length is negative");
    }
    const auto part = static_cast<std::size_t>(length);
    if (part != 0 &&
        count > (static_cast<std::size_t>(-1) / part)) {
      throw std::runtime_error("The array is too large");
    }
    count *= part;
  }
  return count;
}

std::string TextField(const Json& config, const char* key) {
  const auto field = config.find(key);
  if (field == config.end() || !field->is_string()) return {};
  return field->get<std::string>();
}

int IntegerField(const Json& config, const char* key, bool* present) {
  const auto field = config.find(key);
  *present = field != config.end();
  if (!*present || !field->is_number_integer()) return 0;
  return field->get<int>();
}

CodecStep ReadBloscStep(const Json& codec) {
  const Json config = codec.value("configuration", Json::object());
  if (TextField(config, "cname") != "lz4") {
    throw std::runtime_error("The blosc compressor is not lz4");
  }
  bool present = false;
  if (IntegerField(config, "clevel", &present) != kBloscLz4Level ||
      !present) {
    throw std::runtime_error("The blosc level is not 5");
  }
  if (TextField(config, "shuffle") != "noshuffle") {
    throw std::runtime_error("The blosc shuffle is not noshuffle");
  }
  const int blocksize = IntegerField(config, "blocksize", &present);
  if (present && (!config.at("blocksize").is_number_integer() ||
                  blocksize != 0)) {
    throw std::runtime_error("The blosc block size is not 0");
  }
  const int typesize = IntegerField(config, "typesize", &present);
  const bool bad_typesize =
      present && (!config.at("typesize").is_number_integer() ||
                  typesize < 1 || typesize > kBloscMaxTypesize);
  if (bad_typesize) {
    throw std::runtime_error("The blosc typesize is out of range");
  }
  CodecStep step;
  step.name = "blosc";
  step.level = kBloscLz4Level;
  step.typesize = present ? typesize : 1;
  return step;
}

std::vector<std::size_t> Strides(
    const std::vector<std::int64_t>& shape) {
  std::vector<std::size_t> stride(shape.size(), 1);
  for (int dim = static_cast<int>(shape.size()) - 2; dim >= 0; --dim) {
    stride[static_cast<std::size_t>(dim)] =
        stride[static_cast<std::size_t>(dim) + 1] *
        static_cast<std::size_t>(shape[static_cast<std::size_t>(dim) + 1]);
  }
  return stride;
}

std::size_t Offset(const std::vector<std::int64_t>& coord,
                   const std::vector<std::size_t>& stride) {
  std::size_t offset = 0;
  for (std::size_t dim = 0; dim < coord.size(); ++dim) {
    offset += static_cast<std::size_t>(coord[dim]) * stride[dim];
  }
  return offset;
}

ElementType StoredType(const Json& data_type) {
  if (!data_type.is_string()) {
    throw std::runtime_error("The array is not a numeric array");
  }
  const std::string name = data_type.get<std::string>();
  if (name == "bool") return ElementType::kBool;
  if (name == "float32") return ElementType::kFloat32;
  if (name == "float64") return ElementType::kFloat64;
  if (name == "complex64") return ElementType::kComplex64;
  if (name == "int64") return ElementType::kInt64;
  throw std::runtime_error("Unknown data type " + name);
}

Chain ParseChain(const Json& codecs) {
  Chain chain;
  if (!codecs.is_array() || codecs.empty()) {
    throw std::runtime_error("The array has no codec");
  }
  for (const Json& codec : codecs) {
    if (codec.value("name", "") == "sharding_indexed") {
      throw std::runtime_error("Sharded arrays are not supported");
    }
  }
  const std::string first = codecs[0].value("name", "");
  if (first == "dp3core.dysco") {
    chain.dysco_data = true;
  } else if (first == "dp3core.dysco-weights") {
    chain.dysco_weights = true;
  } else {
    if (first != "bytes") {
      throw std::runtime_error("The standard chain starts with bytes");
    }
    const Json config =
        codecs[0].value("configuration", Json::object());
    if (config.contains("endian") &&
        config.value("endian", "") != "little") {
      throw std::runtime_error("The bytes codec is not little endian");
    }
    CodecStep bytes;
    bytes.name = "bytes";
    chain.standard.push_back(bytes);
    if (codecs.size() > 1) {
      if (codecs.size() != 2) {
        throw std::runtime_error(
            "The standard chain is bytes, bytes then zstd, "
            "or bytes then blosc");
      }
      const std::string second = codecs[1].value("name", "");
      if (second == "zstd") {
        const Json zstd_config =
            codecs[1].value("configuration", Json::object());
        CodecStep zstd;
        zstd.name = "zstd";
        zstd.level = zstd_config.value("level", 0);
        zstd.checksum = zstd_config.value("checksum", false);
        chain.standard.push_back(zstd);
      } else if (second == "blosc") {
        chain.standard.push_back(ReadBloscStep(codecs[1]));
      } else {
        throw std::runtime_error(
            "The standard chain is bytes, bytes then zstd, "
            "or bytes then blosc");
      }
    }
  }
  if (chain.dysco_data) {
    const Json config =
        codecs[0].value("configuration", Json::object());
    chain.has_map = config.contains("n_antennas") &&
                    config.contains("antenna1") &&
                    config.contains("antenna2");
    if (chain.has_map) {
      chain.map.n_antenna = config.at("n_antennas").get<std::uint32_t>();
      for (const Json& value : config.at("antenna1")) {
        chain.map.antenna1.push_back(value.get<std::uint32_t>());
      }
      for (const Json& value : config.at("antenna2")) {
        chain.map.antenna2.push_back(value.get<std::uint32_t>());
      }
    }
  }
  return chain;
}

struct ArrayInfo {
  std::vector<std::int64_t> shape;
  std::vector<std::int64_t> chunk_shape;
  Json document;
};

ArrayInfo LoadArray(const std::string& store, const std::string& array) {
  ArrayInfo info;
  info.document = ReadZarrJson(MetadataPath(store, array));
  info.shape = info.document.at("shape").get<std::vector<std::int64_t>>();
  info.chunk_shape = info.document.at("chunk_grid")
                         .at("configuration")
                         .at("chunk_shape")
                         .get<std::vector<std::int64_t>>();
  if (info.shape.empty() || info.shape.size() != info.chunk_shape.size()) {
    throw std::runtime_error("Array " + array + " has a bad chunk grid");
  }
  for (const std::int64_t chunk : info.chunk_shape) {
    if (chunk <= 0) {
      throw std::runtime_error("Array " + array + " has an empty chunk");
    }
  }
  return info;
}

void CheckRange(const std::vector<std::int64_t>& shape, std::int64_t begin,
                std::int64_t end, const std::string& array) {
  if (begin < 0 || end < begin || end > shape[0]) {
    throw std::runtime_error("Slab " + std::to_string(begin) + ":" +
                             std::to_string(end) + " is outside " + array);
  }
}

std::vector<std::byte> ReadChunkFile(const std::filesystem::path& path,
                                     bool* found) {
  std::error_code error;
  *found = std::filesystem::is_regular_file(path, error);
  if (!*found) return {};
  std::ifstream file(path, std::ios::binary);
  file.seekg(0, std::ios::end);
  const std::streamoff end = file.tellg();
  if (!file || end < 0) {
    throw std::runtime_error("Cannot read " + path.string());
  }
  file.seekg(0, std::ios::beg);
  std::vector<std::byte> out(static_cast<std::size_t>(end));
  if (end > 0) {
    file.read(reinterpret_cast<char*>(out.data()), end);
  }
  if (!file) {
    throw std::runtime_error("Cannot read " + path.string());
  }
  return out;
}

std::vector<std::int64_t> ChunkLength(
    const ArrayInfo& info, const std::vector<std::int64_t>& index) {
  std::vector<std::int64_t> length(index.size());
  for (std::size_t dim = 0; dim < index.size(); ++dim) {
    const std::int64_t origin = index[dim] * info.chunk_shape[dim];
    const std::int64_t remain = info.shape[dim] - origin;
    length[dim] = std::min(info.chunk_shape[dim], remain);
  }
  return length;
}

bool ChunkFillsSlab(const Chain& chain,
                   const std::vector<std::int64_t>& origin,
                   const std::vector<std::int64_t>& length,
                   const std::vector<std::int64_t>& chunk_shape,
                   const std::vector<std::int64_t>& shape,
                   std::int64_t begin, std::int64_t end) {
  if (chain.dysco_data || chain.dysco_weights) return false;
  if (std::endian::native != std::endian::little) return false;
  // An edge chunk is stored full-size, so its bytes are not the slab's.
  if (length != chunk_shape) return false;
  if (origin.empty() || origin[0] < begin ||
      origin[0] + length[0] > end) {
    return false;
  }
  for (std::size_t dim = 1; dim < origin.size(); ++dim) {
    if (origin[dim] != 0 || length[dim] != shape[dim]) return false;
  }
  return true;
}

std::size_t SlabElement(const std::vector<std::int64_t>& origin,
                        const std::vector<std::size_t>& stride,
                        std::int64_t begin) {
  return static_cast<std::size_t>(origin[0] - begin) * stride[0];
}

void DecodeChunk(const Chain& chain, ElementType type,
                 const std::vector<std::int64_t>& length,
                 const std::vector<std::byte>& encoded, void* out) {
  if (chain.dysco_data || chain.dysco_weights) {
    if (length.size() != 4) {
      throw std::runtime_error("A Dysco chunk has four dimensions");
    }
    const dp3core::dysco::Shape shape{
        static_cast<std::size_t>(length[0]),
        static_cast<std::size_t>(length[1]),
        static_cast<std::size_t>(length[2]),
        static_cast<std::size_t>(length[3])};
    if (chain.dysco_weights) {
      dp3core::dysco::DecodeWeights(
          encoded.data(), encoded.size(), shape, static_cast<float*>(out));
    } else {
      const dp3core::dysco::AntennaMap* map =
          chain.has_map ? &chain.map : nullptr;
      dp3core::dysco::DecodeData(encoded.data(), encoded.size(), shape, map,
                                 static_cast<std::complex<float>*>(out));
    }
    return;
  }
  DecodeStandardChunk(type, Product(length), encoded, chain.standard, out);
}

/// Copies the box @p length from @p src, laid out as @p src_layout, to
/// @p dst, laid out as @p dst_layout. Both boxes start at the origin.
void CopyBox(const std::vector<std::int64_t>& length,
             const std::vector<std::int64_t>& src_layout,
             const std::byte* src,
             const std::vector<std::int64_t>& dst_layout, std::byte* dst,
             std::size_t width) {
  const std::size_t rank = length.size();
  if (rank == 0 || Product(length) == 0) return;
  const std::vector<std::size_t> src_stride = Strides(src_layout);
  const std::vector<std::size_t> dst_stride = Strides(dst_layout);
  std::vector<std::int64_t> at(rank, 0);
  const std::size_t run =
      static_cast<std::size_t>(length[rank - 1]) * width;
  std::function<void(std::size_t)> walk = [&](std::size_t dim) {
    if (dim + 1 == rank) {
      std::memcpy(dst + Offset(at, dst_stride) * width,
                  src + Offset(at, src_stride) * width, run);
      return;
    }
    for (at[dim] = 0; at[dim] < length[dim]; ++at[dim]) walk(dim + 1);
  };
  walk(0);
}

/// Moves the in-bounds box @p length of a chunk between the chunk
/// buffer, laid out as @p layout, and the slab.
void Transfer(const std::vector<std::int64_t>& origin,
              const std::vector<std::int64_t>& length,
              const std::vector<std::int64_t>& layout,
              const std::vector<std::int64_t>& shape, std::int64_t begin,
              std::int64_t end, std::size_t width, std::byte* chunk,
              std::byte* slab, bool to_slab) {
  const int rank = static_cast<int>(shape.size());
  std::vector<std::int64_t> region = shape;
  region[0] = end - begin;
  const std::vector<std::size_t> chunk_stride = Strides(layout);
  const std::vector<std::size_t> slab_stride = Strides(region);
  std::vector<std::int64_t> local(static_cast<std::size_t>(rank), 0);
  const auto copy_run = [&](int last, std::int64_t local_from,
                            std::int64_t count) {
    std::vector<std::int64_t> at = local;
    at[static_cast<std::size_t>(last)] = local_from;
    std::vector<std::int64_t> out_at(static_cast<std::size_t>(rank));
    out_at[0] = origin[0] + at[0] - begin;
    for (int dim = 1; dim < rank; ++dim) {
      out_at[static_cast<std::size_t>(dim)] =
          origin[static_cast<std::size_t>(dim)] +
          at[static_cast<std::size_t>(dim)];
    }
    const std::size_t bytes = static_cast<std::size_t>(count) * width;
    std::byte* chunk_ptr =
        chunk + Offset(at, chunk_stride) * width;
    std::byte* slab_ptr = slab + Offset(out_at, slab_stride) * width;
    if (to_slab) {
      std::memcpy(slab_ptr, chunk_ptr, bytes);
    } else {
      std::memcpy(chunk_ptr, slab_ptr, bytes);
    }
  };
  std::function<void(int)> walk = [&](int dim) {
    const int last = rank - 1;
    if (dim == last) {
      if (last == 0) {
        const std::int64_t lo = std::max(begin, origin[0]);
        const std::int64_t hi =
            std::min(end, origin[0] + length[0]);
        if (lo >= hi) return;
        copy_run(0, lo - origin[0], hi - lo);
        return;
      }
      const std::int64_t global0 = origin[0] + local[0];
      if (global0 < begin || global0 >= end) return;
      copy_run(last, 0, length[static_cast<std::size_t>(last)]);
      return;
    }
    for (local[static_cast<std::size_t>(dim)] = 0;
         local[static_cast<std::size_t>(dim)] <
         length[static_cast<std::size_t>(dim)];
         ++local[static_cast<std::size_t>(dim)]) {
      if (dim == 0) {
        const std::int64_t global0 = origin[0] + local[0];
        if (global0 < begin || global0 >= end) continue;
      }
      walk(dim + 1);
    }
  };
  walk(0);
}

void VisitChunks(const ArrayInfo& info, std::int64_t begin, std::int64_t end,
                 const std::function<void(const std::vector<std::int64_t>&)>&
                     visitor) {
  if (begin == end) return;
  const std::size_t rank = info.shape.size();
  std::vector<std::int64_t> index(rank, 0);
  std::vector<std::int64_t> limit(rank, 0);
  index[0] = begin / info.chunk_shape[0];
  limit[0] = (end - 1) / info.chunk_shape[0] + 1;
  for (std::size_t dim = 1; dim < rank; ++dim) {
    limit[dim] =
        (info.shape[dim] + info.chunk_shape[dim] - 1) / info.chunk_shape[dim];
  }
  std::function<void(std::size_t)> walk = [&](std::size_t dim) {
    if (dim == rank) {
      visitor(index);
      return;
    }
    const std::int64_t first = dim == 0 ? index[0] : 0;
    for (index[dim] = first; index[dim] < limit[dim]; ++index[dim]) {
      walk(dim + 1);
    }
  };
  walk(0);
}

std::vector<std::int64_t> Origin(const ArrayInfo& info,
                                 const std::vector<std::int64_t>& index) {
  std::vector<std::int64_t> origin(index.size());
  for (std::size_t dim = 0; dim < index.size(); ++dim) {
    origin[dim] = index[dim] * info.chunk_shape[dim];
  }
  return origin;
}

void ReadRegion(const std::string& store, const std::string& array,
                const ArrayInfo& info, const Chain& chain, ElementType type,
                std::int64_t begin, std::int64_t end, void* out) {
  std::vector<std::int64_t> region = info.shape;
  region[0] = end - begin;
  FillElements(type, Product(region), out);
  auto* slab = static_cast<std::byte*>(out);
  const std::size_t width = ElementSize(type);
  const std::vector<std::size_t> stride = Strides(region);
  VisitChunks(info, begin, end, [&](const std::vector<std::int64_t>& index) {
    const std::vector<std::int64_t> length = ChunkLength(info, index);
    const std::vector<std::int64_t> origin = Origin(info, index);
    bool found = false;
    const std::vector<std::byte> encoded =
        ReadChunkFile(ChunkPath(store, array, index), &found);
    if (ChunkFillsSlab(chain, origin, length, info.chunk_shape, info.shape,
                       begin, end)) {
      if (!found) return;
      DecodeNativeChunk(type, Product(length), encoded, chain.standard,
                        slab + SlabElement(origin, stride, begin) * width);
      return;
    }
    // A Zarr v3 chunk is stored full-size. An edge chunk this library
    // wrote before 2026-10-15 was clipped to the array shape; that
    // layout is still read. A Dysco chunk carries the clipped shape.
    std::vector<std::int64_t> layout = info.chunk_shape;
    std::vector<std::byte> chunk(Product(layout) * width);
    if (!found) {
      FillElements(type, Product(layout), chunk.data());
    } else if (chain.dysco_data || chain.dysco_weights) {
      layout = length;
      DecodeChunk(chain, type, length, encoded, chunk.data());
    } else {
      const std::size_t count = DecodeStandardChunkEither(
          type, Product(layout), Product(length), encoded, chain.standard,
          chunk.data());
      if (count != Product(layout)) layout = length;
    }
    Transfer(origin, length, layout, info.shape, begin, end, width,
             chunk.data(), slab, true);
  });
}

void AppendUtf8(char32_t code_point, std::string& text) {
  if (code_point < 0x80) {
    text += static_cast<char>(code_point);
  } else if (code_point < 0x800) {
    text += static_cast<char>(0xC0 | (code_point >> 6));
    text += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point < 0x10000) {
    text += static_cast<char>(0xE0 | (code_point >> 12));
    text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    text += static_cast<char>(0x80 | (code_point & 0x3F));
  } else {
    text += static_cast<char>(0xF0 | (code_point >> 18));
    text += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
    text += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    text += static_cast<char>(0x80 | (code_point & 0x3F));
  }
}

std::uint32_t ReadLe32(const std::byte* in) {
  std::uint32_t value = 0;
  for (int i = 0; i < 4; ++i) {
    value |= static_cast<std::uint32_t>(
                 std::to_integer<unsigned char>(in[i]))
             << (8 * i);
  }
  return value;
}

}  // namespace

void ReadFirstAxis(const std::string& store, const std::string& array,
                   std::int64_t begin, std::int64_t end, ElementType type,
                   void* out) {
  const ArrayInfo info = LoadArray(store, array);
  CheckRange(info.shape, begin, end, array);
  const ElementType stored = StoredType(info.document.at("data_type"));
  if (stored != type) {
    throw std::runtime_error("Array " + array + " has a different type");
  }
  const Chain chain = ParseChain(info.document.at("codecs"));
  if ((chain.dysco_data && type != ElementType::kComplex64) ||
      (chain.dysco_weights && type != ElementType::kFloat32)) {
    throw std::runtime_error("Array " + array + " has a different type");
  }
  ReadRegion(store, array, info, chain, type, begin, end, out);
}

void WriteFirstAxis(const std::string& store, const std::string& array,
                    std::int64_t begin, std::int64_t end, ElementType type,
                    const void* data) {
  const ArrayInfo info = LoadArray(store, array);
  CheckRange(info.shape, begin, end, array);
  const ElementType stored = StoredType(info.document.at("data_type"));
  if (stored != type) {
    throw std::runtime_error("Array " + array + " has a different type");
  }
  const Chain chain = ParseChain(info.document.at("codecs"));
  if (chain.dysco_data || chain.dysco_weights) {
    throw std::runtime_error("Cannot write a slab of encoded array " +
                             array);
  }
  const auto* slab = static_cast<const std::byte*>(data);
  const std::size_t width = ElementSize(type);
  std::vector<std::int64_t> region = info.shape;
  region[0] = end - begin;
  const std::vector<std::size_t> stride = Strides(region);
  VisitChunks(info, begin, end, [&](const std::vector<std::int64_t>& index) {
    const std::vector<std::int64_t> length = ChunkLength(info, index);
    const std::vector<std::int64_t> origin = Origin(info, index);
    if (ChunkFillsSlab(chain, origin, length, info.chunk_shape, info.shape,
                       begin, end)) {
      const std::byte* span =
          slab + SlabElement(origin, stride, begin) * width;
      const std::vector<std::byte> encoded = EncodeNativeChunk(
          type, Product(length), span, chain.standard);
      WriteBytes(ChunkPath(store, array, index),
                 reinterpret_cast<const char*>(encoded.data()),
                 encoded.size());
      return;
    }
    // The chunk is written full-size (Zarr v3). Cells past the array
    // edge hold the fill value. A chunk already on disk may be an edge
    // chunk clipped by this library before 2026-10-15; its box is
    // copied into the full layout.
    const bool covered =
        origin[0] >= begin && origin[0] + length[0] <= end;
    const std::vector<std::int64_t>& layout = info.chunk_shape;
    std::vector<std::byte> chunk(Product(layout) * width);
    FillElements(type, Product(layout), chunk.data());
    if (!covered) {
      bool found = false;
      const std::vector<std::byte> encoded =
          ReadChunkFile(ChunkPath(store, array, index), &found);
      if (found) {
        std::vector<std::byte> stored(Product(layout) * width);
        const std::size_t count = DecodeStandardChunkEither(
            type, Product(layout), Product(length), encoded,
            chain.standard, stored.data());
        if (count == Product(layout)) {
          chunk.swap(stored);
        } else {
          CopyBox(length, length, stored.data(), layout, chunk.data(),
                  width);
        }
      }
    }
    Transfer(origin, length, layout, info.shape, begin, end, width,
             chunk.data(), const_cast<std::byte*>(slab), false);
    const std::vector<std::byte> encoded = EncodeStandardChunk(
        type, Product(layout), chunk.data(), chain.standard);
    WriteBytes(ChunkPath(store, array, index),
               reinterpret_cast<const char*>(encoded.data()), encoded.size());
  });
}

std::vector<double> ReadAllDoubles(const std::string& store,
                                   const std::string& array) {
  const ArrayInfo info = LoadArray(store, array);
  const ElementType stored = StoredType(info.document.at("data_type"));
  if (stored == ElementType::kComplex64) {
    throw std::runtime_error("Cannot read complex64 array " + array +
                             " as doubles");
  }
  const Chain chain = ParseChain(info.document.at("codecs"));
  const std::size_t count = Product(info.shape);
  std::vector<std::byte> native(count * ElementSize(stored));
  ReadRegion(store, array, info, chain, stored, 0, info.shape[0],
             native.data());
  std::vector<double> values(count);
  for (std::size_t i = 0; i < count; ++i) {
    switch (stored) {
      case ElementType::kBool:
        values[i] = static_cast<const bool*>(
                        static_cast<const void*>(native.data()))[i]
                        ? 1.0
                        : 0.0;
        break;
      case ElementType::kFloat32:
        values[i] = static_cast<const float*>(
            static_cast<const void*>(native.data()))[i];
        break;
      case ElementType::kFloat64:
        values[i] = static_cast<const double*>(
            static_cast<const void*>(native.data()))[i];
        break;
      case ElementType::kInt64:
        values[i] = static_cast<double>(static_cast<const std::int64_t*>(
            static_cast<const void*>(native.data()))[i]);
        break;
      case ElementType::kComplex64:
        break;
    }
  }
  return values;
}

std::vector<std::string> ReadAllStrings(const std::string& store,
                                        const std::string& array) {
  const ArrayInfo info = LoadArray(store, array);
  const Json& data_type = info.document.at("data_type");
  if (!data_type.is_object() ||
      data_type.value("name", "") != "fixed_length_utf32") {
    throw std::runtime_error("Array " + array +
                             " is not a fixed_length_utf32 string array");
  }
  if (info.shape.size() != 1) {
    throw std::runtime_error("String array " + array +
                             " is not one-dimensional");
  }
  const Chain chain = ParseChain(info.document.at("codecs"));
  if (chain.dysco_data || chain.dysco_weights) {
    throw std::runtime_error("String array " + array + " is encoded");
  }
  const std::int64_t length_bytes =
      data_type.at("configuration").at("length_bytes").get<std::int64_t>();
  if (length_bytes < 4 || length_bytes % 4 != 0) {
    throw std::runtime_error("String array " + array +
                             " has a bad length_bytes");
  }
  const std::int64_t code_points = length_bytes / 4;
  std::vector<std::string> strings(
      static_cast<std::size_t>(info.shape[0]));
  VisitChunks(info, 0, info.shape[0],
              [&](const std::vector<std::int64_t>& index) {
                const std::int64_t count = ChunkLength(info, index)[0];
                const std::int64_t origin = Origin(info, index)[0];
                bool found = false;
                const std::vector<std::byte> encoded =
                    ReadChunkFile(ChunkPath(store, array, index), &found);
                if (!found) return;
                // Full chunk (Zarr v3), or clipped by this library
                // before 2026-10-15. The rows read are the prefix.
                const std::size_t full =
                    static_cast<std::size_t>(info.chunk_shape[0]) *
                    static_cast<std::size_t>(length_bytes);
                const std::size_t clipped =
                    static_cast<std::size_t>(count) *
                    static_cast<std::size_t>(length_bytes);
                const std::vector<std::byte> bytes = DecodeBytePayloadEither(
                    full, clipped, encoded, chain.standard);
                for (std::int64_t row = 0; row < count; ++row) {
                  std::string text;
                  const std::size_t row_bytes =
                      static_cast<std::size_t>(row) *
                      static_cast<std::size_t>(length_bytes);
                  const std::byte* in = bytes.data() + row_bytes;
                  for (std::int64_t j = 0; j < code_points; ++j) {
                    const char32_t code_point =
                        ReadLe32(in + static_cast<std::size_t>(j) * 4);
                    if (code_point == 0) break;
                    AppendUtf8(code_point, text);
                  }
                  strings[static_cast<std::size_t>(origin + row)] =
                      std::move(text);
                }
              });
  return strings;
}

}  // namespace dp3core::zarr
