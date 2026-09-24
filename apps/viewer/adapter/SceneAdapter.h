#pragma once

#include "adapter/ISceneService.h"

#include <memory>
#include <optional>
#include <string>

namespace brep::viewer::adapter
{

/// Viewer-facing facade over Document / Part / mesh (no concrete Feature*).
class SceneAdapter final : public ISceneService
{
public:
    SceneAdapter() = default;
    explicit SceneAdapter(brep::Document* doc) : m_document(doc)
    {
    }

    void SetDocument(brep::Document* doc) noexcept override
    {
        m_document = doc;
    }
    [[nodiscard]] brep::Document* Document() const noexcept override
    {
        return m_document;
    }

    [[nodiscard]] brep::Part* MainPart() const noexcept override;
    [[nodiscard]] const brep::Part* MainPartConst() const noexcept override;

    [[nodiscard]] MeshBundle MeshForBody(
        const Guid& bodyGuid,
        const brep::io::BodyMeshCache* cache = nullptr) const override;

    [[nodiscard]] std::optional<SceneObject> ObjectForFeature(
        feat::FeatureId id) const override;
    [[nodiscard]] std::optional<SceneObject> ObjectForBody(
        const Guid& bodyGuid) const override;

    [[nodiscard]] Body* AddPrimitive(const PrimitiveSpec& spec) override;
    void RecordAppendPrimitive(feat::FeatureId id, PrimitiveSpec undo) override;
    bool SetPrimitive(feat::FeatureId id, const PrimitiveSpec& spec) override;

    [[nodiscard]] Body* AddBoolean(brep::boolean::BooleanOp op,
                                   feat::FeatureId target, feat::FeatureId tool,
                                   std::string name = "Boolean") override;

    [[nodiscard]] std::optional<BooleanParams> BooleanParamsFor(
        feat::FeatureId id) const override;

    bool RemoveFeature(feat::FeatureId id) override;

    [[nodiscard]] std::optional<feat::FeatureId> FeatureIdFor(
        Guid featureGuid, Guid bodyGuid) const override;

    void UndoFeature(int steps = 1) override;
    void RedoFeature(int steps = 1) override;

    [[nodiscard]] std::optional<PrimitiveSpec> SpecFor(
        Guid featureGuid, Guid bodyGuid) const override;

    [[nodiscard]] Body* AddExtrudePad(const std::vector<Point2d>& profile,
                                      double distance, bool symmetric,
                                      std::string name = "Pad") override;

    [[nodiscard]] Body* DuplicateBody(Guid bodyGuid,
                                      RigidTransform transform) override;
    bool TransformBody(Guid bodyGuid, RigidTransform transform) override;

    [[nodiscard]] static std::unique_ptr<brep::Document> CreateBlank(
        const std::string& name = "Untitled",
        std::shared_ptr<brep::boolean::IBooleanEvaluator> evaluator = nullptr);

private:
    [[nodiscard]] const feat::IFeature* FindFeature(Guid featureGuid,
                                                    Guid bodyGuid) const;

    brep::Document* m_document{nullptr};
};

}  // namespace brep::viewer::adapter
