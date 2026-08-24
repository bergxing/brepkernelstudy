#include "brep/io/XlDocument.h"

#include "brep/feat/BooleanFeature.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/ExtrudeFeature.h"
#include "brep/feat/SketchFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/io/BksCache.h"
#include "brep/Log.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace brep::io
{
namespace
{

std::string g_last_error;

void set_error(std::string msg)
{
  g_last_error = std::move(msg);
  BREP_WARN("xl: {}", g_last_error);
}

// --- CRC32 (ISO polynomial) ------------------------------------------------
std::uint32_t crc32_update(std::uint32_t crc, const std::uint8_t* data,
                           std::size_t len)
{
  crc = ~crc;
  for (std::size_t i = 0; i < len; ++i)
  {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b)
    {
      const std::uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

std::uint32_t crc32(const std::vector<std::uint8_t>& bytes)
{
  return crc32_update(0, bytes.data(), bytes.size());
}

// --- Binary buffer ---------------------------------------------------------
class BinWriter
{
 public:
  void u8(std::uint8_t v)
 {
      m_buf.push_back(v); 
  }
  void u32(std::uint32_t v)
  {
    for (int i = 0; i < 4; ++i) m_buf.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
  }
  void u64(std::uint64_t v)
  {
    for (int i = 0; i < 8; ++i) m_buf.push_back(std::uint8_t((v >> (8 * i)) & 0xff));
  }
  void f64(double v)
  {
    std::uint64_t bits = 0;
    static_assert(sizeof(double) == 8);
    std::memcpy(&bits, &v, 8);
    u64(bits);
  }
  void WriteGuid(const Guid& g)
  {
    for (std::uint8_t b : g.Bytes()) m_buf.push_back(b);
  }
  void str(const std::string& s)
  {
    u32(static_cast<std::uint32_t>(s.size()));
    m_buf.insert(m_buf.end(), s.begin(), s.end());
  }
  void bytes(const std::uint8_t* p, std::size_t n)
  {
    m_buf.insert(m_buf.end(), p, p + n);
  }

  [[nodiscard]] const std::vector<std::uint8_t>& data() const
  {
      return m_buf; 
  }

 private:
  std::vector<std::uint8_t> m_buf;
};

class BinReader
{
 public:
  explicit BinReader(std::vector<std::uint8_t> data) : m_buf(std::move(data))
 {
  }

  [[nodiscard]] bool Ok() const noexcept
 {
      return m_ok; 
  }
  [[nodiscard]] std::string Error() const
  {
      return m_error; 
  }

  std::uint8_t u8()
  {
    if (m_pos >= m_buf.size()) return fail_u8();
    return m_buf[m_pos++];
  }
  std::uint32_t u32()
  {
    if (m_pos + 4 > m_buf.size()) return fail_u32();
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) v |= std::uint32_t(m_buf[m_pos++]) << (8 * i);
    return v;
  }
  std::uint64_t u64()
  {
    if (m_pos + 8 > m_buf.size()) return fail_u64();
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= std::uint64_t(m_buf[m_pos++]) << (8 * i);
    return v;
  }
  double f64()
  {
    const std::uint64_t bits = u64();
    double v = 0;
    std::memcpy(&v, &bits, 8);
    return v;
  }
  Guid ReadGuid()
  {
    if (m_pos + 16 > m_buf.size())
    {
      fail("truncated guid");
      return Guid::Nil();
    }
    std::array<std::uint8_t, 16> bytes{};
    for (int i = 0; i < 16; ++i) bytes[static_cast<std::size_t>(i)] = m_buf[m_pos++];
    return Guid::FromBytes(bytes);
  }
  std::string str()
  {
    const std::uint32_t n = u32();
    if (!m_ok || m_pos + n > m_buf.size())
    {
      fail("truncated string");
      return {};
    }
    std::string s(reinterpret_cast<const char*>(m_buf.data() + m_pos), n);
    m_pos += n;
    return s;
  }

  [[nodiscard]] bool finished() const noexcept
  {
      return m_pos == m_buf.size(); 
  }

 private:
  void fail(const char* msg)
 {
    m_ok = false;
    if (m_error.empty()) m_error = msg;
  }
  std::uint8_t fail_u8()
  {
    fail("truncated u8");
    return 0;
  }
  std::uint32_t fail_u32()
  {
    fail("truncated u32");
    return 0;
  }
  std::uint64_t fail_u64()
  {
    fail("truncated u64");
    return 0;
  }

  std::vector<std::uint8_t> m_buf;
  std::size_t m_pos{0};
  bool m_ok{true};
  std::string m_error;
};

enum class FeatType : std::uint8_t
{
  Box = 1,
  Sketch = 2,
  Extrude = 3,
  Sphere = 4,
  Boolean = 5
};

void write_plane(BinWriter& w, const Plane& p)
{
  w.f64(p.Origin.x());
  w.f64(p.Origin.y());
  w.f64(p.Origin.z());
  w.f64(p.Normal.x());
  w.f64(p.Normal.y());
  w.f64(p.Normal.z());
  w.f64(p.UAxis.x());
  w.f64(p.UAxis.y());
  w.f64(p.UAxis.z());
  w.f64(p.VAxis.x());
  w.f64(p.VAxis.y());
  w.f64(p.VAxis.z());
}

Plane read_plane(BinReader& r)
{
  Plane p;
  p.Origin = Point3d{r.f64(), r.f64(), r.f64()};
  p.Normal = Vector3d{r.f64(), r.f64(), r.f64()};
  p.UAxis = Vector3d{r.f64(), r.f64(), r.f64()};
  p.VAxis = Vector3d{r.f64(), r.f64(), r.f64()};
  return p;
}

void write_feature(BinWriter& w, const feat::IFeature& f)
{
  if (f.TypeName() == "Box")
  {
    const auto& box = static_cast<const feat::BoxFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Box));
    w.WriteGuid(box.Id().Guid);
    w.str(box.DisplayName());
    w.u8(box.Suppressed() ? 1 : 0);
    w.f64(box.Origin().x());
    w.f64(box.Origin().y());
    w.f64(box.Origin().z());
    w.WriteGuid(box.LengthId().Guid);
    w.WriteGuid(box.WidthId().Guid);
    w.WriteGuid(box.HeightId().Guid);
    w.WriteGuid(box.BodyGuid());
    return;
  }
  if (f.TypeName() == "Sphere")
  {
    const auto& sph = static_cast<const feat::SphereFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Sphere));
    w.WriteGuid(sph.Id().Guid);
    w.str(sph.DisplayName());
    w.u8(sph.Suppressed() ? 1 : 0);
    w.f64(sph.Center().x());
    w.f64(sph.Center().y());
    w.f64(sph.Center().z());
    w.WriteGuid(sph.RadiusId().Guid);
    w.WriteGuid(sph.BodyGuid());
    return;
  }
  if (f.TypeName() == "Sketch")
  {
    const auto& skf = static_cast<const feat::SketchFeature&>(f);
    const auto& sk = skf.Sketch();
    w.u8(static_cast<std::uint8_t>(FeatType::Sketch));
    w.WriteGuid(skf.Id().Guid);
    w.str(skf.DisplayName());
    w.u8(skf.Suppressed() ? 1 : 0);
    write_plane(w, sk.Frame());
    w.u32(static_cast<std::uint32_t>(sk.Points().size()));
    for (const auto& pt : sk.Points())
    {
      w.f64(pt.P.u());
      w.f64(pt.P.v());
      w.u8(pt.Fixed ? 1 : 0);
    }
    w.u32(static_cast<std::uint32_t>(sk.Lines().size()));
    for (const auto& ln : sk.Lines())
    {
      w.u32(ln.P0.Value);
      w.u32(ln.P1.Value);
    }
    w.u32(static_cast<std::uint32_t>(sk.Circles().size()));
    for (const auto& c : sk.Circles())
    {
      w.u32(c.Center.Value);
      w.f64(c.Radius);
    }
    w.u32(static_cast<std::uint32_t>(sk.Constraints().size()));
    for (const auto& c : sk.Constraints())
    {
      w.u32(c.Id.Value);
      w.u8(static_cast<std::uint8_t>(c.Kind));
      w.u32(c.A.Value);
      w.u32(c.B.Value);
      w.WriteGuid(c.Dim.Guid);
      w.f64(c.Aux);
    }
    w.u32(sk.NextEntity());
    w.u32(sk.NextConstraint());
    return;
  }
  if (f.TypeName() == "Extrude")
  {
    const auto& ex = static_cast<const feat::ExtrudeFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Extrude));
    w.WriteGuid(ex.Id().Guid);
    w.str(ex.DisplayName());
    w.u8(ex.Suppressed() ? 1 : 0);
    w.WriteGuid(ex.SketchFeatureId().Guid);
    w.WriteGuid(ex.DistanceId().Guid);
    w.WriteGuid(ex.BodyGuid());
    return;
  }
  if (f.TypeName() == "Boolean")
  {
    const auto& b = static_cast<const feat::BooleanFeature&>(f);
    w.u8(static_cast<std::uint8_t>(FeatType::Boolean));
    w.WriteGuid(b.Id().Guid);
    w.str(b.DisplayName());
    w.u8(b.Suppressed() ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(b.Op()));
    w.WriteGuid(b.TargetFeatureId().Guid);
    w.WriteGuid(b.ToolFeatureId().Guid);
    w.WriteGuid(b.BodyGuid());
    return;
  }
  BREP_WARN("xl save: skipping unknown feature type '{}'", f.TypeName());
}

bool read_feature(BinReader& r, Part& part)
{
  const auto type = static_cast<FeatType>(r.u8());
  if (!r.Ok()) return false;

  if (type == FeatType::Box)
  {
    const Guid fid = r.ReadGuid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const Point3d Origin{r.f64(), r.f64(), r.f64()};
    const param::ParameterId length{r.ReadGuid()};
    const param::ParameterId width{r.ReadGuid()};
    const param::ParameterId height{r.ReadGuid()};
    const Guid body = r.ReadGuid();
    if (!r.Ok()) return false;
    auto feature = std::make_unique<feat::BoxFeature>(
        feat::FeatureId{fid}, name, Origin, length, width, height);
    feature->SetBodyGuid(body);
    feature->SetSuppressed(suppressed);
    feature->SetStatus(feat::FeatureStatus::Dirty);
    part.Features().Append(std::move(feature));
    return true;
  }

  if (type == FeatType::Sphere)
  {
    const Guid fid = r.ReadGuid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const Point3d Center{r.f64(), r.f64(), r.f64()};
    const param::ParameterId radius{r.ReadGuid()};
    const Guid body = r.ReadGuid();
    if (!r.Ok()) return false;
    auto feature = std::make_unique<feat::SphereFeature>(
        feat::FeatureId{fid}, name, Center, radius);
    feature->SetBodyGuid(body);
    feature->SetSuppressed(suppressed);
    feature->SetStatus(feat::FeatureStatus::Dirty);
    part.Features().Append(std::move(feature));
    return true;
  }

  if (type == FeatType::Sketch)
  {
    const Guid fid = r.ReadGuid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    Plane frame = read_plane(r);
    const std::uint32_t np = r.u32();
    std::vector<sketch::SketchPoint> points;
    points.reserve(np);
    for (std::uint32_t i = 0; i < np; ++i)
    {
      sketch::SketchPoint pt;
      pt.P = Point2d{r.f64(), r.f64()};
      pt.Fixed = r.u8() != 0;
      points.push_back(pt);
    }
    const std::uint32_t nl = r.u32();
    std::vector<sketch::SketchLine> lines;
    lines.reserve(nl);
    for (std::uint32_t i = 0; i < nl; ++i)
    {
      lines.push_back(sketch::SketchLine{sketch::SketchEntityId{r.u32()},
                                         sketch::SketchEntityId{r.u32()}});
    }
    const std::uint32_t nc = r.u32();
    std::vector<sketch::SketchCircle> circles;
    circles.reserve(nc);
    for (std::uint32_t i = 0; i < nc; ++i)
    {
      circles.push_back(
          sketch::SketchCircle{sketch::SketchEntityId{r.u32()}, r.f64()});
    }
    const std::uint32_t ncons = r.u32();
    std::vector<sketch::Constraint> cons;
    cons.reserve(ncons);
    for (std::uint32_t i = 0; i < ncons; ++i)
    {
      sketch::Constraint c;
      c.Id.Value = r.u32();
      c.Kind = static_cast<sketch::ConstraintKind>(r.u8());
      c.A.Value = r.u32();
      c.B.Value = r.u32();
      c.Dim.Guid = r.ReadGuid();
      c.Aux = r.f64();
      cons.push_back(c);
    }
    const std::uint32_t next_e = r.u32();
    const std::uint32_t next_c = r.u32();
    if (!r.Ok()) return false;
    sketch::Sketch sk;
    sk.Assign(frame, std::move(points), std::move(lines), std::move(circles),
              std::move(cons), next_e, next_c);
    auto feature = std::make_unique<feat::SketchFeature>(feat::FeatureId{fid},
                                                         name, std::move(sk));
    feature->SetSuppressed(suppressed);
    feature->SetStatus(feat::FeatureStatus::Dirty);
    part.Features().Append(std::move(feature));
    return true;
  }

  if (type == FeatType::Extrude)
  {
    const Guid fid = r.ReadGuid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const feat::FeatureId sketch_id{r.ReadGuid()};
    const param::ParameterId dist{r.ReadGuid()};
    const Guid body = r.ReadGuid();
    if (!r.Ok()) return false;
    auto feature = std::make_unique<feat::ExtrudeFeature>(
        feat::FeatureId{fid}, name, sketch_id, dist);
    feature->SetBodyGuid(body);
    feature->SetSuppressed(suppressed);
    feature->SetStatus(feat::FeatureStatus::Dirty);
    part.Features().Append(std::move(feature));
    return true;
  }

  if (type == FeatType::Boolean)
  {
    const Guid fid = r.ReadGuid();
    const std::string name = r.str();
    const bool suppressed = r.u8() != 0;
    const auto op = static_cast<boolean::BooleanOp>(r.u8());
    const feat::FeatureId target{r.ReadGuid()};
    const feat::FeatureId tool{r.ReadGuid()};
    const Guid body = r.ReadGuid();
    if (!r.Ok()) return false;
    auto feature = std::make_unique<feat::BooleanFeature>(
        feat::FeatureId{fid}, name, op, target, tool);
    feature->SetBodyGuid(body);
    feature->SetSuppressed(suppressed);
    feature->SetStatus(feat::FeatureStatus::Dirty);
    part.Features().Append(std::move(feature));
    return true;
  }

  set_error("unknown feature type in .xl file");
  return false;
}

void write_xform(BinWriter& w, const RigidTransform& t)
{
  w.f64(t.Translation.x());
  w.f64(t.Translation.y());
  w.f64(t.Translation.z());
  w.f64(t.XAxis.x());
  w.f64(t.XAxis.y());
  w.f64(t.XAxis.z());
  w.f64(t.YAxis.x());
  w.f64(t.YAxis.y());
  w.f64(t.YAxis.z());
  w.f64(t.ZAxis.x());
  w.f64(t.ZAxis.y());
  w.f64(t.ZAxis.z());
}

RigidTransform read_xform(BinReader& r)
{
  RigidTransform t;
  t.Translation = Point3d{r.f64(), r.f64(), r.f64()};
  t.XAxis = Vector3d{r.f64(), r.f64(), r.f64()};
  t.YAxis = Vector3d{r.f64(), r.f64(), r.f64()};
  t.ZAxis = Vector3d{r.f64(), r.f64(), r.f64()};
  return t;
}

void write_topo_ref(BinWriter& w, const naming::TopologyRef& ref)
{
  w.WriteGuid(ref.Feature.Guid);
  w.str(ref.LocalName);
}

naming::TopologyRef read_topo_ref(BinReader& r)
{
  naming::TopologyRef ref;
  ref.Feature.Guid = r.ReadGuid();
  ref.LocalName = r.str();
  return ref;
}

void write_assembly(BinWriter& w, const asm_::Assembly& assembly)
{
  w.u32(static_cast<std::uint32_t>(assembly.Occurrences().size()));
  for (const auto& occ : assembly.Occurrences())
  {
    w.WriteGuid(occ.Id.Guid);
    w.WriteGuid(occ.PartGuid);
    w.str(occ.Name);
    write_xform(w, occ.Transform);
  }
  w.u32(static_cast<std::uint32_t>(assembly.Mates().size()));
  for (const auto& mate : assembly.Mates())
  {
    w.WriteGuid(mate.Id);
    w.u8(static_cast<std::uint8_t>(mate.Kind));
    w.WriteGuid(mate.A.Guid);
    w.WriteGuid(mate.B.Guid);
    write_topo_ref(w, mate.FaceRefA);
    write_topo_ref(w, mate.FaceRefB);
    w.WriteGuid(mate.Dim.Guid);
    w.f64(mate.Aux);
  }
}

bool read_assembly(BinReader& r, asm_::Assembly& assembly, std::string& err)
{
  const std::uint32_t noc = r.u32();
  for (std::uint32_t i = 0; i < noc; ++i)
  {
    asm_::Occurrence occ;
    occ.Id.Guid = r.ReadGuid();
    occ.PartGuid = r.ReadGuid();
    occ.Name = r.str();
    occ.Transform = read_xform(r);
    if (!r.Ok())
    {
      err = r.Error();
      return false;
    }
    assembly.Occurrences().push_back(std::move(occ));
  }
  const std::uint32_t nm = r.u32();
  for (std::uint32_t i = 0; i < nm; ++i)
  {
    asm_::Mate mate;
    mate.Id = r.ReadGuid();
    mate.Kind = static_cast<asm_::MateKind>(r.u8());
    mate.A.Guid = r.ReadGuid();
    mate.B.Guid = r.ReadGuid();
    mate.FaceRefA = read_topo_ref(r);
    mate.FaceRefB = read_topo_ref(r);
    mate.Dim.Guid = r.ReadGuid();
    mate.Aux = r.f64();
    if (!r.Ok())
    {
      err = r.Error();
      return false;
    }
    assembly.Mates().push_back(std::move(mate));
  }
  return true;
}

std::vector<std::uint8_t> encode_payload(const Document& doc)
{
  BinWriter w;
  w.WriteGuid(doc.Guid);
  w.str(doc.Name);
  w.u32(static_cast<std::uint32_t>(doc.Parts().size()));
  for (const auto& part_ptr : doc.Parts())
  {
    const Part& part = *part_ptr;
    w.WriteGuid(part.Guid);
    w.str(part.Name);

    const auto& params = part.Parameters().All();
    w.u32(static_cast<std::uint32_t>(params.size()));
    for (const auto& p : params)
    {
      w.WriteGuid(p.Id.Guid);
      w.str(p.Name);
      w.u8(static_cast<std::uint8_t>(p.Kind));
      w.f64(p.Value);
      w.u8(p.UserDriven ? 1 : 0);
    }

    const auto& features = part.Features().Features();
    w.u32(static_cast<std::uint32_t>(features.size()));
    for (const auto& f : features)
    {
      if (f) write_feature(w, *f);
    }
  }
  // schema v2+: assembly / mates / topology refs
  write_assembly(w, doc.Assembly());
  return w.data();
}

std::unique_ptr<Document> decode_payload(const std::vector<std::uint8_t>& payload,
                                         std::uint32_t schema, std::string& err)
{
  BinReader r(payload);
  const Guid doc_guid = r.ReadGuid();
  const std::string doc_name = r.str();
  const std::uint32_t part_count = r.u32();
  if (!r.Ok())
  {
    err = r.Error();
    return nullptr;
  }

  auto doc = Document::Create(doc_name);
  // Re-register under loaded Guid.
  doc->Registry().Remove(doc->Guid);
  doc->Guid = doc_guid;
  doc->Registry().Add(*doc);

  for (std::uint32_t pi = 0; pi < part_count; ++pi)
  {
    const Guid partGuid = r.ReadGuid();
    const std::string part_name = r.str();
    Part& part = doc->AddPart(part_name);
    doc->Registry().Remove(part.Guid);
    part.Guid = partGuid;
    doc->Registry().Add(part);

    const std::uint32_t pc = r.u32();
    for (std::uint32_t i = 0; i < pc; ++i)
    {
      const Guid pid = r.ReadGuid();
      const std::string pname = r.str();
      const auto kind = static_cast<param::ParamKind>(r.u8());
      const double Value = r.f64();
      const bool UserDriven = r.u8() != 0;
      if (!r.Ok())
      {
        err = r.Error();
        return nullptr;
      }
      if (!part.Parameters().AddWithId(param::ParameterId{pid}, pname, kind,
                                        Value, UserDriven))
      {
        err = "duplicate or invalid parameter Guid";
        return nullptr;
      }
    }

    const std::uint32_t fc = r.u32();
    for (std::uint32_t i = 0; i < fc; ++i)
    {
      if (!read_feature(r, part))
    {
        err = g_last_error.empty() ? r.Error() : g_last_error;
        return nullptr;
      }
    }

    part.Features().MarkAllDirty();
    const auto regen = part.Regenerate();
    if (!regen.Ok)
    {
      err = "regenerate failed after load: " + regen.Message;
      return nullptr;
    }
  }

  if (schema >= 2)
  {
    if (!read_assembly(r, doc->Assembly(), err)) return nullptr;
    // Re-apply mates so transforms match solved state.
    param::ParameterStore* params = nullptr;
    if (Part* main = doc->MainPart()) params = &main->Parameters();
    asm_::MateSolver::Solve(doc->Assembly(), params);
  }

  if (!r.Ok())
  {
    err = r.Error();
    return nullptr;
  }
  if (!r.finished())
  {
    err = "trailing bytes in .xl payload";
    return nullptr;
  }

  doc->MarkClean();
  return doc;
}

}  // namespace

const std::string& LastXlError()
{
    return g_last_error; 
}

XlSaveResult SaveXl(const Document& doc, const std::filesystem::path& path)
{
  XlSaveResult result;
  try {
    const auto payload = encode_payload(doc);
    const std::uint32_t sum = crc32(payload);

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
      result.Error = "cannot open file for writing";
      set_error(result.Error);
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
    if (!out)
    {
      result.Error = "write failed";
      set_error(result.Error);
      return result;
    }

    // Optional mesh sidecar for faster viewer open.
    const auto cache = SaveBksCache(doc, path);
    if (!cache.Ok)
    {
      BREP_WARN("xl: mesh cache not written: {}", cache.Error);
    }

    result.Ok = true;
    g_last_error.clear();
    BREP_INFO("saved .xl '{}' ({} bytes payload, schema={})", path.string(),
              payload.size(), kXlSchemaVersion);
    return result;
  } catch (const std::exception& ex)
  {
    result.Error = ex.what();
    set_error(result.Error);
    return result;
  }
}

XlLoadResult LoadXl(const std::filesystem::path& path)
{
  XlLoadResult result;
  try {
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
      result.Error = "cannot open file for reading";
      set_error(result.Error);
      return result;
    }

    std::vector<std::uint8_t> file_bytes(
        (std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (file_bytes.size() < 4 + 4 + 4 + 8 + 4)
    {
      result.Error = "file too small";
      set_error(result.Error);
      return result;
    }
    BinReader hdr(std::move(file_bytes));
    const char m0 = static_cast<char>(hdr.u8());
    const char m1 = static_cast<char>(hdr.u8());
    const char m2 = static_cast<char>(hdr.u8());
    const char m3 = static_cast<char>(hdr.u8());
    if (!hdr.Ok() || m0 != 'X' || m1 != 'L' || m2 != '0' || m3 != '1')
    {
      result.Error = "not an XL01 document";
      set_error(result.Error);
      return result;
    }
    const std::uint32_t schema = hdr.u32();
    const std::uint32_t flags = hdr.u32();
    const std::uint64_t payload_size = hdr.u64();
    (void)flags;
    if (!hdr.Ok())
    {
      result.Error = "truncated header";
      set_error(result.Error);
      return result;
    }
    if (schema < kXlSchemaVersionMin || schema > kXlSchemaVersion)
    {
      result.Error = "unsupported schema_version " + std::to_string(schema);
      set_error(result.Error);
      return result;
    }
    if (payload_size > 64ull * 1024ull * 1024ull)
    {
      result.Error = "payload too large";
      set_error(result.Error);
      return result;
    }

    std::vector<std::uint8_t> payload(static_cast<std::size_t>(payload_size));
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = hdr.u8();
    const std::uint32_t file_crc = hdr.u32();
    if (!hdr.Ok())
    {
      result.Error = "truncated payload/crc";
      set_error(result.Error);
      return result;
    }

    const std::uint32_t calc = crc32(payload);
    if (calc != file_crc)
    {
      result.Error = "CRC mismatch (file corrupted or tampered)";
      set_error(result.Error);
      return result;
    }

    std::string err;
    result.document = decode_payload(payload, schema, err);
    if (!result.document)
    {
      result.Error = err.empty() ? "decode failed" : err;
      set_error(result.Error);
      return result;
    }

    result.document->SetPath(path.string());
    g_last_error.clear();
    BREP_INFO("loaded .xl '{}' parts={} occurrences={} schema={}",
              path.string(), result.document->Parts().size(),
              result.document->Assembly().Occurrences().size(), schema);
    return result;
  } catch (const std::exception& ex)
  {
    result.Error = ex.what();
    set_error(result.Error);
    return result;
  }
}

}  // namespace brep::io
