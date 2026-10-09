# dp3core-zarr: a standalone Zarr v3 store for DP3Core.
# SPDX-License-Identifier: GPL-3.0-only
#
# Convention notice: field names, attributes, chunk keys, and codec
# identifiers are unofficial DP3Core conventions, not endorsed by LOFAR,
# ASTRON, or the MSv4 group.
#
# A header in this repository does not include a DP3 header.
# DP3 may link this library. This library may link dp3core-dysco.
# That dysco link is not in this commit.

cmake_minimum_required(VERSION 3.20)
project(dp3core_zarr VERSION 0.1.0 LANGUAGES CXX)

option(BUILD_TESTING "Build the tests" ON)

function(dp3core_zarr_configure_target target)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endfunction()

enable_testing()

add_subdirectory(core)

if(BUILD_TESTING)
  add_subdirectory(core/tests)
endif()
