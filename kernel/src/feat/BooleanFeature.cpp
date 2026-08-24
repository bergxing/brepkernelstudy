#include "brep/feat/BooleanFeature.h"

#include "brep/bool/Boolean.h"
#include "brep/Builder.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/Guid.h"
#include "brep/Log.h"
#include "brep/Part.h"

namespace brep::feat
{
namespace
{

Body* MaterializeOperand(Part& part, IFeature& feature, param::ParameterStore& params,
                         Model& scratch)
{
    if (Body* live = part.FindBody(feature.BodyGuid()))
{
        return live;
    }

    if (feature.TypeName() == "Box")
    {
        auto& box = static_cast<BoxFeature&>(feature);
        return MakeBox(scratch, box.ToSpec(params));
    }
    if (feature.TypeName() == "Sphere")
    {
        auto& sphere = static_cast<SphereFeature&>(feature);
        return MakeSphere(scratch, sphere.ToSpec(params));
    }

    BREP_WARN("BooleanFeature: cannot materialize operand type '{}'", feature.TypeName());
    return nullptr;
}

void SuppressOperandBody(Part& part, IFeature& feature)
{
    feature.SetSuppressed(true);
    feature.SetStatus(FeatureStatus::Suppressed);
    const Guid guid = feature.BodyGuid();
    if (!guid.IsValid())
    {
        return;
    }
    if (!part.FindBody(guid))
    {
        return;
    }
    part.UnregisterBody(guid);
    part.Model().RemoveBody(guid);
}

}  // namespace

BooleanFeature::BooleanFeature(FeatureId id, std::string name, boolean::BooleanOp op,
                               FeatureId target, FeatureId tool)
    : m_id(id), m_name(std::move(name)), m_op(op), m_target(target), m_tool(tool)
                               {
}

std::unique_ptr<BooleanFeature> BooleanFeature::Create(boolean::BooleanOp op, FeatureId target,
                                                       FeatureId tool, std::string name)
{
    FeatureId fid{Guid::Generate()};
    if (name.empty())
    {
        name = "Boolean";
    }
    return std::make_unique<BooleanFeature>(fid, std::move(name), op, target, tool);
}

void BooleanFeature::CollectParameters(param::ParameterStore& /*store*/)
    {
}

bool BooleanFeature::Rebuild(Part& part, param::ParameterStore& params)
{
    IFeature* targetF = part.Features().Find(m_target);
    IFeature* toolF = part.Features().Find(m_tool);
    if (!targetF || !toolF)
    {
        BREP_ERROR("BooleanFeature '{}': missing target/tool feature", m_name);
        return false;
    }
    if (targetF == toolF || m_target == m_tool)
    {
        BREP_ERROR("BooleanFeature '{}': target and tool must differ", m_name);
        return false;
    }

    Model scratch;
    Body* bodyA = MaterializeOperand(part, *targetF, params, scratch);
    Body* bodyB = MaterializeOperand(part, *toolF, params, scratch);
    if (!bodyA || !bodyB)
    {
        BREP_ERROR("BooleanFeature '{}': failed to resolve operand bodies", m_name);
        return false;
    }

    const boolean::BooleanResult eval = part.BooleanEvaluator().Evaluate(
        m_op, part.Model(), *bodyA, *bodyB, boolean::BooleanContext{});
    if (!eval.Ok())
    {
        BREP_WARN("BooleanFeature '{}': evaluate failed: {}", m_name, eval.Diagnostics);
        return false;
    }

    Body* out = part.RebuildBooleanBody(m_bodyGuid, eval.OutputBody);
    if (!out)
    {
        return false;
    }
    m_bodyGuid = out->Guid;

    SuppressOperandBody(part, *targetF);
    SuppressOperandBody(part, *toolF);
    return true;
}

}  // namespace brep::feat
