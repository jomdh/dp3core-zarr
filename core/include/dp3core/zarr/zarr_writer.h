// SPDX-License-Identifier: GPL-3.0-only
//
// This header does not include a DP3 header. The codec list is owned here.
// A DP3 caller translates its own binding into CodecSpec at the call.
#ifndef DP3CORE_ZARR_ZARR_WRITER_H_
#define DP3CORE_ZARR_ZARR_WRITER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "dp3core/zarr/zarr_store.h"

namespace dp3core::zarr {

/// Description of a new array. Attributes are flattened, as in ZarrStore.
struct ArraySpec {
  ElementType type = ElementType::kFloat64;
  std::vector<std::int64_t> shape;
  std::vector<std::int64_t> chunk_shape;
  std::vector<std::string> dimension_names;
  Attributes attributes;
};

/// One codec parameter. The vector is for fields such as an antenna map.
using CodecParameterValue =
    std::variant<bool, int, unsigned int, double, std::string,
                 std::vector<std::int64_t>>;

struct CodecParameter {
  std::string name;
  CodecParameterValue value;
};

/// One entry of a zarr.json "codecs" list: a name plus its configuration.
struct CodecSpec {
  std::string name;
  std::vector<CodecParameter> parameters;
};

/// Creates groups, array documents, string chunks, and pre-encoded chunks.
/// WriteSlab encodes bytes, bytes then zstd, and bytes then blosc.
/// It does not encode Dysco.
class ZarrWriter {
 public:
  explicit ZarrWriter(std::string path);

  std::string Path() const;

  void CreateGroup(const std::string& node, const Attributes& attributes);

  /// bytes, then zstd at @p zstd_level. Level 0 writes bytes only.
  /// The fill value is 0, or false for bool.
  void CreateArray(const std::string& array, const ArraySpec& spec,
                   int zstd_level);

  /// One-dimensional fixed_length_utf32, little endian, padded with U+0000.
  void CreateStringArray(const std::string& array,
                         const std::string& dimension_name,
                         const std::vector<std::string>& values,
                         const Attributes& attributes);

  /// Stores @p codecs as the array's codec list and does not encode them.
  void CreateEncodedArray(const std::string& array, const ArraySpec& spec,
                          const std::vector<CodecSpec>& codecs);

  /// Writes @p bytes to c/i/j/... for @p chunk_index.
  void WriteEncodedChunk(const std::string& array,
                         const std::vector<std::int64_t>& chunk_index,
                         const std::vector<std::byte>& bytes);

  /// Sets the length of the first dimension.
  void Resize(const std::string& array, std::int64_t length);

  /// Writes the slab [@p begin, @p end) on the first dimension.
  /// @p data holds (end - begin) times the product of the other dimensions
  /// elements of @p type, in C order. A partial chunk is read, overlaid,
  /// and written back. A missing chunk is fill value 0 before the overlay.
  void WriteSlab(const std::string& array, std::int64_t begin,
                 std::int64_t end, ElementType type, const void* data);

 private:
  std::string path_;
};

/// Creates a store at @p path. @p path must not exist.
std::unique_ptr<ZarrWriter> CreateZarr(const std::string& path);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_ZARR_WRITER_H_
