#pragma once

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include <memory>
#include <optional>
#include <string>

namespace brep::viewer::adapter
{

struct BoxParams
{
  double length{0.0};
  double width{0.0};
  double height{0.0};
};

struct SphereParams
{
  double radius{0.0};
};

struct BooleanParams
{
  brep::boolean::BooleanOp op{brep::boolean::BooleanOp::Union};
};

struct MeshBundle
{
  TriangleMesh faces;
  EdgeMesh edges;
};

struct SceneObject
{
  Guid body_guid;
  Guid feature_guid;
  std::string name;
  std::string type_name;  // "Box", "Sphere", "Boolean", "Extrude", ...
  std::optional<BoxParams> box;
  std::optional<SphereParams> sphere;
  std::optional<BooleanParams> boolean_info;
};

/// Viewer-facing facade over Document / Part / mesh (no concrete Feature*).
class SceneAdapter
{
 public:
  SceneAdapter() = default;
  explicit SceneAdapter(brep::Document* doc) : m_document(doc)
  {
  }

  void set_document(brep::Document* doc) noexcept
  {
      m_document = doc; 
  }
  [[nodiscard]] brep::Document* document() const noexcept
  {
      return m_document; 
  }

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

  [[nodiscard]] Body* add_sphere(const SphereSpec& spec);
  void record_append_sphere(feat::FeatureId id, SphereSpec undo_spec);

  [[nodiscard]] Body* add_boolean(brep::boolean::BooleanOp op,
                                  feat::FeatureId target, feat::FeatureId tool,
                                  std::string name = "Boolean");

  [[nodiscard]] std::optional<SphereParams> sphere_params(
      feat::FeatureId id) const;
  bool set_sphere_params(feat::FeatureId id, const SphereParams& params);

  [[nodiscard]] std::optional<BooleanParams> boolean_params(
      feat::FeatureId id) const;

  /// Remove a feature via FeatureHistory (supports undo/redo).
  bool remove_feature(feat::FeatureId id);

  [[nodiscard]] std::optional<feat::FeatureId> feature_id_for(
      Guid feature_guid, Guid body_guid) const;

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

  brep::Document* m_document{nullptr};
};

}  // namespace brep::viewer::adapter
