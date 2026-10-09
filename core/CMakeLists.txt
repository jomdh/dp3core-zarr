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

# zstd 1.5.7, the version TensorStore v0.1.85 vendors. The sources are
# compiled here. This is not a CMake package named zstd.
set(DP3CORE_ZARR_ZSTD_VERSION 1.5.7)
set(_dp3core_zarr_zstd_src "${CMAKE_BINARY_DIR}/dp3core-zarr-zstd-src")
set(DP3CORE_ZARR_ZSTD_ROOT
    "${_dp3core_zarr_zstd_src}/zstd-${DP3CORE_ZARR_ZSTD_VERSION}")
if(NOT EXISTS "${DP3CORE_ZARR_ZSTD_ROOT}/lib/zstd.h")
  string(CONCAT DP3CORE_ZARR_ZSTD_TGZ
         "${CMAKE_BINARY_DIR}/dp3core-zarr-zstd-"
         "${DP3CORE_ZARR_ZSTD_VERSION}.tar.gz")
  string(CONCAT DP3CORE_ZARR_ZSTD_URL
         "https://github.com/facebook/zstd/releases/download/v"
         "${DP3CORE_ZARR_ZSTD_VERSION}/zstd-"
         "${DP3CORE_ZARR_ZSTD_VERSION}.tar.gz")
  set(DP3CORE_ZARR_ZSTD_HASH
      "eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3")
  file(DOWNLOAD "${DP3CORE_ZARR_ZSTD_URL}" "${DP3CORE_ZARR_ZSTD_TGZ}"
       EXPECTED_HASH SHA256=${DP3CORE_ZARR_ZSTD_HASH})
  file(ARCHIVE_EXTRACT INPUT "${DP3CORE_ZARR_ZSTD_TGZ}"
       DESTINATION "${_dp3core_zarr_zstd_src}")
endif()
if(NOT EXISTS "${DP3CORE_ZARR_ZSTD_ROOT}/lib/zstd.h")
  message(FATAL_ERROR
          "zstd ${DP3CORE_ZARR_ZSTD_VERSION} was not extracted")
endif()

set(_dp3core_zarr_zstd_lib "${DP3CORE_ZARR_ZSTD_ROOT}/lib")
set(DP3CORE_ZARR_ZSTD_SOURCES
  common/debug.c
  common/entropy_common.c
  common/error_private.c
  common/fse_decompress.c
  common/pool.c
  common/threading.c
  common/xxhash.c
  common/zstd_common.c
  compress/fse_compress.c
  compress/hist.c
  compress/huf_compress.c
  compress/zstd_compress.c
  compress/zstd_compress_literals.c
  compress/zstd_compress_sequences.c
  compress/zstd_compress_superblock.c
  compress/zstd_double_fast.c
  compress/zstd_fast.c
  compress/zstd_lazy.c
  compress/zstd_ldm.c
  compress/zstd_opt.c
  compress/zstd_preSplit.c
  compress/zstdmt_compress.c
  decompress/huf_decompress.c
  decompress/zstd_ddict.c
  decompress/zstd_decompress.c
  decompress/zstd_decompress_block.c
)
list(TRANSFORM DP3CORE_ZARR_ZSTD_SOURCES
     PREPEND "${_dp3core_zarr_zstd_lib}/")
add_library(dp3core_zarr_zstd STATIC ${DP3CORE_ZARR_ZSTD_SOURCES})
target_include_directories(dp3core_zarr_zstd
  PUBLIC "${_dp3core_zarr_zstd_lib}")

# c-blosc c8072c21, the commit TensorStore v0.1.85 vendors.
# The bundled lz4 is compiled here. zstd, zlib, and snappy stay
# out, so this target does not define ZSTD_decompress.
set(DP3CORE_ZARR_BLOSC_COMMIT
    "c8072c21dcce8daf6dd19a0bbae8c53458ad68cc")
set(_dp3core_zarr_blosc_src "${CMAKE_BINARY_DIR}/dp3core-zarr-blosc-src")
set(DP3CORE_ZARR_BLOSC_ROOT
    "${_dp3core_zarr_blosc_src}/c-blosc-${DP3CORE_ZARR_BLOSC_COMMIT}")
if(NOT EXISTS "${DP3CORE_ZARR_BLOSC_ROOT}/blosc/blosc.h")
  string(CONCAT DP3CORE_ZARR_BLOSC_TGZ
         "${CMAKE_BINARY_DIR}/dp3core-zarr-blosc-"
         "${DP3CORE_ZARR_BLOSC_COMMIT}.tar.gz")
  string(CONCAT DP3CORE_ZARR_BLOSC_URL
         "https://github.com/Blosc/c-blosc/archive/"
         "${DP3CORE_ZARR_BLOSC_COMMIT}.tar.gz")
  set(DP3CORE_ZARR_BLOSC_HASH
      "339b6b42a72991314abf9fbadaeff0c34955c1fafa1710fdfcef45d1bc03c030")
  file(DOWNLOAD "${DP3CORE_ZARR_BLOSC_URL}" "${DP3CORE_ZARR_BLOSC_TGZ}"
       EXPECTED_HASH SHA256=${DP3CORE_ZARR_BLOSC_HASH})
  file(ARCHIVE_EXTRACT INPUT "${DP3CORE_ZARR_BLOSC_TGZ}"
       DESTINATION "${_dp3core_zarr_blosc_src}")
endif()
if(NOT EXISTS "${DP3CORE_ZARR_BLOSC_ROOT}/blosc/blosc.h")
  message(FATAL_ERROR "c-blosc ${DP3CORE_ZARR_BLOSC_COMMIT} was not extracted")
endif()

set(_dp3core_zarr_blosc_dir "${DP3CORE_ZARR_BLOSC_ROOT}/blosc")
set(_dp3core_zarr_lz4_dir
    "${DP3CORE_ZARR_BLOSC_ROOT}/internal-complibs/lz4-1.10.0")
set(DP3CORE_ZARR_BLOSC_SOURCES
  blosc.c
  blosclz.c
  fastcopy.c
  shuffle.c
  shuffle-generic.c
  bitshuffle-generic.c
)
list(TRANSFORM DP3CORE_ZARR_BLOSC_SOURCES
     PREPEND "${_dp3core_zarr_blosc_dir}/")
list(APPEND DP3CORE_ZARR_BLOSC_SOURCES
     "${_dp3core_zarr_lz4_dir}/lz4.c"
     "${_dp3core_zarr_lz4_dir}/lz4hc.c")
add_library(dp3core_zarr_blosc STATIC ${DP3CORE_ZARR_BLOSC_SOURCES})
target_include_directories(dp3core_zarr_blosc
  PUBLIC "${_dp3core_zarr_blosc_dir}"
  PRIVATE "${_dp3core_zarr_lz4_dir}")
target_compile_definitions(dp3core_zarr_blosc PRIVATE HAVE_LZ4)
find_package(Threads REQUIRED)
target_link_libraries(dp3core_zarr_blosc PUBLIC Threads::Threads)

add_library(dp3core_zarr_core
  src/version.cc
  src/array_document.cc
  src/blosc_lz4.cc
  src/codec_chain.cc
  src/slab.cc
  src/zarr_store.cc
  src/zarr_writer.cc
)
add_library(dp3core_zarr::core ALIAS dp3core_zarr_core)
target_include_directories(dp3core_zarr_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  PRIVATE "${DP3CORE_ZARR_JSON_ROOT}/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(dp3core_zarr_core
  PRIVATE dp3core_zarr_zstd dp3core_zarr_blosc dp3core_dysco::core)
set_target_properties(dp3core_zarr_core PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
)
dp3core_zarr_configure_target(dp3core_zarr_core)
target_compile_definitions(dp3core_zarr_core PRIVATE
  DP3CORE_ZARR_VERSION_STRING="${PROJECT_VERSION}")
