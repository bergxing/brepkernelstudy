#pragma once

#include "brep/builder.hpp"
#include "brep/feat/feature_history.hpp"
#include "brep/feat/feature_tree.hpp"
#include "brep/feat/regenerator.hpp"
#include "brep/iobject.hpp"
#include "brep/model.hpp"
#include "brep/ops/profile.hpp"
#include "brep/param/parameter.hpp"

namespace brep {

class Document;

/// Part: owns B-Rep Model, parameters, and feature tree.
class Part final : public IObject {
 public:
  explicit Part(std::string part_name = "Part");

  [[nodiscard]] ObjectKind kind() const noexcept override {
    return ObjectKind::Part;
  }

  [[nodiscard]] Model& model() noexcept { return model_; }
  [[nodiscard]] const Model& model() const noexcept { return model_; }

  [[nodiscard]] Document* document() noexcept { return document_; }
  [[nodiscard]] const Document* document() const noexcept { return document_; }

  [[nodiscard]] param::ParameterStore& parameters() noexcept { return params_; }
  [[nodiscard]] const param::ParameterStore& parameters() const noexcept {
    return params_;
  }

  [[nodiscard]] feat::FeatureTree& features() noexcept { return features_; }
  [[nodiscard]] const feat::FeatureTree& features() const noexcept {
    return features_;
  }

  [[nodiscard]] feat::FeatureHistory& feature_history() noexcept {
    return history_;
  }
  [[nodiscard]] const feat::FeatureHistory& feature_history() const noexcept {
    return history_;
  }

  /// Build box via BoxFeature + regenerate. Returns the Body.
  Body* add_box(const BoxSpec& spec = {});

  /// Build sphere via SphereFeature + regenerate. Returns the Body.
  Body* add_sphere(const SphereSpec& spec = {});

  /// Append a rectangle sketch feature (for parametric extrude workflows).
  feat::FeatureId add_rectangle_sketch(std::string name, Point2d min,
                                       Point2d max);

  /// Append extrude of an existing sketch feature.
  Body* add_extrude(feat::FeatureId sketch_feature, double distance,
                    std::string name = "Extrude");

  feat::RegenResult regenerate();

  bool remove_feature(feat::FeatureId id);

  bool edit_feature_params(
      feat::FeatureId id,
      std::initializer_list<std::pair<std::string_view, double>> named_vals);

  /// Register an existing Body on the Document registry.
  void register_body(Body& body);
  void unregister_body(const Guid& guid);

  /// Replace or create an AABB box body, preserving Guid when possible.
  Body* rebuild_box_body(Guid keep_guid, const BoxSpec& spec);

  /// Replace or create a sphere body, preserving Guid when possible.
  Body* rebuild_sphere_body(Guid keep_guid, const SphereSpec& spec);

  /// Replace or create an extruded body, preserving Guid when possible.
  Body* rebuild_extrude_body(Guid keep_guid, const ops::ExtrudeSpec& spec);

  Body* find_body(const Guid& guid);
  [[nodiscard]] const Body* find_body(const Guid& guid) const;

 private:
  friend class Document;
  void set_document(Document* doc) noexcept { document_ = doc; }

  Document* document_{nullptr};
  Model model_;
  param::ParameterStore params_;
  feat::FeatureTree features_;
  feat::FeatureHistory history_;
};

}  // namespace brep
