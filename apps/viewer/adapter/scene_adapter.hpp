#pragma once

#include "api/core.hpp"
#include "api/mesh.hpp"
#include "api/modeling.hpp"
#include "api/persistence.hpp"

#include <memory>
#include <optional>
#include <string>

namespace brep::viewer::adapter {

struct BoxParams {
  double length{0.0};
  double width{0.0};
  double height{0.0};
};

struct MeshBundle {
  TriangleMesh faces;
  EdgeMesh edges;
};

struct SceneObject {
  Guid body_guid;
  Guid feature_guid;
  std::string name;
  std::string type_name;  // "Box", "Extrude", …
  std::optional<BoxParams> box;
};

/// Viewer-facing facade over Document / Part / mesh (no concrete Feature*).
class SceneAdapter {
 public:
  SceneAdapter() = default;
  explicit SceneAdapter(brep::Document* doc) : document_(doc) {}

  void set_document(brep::Document* doc) noexcept { document_ = doc; }
  [[nodiscard]] brep::Document* document() const noexcept { return document_; }

  [[nodiscard]] brep::Part* main_part() const noexcept;
  [[nodiscard]] const brep::Part* main_part_const() const noexcept;

  /// Tessellate a body, preferring optional BKS cache when present.
  [[nodiscard]] MeshBundle mesh_for_body(
      const Guid& body_guid,
      const brep::io::BodyMeshCache* cache = nullptr) const;

  [[nodiscard]] std::optional<SceneObject> object_for_feature(
      feat::FeatureId id) const;
  [[nodiscard]] std::optional<SceneObject> object_for_body(
      const Guid& body_guid) const;

  [[nodiscard]] std::optional<BoxParams> box_params(
      feat::FeatureId id) const;
  bool set_box_params(feat::FeatureId id, const BoxParams& params);

  [[nodiscard]] Body* add_box(const BoxSpec& spec);
  void record_append_feature(feat::FeatureId id, BoxSpec undo_spec);

  void undo_feature(int steps = 1);
  void redo_feature(int steps = 1);

  [[nodiscard]] std::optional<BoxSpec> box_spec_for(
      Guid feature_guid, Guid body_guid) const;

  [[nodiscard]] static std::unique_ptr<brep::Document> create_blank(
      const std::string& name = "Untitled");

 private:
  [[nodiscard]] const feat::IFeature* find_box_feature(
      feat::FeatureId id) const;
  [[nodiscard]] std::optional<BoxParams> box_params_from_feature(
      const feat::IFeature& feature) const;

  brep::Document* document_{nullptr};
};

}  // namespace brep::viewer::adapter
