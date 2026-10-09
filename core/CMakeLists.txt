# SPDX-License-Identifier: GPL-3.0-only
add_library(dp3core_zarr_core
  src/version.cc
)
add_library(dp3core_zarr::core ALIAS dp3core_zarr_core)
target_include_directories(dp3core_zarr_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
set_target_properties(dp3core_zarr_core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
)
dp3core_zarr_configure_target(dp3core_zarr_core)
target_compile_definitions(dp3core_zarr_core PRIVATE
  DP3CORE_ZARR_VERSION_STRING="${PROJECT_VERSION}")
