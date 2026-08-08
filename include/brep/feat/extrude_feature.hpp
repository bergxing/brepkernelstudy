#pragma once

#include "brep/feat/feature.hpp"
#include "brep/feat/sketch_feature.hpp"

#include <memory>

namespace brep::feat {

class ExtrudeFeature final : public IFeature {
 public:
  ExtrudeFeature(FeatureId id, std::string name, FeatureId sketch_feature,
                 param::ParameterId distance);

  [[nodiscard]] FeatureId id() const override { return id_; }
  [[nodiscard]] std::string_view type_name() const override { return "Extrude"; }
  [[nodiscard]] FeatureStatus status() const override { return status_; }
  void set_status(FeatureStatus s) override { status_ = s; }
  void set_suppressed(bool suppressed) override { suppressed_ = suppressed; }
  [[nodiscard]] bool suppressed() const override { return suppressed_; }

  void collect_parameters(param::ParameterStore& store) override;
  bool rebuild(Part& part, param::ParameterStore& params) override;

  [[nodiscard]] Guid body_guid() const override { return body_guid_; }
  void set_body_guid(Guid g) override { body_guid_ = g; }
  [[nodiscard]] std::string display_name() const override { return name_; }

  [[nodiscard]] FeatureId sketch_feature_id() const noexcept {
    return sketch_feature_;
  }
  [[nodiscard]] param::ParameterId distance_id() const noexcept {
    return distance_;
  }

  static std::unique_ptr<ExtrudeFeature> create(param::ParameterStore& store,
                                                std::string name,
                                                FeatureId sketch_feature,
                                                double distance);

 private:
  FeatureId id_{};
  std::string name_;
  FeatureId sketch_feature_{};
  param::ParameterId distance_{};
  Guid body_guid_{};
  FeatureStatus status_{FeatureStatus::Dirty};
  bool suppressed_{false};
};

}  // namespace brep::feat
