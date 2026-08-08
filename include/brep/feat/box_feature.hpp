#pragma once

#include "brep/builder.hpp"
#include "brep/feat/feature.hpp"
#include "brep/math.hpp"

#include <memory>

namespace brep::feat {

class BoxFeature final : public IFeature {
 public:
  BoxFeature(FeatureId id, std::string name, Point3d origin,
             param::ParameterId length, param::ParameterId width,
             param::ParameterId height);

  [[nodiscard]] FeatureId id() const override { return id_; }
  [[nodiscard]] std::string_view type_name() const override { return "Box"; }
  [[nodiscard]] FeatureStatus status() const override { return status_; }
  void set_status(FeatureStatus s) override { status_ = s; }
  void set_suppressed(bool suppressed) override { suppressed_ = suppressed; }
  [[nodiscard]] bool suppressed() const override { return suppressed_; }

  void collect_parameters(param::ParameterStore& store) override;
  bool rebuild(Part& part, param::ParameterStore& params) override;

  [[nodiscard]] Guid body_guid() const override { return body_guid_; }
  void set_body_guid(Guid g) override { body_guid_ = g; }
  [[nodiscard]] std::string display_name() const override { return name_; }

  [[nodiscard]] Point3d origin() const noexcept { return origin_; }
  void set_origin(Point3d o) noexcept { origin_ = o; }

  [[nodiscard]] param::ParameterId length_id() const noexcept { return length_; }
  [[nodiscard]] param::ParameterId width_id() const noexcept { return width_; }
  [[nodiscard]] param::ParameterId height_id() const noexcept { return height_; }

  [[nodiscard]] BoxSpec to_spec(const param::ParameterStore& params) const;

  static std::unique_ptr<BoxFeature> create(param::ParameterStore& store,
                                            const BoxSpec& spec);

 private:
  FeatureId id_{};
  std::string name_;
  Point3d origin_{};
  param::ParameterId length_{};
  param::ParameterId width_{};
  param::ParameterId height_{};
  Guid body_guid_{};
  FeatureStatus status_{FeatureStatus::Dirty};
  bool suppressed_{false};
};

}  // namespace brep::feat
