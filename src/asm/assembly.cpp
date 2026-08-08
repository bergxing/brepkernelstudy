#include "brep/asm/assembly.hpp"

#include <cmath>

namespace brep::asm_ {

OccurrenceId Assembly::add_occurrence(Guid part_guid, RigidTransform xf,
                                      std::string name) {
  Occurrence occ;
  occ.id.guid = Guid::generate();
  occ.part_guid = part_guid;
  occ.transform = xf;
  occ.name = std::move(name);
  const OccurrenceId id = occ.id;
  occurrences_.push_back(std::move(occ));
  return id;
}

Guid Assembly::add_mate(Mate mate) {
  if (mate.id.is_nil()) mate.id = Guid::generate();
  const Guid id = mate.id;
  mates_.push_back(std::move(mate));
  return id;
}

Occurrence* Assembly::find(OccurrenceId id) {
  for (auto& o : occurrences_) {
    if (o.id == id) return &o;
  }
  return nullptr;
}

const Occurrence* Assembly::find(OccurrenceId id) const {
  for (const auto& o : occurrences_) {
    if (o.id == id) return &o;
  }
  return nullptr;
}

solve2d::SolveReport MateSolver::solve(Assembly& assembly,
                                       param::ParameterStore* params) {
  solve2d::SolveReport report;
  report.status = solve2d::SolveStatus::Solved;
  report.message = "ok";

  // V1: treat first occurrence as fixed ground; apply mates by copying /
  // offsetting translation of the second occurrence.
  if (assembly.occurrences().empty()) {
    report.status = solve2d::SolveStatus::Solved;
    return report;
  }

  for (const auto& mate : assembly.mates()) {
    Occurrence* a = assembly.find(mate.a);
    Occurrence* b = assembly.find(mate.b);
    if (!a || !b) {
      report.status = solve2d::SolveStatus::Failed;
      report.message = "mate references missing occurrence";
      return report;
    }

    switch (mate.kind) {
      case MateKind::Coincident:
      case MateKind::Parallel:
        // Align B origin to A origin (simplified face mate).
        b->transform.translation = a->transform.translation;
        b->transform.x_axis = a->transform.x_axis;
        b->transform.y_axis = a->transform.y_axis;
        b->transform.z_axis = a->transform.z_axis;
        break;
      case MateKind::Distance: {
        double dist = mate.aux;
        if (params && !mate.dim.guid.is_nil()) {
          if (auto v = params->get(mate.dim)) dist = *v;
        }
        b->transform.translation =
            a->transform.translation + a->transform.z_axis * dist;
        break;
      }
      case MateKind::Angle:
        // Rotate B around A.z by aux radians (simple).
        {
          const double ang = mate.aux;
          const double c = std::cos(ang);
          const double s = std::sin(ang);
          const Vector3d x = a->transform.x_axis;
          const Vector3d y = a->transform.y_axis;
          b->transform.x_axis = x * c + y * s;
          b->transform.y_axis = y * c - x * s;
          b->transform.z_axis = a->transform.z_axis;
          b->transform.translation = a->transform.translation;
        }
        break;
      case MateKind::Concentric:
        b->transform.translation = a->transform.translation;
        b->transform.z_axis = a->transform.z_axis;
        break;
    }
  }

  report.dof = static_cast<int>(assembly.occurrences().size());
  return report;
}

}  // namespace brep::asm_
