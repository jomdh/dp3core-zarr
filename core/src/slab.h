// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Joaquin Martinez
#ifndef DP3CORE_ZARR_SLAB_H_
#define DP3CORE_ZARR_SLAB_H_

#include <cstdint>
#include <string>
#include <vector>

#include "dp3core/zarr/zarr_store.h"

namespace dp3core::zarr {

void ReadFirstAxis(const std::string& store, const std::string& array,
                   std::int64_t begin, std::int64_t end, ElementType type,
                   void* out);

void WriteFirstAxis(const std::string& store, const std::string& array,
                    std::int64_t begin, std::int64_t end, ElementType type,
                    const void* data);

std::vector<double> ReadAllDoubles(const std::string& store,
                                   const std::string& array);

std::vector<std::string> ReadAllStrings(const std::string& store,
                                        const std::string& array);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_SLAB_H_
