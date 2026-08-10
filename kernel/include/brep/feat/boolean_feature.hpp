#pragma once

#include "brep/bool/types.hpp"
#include "brep/feat/feature.hpp"

#include <memory>
#include <string>

namespace brep::feat {

class BooleanFeature final : public IFeature {
 public:
  BooleanFeature(FeatureId id, std::string name, boolean::BooleanOp op,
                 FeatureId target, FeatureId tool);

  [[nodiscard]] FeatureId id() const override { return id_; }
  [[nodiscard]] std::string_view type_name() const override { return "Boolean"; }
  [[nodiscard]] FeatureStatus status() const override { return status_; }
  void set_status(FeatureStatus s) override { status_ = s; }
  void set_suppressed(bool suppressed) override { suppressed_ = suppressed; }
  [[nodiscard]] bool suppressed() const override { return suppressed_; }

  void collect_parameters(param::ParameterStore& store) override;
  bool rebuild(Part& part, param::ParameterStore& params) override;

  [[nodiscard]] Guid body_guid() const override { return body_guid_; }
  void set_body_guid(Guid g) override { body_guid_ = g; }
  [[nodiscard]] std::string display_name() const override { return name_; }

  [[nodiscard]] boolean::BooleanOp op() const noexcept { return op_; }
  [[nodiscard]] FeatureId target_feature_id() const noexcept { return target_; }
  [[nodiscard]] FeatureId tool_feature_id() const noexcept { return tool_; }

  static std::unique_ptr<BooleanFeature> create(boolean::BooleanOp op,
                                                FeatureId target,
                                                FeatureId tool,
                                                std::string name = "Boolean");

 private:
  FeatureId id_{};
  std::string name_;
  boolean::BooleanOp op_{boolean::BooleanOp::Union};
  FeatureId target_{};
  FeatureId tool_{};
  Guid body_guid_{};
  FeatureStatus status_{FeatureStatus::Dirty};
  bool suppressed_{false};
};

}  // namespace brep::feat
