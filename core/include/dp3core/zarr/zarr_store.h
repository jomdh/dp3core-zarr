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
/// Numeric slabs are a later addition. This class reads metadata only.
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

 private:
  std::string path_;
};

/// Opens @p path for reading. @p path must already contain a root zarr.json.
std::unique_ptr<ZarrStore> OpenZarr(const std::string& path);

}  // namespace dp3core::zarr

#endif  // DP3CORE_ZARR_ZARR_STORE_H_
