#include "brep/io/bks_cache.hpp"

#include "brep/log.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace brep::io {
namespace {

std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data,
                           std::size_t len) {
  crc = ~crc;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      const std::uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

class BinWriter {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
      buf_.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
  }
  void f64(double v) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &v, 8);
    for (int i = 0; i < 8; ++i)
      buf_.push_back(std::uint8_t((bits >> (8 * i)) & 0xff));
  }
  void guid(const Guid& g) {
    for (std::uint8_t b : g.bytes()) buf_.push_back(b);
  }
  void bytes(const std::uint8_t* p, std::size_t n) {
    buf_.insert(buf_.end(), p, p + n);
  }
  [[nodiscard]] const std::vector<std::uint8_t>& data() const { return buf_; }

 private:
  std::vector<std::uint8_t> buf_;
};

class BinReader {
 public:
  explicit BinReader(std::vector<std::uint8_t> data) : buf_(std::move(data)) {}
  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] const std::string& error() const { return error_; }

  std::uint8_t u8() {
    if (pos_ >= buf_.size()) {
      fail("trunc");
      return 0;
    }
    return buf_[pos_++];
  }
  std::uint32_t u32() {
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= std::uint32_t(u8()) << (8 * i);
    return v;
  }
  double f64() {
    std::uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) bits |= std::uint64_t(u8()) << (8 * i);
    double v = 0;
    std::memcpy(&v, &bits, 8);
    return v;
  }
  Guid guid() {
    std::array<std::uint8_t, 16> b{};
    for (int i = 0; i < 16; ++i) b[static_cast<std::size_t>(i)] = u8();
    return Guid::from_bytes(b);
  }
  [[nodiscard]] bool finished() const noexcept { return pos_ == buf_.size(); }

 private:
  void fail(const char* m) {
    ok_ = false;
    if (error_.empty()) error_ = m;
  }
  std::vector<std::uint8_t> buf_;
  std::size_t pos_{0};
  bool ok_{true};
  std::string error_;
};

void write_mesh(BinWriter& w, const TriangleMesh& tri, const EdgeMesh& edges) {
  w.u32(static_cast<std::uint32_t>(tri.vertices.size()));
  for (const auto& v : tri.vertices) {
    w.f64(v.position.x());
    w.f64(v.position.y());
    w.f64(v.position.z());
    w.f64(v.normal.x());
    w.f64(v.normal.y());
    w.f64(v.normal.z());
    w.f64(v.uv.u());
    w.f64(v.uv.v());
  }
  w.u32(static_cast<std::uint32_t>(tri.indices.size()));
  for (std::uint32_t idx : tri.indices) w.u32(idx);

  w.u32(static_cast<std::uint32_t>(edges.positions.size()));
  for (const auto& p : edges.positions) {
    w.f64(p.x());
    w.f64(p.y());
    w.f64(p.z());
  }
}

bool read_mesh(BinReader& r, TriangleMesh& tri, EdgeMesh& edges) {
  const std::uint32_t nv = r.u32();
  tri.vertices.resize(nv);
  for (std::uint32_t i = 0; i < nv; ++i) {
    tri.vertices[i].position = Point3d{r.f64(), r.f64(), r.f64()};
    tri.vertices[i].normal = Vector3d{r.f64(), r.f64(), r.f64()};
    tri.vertices[i].uv = Point2d{r.f64(), r.f64()};
  }
  const std::uint32_t ni = r.u32();
  tri.indices.resize(ni);
  for (std::uint32_t i = 0; i < ni; ++i) tri.indices[i] = r.u32();

  const std::uint32_t ne = r.u32();
  edges.positions.resize(ne);
  for (std::uint32_t i = 0; i < ne; ++i) {
    edges.positions[i] = Point3d{r.f64(), r.f64(), r.f64()};
  }
  return r.ok();
}

}  // namespace

std::filesystem::path bks_cache_path_for(const std::filesystem::path& xl_path) {
  return xl_path.parent_path() /
         (xl_path.stem().string() + std::string(".bks.cache"));
}

CacheSaveResult save_bks_cache(const Document& doc,
                               const std::filesystem::path& xl_path) {
  CacheSaveResult result;
  try {
    BinWriter w;
    w.bytes(reinterpret_cast<const std::uint8_t*>("BKSC"), 4);
    w.u32(1);  // cache schema
    w.guid(doc.guid);

    std::uint32_t body_count = 0;
    for (const auto& part : doc.parts()) {
      body_count += static_cast<std::uint32_t>(part->model().bodies().size());
    }
    w.u32(body_count);

    for (const auto& part : doc.parts()) {
      for (const auto& body : part->model().bodies()) {
        if (!body) continue;
        w.guid(body->guid);
        write_mesh(w, tessellate_body(*body), extract_edges(*body));
      }
    }

    const std::uint32_t sum =
        crc32_update(0, w.data().data(), w.data().size());
    w.u32(sum);

    const auto cache_path = bks_cache_path_for(xl_path);
    std::ofstream out(cache_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      result.error = "cannot write cache";
      return result;
    }
    out.write(reinterpret_cast<const char*>(w.data().data()),
              static_cast<std::streamsize>(w.data().size()));
    result.ok = static_cast<bool>(out);
    if (result.ok) {
      BREP_INFO("saved mesh cache '{}' bodies={}", cache_path.string(),
                body_count);
    }
    return result;
  } catch (const std::exception& ex) {
    result.error = ex.what();
    return result;
  }
}

CacheLoadResult load_bks_cache(const std::filesystem::path& xl_path,
                               const Guid& expected_document_guid) {
  CacheLoadResult result;
  try {
    const auto cache_path = bks_cache_path_for(xl_path);
    std::ifstream in(cache_path, std::ios::binary);
    if (!in) {
      result.error = "no cache";
      return result;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                    std::istreambuf_iterator<char>());
    if (bytes.size() < 4 + 4 + 16 + 4 + 4) {
      result.error = "cache too small";
      return result;
    }

    // Split CRC (last 4 LE bytes)
    std::uint32_t file_crc = 0;
    for (int i = 0; i < 4; ++i) {
      file_crc |= std::uint32_t(bytes[bytes.size() - 4 + static_cast<std::size_t>(i)])
                  << (8 * i);
    }
    bytes.resize(bytes.size() - 4);
    const std::uint32_t calc = crc32_update(0, bytes.data(), bytes.size());
    if (calc != file_crc) {
      // Also try native-endian CRC written by buggy first version path
      std::uint32_t native = 0;
      // already checked LE; if mismatch, reject
      result.error = "cache CRC mismatch";
      return result;
    }

    BinReader r(std::move(bytes));
    if (r.u8() != 'B' || r.u8() != 'K' || r.u8() != 'S' || r.u8() != 'C') {
      result.error = "bad cache magic";
      return result;
    }
    const std::uint32_t schema = r.u32();
    if (schema != 1) {
      result.error = "unsupported cache schema";
      return result;
    }
    result.cache.document_guid = r.guid();
    if (result.cache.document_guid != expected_document_guid) {
      result.error = "cache document Guid mismatch";
      return result;
    }

    const std::uint32_t n = r.u32();
    for (std::uint32_t i = 0; i < n; ++i) {
      const Guid body = r.guid();
      TriangleMesh tri;
      EdgeMesh edges;
      if (!read_mesh(r, tri, edges)) {
        result.error = r.error();
        return result;
      }
      result.cache.triangles.emplace(body, std::move(tri));
      result.cache.edges.emplace(body, std::move(edges));
    }
    if (!r.ok() || !r.finished()) {
      result.error = "cache decode failed";
      return result;
    }
    result.ok = true;
    BREP_INFO("loaded mesh cache bodies={}", n);
    return result;
  } catch (const std::exception& ex) {
    result.error = ex.what();
    return result;
  }
}

}  // namespace brep::io
