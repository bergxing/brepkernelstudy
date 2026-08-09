#pragma once

#include "brep/guid.hpp"
#include "brep/naming/topology_ref.hpp"
#include "brep/param/parameter.hpp"
#include "brep/plane.hpp"
#include "brep/solve2d/solver.hpp"

#include <string>
#include <vector>

namespace brep::asm_ {

struct OccurrenceId {
  Guid guid{};
  [[nodiscard]] friend bool operator==(const OccurrenceId& a,
                                       const OccurrenceId& b) noexcept {
    return a.guid == b.guid;
  }
};

struct Occurrence {
  OccurrenceId id{};
  Guid part_guid{};
  RigidTransform transform{};
  std::string name;
};

enum class MateKind { Coincident, Concentric, Distance, Angle, Parallel };

struct Mate {
  Guid id{};
  MateKind kind{MateKind::Coincident};
  OccurrenceId a{};
  OccurrenceId b{};
  naming::TopologyRef face_ref_a{};
  naming::TopologyRef face_ref_b{};
  param::ParameterId dim{};
  double aux{0.0};
};

class Assembly {
 public:
  OccurrenceId add_occurrence(Guid part_guid, RigidTransform xf = {},
                              std::string name = {});
  Guid add_mate(Mate mate);
  [[nodiscard]] std::vector<Occurrence>& occurrences() noexcept {
    return occurrences_;
  }
  [[nodiscard]] const std::vector<Occurrence>& occurrences() const noexcept {
    return occurrences_;
  }
  [[nodiscard]] std::vector<Mate>& mates() noexcept { return mates_; }
  [[nodiscard]] const std::vector<Mate>& mates() const noexcept {
    return mates_;
  }

  Occurrence* find(OccurrenceId id);
  const Occurrence* find(OccurrenceId id) const;

 private:
  std::vector<Occurrence> occurrences_;
  std::vector<Mate> mates_;
};

class MateSolver {
 public:
  /// Solve mates; updates Occurrence transforms. Distance mates read params.
  static solve2d::SolveReport solve(Assembly& assembly,
                                    param::ParameterStore* params);
};

}  // namespace brep::asm_
