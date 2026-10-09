// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
//
// Filesystem writes match DP3 io/zarr/TensorStoreZarr.cc.
// WriteSlab encodes bytes and bytes-then-zstd.
#include "dp3core/zarr/zarr_writer.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "dp3core/zarr/array_document.h"
#include "dp3core/zarr/chunk_key.h"
#include "slab.h"

namespace dp3core::zarr {
namespace {

std::string DataTypeName(ElementType type) {
  switch (type) {
    case ElementType::kBool:
      return "bool";
    case ElementType::kFloat32:
      return "float32";
    case ElementType::kFloat64:
      return "float64";
    case ElementType::kComplex64:
      return "complex64";
    case ElementType::kInt64:
      return "int64";
  }
  throw std::logic_error("Unknown element type");
}

Json FillValue(ElementType type) {
  switch (type) {
    case ElementType::kBool:
      return false;
    case ElementType::kComplex64:
      return Json::array({0.0, 0.0});
    case ElementType::kInt64:
      return 0;
    case ElementType::kFloat32:
    case ElementType::kFloat64:
      return 0.0;
  }
  throw std::logic_error("Unknown element type");
}

Json BytesCodec(bool multi_byte) {
  Json codec{{"name", "bytes"}};
  if (multi_byte) codec["configuration"] = {{"endian", "little"}};
  return codec;
}

Json CodecJson(const CodecSpec& codec) {
  Json configuration = Json::object();
  for (const CodecParameter& parameter : codec.parameters) {
    std::visit(
        [&](const auto& value) { configuration[parameter.name] = value; },
        parameter.value);
  }
  return Json{{"name", codec.name}, {"configuration", configuration}};
}

std::u32string DecodeUtf8(const std::string& value) {
  std::u32string text;
  for (std::size_t i = 0; i < value.size();) {
    const unsigned char lead = static_cast<unsigned char>(value[i]);
    const int width = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
    char32_t code_point = width == 1   ? lead
                          : width == 2 ? lead & 0x1F
                          : width == 3 ? lead & 0x0F
                                       : lead & 0x07;
    for (int j = 1;
         j < width && i + static_cast<std::size_t>(j) < value.size(); ++j) {
      const auto next = static_cast<unsigned char>(
          value[i + static_cast<std::size_t>(j)]);
      code_point = (code_point << 6) | (next & 0x3F);
    }
    text.push_back(code_point);
    i += static_cast<std::size_t>(width);
  }
  return text;
}

void AppendLittleEndian(std::uint32_t value, std::string& bytes) {
  bytes.push_back(static_cast<char>(value & 0xFF));
  bytes.push_back(static_cast<char>((value >> 8) & 0xFF));
  bytes.push_back(static_cast<char>((value >> 16) & 0xFF));
  bytes.push_back(static_cast<char>((value >> 24) & 0xFF));
}

}  // namespace

ZarrWriter::ZarrWriter(std::string path) : path_(std::move(path)) {}

std::string ZarrWriter::Path() const { return path_; }

void ZarrWriter::CreateGroup(const std::string& node,
                             const Attributes& attributes) {
  WriteZarrJson(MetadataPath(path_, node), MakeGroupDocument(attributes));
}

void ZarrWriter::CreateArray(const std::string& array, const ArraySpec& spec,
                             int zstd_level) {
  Json codecs = Json::array({BytesCodec(spec.type != ElementType::kBool)});
  if (zstd_level != 0) {
    codecs.push_back(
        {{"name", "zstd"},
         {"configuration", {{"level", zstd_level}, {"checksum", false}}}});
  }
  WriteZarrJson(MetadataPath(path_, array),
                MakeArrayDocument(spec, DataTypeName(spec.type),
                                  FillValue(spec.type), codecs));
}

void ZarrWriter::CreateStringArray(const std::string& array,
                                   const std::string& dimension_name,
                                   const std::vector<std::string>& values,
                                   const Attributes& attributes) {
  std::vector<std::u32string> texts;
  std::size_t length = 1;
  for (const std::string& value : values) {
    std::u32string text = DecodeUtf8(value);
    length = std::max(length, text.size());
    texts.push_back(std::move(text));
  }
  ArraySpec spec;
  spec.shape = {static_cast<std::int64_t>(values.size())};
  spec.chunk_shape = {std::max<std::int64_t>(
      1, static_cast<std::int64_t>(values.size()))};
  spec.dimension_names = {dimension_name};
  spec.attributes = attributes;
  const auto length_bytes = static_cast<std::int64_t>(4 * length);
  const Json data_type{{"name", "fixed_length_utf32"},
                       {"configuration", {{"length_bytes", length_bytes}}}};
  WriteZarrJson(MetadataPath(path_, array),
                MakeArrayDocument(spec, data_type, "",
                                  Json::array({BytesCodec(true)})));
  if (values.empty()) return;
  std::string bytes;
  bytes.reserve(values.size() * length * 4);
  for (std::size_t i = 0; i < texts.size(); ++i) {
    for (std::size_t j = 0; j < length; ++j) {
      const char32_t code_point = j < texts[i].size() ? texts[i][j] : 0;
      AppendLittleEndian(static_cast<std::uint32_t>(code_point), bytes);
    }
  }
  WriteBytes(ChunkPath(path_, array, {0}), bytes.data(), bytes.size());
}

void ZarrWriter::CreateEncodedArray(const std::string& array,
                                    const ArraySpec& spec,
                                    const std::vector<CodecSpec>& codecs) {
  Json codec_list = Json::array();
  for (const CodecSpec& codec : codecs) {
    codec_list.push_back(CodecJson(codec));
  }
  WriteZarrJson(MetadataPath(path_, array),
                MakeArrayDocument(spec, DataTypeName(spec.type),
                                  FillValue(spec.type), codec_list));
}

void ZarrWriter::WriteEncodedChunk(
    const std::string& array, const std::vector<std::int64_t>& chunk_index,
    const std::vector<std::byte>& bytes) {
  WriteBytes(ChunkPath(path_, array, chunk_index),
             reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

void ZarrWriter::WriteSlab(const std::string& array, std::int64_t begin,
                           std::int64_t end, ElementType type,
                           const void* data) {
  WriteFirstAxis(path_, array, begin, end, type, data);
}

void ZarrWriter::Resize(const std::string& array, std::int64_t length) {
  const std::filesystem::path path = MetadataPath(path_, array);
  Json metadata = ReadZarrJson(path);
  metadata["shape"][0] = length;
  WriteZarrJson(path, metadata);
}

std::unique_ptr<ZarrWriter> CreateZarr(const std::string& path) {
  if (std::filesystem::exists(path)) {
    throw std::runtime_error(path + " exists");
  }
  std::filesystem::create_directories(path);
  return std::make_unique<ZarrWriter>(path);
}

}  // namespace dp3core::zarr
