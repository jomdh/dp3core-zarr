# SPDX-License-Identifier: GPL-3.0-only
add_executable(test_version test_version.cc)
target_link_libraries(test_version PRIVATE dp3core_zarr::core)
dp3core_zarr_configure_target(test_version)
add_test(NAME test_version COMMAND test_version)

add_executable(test_filesystem test_filesystem.cc)
target_link_libraries(test_filesystem PRIVATE dp3core_zarr::core)
dp3core_zarr_configure_target(test_filesystem)
add_test(NAME test_filesystem COMMAND test_filesystem)
