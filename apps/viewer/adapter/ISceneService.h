#pragma once

#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "api/Persistence.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace brep::viewer::adapter
{

struct BooleanParams
{
    brep::boolean::BooleanOp Op{brep::boolean::BooleanOp::Union};
};

struct MeshBundle
{
    TriangleMesh Faces;
    EdgeMesh Edges;
};

/// Identity DTO for the scene tree / property panel. Dimensions live on
/// PrimitiveSpec via SpecFor — do not add per-type optionals here.
struct SceneObject
{
    Guid BodyGuid;
    Guid FeatureGuid;
    std::string Name;
    std::string TypeName;
};

/// Viewer-facing facade over Document / Part / mesh (no concrete Feature*).
class ISceneService
{
public:
    virtual ~ISceneService() = default;

    virtual void SetDocument(brep::Document* doc) noexcept = 0;
    [[nodiscard]] virtual brep::Document* Document() const noexcept = 0;

    [[nodiscard]] virtual brep::Part* MainPart() const noexcept = 0;
    [[nodiscard]] virtual const brep::Part* MainPartConst() const noexcept = 0;

    [[nodiscard]] virtual MeshBundle MeshForBody(
        const Guid& bodyGuid,
        const brep::io::BodyMeshCache* cache = nullptr) const = 0;

    [[nodiscard]] virtual std::optional<SceneObject> ObjectForFeature(
        feat::FeatureId id) const = 0;
    [[nodiscard]] virtual std::optional<SceneObject> ObjectForBody(
        const Guid& bodyGuid) const = 0;

    [[nodiscard]] virtual Body* AddPrimitive(const PrimitiveSpec& spec) = 0;
    virtual void RecordAppendPrimitive(feat::FeatureId id,
                                       PrimitiveSpec undo) = 0;
    virtual bool SetPrimitive(feat::FeatureId id,
                              const PrimitiveSpec& spec) = 0;

    [[nodiscard]] virtual Body* AddBoolean(brep::boolean::BooleanOp op,
                                           feat::FeatureId target,
                                           feat::FeatureId tool,
                                           std::string name = "Boolean") = 0;

    [[nodiscard]] virtual std::optional<BooleanParams> BooleanParamsFor(
        feat::FeatureId id) const = 0;

    virtual bool RemoveFeature(feat::FeatureId id) = 0;

    [[nodiscard]] virtual std::optional<feat::FeatureId> FeatureIdFor(
        Guid featureGuid, Guid bodyGuid) const = 0;

    virtual void UndoFeature(int steps = 1) = 0;
    virtual void RedoFeature(int steps = 1) = 0;

    /// Reconstructable primitive spec (Box / Sphere / …). Empty for Boolean,
    /// Extrude, and other non-primitive features. Do not add XxxSpecFor.
    [[nodiscard]] virtual std::optional<PrimitiveSpec> SpecFor(
        Guid featureGuid, Guid bodyGuid) const = 0;

    [[nodiscard]] virtual Body* AddExtrudePad(const std::vector<Point2d>& profile,
                                              double distance, bool symmetric,
                                              std::string name = "Pad") = 0;

    [[nodiscard]] virtual Body* DuplicateBody(Guid bodyGuid,
                                              RigidTransform transform) = 0;
    virtual bool TransformBody(Guid bodyGuid, RigidTransform transform) = 0;
};

}  // namespace brep::viewer::adapter
