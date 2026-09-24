#include "brep/feat/BooleanFeature.h"



#include "brep/bool/Boolean.h"

#include "brep/Guid.h"

#include "brep/Log.h"

#include "brep/Part.h"



namespace brep::feat

{

namespace

{



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

    Body* bodyA = part.MaterializeFeatureBody(*targetF, params, scratch);

    Body* bodyB = part.MaterializeFeatureBody(*toolF, params, scratch);

    if (!bodyA || !bodyB)

    {

        part.SetRegenError("Boolean failed to resolve operand bodies");

        BREP_ERROR("BooleanFeature '{}': failed to resolve operand bodies", m_name);

        return false;

    }



    const boolean::BooleanResult eval = part.BooleanEvaluator().Evaluate(

        m_op, part.Model(), *bodyA, *bodyB, boolean::BooleanContext{});

    if (!eval.Ok())

    {

        const std::string detail =

            eval.Diagnostics.empty()

                ? "Boolean evaluate failed"

                : std::string("Boolean evaluate failed: ") + eval.Diagnostics;

        part.SetRegenError(detail);

        BREP_WARN("BooleanFeature '{}': {}", m_name, detail);

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

