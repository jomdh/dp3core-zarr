# dp3core-zarr

This repository is the Zarr v3 store for DP3Core.

The store reads a local Zarr v3 directory.
The store writes a local Zarr v3 directory.
The public classes are `ZarrStore` and `ZarrWriter`.
`ZarrStore` reads groups, children, attributes, and array metadata.
`ZarrWriter` writes groups and array documents.
`ZarrWriter` writes UTF-32 string chunks and pre-encoded chunks.
Numeric slabs are a later addition.
The library target is `dp3core_zarr::core`.
`LibraryVersion()` returns the version string.

## Fence

A header in this repository does not include a DP3 header.
The intended arrow is DP3, then this library, then `dp3core-dysco`.
DP3 will link this library.
This library will link `dp3core-dysco` when the slab reader needs Dysco.
`dp3core-dysco` stays free of this library.
This commit does not link `dp3core-dysco`.

`MsV4Source` and `MsV4Sink` stay in the DP3 tree.
`DyscoZarrCodec` and `BitroundZstdCodec` stay in the DP3 tree.
`MsV4` translates its codec list at the call.

## Build

The language is C++20.
CMake 3.20 or newer configures the build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The public headers are `dp3core/zarr/zarr_store.h`
and `dp3core/zarr/zarr_writer.h`.
`OpenZarr` opens a store.
`CreateZarr` creates a store.

## Licence

GPL-3.0-only. See `LICENSE`.
