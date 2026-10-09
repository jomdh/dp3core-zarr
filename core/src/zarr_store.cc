// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/zarr/zarr_store.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include "dp3core/zarr/array_document.h"
#include "dp3core/zarr/chunk_key.h"
#include "slab.h"

namespace dp3core::zarr {

ZarrStore::ZarrStore(std::string path) : path_(std::move(path)) {}

std::string ZarrStore::Path() const { return path_; }

bool ZarrStore::Exists(const std::string& node) const {
  std::error_code error;
  return std::filesystem::is_regular_file(MetadataPath(path_, node), error);
}

std::vector<std::string> ZarrStore::ListChildren(
    const std::string& node) const {
  std::vector<std::string> children;
  std::error_code error;
  const std::filesystem::path directory =
      std::filesystem::path(path_) / node;
  for (const auto& entry :
       std::filesystem::directory_iterator(directory, error)) {
    if (std::filesystem::is_regular_file(entry.path() / kZarrJsonName,
                                         error)) {
      children.push_back(entry.path().filename().string());
    }
  }
  std::sort(children.begin(), children.end());
  return children;
}

Attributes ZarrStore::ReadAttributes(const std::string& node) const {
  Attributes attributes;
  const Json metadata = ReadZarrJson(MetadataPath(path_, node));
  const auto iterator = metadata.find("attributes");
  if (iterator != metadata.end()) Flatten(*iterator, "", attributes);
  return attributes;
}

ArrayMetadata ZarrStore::ReadArrayMetadata(const std::string& array) const {
  const Json metadata = ReadZarrJson(MetadataPath(path_, array));
  ArrayMetadata result;
  result.shape = metadata.at("shape").get<std::vector<std::int64_t>>();
  result.chunk_shape = metadata.at("chunk_grid")
                           .at("configuration")
                           .at("chunk_shape")
                           .get<std::vector<std::int64_t>>();
  const auto names = metadata.find("dimension_names");
  if (names != metadata.end() && names->is_array()) {
    for (const Json& name : *names) {
      result.dimension_names.push_back(
          name.is_string() ? name.get<std::string>() : std::string());
    }
  } else {
    result.dimension_names.resize(result.shape.size());
  }
  return result;
}

std::vector<double> ZarrStore::ReadDoubles(const std::string& array) const {
  return ReadAllDoubles(path_, array);
}

std::vector<std::string> ZarrStore::ReadStrings(
    const std::string& array) const {
  return ReadAllStrings(path_, array);
}

void ZarrStore::ReadSlab(const std::string& array, std::int64_t begin,
                         std::int64_t end, ElementType type,
                         void* out) const {
  ReadFirstAxis(path_, array, begin, end, type, out);
}

std::unique_ptr<ZarrStore> OpenZarr(const std::string& path) {
  if (!std::filesystem::is_regular_file(std::filesystem::path(path) /
                                        kZarrJsonName)) {
    throw std::runtime_error("No Zarr store at " + path);
  }
  return std::make_unique<ZarrStore>(path);
}

}  // namespace dp3core::zarr
