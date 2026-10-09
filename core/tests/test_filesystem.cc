// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/zarr/zarr_store.h"
#include "dp3core/zarr/zarr_writer.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  std::cerr << "FAIL " << what << '\n';
  ++g_failures;
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

class TempStore {
 public:
  TempStore()
      : path_(std::filesystem::temp_directory_path() /
              ("dp3core-zarr-test-" + std::to_string(++sequence_))) {
    std::filesystem::remove_all(path_);
  }
  ~TempStore() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  static int sequence_;
  std::filesystem::path path_;
};

int TempStore::sequence_ = 0;

dp3core::zarr::ArraySpec FloatSpec() {
  dp3core::zarr::ArraySpec spec;
  spec.type = dp3core::zarr::ElementType::kFloat64;
  spec.shape = {4};
  spec.chunk_shape = {2};
  spec.dimension_names = {"frequency"};
  spec.attributes["scale"] = 1.5;
  return spec;
}

void TestGroupAndChildren() {
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  Expect(writer->Path() == temp.path().string(), "writer path");
  bool second = false;
  try {
    dp3core::zarr::CreateZarr(temp.path().string());
  } catch (const std::runtime_error&) {
    second = true;
  }
  Expect(second, "second create throws");

  bool opened_early = false;
  try {
    dp3core::zarr::OpenZarr(temp.path().string());
  } catch (const std::runtime_error&) {
    opened_early = true;
  }
  Expect(opened_early, "open before root document throws");

  dp3core::zarr::Attributes root;
  root["telescope"] = std::string("LOFAR");
  root["integration_time/0"] = std::int64_t{1};
  root["integration_time/1"] = std::int64_t{2};
  root["flag"] = true;
  writer->CreateGroup("", root);
  writer->CreateGroup("b", {});
  writer->CreateGroup("a", {});

  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  Expect(store->Exists(""), "root exists");
  Expect(store->Exists("a"), "group a exists");
  Expect(!store->Exists("missing"), "missing node");
  const auto children = store->ListChildren("");
  Expect(children.size() == 2 && children[0] == "a" && children[1] == "b",
         "children are sorted");

  const auto attributes = store->ReadAttributes("");
  Expect(std::get<std::string>(attributes.at("telescope")) == "LOFAR",
         "string attribute");
  Expect(std::get<bool>(attributes.at("flag")), "bool attribute");
  Expect(std::get<double>(attributes.at("integration_time/0")) == 1.0,
         "array attribute 0");
  Expect(std::get<double>(attributes.at("integration_time/1")) == 2.0,
         "array attribute 1");
  const std::string text = ReadFile(temp.path() / "zarr.json");
  Expect(text.find("\"integration_time\": [\n      1,\n      2\n    ]") !=
             std::string::npos,
         "attribute array is JSON integers");
}

void TestArrayDocument() {
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray("freq", FloatSpec(), 5);
  dp3core::zarr::ArraySpec flags;
  flags.type = dp3core::zarr::ElementType::kBool;
  flags.shape = {2};
  flags.chunk_shape = {2};
  writer->CreateArray("flag", flags, 0);
  dp3core::zarr::ArraySpec vis;
  vis.type = dp3core::zarr::ElementType::kComplex64;
  vis.shape = {1, 1};
  vis.chunk_shape = {1, 1};
  writer->CreateArray("vis", vis, 0);

  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  const auto metadata = store->ReadArrayMetadata("freq");
  Expect(metadata.shape == std::vector<std::int64_t>{4}, "shape");
  Expect(metadata.chunk_shape == std::vector<std::int64_t>{2}, "chunk shape");
  Expect(metadata.dimension_names.size() == 1 &&
             metadata.dimension_names[0] == "frequency",
         "dimension name");
  Expect(std::get<double>(store->ReadAttributes("freq").at("scale")) == 1.5,
         "array attribute");

  const std::string freq = ReadFile(temp.path() / "freq" / "zarr.json");
  Expect(freq.find("\"float64\"") != std::string::npos, "float64 dtype");
  Expect(freq.find("\"endian\": \"little\"") != std::string::npos,
         "bytes endian");
  Expect(freq.find("\"name\": \"zstd\"") != std::string::npos, "zstd name");
  Expect(freq.find("\"level\": 5") != std::string::npos, "zstd level");
  Expect(freq.find("\"checksum\": false") != std::string::npos,
         "zstd checksum");
  Expect(freq.find("\"fill_value\": 0.0") != std::string::npos, "float fill");

  const std::string flag = ReadFile(temp.path() / "flag" / "zarr.json");
  Expect(flag.find("endian") == std::string::npos, "bool has no endian");
  Expect(flag.find("zstd") == std::string::npos, "level 0 omits zstd");
  Expect(flag.find("\"fill_value\": false") != std::string::npos, "bool fill");

  const std::string complex_text = ReadFile(temp.path() / "vis" / "zarr.json");
  Expect(complex_text.find("\"complex64\"") != std::string::npos,
         "complex dtype");
  Expect(complex_text.find("\"fill_value\": [\n    0.0,\n    0.0\n  ]") !=
             std::string::npos,
         "complex fill");

  writer->Resize("freq", 9);
  Expect(store->ReadArrayMetadata("freq").shape[0] == 9, "resized shape");
  Expect(store->ReadArrayMetadata("freq").chunk_shape[0] == 2,
         "resize keeps the chunk shape");
}

void TestStringArray() {
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateStringArray("pol", "polarization", {"A", "éB"}, {});
  const std::string document = ReadFile(temp.path() / "pol" / "zarr.json");
  Expect(document.find("\"fixed_length_utf32\"") != std::string::npos,
         "utf32 name");
  Expect(document.find("\"length_bytes\": 8") != std::string::npos,
         "two code points");
  const std::string chunk = ReadFile(temp.path() / "pol" / "c" / "0");
  const std::string expected(
      "\x41\x00\x00\x00\x00\x00\x00\x00"
      "\xe9\x00\x00\x00\x42\x00\x00\x00",
      16);
  Expect(chunk == expected, "little-endian utf32 chunk");

  writer->CreateStringArray("empty", "row", {}, {});
  Expect(!std::filesystem::exists(temp.path() / "empty" / "c" / "0"),
         "empty string array writes no chunk");
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  Expect(store->ReadArrayMetadata("empty").shape[0] == 0, "empty shape");
}

void TestEncodedChunk() {
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  dp3core::zarr::ArraySpec spec;
  spec.type = dp3core::zarr::ElementType::kComplex64;
  spec.shape = {4, 2};
  spec.chunk_shape = {2, 2};
  dp3core::zarr::CodecSpec codec;
  codec.name = "dp3core.dysco";
  codec.parameters.push_back({"bits", 10});
  codec.parameters.push_back(
      {"antenna1", std::vector<std::int64_t>{0, 1}});
  writer->CreateEncodedArray("vis", spec, {codec});
  const std::vector<std::byte> bytes{std::byte{0x01}, std::byte{0x02},
                                     std::byte{0x00}, std::byte{0xff}};
  writer->WriteEncodedChunk("vis", {1, 0}, bytes);
  const std::string document = ReadFile(temp.path() / "vis" / "zarr.json");
  Expect(document.find("\"name\": \"dp3core.dysco\"") != std::string::npos,
         "codec name");
  Expect(document.find("\"bits\": 10") != std::string::npos, "codec bits");
  const std::string antenna =
      "\"antenna1\": [\n          0,\n          1\n        ]";
  Expect(document.find(antenna) != std::string::npos, "codec antenna map");
  const std::string chunk = ReadFile(temp.path() / "vis" / "c" / "1" / "0");
  Expect(chunk.size() == 4 && static_cast<unsigned char>(chunk[3]) == 0xff,
         "encoded chunk bytes");
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  Expect(store->ListChildren("vis").empty(), "chunk directory is not a child");
}

}  // namespace

int main() {
  try {
    TestGroupAndChildren();
    TestArrayDocument();
    TestStringArray();
    TestEncodedChunk();
  } catch (const std::exception& error) {
    std::cerr << "FAIL exception " << error.what() << '\n';
    return EXIT_FAILURE;
  }
  if (g_failures != 0) {
    std::cerr << g_failures << " checks failed\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
