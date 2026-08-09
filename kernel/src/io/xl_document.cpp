#include "brep/io/xl_document.hpp"

#include "brep/feat/box_feature.hpp"
#include "brep/feat/extrude_feature.hpp"
#include "brep/feat/sketch_feature.hpp"
#include "brep/feat/sphere_feature.hpp"
#include "brep/io/bks_cache.hpp"
#include "brep/log.hpp"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace brep::io {
namespace {

std::string g_last_error;

void set_error(std::string msg) {
  g_last_error = std::move(msg);
  BREP_WARN("xl: {}", g_last_error);
}

// --- CRC32 (ISO polynomial) ------------------------------------------------
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

std::uint32_t crc32(const std::vector<std::uint8_t>& bytes) {
  return crc32_update(0, bytes.data(), bytes.size());
}

// --- Binary buffer ---------------------------------------------------------
class BinWriter {
 public:
  void u8(std::uint8_t v) { buf_.push_back(v); }
  void u32(std::uint32_t v) {
    for (int i = 0; i < 4; ++i) buf_.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
  }
  void u64(std::uint64_t v) {
    for (int i = 0; i < 8; ++i) buf_.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
  }
  void f64(double v) {
    std::uint64_t bits = 0;
    static_assert(sizeof(double) == 8);
    std::memcpy(&bits, &v, 8);
    u64(bits);
  }
  void guid(const Guid& g) {
    for (std::uint8_t b : g.bytes()) buf_.push_back(b);
  }
  void str(const std::string& s) {
    u32(static_cast<std::uint32_t>(s.size()));
    buf_.insert(buf_.end(), s.begin(), s.end());
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
  [[nodiscard]] std::string error() const { return error_; }

  std::uint8_t u8() {
    if (pos_ >= buf_.size()) return fail_u8();
    return buf_[pos_++];
  }
  std::uint32_t u32() {
    if (pos_ + 4 > buf_.size()) return fail_u32();
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= std::uint32_t(buf_[pos_++]) << (8 * i);
    return v;
  }
  std::uint64_t u64() {
    if (pos_ + 8 > buf_.size()) return fail_u64();
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= std::uint64_t(buf_[pos_++]) << (8 * i);
    return v;
  }
  double f64() {
    const std::uint64_t bits = u64();
    double v = 0;
    std::memcpy(&v, &bits, 8);
    return v;
  }
  Guid guid() {
    if (pos_ + 16 > buf_.size()) {
      fail("truncated guid");
      return Guid::nil();
    }
    std::array<std::uint8_t, 16> bytes{};
    for (int i = 0; i < 16; ++i) bytes[static_cast<std::size_t>(i)] = buf_[pos_++];
    return Guid::from_bytes(bytes);
  }
  std::string str() {
    const std::uint32_t n = u32();
    if (!ok_ || pos_ + n > buf_.size()) {
      fail("truncated string");
      return {};
    }
    std::string s(reinterpret_cast<const char*>(buf_.data() + pos_), n);
    pos_ += n;
    return s;
  }

  [[nodiscard]] bool finished() const noexcept { return pos_ == buf_.size(); }

 private:
  void fail(const char* msg) {
    ok_ = false;
    if (error_.empty()) error_ = msg;
  }
  std::uint8_t fail_u8() {
    fail("truncated u8");
    return 0;
  }
  std::uint32_t fail_u32() {
    fail("truncated u32");
    return 0;
  }
  std::uint64_t fail_u64() {
    fail("truncated u64");
    return 0;
  }

  std::vector<std::uint8_t> buf_;
  std::size_t pos_{0};
  bool ok_{true};
  std::string error_;
};

enum class FeatType : std::uint8_t { Box = 1, Sketch = 2, Extrude = 3, Sphere = 4 };

void write_plane(BinWriter& w, const Plane& p) {
  w.f64(p.origin.x());
  w.f64(p.origin.y());
  w.f64(p.origin.z());
  w.f64(p.normal.x());
  w.f64(p.normal.y());
  w.f64(p.normal.z());
  w.f64(p.u_axis.x());
  w.f64(p.u_axis.y());
  w.f64(p.u_axis.z());
  w.f64(p.v_axis.x());
  w.f64(p.v_axis.y());
  w.f64(p.v_axis.z());
}

Plane read_plane(BinReader& r) {
  Plane p;
  p.origin = Point3d{r.f64(), r.f64(), r.f64()};
  p.normal = Vector3d{r.f64(), r.f64(), r.f64()};
  p.u_axis = Vector3d{r.f64(), r.f64(), r.f64()};
  p.v_axis = Vector3d{r.f64(), r.f64(), r.f64()};
  return p;
}

void write_feature(BinWriter& w, const feat::IFeature& f) {
  if (f.type_name() == "Box") {
    const auto& box = static_cast<const feat::BoxFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Box));
    w.guid(box.id().guid);
    w.str(box.display_name());
    w.u8(box.suppressed() ? 1 : 0);
    w.f64(box.origin().x());
    w.f64(box.origin().y());
    w.f64(box.origin().z());
    w.guid(box.length_id().guid);
    w.guid(box.width_id().guid);
    w.guid(box.height_id().guid);
    w.guid(box.body_guid());
    return;
  }
  if (f.type_name() == "Sphere") {
    const auto& sph = static_cast<const feat::SphereFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Sphere));
    w.guid(sph.id().guid);
    w.str(sph.display_name());
    w.u8(sph.suppressed() ? 1 : 0);
    w.f64(sph.center().x());
    w.f64(sph.center().y());
    w.f64(sph.center().z());
    w.guid(sph.radius_id().guid);
    w.guid(sph.body_guid());
    return;
  }
  if (f.type_name() == "Sketch") {
    const auto& skf = static_cast<const feat::SketchFeature&>(f);
    const auto& sk = skf.sketch();
    w.u8(static_cast<std::uint8_t>(FeatType::Sketch));
    w.guid(skf.id().guid);
    w.str(skf.display_name());
    w.u8(skf.suppressed() ? 1 : 0);
    write_plane(w, sk.frame());
    w.u32(static_cast<std::uint32_t>(sk.points().size()));
    for (const auto& pt : sk.points()) {
      w.f64(pt.p.u());
      w.f64(pt.p.v());
      w.u8(pt.fixed ? 1 : 0);
    }
    w.u32(static_cast<std::uint32_t>(sk.lines().size()));
    for (const auto& ln : sk.lines()) {
      w.u32(ln.p0.value);
      w.u32(ln.p1.value);
    }
    w.u32(static_cast<std::uint32_t>(sk.circles().size()));
    for (const auto& c : sk.circles()) {
      w.u32(c.center.value);
      w.f64(c.radius);
    }
    w.u32(static_cast<std::uint32_t>(sk.constraints().size()));
    for (const auto& c : sk.constraints()) {
      w.u32(c.id.value);
      w.u8(static_cast<std::uint8_t>(c.kind));
      w.u32(c.a.value);
      w.u32(c.b.value);
      w.guid(c.dim.guid);
      w.f64(c.aux);
    }
    w.u32(sk.next_entity());
    w.u32(sk.next_constraint());
    return;
  }
  if (f.type_name() == "Extrude") {
    const auto& ex = static_cast<const feat::ExtrudeFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Extrude));
    w.guid(ex.id().guid);
    w.str(ex.display_name());
    w.u8(ex.suppressed() ? 1 : 0);
    w.guid(ex.sketch_feature_id().guid);
    w.guid(ex.distance_id().guid);
    w.guid(ex.body_guid());
    return;
  }
  BREP_WARN("xl save: skipping unknown feature type '{}'", f.type_name());
}

bool read_feature(BinReader& r, Part& part) {
  const auto type = static_cast<FeatType>(r.u8());
  if (!r.ok()) return false;

  if (type == FeatType::Box) {
    const Guid fid = r.guid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const Point3d origin{r.f64(), r.f64(), r.f64()};
    const param::ParameterId length{r.guid()};
    const param::ParameterId width{r.guid()};
    const param::ParameterId height{r.guid()};
    const Guid body = r.guid();
    if (!r.ok()) return false;
    auto feature = std::make_unique<feat::BoxFeature>(
        feat::FeatureId{fid}, name, origin, length, width, height);
    feature->set_body_guid(body);
    feature->set_suppressed(suppressed);
    feature->set_status(feat::FeatureStatus::Dirty);
    part.features().append(std::move(feature));
    return true;
  }

  if (type == FeatType::Sphere) {
    const Guid fid = r.guid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const Point3d center{r.f64(), r.f64(), r.f64()};
    const param::ParameterId radius{r.guid()};
    const Guid body = r.guid();
    if (!r.ok()) return false;
    auto feature = std::make_unique<feat::SphereFeature>(
        feat::FeatureId{fid}, name, center, radius);
    feature->set_body_guid(body);
    feature->set_suppressed(suppressed);
    feature->set_status(feat::FeatureStatus::Dirty);
    part.features().append(std::move(feature));
    return true;
  }

  if (type == FeatType::Sketch) {
    const Guid fid = r.guid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    Plane frame = read_plane(r);
    const std::uint32_t np = r.u32();
    std::vector<sketch::SketchPoint> points;
    points.reserve(np);
    for (std::uint32_t i = 0; i < np; ++i) {
      sketch::SketchPoint pt;
      pt.p = Point2d{r.f64(), r.f64()};
      pt.fixed = r.u8() != 0;
      points.push_back(pt);
    }
    const std::uint32_t nl = r.u32();
    std::vector<sketch::SketchLine> lines;
    lines.reserve(nl);
    for (std::uint32_t i = 0; i < nl; ++i) {
      lines.push_back(sketch::SketchLine{sketch::SketchEntityId{r.u32()},
                                         sketch::SketchEntityId{r.u32()}});
    }
    const std::uint32_t nc = r.u32();
    std::vector<sketch::SketchCircle> circles;
    circles.reserve(nc);
    for (std::uint32_t i = 0; i < nc; ++i) {
      circles.push_back(
          sketch::SketchCircle{sketch::SketchEntityId{r.u32()}, r.f64()});
    }
    const std::uint32_t ncons = r.u32();
    std::vector<sketch::Constraint> cons;
    cons.reserve(ncons);
    for (std::uint32_t i = 0; i < ncons; ++i) {
      sketch::Constraint c;
      c.id.value = r.u32();
      c.kind = static_cast<sketch::ConstraintKind>(r.u8());
      c.a.value = r.u32();
      c.b.value = r.u32();
      c.dim.guid = r.guid();
      c.aux = r.f64();
      cons.push_back(c);
    }
    const std::uint32_t next_e = r.u32();
    const std::uint32_t next_c = r.u32();
    if (!r.ok()) return false;
    sketch::Sketch sk;
    sk.assign(frame, std::move(points), std::move(lines), std::move(circles),
              std::move(cons), next_e, next_c);
    auto feature = std::make_unique<feat::SketchFeature>(feat::FeatureId{fid},
                                                         name, std::move(sk));
    feature->set_suppressed(suppressed);
    feature->set_status(feat::FeatureStatus::Dirty);
    part.features().append(std::move(feature));
    return true;
  }

  if (type == FeatType::Extrude) {
    const Guid fid = r.guid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const feat::FeatureId sketch_id{r.guid()};
    const param::ParameterId dist{r.guid()};
    const Guid body = r.guid();
    if (!r.ok()) return false;
    auto feature = std::make_unique<feat::ExtrudeFeature>(
        feat::FeatureId{fid}, name, sketch_id, dist);
    feature->set_body_guid(body);
    feature->set_suppressed(suppressed);
    feature->set_status(feat::FeatureStatus::Dirty);
    part.features().append(std::move(feature));
    return true;
  }

  set_error("unknown feature type in .xl file");
  return false;
}

void write_xform(BinWriter& w, const RigidTransform& t) {
  w.f64(t.translation.x());
  w.f64(t.translation.y());
  w.f64(t.translation.z());
  w.f64(t.x_axis.x());
  w.f64(t.x_axis.y());
  w.f64(t.x_axis.z());
  w.f64(t.y_axis.x());
  w.f64(t.y_axis.y());
  w.f64(t.y_axis.z());
  w.f64(t.z_axis.x());
  w.f64(t.z_axis.y());
  w.f64(t.z_axis.z());
}

RigidTransform read_xform(BinReader& r) {
  RigidTransform t;
  t.translation = Point3d{r.f64(), r.f64(), r.f64()};
  t.x_axis = Vector3d{r.f64(), r.f64(), r.f64()};
  t.y_axis = Vector3d{r.f64(), r.f64(), r.f64()};
  t.z_axis = Vector3d{r.f64(), r.f64(), r.f64()};
  return t;
}

void write_topo_ref(BinWriter& w, const naming::TopologyRef& ref) {
  w.guid(ref.feature.guid);
  w.str(ref.local_name);
}

naming::TopologyRef read_topo_ref(BinReader& r) {
  naming::TopologyRef ref;
  ref.feature.guid = r.guid();
  ref.local_name = r.str();
  return ref;
}

void write_assembly(BinWriter& w, const asm_::Assembly& assembly) {
  w.u32(static_cast<std::uint32_t>(assembly.occurrences().size()));
  for (const auto& occ : assembly.occurrences()) {
    w.guid(occ.id.guid);
    w.guid(occ.part_guid);
    w.str(occ.name);
    write_xform(w, occ.transform);
  }
  w.u32(static_cast<std::uint32_t>(assembly.mates().size()));
  for (const auto& mate : assembly.mates()) {
    w.guid(mate.id);
    w.u8(static_cast<std::uint8_t>(mate.kind));
    w.guid(mate.a.guid);
    w.guid(mate.b.guid);
    write_topo_ref(w, mate.face_ref_a);
    write_topo_ref(w, mate.face_ref_b);
    w.guid(mate.dim.guid);
    w.f64(mate.aux);
  }
}

bool read_assembly(BinReader& r, asm_::Assembly& assembly, std::string& err) {
  const std::uint32_t noc = r.u32();
  for (std::uint32_t i = 0; i < noc; ++i) {
    asm_::Occurrence occ;
    occ.id.guid = r.guid();
    occ.part_guid = r.guid();
    occ.name = r.str();
    occ.transform = read_xform(r);
    if (!r.ok()) {
      err = r.error();
      return false;
    }
    assembly.occurrences().push_back(std::move(occ));
  }
  const std::uint32_t nm = r.u32();
  for (std::uint32_t i = 0; i < nm; ++i) {
    asm_::Mate mate;
    mate.id = r.guid();
    mate.kind = static_cast<asm_::MateKind>(r.u8());
    mate.a.guid = r.guid();
    mate.b.guid = r.guid();
    mate.face_ref_a = read_topo_ref(r);
    mate.face_ref_b = read_topo_ref(r);
    mate.dim.guid = r.guid();
    mate.aux = r.f64();
    if (!r.ok()) {
      err = r.error();
      return false;
    }
    assembly.mates().push_back(std::move(mate));
  }
  return true;
}

std::vector<std::uint8_t> encode_payload(const Document& doc) {
  BinWriter w;
  w.guid(doc.guid);
  w.str(doc.name);
  w.u32(static_cast<std::uint32_t>(doc.parts().size()));
  for (const auto& part_ptr : doc.parts()) {
    const Part& part = *part_ptr;
    w.guid(part.guid);
    w.str(part.name);

    const auto& params = part.parameters().all();
    w.u32(static_cast<std::uint32_t>(params.size()));
    for (const auto& p : params) {
      w.guid(p.id.guid);
      w.str(p.name);
      w.u8(static_cast<std::uint8_t>(p.kind));
      w.f64(p.value);
      w.u8(p.user_driven ? 1 : 0);
    }

    const auto& features = part.features().features();
    w.u32(static_cast<std::uint32_t>(features.size()));
    for (const auto& f : features) {
      if (f) write_feature(w, *f);
    }
  }
  // schema v2+: assembly / mates / topology refs
  write_assembly(w, doc.assembly());
  return w.data();
}

std::unique_ptr<Document> decode_payload(const std::vector<std::uint8_t>& payload,
                                         std::uint32_t schema, std::string& err) {
  BinReader r(payload);
  const Guid doc_guid = r.guid();
  const std::string doc_name = r.str();
  const std::uint32_t part_count = r.u32();
  if (!r.ok()) {
    err = r.error();
    return nullptr;
  }

  auto doc = Document::create(doc_name);
  // Re-register under loaded Guid.
  doc->registry().remove(doc->guid);
  doc->guid = doc_guid;
  doc->registry().add(*doc);

  for (std::uint32_t pi = 0; pi < part_count; ++pi) {
    const Guid part_guid = r.guid();
    const std::string part_name = r.str();
    Part& part = doc->add_part(part_name);
    doc->registry().remove(part.guid);
    part.guid = part_guid;
    doc->registry().add(part);

    const std::uint32_t pc = r.u32();
    for (std::uint32_t i = 0; i < pc; ++i) {
      const Guid pid = r.guid();
      const std::string pname = r.str();
      const auto kind = static_cast<param::ParamKind>(r.u8());
      const double value = r.f64();
      const bool user_driven = r.u8() != 0;
      if (!r.ok()) {
        err = r.error();
        return nullptr;
      }
      if (!part.parameters().add_with_id(param::ParameterId{pid}, pname, kind,
                                        value, user_driven)) {
        err = "duplicate or invalid parameter Guid";
        return nullptr;
      }
    }

    const std::uint32_t fc = r.u32();
    for (std::uint32_t i = 0; i < fc; ++i) {
      if (!read_feature(r, part)) {
        err = g_last_error.empty() ? r.error() : g_last_error;
        return nullptr;
      }
    }

    part.features().mark_all_dirty();
    const auto regen = part.regenerate();
    if (!regen.ok) {
      err = "regenerate failed after load: " + regen.message;
      return nullptr;
    }
  }

  if (schema >= 2) {
    if (!read_assembly(r, doc->assembly(), err)) return nullptr;
    // Re-apply mates so transforms match solved state.
    param::ParameterStore* params = nullptr;
    if (Part* main = doc->main_part()) params = &main->parameters();
    asm_::MateSolver::solve(doc->assembly(), params);
  }

  if (!r.ok()) {
    err = r.error();
    return nullptr;
  }
  if (!r.finished()) {
    err = "trailing bytes in .xl payload";
    return nullptr;
  }

  doc->mark_clean();
  return doc;
}

}  // namespace

const std::string& last_xl_error() { return g_last_error; }

XlSaveResult save_xl(const Document& doc, const std::filesystem::path& path) {
  XlSaveResult result;
  try {
    const auto payload = encode_payload(doc);
    const std::uint32_t sum = crc32(payload);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      result.error = "cannot open file for writing";
      set_error(result.error);
      return result;
    }

    BinWriter header;
    header.bytes(reinterpret_cast<const std::uint8_t*>("XL01"), 4);
    header.u32(kXlSchemaVersion);
    header.u32(0);  // flags
    header.u64(payload.size());
    header.bytes(payload.data(), payload.size());
    header.u32(sum);
    out.write(reinterpret_cast<const char*>(header.data().data()),
              static_cast<std::streamsize>(header.data().size()));
    if (!out) {
      result.error = "write failed";
      set_error(result.error);
      return result;
    }

    // Optional mesh sidecar for faster viewer open.
    const auto cache = save_bks_cache(doc, path);
    if (!cache.ok) {
      BREP_WARN("xl: mesh cache not written: {}", cache.error);
    }

    result.ok = true;
    g_last_error.clear();
    BREP_INFO("saved .xl '{}' ({} bytes payload, schema={})", path.string(),
              payload.size(), kXlSchemaVersion);
    return result;
  } catch (const std::exception& ex) {
    result.error = ex.what();
    set_error(result.error);
    return result;
  }
}

XlLoadResult load_xl(const std::filesystem::path& path) {
  XlLoadResult result;
  try {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      result.error = "cannot open file for reading";
      set_error(result.error);
      return result;
    }

    std::vector<std::uint8_t> file_bytes(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (file_bytes.size() < 4 + 4 + 4 + 8 + 4) {
      result.error = "file too small";
      set_error(result.error);
      return result;
    }
    BinReader hdr(std::move(file_bytes));
    const char m0 = static_cast<char>(hdr.u8());
    const char m1 = static_cast<char>(hdr.u8());
    const char m2 = static_cast<char>(hdr.u8());
    const char m3 = static_cast<char>(hdr.u8());
    if (!hdr.ok() || m0 != 'X' || m1 != 'L' || m2 != '0' || m3 != '1') {
      result.error = "not an XL01 document";
      set_error(result.error);
      return result;
    }
    const std::uint32_t schema = hdr.u32();
    const std::uint32_t flags = hdr.u32();
    const std::uint64_t payload_size = hdr.u64();
    (void)flags;
    if (!hdr.ok()) {
      result.error = "truncated header";
      set_error(result.error);
      return result;
    }
    if (schema < kXlSchemaVersionMin || schema > kXlSchemaVersion) {
      result.error = "unsupported schema_version " + std::to_string(schema);
      set_error(result.error);
      return result;
    }
    if (payload_size > 64ull * 1024ull * 1024ull) {
      result.error = "payload too large";
      set_error(result.error);
      return result;
    }

    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_size));
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = hdr.u8();
    const std::uint32_t file_crc = hdr.u32();
    if (!hdr.ok()) {
      result.error = "truncated payload/crc";
      set_error(result.error);
      return result;
    }

    const std::uint32_t calc = crc32(payload);
    if (calc != file_crc) {
      result.error = "CRC mismatch (file corrupted or tampered)";
      set_error(result.error);
      return result;
    }

    std::string err;
    result.document = decode_payload(payload, schema, err);
    if (!result.document) {
      result.error = err.empty() ? "decode failed" : err;
      set_error(result.error);
      return result;
    }

    result.document->set_path(path.string());
    g_last_error.clear();
    BREP_INFO("loaded .xl '{}' parts={} occurrences={} schema={}",
              path.string(), result.document->parts().size(),
              result.document->assembly().occurrences().size(), schema);
    return result;
  } catch (const std::exception& ex) {
    result.error = ex.what();
    set_error(result.error);
    return result;
  }
}

}  // namespace brep::io
