#pragma once

#include "brep/feat/feature.hpp"
#include "brep/math.hpp"
#include "brep/plane.hpp"
#include "brep/sketch/sketch.hpp"

#include <memory>
#include <string>

namespace brep::feat {

class SketchFeature final : public IFeature {
 public:
  SketchFeature(FeatureId id, std::string name, sketch::Sketch sketch);

  [[nodiscard]] FeatureId id() const override { return id_; }
  [[nodiscard]] std::string_view type_name() const override { return "Sketch"; }
  [[nodiscard]] FeatureStatus status() const override { return status_; }
  void set_status(FeatureStatus s) override { status_ = s; }
  void set_suppressed(bool suppressed) override { suppressed_ = suppressed; }
  [[nodiscard]] bool suppressed() const override { return suppressed_; }

  void collect_parameters(param::ParameterStore& store) override;
  bool rebuild(Part& part, param::ParameterStore& params) override;

  [[nodiscard]] Guid body_guid() const override { return {}; }
  void set_body_guid(Guid) override {}
  [[nodiscard]] std::string display_name() const override { return name_; }

  [[nodiscard]] sketch::Sketch& sketch() noexcept { return sketch_; }
  [[nodiscard]] const sketch::Sketch& sketch() const noexcept { return sketch_; }

  static std::unique_ptr<SketchFeature> create_rectangle(
      param::ParameterStore& store, std::string name, Point2d min, Point2d max,
      brep::Plane frame = brep::Plane::xz_y_up());

 private:
  FeatureId id_{};
  std::string name_;
  sketch::Sketch sketch_{};
  FeatureStatus status_{FeatureStatus::Dirty};
  bool suppressed_{false};
};

}  // namespace brep::feat
