#pragma once

#include "brep/builder.hpp"
#include "brep/feat/feature.hpp"
#include "brep/math.hpp"

#include <memory>

namespace brep::feat {

class SphereFeature final : public IFeature {
 public:
  SphereFeature(FeatureId id, std::string name, Point3d center,
                param::ParameterId radius);

  [[nodiscard]] FeatureId id() const override { return id_; }
  [[nodiscard]] std::string_view type_name() const override { return "Sphere"; }
  [[nodiscard]] FeatureStatus status() const override { return status_; }
  void set_status(FeatureStatus s) override { status_ = s; }
  void set_suppressed(bool suppressed) override { suppressed_ = suppressed; }
  [[nodiscard]] bool suppressed() const override { return suppressed_; }

  void collect_parameters(param::ParameterStore& store) override;
  bool rebuild(Part& part, param::ParameterStore& params) override;

  [[nodiscard]] Guid body_guid() const override { return body_guid_; }
  void set_body_guid(Guid g) override { body_guid_ = g; }
  [[nodiscard]] std::string display_name() const override { return name_; }

  [[nodiscard]] Point3d center() const noexcept { return center_; }
  void set_center(Point3d c) noexcept { center_ = c; }

  [[nodiscard]] param::ParameterId radius_id() const noexcept { return radius_; }

  [[nodiscard]] SphereSpec to_spec(const param::ParameterStore& params) const;

  static std::unique_ptr<SphereFeature> create(param::ParameterStore& store,
                                               const SphereSpec& spec);

 private:
  FeatureId id_{};
  std::string name_;
  Point3d center_{};
  param::ParameterId radius_{};
  Guid body_guid_{};
  FeatureStatus status_{FeatureStatus::Dirty};
  bool suppressed_{false};
};

}  // namespace brep::feat
