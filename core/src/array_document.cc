// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
//
// zarr.json text matches the filesystem writer in
// DP3 io/zarr/TensorStoreZarr.cc.
#include "dp3core/zarr/array_document.h"

#include <fstream>
#include <functional>
#include <stdexcept>
#include <utility>

namespace dp3core::zarr {

void Flatten(const Json& json, const std::string& prefix,
             Attributes& attributes) {
  const auto key = [&prefix](const std::string& name) {
    return prefix.empty() ? name : prefix + "/" + name;
  };
  if (json.is_object()) {
    for (const auto& [name, value] : json.items()) {
      Flatten(value, key(name), attributes);
    }
  } else if (json.is_array()) {
    for (std::size_t i = 0; i < json.size(); ++i) {
      Flatten(json[i], key(std::to_string(i)), attributes);
    }
  } else if (json.is_boolean()) {
    attributes[prefix] = json.get<bool>();
  } else if (json.is_number()) {
    attributes[prefix] = json.get<double>();
  } else if (json.is_string()) {
    attributes[prefix] = json.get<std::string>();
  }
}

Json Unflatten(const Attributes& attributes) {
  Json json = Json::object();
  for (const auto& [key, value] : attributes) {
    Json* node = &json;
    std::size_t begin = 0;
    for (std::size_t end = key.find('/'); end != std::string::npos;
         begin = end + 1, end = key.find('/', begin)) {
      node = &(*node)[key.substr(begin, end - begin)];
    }
    Json& leaf = (*node)[key.substr(begin)];
    std::visit([&leaf](const auto& item) { leaf = item; }, value);
  }
  // An object whose keys are "0" .. "n-1" is a JSON array.
  std::function<void(Json&)> to_arrays = [&](Json& node) {
    if (!node.is_object()) return;
    for (auto& item : node.items()) to_arrays(item.value());
    std::vector<Json> elements;
    for (std::size_t i = 0; i < node.size(); ++i) {
      const auto iterator = node.find(std::to_string(i));
      if (iterator == node.end()) return;
      elements.push_back(*iterator);
    }
    if (!elements.empty()) node = elements;
  };
  to_arrays(json);
  return json;
}

Json ReadZarrJson(const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("No zarr.json at " + path.string());
  }
  return Json::parse(file);
}

void WriteBytes(const std::filesystem::path& path, const char* data,
                std::size_t size) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(data, static_cast<std::streamsize>(size));
  if (!file) {
    throw std::runtime_error("Cannot write " + path.string());
  }
}

void WriteZarrJson(const std::filesystem::path& path, const Json& document) {
  const std::string text = document.dump(2);
  WriteBytes(path, text.data(), text.size());
}

Json MakeGroupDocument(const Attributes& attributes) {
  return Json{{"zarr_format", 3},
              {"node_type", "group"},
              {"attributes", Unflatten(attributes)}};
}

Json MakeArrayDocument(const ArraySpec& spec, const Json& data_type,
                       const Json& fill_value, const Json& codecs) {
  return Json{
      {"zarr_format", 3},
      {"node_type", "array"},
      {"shape", spec.shape},
      {"data_type", data_type},
      {"chunk_grid",
       {{"name", "regular"},
        {"configuration", {{"chunk_shape", spec.chunk_shape}}}}},
      {"chunk_key_encoding",
       {{"name", "default"}, {"configuration", {{"separator", "/"}}}}},
      {"fill_value", fill_value},
      {"codecs", codecs},
      {"attributes", Unflatten(spec.attributes)},
      {"dimension_names", spec.dimension_names}};
}

}  // namespace dp3core::zarr
