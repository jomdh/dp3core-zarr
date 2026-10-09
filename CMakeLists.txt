# dp3core-zarr: a standalone Zarr v3 store for DP3Core.
# SPDX-License-Identifier: GPL-3.0-only
#
# Convention notice: field names, attributes, chunk keys, and codec
# identifiers are unofficial DP3Core conventions, not endorsed by LOFAR,
# ASTRON, or the MSv4 group.
#
# A header in this repository does not include a DP3 header.
# DP3 may link this library. This library links dp3core-dysco
# to decode a Dysco chunk. dp3core-dysco does not link this library.

cmake_minimum_required(VERSION 3.20)
project(dp3core_zarr VERSION 0.1.0 LANGUAGES C CXX)

option(BUILD_TESTING "Build the tests" ON)

function(dp3core_zarr_configure_target target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

enable_testing()

# DP3 adds dp3core-dysco before this repository, so the target exists.
# A standalone build takes the source directory instead.
if(NOT TARGET dp3core_dysco::core)
  set(DP3CORE_DYSCO_SOURCE_DIR
      ""
      CACHE PATH "Source directory of the dp3core-dysco repository")
  if(NOT EXISTS
     "${DP3CORE_DYSCO_SOURCE_DIR}/core/include/dp3core/dysco/codec.h")
    message(FATAL_ERROR
            "dp3core-zarr needs DP3CORE_DYSCO_SOURCE_DIR, the source "
            "directory of dp3core-dysco "
            "(got '${DP3CORE_DYSCO_SOURCE_DIR}').")
  endif()
  set(DP3CORE_DYSCO_TENSORSTORE OFF)
  set(DP3CORE_DYSCO_PYTHON OFF)
  set(DP3CORE_DYSCO_HARNESS OFF)
  set(_dp3core_zarr_saved_testing ${BUILD_TESTING})
  set(BUILD_TESTING OFF)
  add_subdirectory(${DP3CORE_DYSCO_SOURCE_DIR}
                   ${CMAKE_BINARY_DIR}/dp3core_dysco EXCLUDE_FROM_ALL)
  set(BUILD_TESTING ${_dp3core_zarr_saved_testing})
endif()

add_subdirectory(core)

if(BUILD_TESTING)
  add_subdirectory(core/tests)
endif()
