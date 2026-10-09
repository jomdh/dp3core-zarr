// SPDX-License-Identifier: GPL-3.0-only
//
// DP3Core convention notice: field names, attributes, chunk keys, and codec
// identifiers of this library are unofficial DP3Core conventions, not endorsed
// by LOFAR, ASTRON, or the MSv4 schema group.
//
// This header does not include a DP3 header.
#ifndef DP3CORE_ZARR_VERSION_H_
#define DP3CORE_ZARR_VERSION_H_

#include <string>

namespace dp3core::zarr {

/// Library version, for example "0.1.0".
std::string LibraryVersion();

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_VERSION_H_
