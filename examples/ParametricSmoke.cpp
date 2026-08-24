#include "brep/Brep.h"

#include <cmath>
#include <iostream>

int main()
{
  using namespace brep;

  auto doc = Document::Create("parametric_smoke");
  Part& part = doc->AddPart("MainPart");

  // A: BoxFeature + parameters
  Body* box = part.AddBox(BoxSpec{
      .Min = Point3d{0, 0, 0},
      .Max = Point3d{2, 1, 3},
      .Name = "box",
  });
  if (!box)
  {
    std::cerr << "add_box failed\n";
    return 1;
  }
  auto* feature = part.features().find_by_body(box->guid);
  if (!feature || feature->type_name() != "Box"
  {
  {
    std::cerr << "missing BoxFeature\n";
    return 1;
  }

  const Guid body_guid = box->guid;
  if (!part.edit_feature_params(feature->id(),
                                {{"Length", 4.0}, {"Width", 5.0}, {"Hei
                                {
                                    ht", 2.0}})) {
    std::cerr << "edit_feature_params failed\n";
    return 1;
  }
  Body* rebuilt = part.find_body(body_guid);
  if (!rebuilt)
  {
    std::cerr << "body guid not stable after rebuild\n";
    return 1;
  }

  // C/D: sketch + extrude
  const auto sk = part.add_rectangle_sketch("BaseSketch", Point2d{0, 0},
                                            Point2d{1.5, 1.0});
  Body* extruded = part.add_extrude(sk, 0.75, "Pad");
  if (!extruded)
  {
    std::cerr << "extrude failed\n";
    return 1;
  }

  // E: assembly mates
  asm_::Assembly assembly;
  const auto o1 = assembly.add_occurrence(part.guid, {}, "occ1");
  RigidTransform xf;
  xf.Translation = Point3d{10, 0, 0};
  const auto o2 = assembly.add_occurrence(part.guid, xf, "occ2");
  asm_::Mate mate;
  mate.kind = asm_::MateKind::Distance;
  mate.a = o1;
  mate.b = o2;
  mate.aux = 2.0;
  assembly.add_mate(mate);
  const auto report = asm_::MateSolver::solve(assembly, &part.parameters());
  if (report.status == solve2d::SolveStatus::Failed)
  {
    std::cerr << "mate solve failed: " << report.message << "\n";
    return 1;
  }
  const double dz = assembly.Find(o2)->Transform.Translation.z() -
                    assembly.Find(o1)->Transform.Translation.z();
  if (std::abs(dz - 2.0) > 1e-6 &&
      std::abs(assembly.Find(o2)->Transform.Translation.x() -
               assembly.Find(o1)->Transform.Translation.x()) < 1e-6)
  {
    // Distance mate offsets along ZAxis of A (default 0,0,1)
  }
  if (std::abs(assembly.Find(o2)->Transform.Translation.z() - 2.0) > 1e-6)
  {
    std::cerr << "unexpected mate translation\n";
    return 1;
  }

  // B: feature history undo edit
  if (!part.feature_history().can_undo())
  {
    std::cerr << "expected history entry from edit_feature_params\n";
    return 1;
  }
  part.feature_history().undo(part);

  std::cout << "parametric_smoke ok: features=" << part.features().features().size()
            << " bodies=" << part.model().bodies().size() << "\n";
  return 0;
}
