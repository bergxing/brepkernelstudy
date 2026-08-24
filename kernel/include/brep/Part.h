#pragma once

#include "brep/bool/Evaluator.h"
#include "brep/bool/Types.h"
#include "brep/Builder.h"
#include "brep/feat/FeatureHistory.h"
#include "brep/feat/FeatureTree.h"
#include "brep/feat/Regenerator.h"
#include "brep/Guid.h"
#include "brep/IObject.h"
#include "brep/Model.h"
#include "brep/ops/Profile.h"
#include "brep/param/Parameter.h"

#include <memory>

namespace brep
{

class Document;

/// Part: owns B-Rep Model, parameters, and feature tree.
class Part final : public IObject
{
public:
    explicit Part(std::string partName = "Part");

    [[nodiscard]] ObjectKind Kind() const noexcept override
    {
        return ObjectKind::Part;
    }

    // IObject::Guid / Model() / Document() hide type names in class scope — qualify.
    [[nodiscard]] brep::Model& Model() noexcept
    {
        return m_model;
    }
    [[nodiscard]] const brep::Model& Model() const noexcept
    {
        return m_model;
    }

    [[nodiscard]] brep::Document* Document() noexcept
    {
        return m_document;
    }
    [[nodiscard]] const brep::Document* Document() const noexcept
    {
        return m_document;
    }

    [[nodiscard]] param::ParameterStore& Parameters() noexcept
    {
        return m_params;
    }
    [[nodiscard]] const param::ParameterStore& Parameters() const noexcept
    {
        return m_params;
    }

    [[nodiscard]] feat::FeatureTree& Features() noexcept
    {
        return m_features;
    }
    [[nodiscard]] const feat::FeatureTree& Features() const noexcept
    {
        return m_features;
    }

    [[nodiscard]] feat::FeatureHistory& FeatureHistory() noexcept
    {
        return m_history;
    }
    [[nodiscard]] const feat::FeatureHistory& FeatureHistory() const noexcept
    {
        return m_history;
    }

    /// Build box via BoxFeature + regenerate. Returns the Body.
    Body* AddBox(const BoxSpec& spec = {});

    /// Build sphere via SphereFeature + regenerate. Returns the Body.
    Body* AddSphere(const SphereSpec& spec = {});

    /// Append a rectangle sketch feature (for parametric extrude workflows).
    feat::FeatureId AddRectangleSketch(std::string name, Point2d min,
                                       Point2d max);

    /// Append extrude of an existing sketch feature.
    Body* AddExtrude(feat::FeatureId sketchFeature, double distance,
                     std::string name = "Extrude");

    /// Append boolean of two body-producing features (suppresses operands on success).
    Body* AddBoolean(boolean::BooleanOp op, feat::FeatureId target,
                     feat::FeatureId tool, std::string name = "Boolean");

    feat::RegenResult Regenerate();

    bool RemoveFeature(feat::FeatureId id);

    bool EditFeatureParams(
        feat::FeatureId id,
        std::initializer_list<std::pair<std::string_view, double>> namedVals);

    /// Register an existing Body on the Document registry.
    void RegisterBody(Body& body);
    void UnregisterBody(const brep::Guid& guid);

    /// Replace or create an AABB box body, preserving Guid when possible.
    Body* RebuildBoxBody(brep::Guid keepGuid, const BoxSpec& spec);

    /// Replace or create a sphere body, preserving Guid when possible.
    Body* RebuildSphereBody(brep::Guid keepGuid, const SphereSpec& spec);

    /// Replace or create an extruded body, preserving Guid when possible.
    Body* RebuildExtrudeBody(brep::Guid keepGuid, const ops::ExtrudeSpec& spec);

    /// Adopt a boolean result body already created in this Part's model.
    Body* RebuildBooleanBody(brep::Guid keepGuid, Body* resultBody);

    void SetBooleanEvaluator(
        std::shared_ptr<boolean::IBooleanEvaluator> evaluator);
    [[nodiscard]] boolean::IBooleanEvaluator& BooleanEvaluator();

    Body* FindBody(const brep::Guid& guid);
    [[nodiscard]] const Body* FindBody(const brep::Guid& guid) const;

private:
    friend class Document;
    void SetDocument(brep::Document* doc) noexcept
    {
        m_document = doc;
    }

    brep::Document* m_document{nullptr};
    brep::Model m_model;
    param::ParameterStore m_params;
    feat::FeatureTree m_features;
    feat::FeatureHistory m_history;
    std::shared_ptr<boolean::IBooleanEvaluator> m_booleanEvaluator;
};

}  // namespace brep
