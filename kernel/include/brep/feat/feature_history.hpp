#pragma once

#include "brep/builder.hpp"
#include "brep/feat/feature.hpp"
#include "brep/math.hpp"
#include "brep/param/parameter.hpp"

#include <string>
#include <utility>
#include <vector>

namespace brep {
class Part;
}

namespace brep::feat {

enum class TxKind {
  AppendFeature,
  RemoveFeature,
  EditParameters,
  SuppressFeature,
  UnsuppressFeature,
};

struct FeatureTransaction {
  TxKind kind{TxKind::AppendFeature};
  FeatureId feature{};
  std::string feature_type;  // "Box", "Sketch", "Extrude"
  BoxSpec box_spec{};
  Point3d box_origin{};
  std::string sketch_name;
  FeatureId sketch_feature{};  // for Extrude upstream
  double extrude_distance{1.0};
  std::vector<std::pair<param::ParameterId, double>> param_before;
  std::vector<std::pair<param::ParameterId, double>> param_after;
  bool was_suppressed{false};
};

class FeatureHistory {
 public:
  void record(FeatureTransaction tx);
  void apply_and_record(Part& part, FeatureTransaction tx);

  bool undo(Part& part);
  bool redo(Part& part);

  void clear();
  [[nodiscard]] bool can_undo() const noexcept { return index_ > 0; }
  [[nodiscard]] bool can_redo() const noexcept {
    return index_ < static_cast<int>(entries_.size());
  }

 private:
  bool apply_forward(Part& part, FeatureTransaction& tx);
  bool apply_reverse(Part& part, FeatureTransaction& tx);

  std::vector<FeatureTransaction> entries_;
  int index_{0};
};

}  // namespace brep::feat
