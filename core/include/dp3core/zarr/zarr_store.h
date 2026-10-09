// SPDX-License-Identifier: GPL-3.0-only
//
// DP3Core convention notice: field names, attributes, chunk keys, and codec
// identifiers of this library are unofficial DP3Core conventions, not endorsed
// by LOFAR, ASTRON, or the MSv4 schema group.
//
// This header does not include a DP3 header.
#ifndef DP3CORE_ZARR_ZARR_STORE_H_
#define DP3CORE_ZARR_ZARR_STORE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace dp3core::zarr {

/// Element types this library writes into zarr.json.
enum class ElementType { kBool, kFloat32, kFloat64, kComplex64, kInt64 };

/// Shape and chunking of one array. A missing dimension name is empty.
struct ArrayMetadata {
  std::vector<std::int64_t> shape;
  std::vector<std::int64_t> chunk_shape;
  std::vector<std::string> dimension_names;
};

/// A scalar attribute. A reader returns every JSON number as double.
/// A writer stores std::int64_t as a JSON integer.
using AttributeValue = std::variant<bool, double, std::string, std::int64_t>;

/// Flattened attributes. Nested object keys are joined with '/'.
/// Array elements use their index, for example "integration_time/0".
/// JSON null is omitted.
using Attributes = std::map<std::string, AttributeValue>;

/// The file that marks a Zarr v3 group or array.
inline constexpr char kZarrJsonName[] = "zarr.json";

/// Reads one local Zarr v3 directory. Node "" is the root.
class ZarrStore {
 public:
  explicit ZarrStore(std::string path);

  std::string Path() const;

  /// True when @p node has a zarr.json file.
  bool Exists(const std::string& node) const;

  /// Names of groups and arrays directly below @p node, sorted.
  std::vector<std::string> ListChildren(const std::string& node) const;

  Attributes ReadAttributes(const std::string& node) const;

  ArrayMetadata ReadArrayMetadata(const std::string& array) const;

  /// The whole numeric array, converted to double, in C order.
  /// Complex64 is rejected. Visibility cubes use ReadSlab.
  std::vector<double> ReadDoubles(const std::string& array) const;

  /// A one-dimensional fixed_length_utf32 array. A U+0000 ends a string.
  std::vector<std::string> ReadStrings(const std::string& array) const;

  /// The slab [@p begin, @p end) on the first dimension, in C order.
  /// @p out holds (end - begin) times the product of the other dimensions
  /// elements of @p type.
  void ReadSlab(const std::string& array, std::int64_t begin,
                std::int64_t end, ElementType type, void* out) const;

 private:
  std::string path_;
};

/// Opens @p path for reading. @p path must already contain a root zarr.json.
std::unique_ptr<ZarrStore> OpenZarr(const std::string& path);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_ZARR_STORE_H_
