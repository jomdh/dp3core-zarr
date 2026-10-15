// Copyright (C) 2026 Joaquin Martinez
// SPDX-License-Identifier: GPL-3.0-only
#include "dp3core/dysco/codec.h"
#include "dp3core/zarr/zarr_store.h"
#include "dp3core/zarr/zarr_writer.h"

#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "dp3core/zarr/chunk_key.h"
#include "dp3core/zarr/codec_chain.h"

namespace {

int g_failures = 0;

void Expect(bool ok, const char* what) {
  if (ok) return;
  std::cerr << "FAIL " << what << '\n';
  ++g_failures;
}

class TempStore {
 public:
  TempStore()
      : path_(std::filesystem::temp_directory_path() /
              ("dp3core-zarr-slab-" + std::to_string(++sequence_))) {
    std::filesystem::remove_all(path_);
  }
  ~TempStore() { std::filesystem::remove_all(path_); }
  const std::filesystem::path& path() const { return path_; }

 private:
  static int sequence_;
  std::filesystem::path path_;
};

int TempStore::sequence_ = 0;

dp3core::zarr::ArraySpec Spec(dp3core::zarr::ElementType type,
                              std::vector<std::int64_t> shape,
                              std::vector<std::int64_t> chunk) {
  dp3core::zarr::ArraySpec spec;
  spec.type = type;
  spec.shape = std::move(shape);
  spec.chunk_shape = std::move(chunk);
  return spec;
}

template <typename T>
void ExpectEqual(const std::vector<T>& got, const std::vector<T>& want,
                 const char* what) {
  Expect(got.size() == want.size(), what);
  if (got.size() != want.size()) return;
  for (std::size_t i = 0; i < got.size(); ++i) {
    if (!(got[i] == want[i])) {
      Expect(false, what);
      return;
    }
  }
}

void TestElementTypes() {
  using dp3core::zarr::ElementType;
  const int levels[] = {0, 5};
  for (const int level : levels) {
    TempStore temp;
    auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
    writer->CreateGroup("", {});
    const bool flags[3] = {true, false, true};
    const std::vector<float> f32 = {1.5f, -2.0f, 0.0f};
    const std::vector<double> f64 = {1.5, -2.0, 4.0};
    const std::vector<std::int64_t> i64 = {1, -2, 99};
    const std::vector<std::complex<float>> c64 = {{1.0f, -1.0f},
                                                   {0.0f, 2.5f},
                                                   {-3.0f, 0.5f}};
    writer->CreateArray("flag", Spec(ElementType::kBool, {3}, {2}), level);
    writer->CreateArray("f32", Spec(ElementType::kFloat32, {3}, {2}), level);
    writer->CreateArray("f64", Spec(ElementType::kFloat64, {3}, {2}), level);
    writer->CreateArray("i64", Spec(ElementType::kInt64, {3}, {2}), level);
    writer->CreateArray("c64", Spec(ElementType::kComplex64, {3}, {2}),
                        level);
    writer->WriteSlab("flag", 0, 3, ElementType::kBool, flags);
    writer->WriteSlab("f32", 0, 3, ElementType::kFloat32, f32.data());
    writer->WriteSlab("f64", 0, 3, ElementType::kFloat64, f64.data());
    writer->WriteSlab("i64", 0, 3, ElementType::kInt64, i64.data());
    writer->WriteSlab("c64", 0, 3, ElementType::kComplex64, c64.data());

    auto store = dp3core::zarr::OpenZarr(temp.path().string());
    bool got_flags[3] = {};
    std::vector<float> got_f32(3);
    std::vector<double> got_f64(3);
    std::vector<std::int64_t> got_i64(3);
    std::vector<std::complex<float>> got_c64(3);
    store->ReadSlab("flag", 0, 3, ElementType::kBool, got_flags);
    store->ReadSlab("f32", 0, 3, ElementType::kFloat32, got_f32.data());
    store->ReadSlab("f64", 0, 3, ElementType::kFloat64, got_f64.data());
    store->ReadSlab("i64", 0, 3, ElementType::kInt64, got_i64.data());
    store->ReadSlab("c64", 0, 3, ElementType::kComplex64, got_c64.data());
    ExpectEqual(got_f32, f32, "float32 round trip");
    ExpectEqual(got_f64, f64, "float64 round trip");
    ExpectEqual(got_i64, i64, "int64 round trip");
    ExpectEqual(got_c64, c64, "complex64 round trip");
    Expect(got_flags[0] && !got_flags[1] && got_flags[2],
           "bool round trip");
    ExpectEqual(store->ReadDoubles("f32"),
                std::vector<double>{1.5, -2.0, 0.0}, "float32 as double");
    ExpectEqual(store->ReadDoubles("i64"), std::vector<double>{1, -2, 99},
                "int64 as double");
    bool rejected = false;
    try {
      store->ReadDoubles("c64");
    } catch (const std::runtime_error&) {
      rejected = true;
    }
    Expect(rejected, "complex64 doubles are rejected");
  }
}

void TestInsideChunk() {
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray("row", Spec(ElementType::kFloat32, {8}, {4}), 1);
  std::vector<float> values(8);
  for (int i = 0; i < 8; ++i) values[static_cast<std::size_t>(i)] = i;
  writer->WriteSlab("row", 0, 8, ElementType::kFloat32, values.data());
  const std::vector<float> replacement = {9.0f, 8.0f};
  writer->WriteSlab("row", 1, 3, ElementType::kFloat32, replacement.data());
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<float> got(8);
  store->ReadSlab("row", 0, 8, ElementType::kFloat32, got.data());
  ExpectEqual(got, std::vector<float>{0, 9, 8, 3, 4, 5, 6, 7},
              "inside-chunk overlay");
}

void TestMissingAndSeveralChunks() {
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray("grid", Spec(ElementType::kFloat64, {6, 5}, {2, 2}),
                      3);
  std::vector<double> values(30);
  for (int t = 0; t < 6; ++t) {
    for (int f = 0; f < 5; ++f) {
      values[static_cast<std::size_t>(t * 5 + f)] = t * 10.0 + f;
    }
  }
  writer->WriteSlab("grid", 0, 6, ElementType::kFloat64, values.data());
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<double> slab(3 * 5);
  store->ReadSlab("grid", 2, 5, ElementType::kFloat64, slab.data());
  std::vector<double> want;
  for (int t = 2; t < 5; ++t) {
    for (int f = 0; f < 5; ++f) want.push_back(t * 10.0 + f);
  }
  ExpectEqual(slab, want, "slab across time and frequency chunks");

  writer->CreateArray("hole", Spec(ElementType::kInt64, {4}, {2}), 0);
  std::vector<std::int64_t> hole(2);
  store->ReadSlab("hole", 2, 4, ElementType::kInt64, hole.data());
  ExpectEqual(hole, std::vector<std::int64_t>{0, 0}, "missing chunk is 0");
  const std::int64_t seven = 7;
  writer->WriteSlab("hole", 2, 3, ElementType::kInt64, &seven);
  store->ReadSlab("hole", 2, 4, ElementType::kInt64, hole.data());
  ExpectEqual(hole, std::vector<std::int64_t>{7, 0},
              "partial write into a missing chunk");
}

void TestStrings() {
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateStringArray("pol", "polarization", {"A", "éB", ""}, {});
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  ExpectEqual(store->ReadStrings("pol"),
              std::vector<std::string>{"A", "éB", ""}, "string array");
}

void TestDysco() {
  using dp3core::dysco::AntennaMap;
  using dp3core::dysco::DataParams;
  using dp3core::dysco::DecodeData;
  using dp3core::dysco::DecodeWeights;
  using dp3core::dysco::EncodeData;
  using dp3core::dysco::EncodeWeights;
  using dp3core::dysco::EncodedSize;
  using dp3core::dysco::Normalization;
  using dp3core::dysco::Shape;
  using dp3core::dysco::WeightParams;
  using dp3core::zarr::CodecSpec;
  using dp3core::zarr::ElementType;

  const Shape shape{2, 3, 2, 2};
  AntennaMap map;
  map.n_antenna = 3;
  map.antenna1 = {0, 0, 1};
  map.antenna2 = {1, 2, 2};
  const std::size_t count =
      shape.n_time * shape.n_baseline * shape.n_channel * shape.n_pol;
  std::vector<std::complex<float>> data(count);
  std::vector<float> weights(count);
  for (std::size_t i = 0; i < count; ++i) {
    data[i] = {static_cast<float>(i) * 0.25f,
               static_cast<float>(i) * -0.1f};
    weights[i] = static_cast<float>(i + 1);
  }
  DataParams data_params;
  data_params.bits = 10;
  data_params.normalization = Normalization::kAF;
  data_params.seed = 3;
  std::vector<std::byte> encoded_data(EncodedSize(data_params, shape, &map));
  EncodeData(data_params, shape, &map, data.data(), encoded_data.data(),
             encoded_data.size());
  WeightParams weight_params;
  weight_params.bits = 12;
  std::vector<std::byte> encoded_weights(EncodedSize(weight_params, shape));
  EncodeWeights(weight_params, shape, weights.data(), encoded_weights.data(),
                encoded_weights.size());

  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  auto spec = Spec(ElementType::kComplex64, {2, 3, 2, 2}, {2, 3, 2, 2});
  CodecSpec data_codec;
  data_codec.name = "dp3core.dysco";
  data_codec.parameters.push_back({"bits", 10u});
  data_codec.parameters.push_back({"n_antennas", 3u});
  data_codec.parameters.push_back(
      {"antenna1", std::vector<std::int64_t>{0, 0, 1}});
  data_codec.parameters.push_back(
      {"antenna2", std::vector<std::int64_t>{1, 2, 2}});
  writer->CreateEncodedArray("vis", spec, {data_codec});
  writer->WriteEncodedChunk("vis", {0, 0, 0, 0}, encoded_data);

  spec.type = ElementType::kFloat32;
  CodecSpec weight_codec;
  weight_codec.name = "dp3core.dysco-weights";
  weight_codec.parameters.push_back({"bits", 12u});
  writer->CreateEncodedArray("weight", spec, {weight_codec});
  writer->WriteEncodedChunk("weight", {0, 0, 0, 0}, encoded_weights);

  std::vector<std::complex<float>> expected_data(count);
  DecodeData(encoded_data.data(), encoded_data.size(), shape, &map,
             expected_data.data());
  std::vector<float> expected_weights(count);
  DecodeWeights(encoded_weights.data(), encoded_weights.size(), shape,
                expected_weights.data());

  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<std::complex<float>> got_data(count);
  store->ReadSlab("vis", 0, 2, ElementType::kComplex64, got_data.data());
  ExpectEqual(got_data, expected_data, "dysco data chunk");
  const std::size_t row = shape.n_baseline * shape.n_channel * shape.n_pol;
  std::vector<std::complex<float>> got_row(row);
  store->ReadSlab("vis", 1, 2, ElementType::kComplex64, got_row.data());
  ExpectEqual(got_row,
              std::vector<std::complex<float>>(expected_data.begin() + row,
                                               expected_data.end()),
              "dysco slab inside the chunk");
  std::vector<float> got_weights(count);
  store->ReadSlab("weight", 0, 2, ElementType::kFloat32, got_weights.data());
  ExpectEqual(got_weights, expected_weights, "dysco weights chunk");

  bool rejected = false;
  try {
    writer->WriteSlab("vis", 0, 1, ElementType::kComplex64, data.data());
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  Expect(rejected, "dysco slab write is rejected");
}

void ExpectRejectedBlosc(const char* cname, int level, const char* shuffle,
                         const char* what) {
  using dp3core::zarr::CodecSpec;
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  CodecSpec bytes;
  bytes.name = "bytes";
  bytes.parameters.push_back({"endian", std::string("little")});
  CodecSpec blosc;
  blosc.name = "blosc";
  blosc.parameters.push_back({"cname", std::string(cname)});
  blosc.parameters.push_back({"clevel", level});
  blosc.parameters.push_back({"shuffle", std::string(shuffle)});
  writer->CreateEncodedArray("bad", Spec(ElementType::kFloat32, {2}, {2}),
                             {bytes, blosc});
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<float> out(2);
  bool rejected = false;
  try {
    store->ReadSlab("bad", 0, 2, ElementType::kFloat32, out.data());
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  Expect(rejected, what);
}

void TestBlosc() {
  using dp3core::zarr::CodecSpec;
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  CodecSpec bytes;
  bytes.name = "bytes";
  bytes.parameters.push_back({"endian", std::string("little")});
  CodecSpec blosc;
  blosc.name = "blosc";
  blosc.parameters.push_back({"cname", std::string("lz4")});
  blosc.parameters.push_back({"clevel", 5});
  blosc.parameters.push_back({"shuffle", std::string("noshuffle")});
  blosc.parameters.push_back({"typesize", 4});
  writer->CreateEncodedArray("row", Spec(ElementType::kFloat32, {4}, {4}),
                             {bytes, blosc});
  const std::vector<float> values = {1.25f, 1.25f, 1.25f, -4.0f};
  writer->WriteSlab("row", 0, 4, ElementType::kFloat32, values.data());
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<float> got(4);
  store->ReadSlab("row", 0, 4, ElementType::kFloat32, got.data());
  ExpectEqual(got, values, "blosc lz4 level 5 noshuffle");
  ExpectRejectedBlosc("zlib", 5, "noshuffle", "blosc compressor is rejected");
  ExpectRejectedBlosc("lz4", 1, "noshuffle", "blosc level is rejected");
  ExpectRejectedBlosc("lz4", 5, "shuffle", "blosc shuffle is rejected");
}

void TestMatchingChunk() {
  using dp3core::zarr::CodecStep;
  using dp3core::zarr::ElementType;
  constexpr int kTime = 3;
  constexpr int kBaseline = 2;
  constexpr int kChannel = 2;
  constexpr int kPol = 2;
  constexpr int kChunkTime = 2;
  const int count = kTime * kBaseline * kChannel * kPol;
  const int row = kBaseline * kChannel * kPol;
  std::vector<std::complex<float>> values(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    values[static_cast<std::size_t>(i)] = {
        static_cast<float>(i), static_cast<float>(-i) * 0.5f};
  }
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray(
      "vis",
      Spec(ElementType::kComplex64, {kTime, kBaseline, kChannel, kPol},
           {kChunkTime, kBaseline, kChannel, kPol}),
      3);
  writer->WriteSlab("vis", 0, kTime, ElementType::kComplex64,
                    values.data());

  const std::size_t chunk_count = static_cast<std::size_t>(kChunkTime * row);
  CodecStep bytes;
  bytes.name = "bytes";
  CodecStep zstd;
  zstd.name = "zstd";
  zstd.level = 3;
  const std::vector<std::byte> expect =
      dp3core::zarr::EncodeStandardChunk(ElementType::kComplex64, chunk_count,
                                         values.data(), {bytes, zstd});
  const auto path =
      dp3core::zarr::ChunkPath(temp.path(), "vis", {0, 0, 0, 0});
  std::ifstream file(path, std::ios::binary);
  const std::vector<char> stored((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
  Expect(file && stored.size() == expect.size(), "matching chunk size");
  if (stored.size() == expect.size()) {
    const bool same =
        std::memcmp(stored.data(), expect.data(), expect.size()) == 0;
    Expect(same, "matching chunk bytes");
  }

  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<std::complex<float>> whole(static_cast<std::size_t>(count));
  store->ReadSlab("vis", 0, kTime, ElementType::kComplex64, whole.data());
  ExpectEqual(whole, values, "matching full read");

  std::vector<std::complex<float>> first(chunk_count);
  store->ReadSlab("vis", 0, kChunkTime, ElementType::kComplex64,
                  first.data());
  ExpectEqual(first,
              std::vector<std::complex<float>>(values.begin(),
                                               values.begin() + chunk_count),
              "matching first chunk");

  const std::size_t tail =
      static_cast<std::size_t>((kTime - kChunkTime) * row);
  std::vector<std::complex<float>> last(tail);
  store->ReadSlab("vis", kChunkTime, kTime, ElementType::kComplex64,
                  last.data());
  ExpectEqual(last,
              std::vector<std::complex<float>>(values.begin() + chunk_count,
                                               values.end()),
              "matching short chunk");

  std::vector<std::complex<float>> middle(static_cast<std::size_t>(2 * row));
  store->ReadSlab("vis", 1, 3, ElementType::kComplex64, middle.data());
  ExpectEqual(middle,
              std::vector<std::complex<float>>(values.begin() + row,
                                               values.begin() + 3 * row),
              "partial read across chunks");
}

void TestShardingRejected() {
  using dp3core::zarr::CodecSpec;
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  CodecSpec shard;
  shard.name = "sharding_indexed";
  writer->CreateEncodedArray("bad", Spec(ElementType::kFloat32, {2}, {2}),
                             {shard});
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<float> out(2);
  bool rejected = false;
  try {
    store->ReadSlab("bad", 0, 2, ElementType::kFloat32, out.data());
  } catch (const std::runtime_error&) {
    rejected = true;
  }
  Expect(rejected, "sharding is rejected");
}

}  // namespace

// B57 (V1-M1-S034). A Zarr v3 chunk is full-size even past the array
// edge. The reader and the writer clipped it before 2026-10-15.
void TestEdgeChunkIsFullSize() {
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray("row", Spec(ElementType::kFloat64, {7}, {4}), 0);
  std::vector<double> values(7);
  for (int i = 0; i < 7; ++i) values[static_cast<std::size_t>(i)] = 10.0 + i;
  writer->WriteSlab("row", 0, 7, ElementType::kFloat64, values.data());
  const std::filesystem::path edge =
      dp3core::zarr::ChunkPath(temp.path(), "row", {1});
  Expect(std::filesystem::file_size(edge) == 4 * sizeof(double),
         "edge chunk is stored full-size");
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<double> got(7);
  store->ReadSlab("row", 0, 7, ElementType::kFloat64, got.data());
  ExpectEqual(got, values, "full read across the edge chunk");
  std::vector<double> tail(2);
  store->ReadSlab("row", 5, 7, ElementType::kFloat64, tail.data());
  ExpectEqual(tail, std::vector<double>{15.0, 16.0},
              "read inside the edge chunk");

  // zstd, with an edge on both axes.
  writer->CreateArray("grid", Spec(ElementType::kFloat64, {5, 3}, {2, 2}),
                      3);
  std::vector<double> grid(15);
  for (int i = 0; i < 15; ++i) grid[static_cast<std::size_t>(i)] = i;
  writer->WriteSlab("grid", 0, 5, ElementType::kFloat64, grid.data());
  const std::filesystem::path corner =
      dp3core::zarr::ChunkPath(temp.path(), "grid", {2, 1});
  Expect(std::filesystem::exists(corner), "corner edge chunk exists");
  std::vector<double> back(15);
  store->ReadSlab("grid", 0, 5, ElementType::kFloat64, back.data());
  ExpectEqual(back, grid, "edge chunks on both axes round-trip");
  std::vector<double> last(3);
  store->ReadSlab("grid", 4, 5, ElementType::kFloat64, last.data());
  ExpectEqual(last, std::vector<double>{12.0, 13.0, 14.0},
              "last row from the edge chunks");
  const std::vector<double> patch = {70.0, 71.0, 72.0};
  writer->WriteSlab("grid", 3, 4, ElementType::kFloat64, patch.data());
  store->ReadSlab("grid", 0, 5, ElementType::kFloat64, back.data());
  std::vector<double> want = grid;
  want[9] = 70.0;
  want[10] = 71.0;
  want[11] = 72.0;
  ExpectEqual(back, want, "partial write into full-size edge chunks");
}

void TestClippedEdgeChunkStillReads() {
  // A store written by this library before 2026-10-15 holds the edge
  // chunk clipped to the array shape. It is still read, and a write to
  // that chunk restores the full size.
  using dp3core::zarr::ElementType;
  TempStore temp;
  auto writer = dp3core::zarr::CreateZarr(temp.path().string());
  writer->CreateGroup("", {});
  writer->CreateArray("row", Spec(ElementType::kFloat64, {7}, {4}), 0);
  std::vector<double> values(7);
  for (int i = 0; i < 7; ++i) values[static_cast<std::size_t>(i)] = 10.0 + i;
  writer->WriteSlab("row", 0, 7, ElementType::kFloat64, values.data());
  const std::filesystem::path edge =
      dp3core::zarr::ChunkPath(temp.path(), "row", {1});
  {
    std::ofstream file(edge, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(values.data() + 4),
               3 * sizeof(double));
  }
  Expect(std::filesystem::file_size(edge) == 3 * sizeof(double),
         "clipped edge chunk written by hand");
  auto store = dp3core::zarr::OpenZarr(temp.path().string());
  std::vector<double> got(7);
  store->ReadSlab("row", 0, 7, ElementType::kFloat64, got.data());
  ExpectEqual(got, values, "clipped edge chunk still reads");
  const double replacement = 99.0;
  writer->WriteSlab("row", 5, 6, ElementType::kFloat64, &replacement);
  Expect(std::filesystem::file_size(edge) == 4 * sizeof(double),
         "a write restores the full size");
  store->ReadSlab("row", 0, 7, ElementType::kFloat64, got.data());
  values[5] = 99.0;
  ExpectEqual(got, values, "the other rows of the clipped chunk survive");
}

int main() {
  try {
    TestElementTypes();
    TestInsideChunk();
    TestMissingAndSeveralChunks();
    TestEdgeChunkIsFullSize();
    TestClippedEdgeChunkStillReads();
    TestStrings();
    TestDysco();
    TestBlosc();
    TestMatchingChunk();
    TestShardingRejected();
  } catch (const std::exception& error) {
    std::cerr << "FAIL exception " << error.what() << '\n';
    return 1;
  }
  if (g_failures != 0) {
    std::cerr << g_failures << " failures\n";
    return 1;
  }
  return 0;
}
