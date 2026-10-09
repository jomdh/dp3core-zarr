// SPDX-License-Identifier: GPL-3.0-only
//
// The zarr.json record. Callers of ZarrStore do not need this header.
#ifndef DP3CORE_ZARR_ARRAY_DOCUMENT_H_
#define DP3CORE_ZARR_ARRAY_DOCUMENT_H_

#include <cstddef>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "dp3core/zarr/zarr_store.h"
#include "dp3core/zarr/zarr_writer.h"

namespace dp3core::zarr {

using Json = nlohmann::json;

void Flatten(const Json& json, const std::string& prefix,
             Attributes& attributes);

Json Unflatten(const Attributes& attributes);

Json ReadZarrJson(const std::filesystem::path& path);

void WriteBytes(const std::filesystem::path& path, const char* data,
                std::size_t size);

void WriteZarrJson(const std::filesystem::path& path, const Json& document);

Json MakeGroupDocument(const Attributes& attributes);

Json MakeArrayDocument(const ArraySpec& spec, const Json& data_type,
                       const Json& fill_value, const Json& codecs);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_ARRAY_DOCUMENT_H_
