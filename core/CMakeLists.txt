# SPDX-License-Identifier: GPL-3.0-only
# nlohmann/json 3.12.0, the version the DP3 Zarr build already uses.
# The archive is extracted here. It is not the CMake package
# nlohmann_json, so a DP3 build can still fetch TensorStore's copy.
set(DP3CORE_ZARR_JSON_VERSION 3.12.0)
set(_dp3core_zarr_json_src "${CMAKE_BINARY_DIR}/dp3core-zarr-json-src")
set(DP3CORE_ZARR_JSON_ROOT
    "${_dp3core_zarr_json_src}/json-${DP3CORE_ZARR_JSON_VERSION}")
if(NOT EXISTS "${DP3CORE_ZARR_JSON_ROOT}/include/nlohmann/json.hpp")
  set(DP3CORE_ZARR_JSON_ZIP
      "${CMAKE_BINARY_DIR}/dp3core-zarr-json-${DP3CORE_ZARR_JSON_VERSION}.zip")
  string(CONCAT DP3CORE_ZARR_JSON_URL
         "https://github.com/nlohmann/json/archive/v"
         "${DP3CORE_ZARR_JSON_VERSION}.zip")
  set(DP3CORE_ZARR_JSON_HASH
      "34660b5e9a407195d55e8da705ed26cc6d175ce5a6b1fb957e701fb4d5b04022")
  file(DOWNLOAD "${DP3CORE_ZARR_JSON_URL}" "${DP3CORE_ZARR_JSON_ZIP}"
       EXPECTED_HASH SHA256=${DP3CORE_ZARR_JSON_HASH})
  file(ARCHIVE_EXTRACT INPUT "${DP3CORE_ZARR_JSON_ZIP}"
       DESTINATION "${CMAKE_BINARY_DIR}/dp3core-zarr-json-src")
endif()
if(NOT EXISTS "${DP3CORE_ZARR_JSON_ROOT}/include/nlohmann/json.hpp")
  message(FATAL_ERROR
          "nlohmann/json ${DP3CORE_ZARR_JSON_VERSION} was not extracted")
endif()

add_library(dp3core_zarr_core
  src/version.cc
  src/array_document.cc
  src/zarr_store.cc
  src/zarr_writer.cc
)
add_library(dp3core_zarr::core ALIAS dp3core_zarr_core)
target_include_directories(dp3core_zarr_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE "${DP3CORE_ZARR_JSON_ROOT}/include"
)
set_target_properties(dp3core_zarr_core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
)
dp3core_zarr_configure_target(dp3core_zarr_core)
target_compile_definitions(dp3core_zarr_core PRIVATE
  DP3CORE_ZARR_VERSION_STRING="${PROJECT_VERSION}")
