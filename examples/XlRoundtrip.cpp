#include "brep/Brep.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
  using namespace brep;
  namespace fs = std::filesystem;

  auto doc = Document::Create("xl_roundtrip");
  Part& part = doc->AddPart("MainPart");

  // Box + params
  Body* body = part.AddBox(BoxSpec{
      .Min = Point3d{0, 0, 0},
      .Max = Point3d{2, 1, 3},
      .Name = "box",
  });
  if (!body) return 1;
  const Guid body_guid = body->guid;
  auto* feature = part.features().find_by_body(body_guid);
  if (!feature) return 1;
  part.edit_feature_params(feature->id(),
                           {{"Length", 4.0}, {"Width", 5.0}, {"Height", 2.5}});

  // V1.1: Sketch + Extrude
  const auto sk =
      part.add_rectangle_sketch("BaseSketch", Point2d{0, 0}, Point2d{1.5, 1.0});
  Body* pad = part.add_extrude(sk, 0.75, "Pad");
  if (!pad)
  {
    std::cerr << "extrude failed\n";
    return 1;
  }
  const Guid pad_guid = pad->guid;

  // V2: Assembly / Mate / TopologyRef
  const auto o1 = doc->assembly().add_occurrence(part.guid, {}, "occ1");
  RigidTransform xf;
  xf.Translation = Point3d{0, 0, 0};
  const auto o2 = doc->assembly().add_occurrence(part.guid, xf, "occ2");
  asm_::Mate mate;
  mate.kind = asm_::MateKind::Distance;
  mate.a = o1;
  mate.b = o2;
  mate.aux = 3.0;
  mate.face_ref_a = naming::TopologyRef{feature->id(), "end_face"};
  mate.face_ref_b = naming::TopologyRef{sk, "profile"};
  doc->assembly().add_mate(mate);
  asm_::MateSolver::solve(doc->assembly(), &part.parameters());

  const fs::path path = fs::temp_directory_path() / "brep_xl_roundtrip.xl";
  auto saved = io::SaveXl(*doc, path);
  if (!saved.Ok)
  {
    std::cerr << "save failed: " << saved.Error << "\n";
    return 1;
  }
  if (!fs::exists(io::BksCachePathFor(path)))
  {
    std::cerr << "expected .bks.cache sidecar\n";
    return 1;
  }

  auto loaded = io::LoadXl(path);
  if (!loaded.Ok())
  {
    std::cerr << "load failed: " << loaded.Error << "\n";
    return 1;
  }

  Part* p2 = loaded.document->MainPart();
  if (!p2) return 1;
  if (!p2->find_body(body_guid) || !p2->find_body(pad_guid))
  {
    std::cerr << "body guids not restored\n";
    return 1;
  }

  bool has_sketch = false;
  bool has_extrude = false;
  for (const auto& f : p2->features().features())
  {
    if (f->type_name() == "Sketch") has_sketch = true;
    if (f->type_name() == "Extrude") has_extrude = true;
  }
  if (!has_sketch || !has_extrude)
  {
    std::cerr << "sketch/extrude missing after load\n";
    return 1;
  }

  if (loaded.document->assembly().occurrences().size() != 2 ||
      loaded.document->assembly().mates().size() != 1)
  {
    std::cerr << "assembly not restored\n";
    return 1;
  }
  const auto& mate2 = loaded.document->assembly().mates().front();
  if (mate2.face_ref_a.local_name != "end_face" ||
      std::abs(mate2.aux - 3.0) > 1e-9)
  {
    std::cerr << "mate/topology ref mismatch\n";
    return 1;
  }

  auto cache = io::LoadBksCache(path, loaded.document->guid);
  if (!cache.Ok || !cache.Cache.Has(body_guid) || !cache.Cache.Has(pad_guid))
  {
    std::cerr << "cache load failed: " << cache.Error << "\n";
    return 1;
  }

  // Tamper CRC and expect failure.
  {
    std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
    f.seekg(-1, std::ios::end);
    char c = 0;
    f.read(&c, 1);
    c ^= 0x5A;
    f.seekp(-1, std::ios::end);
    f.write(&c, 1);
  }
  auto bad = io::LoadXl(path);
  if (bad.Ok())
  {
    std::cerr << "tampered file should fail CRC\n";
    return 1;
  }

  fs::remove(path);
  fs::remove(io::BksCachePathFor(path));
  std::cout << "xl_roundtrip ok (v1.1 sketch/extrude + v2 assembly + cache)\n";
  return 0;
}
