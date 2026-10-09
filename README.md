# dp3core-zarr

This repository is the Zarr v3 store for DP3Core.

The store reads a local Zarr v3 directory.
The store writes a local Zarr v3 directory.
The public classes are `ZarrStore` and `ZarrWriter`.
`ZarrStore` reads groups, children, attributes, and array metadata.
`ZarrStore` reads numeric slabs, doubles, and UTF-32 strings.
`ZarrWriter` writes groups, array documents, and numeric slabs.
`ZarrWriter` writes UTF-32 string chunks and pre-encoded chunks.
A numeric slab uses bytes, or bytes then zstd.
A Dysco chunk is decoded on read.
blosc is a later addition.
The library target is `dp3core_zarr::core`.
`LibraryVersion()` returns the version string.

## Fence

A header in this repository does not include a DP3 header.
The intended arrow is DP3, then this library, then `dp3core-dysco`.
DP3 will link this library.
This library links `dp3core_dysco::core` to decode a Dysco chunk.
`dp3core-dysco` stays free of this library.

`MsV4Source` and `MsV4Sink` stay in the DP3 tree.
`DyscoZarrCodec` and `BitroundZstdCodec` stay in the DP3 tree.
`MsV4` translates its codec list at the call.

## Build

The language is C++20.
CMake 3.20 or newer configures the build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DDP3CORE_DYSCO_SOURCE_DIR=/path/to/dp3core-dysco
cmake --build build
ctest --test-dir build --output-on-failure
```

A build inside DP3 already has `dp3core_dysco::core`.
That build does not need `DP3CORE_DYSCO_SOURCE_DIR` again.

The public headers are `dp3core/zarr/zarr_store.h`
and `dp3core/zarr/zarr_writer.h`.
`OpenZarr` opens a store.
`CreateZarr` creates a store.

## Licence

GPL-3.0-only. See `LICENSE`.
