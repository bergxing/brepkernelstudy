#include "brep/bool/Pipeline.h"

#include "brep/bool/BoxRecognize.h"
#include "brep/bool/Classify.h"
#include "brep/bool/IntersectorRegistry.h"
#include "brep/bool/SphereRecognize.h"
#include "brep/Log.h"

#include <algorithm>
#include <sstream>

namespace brep::boolean
{
namespace
{

[[nodiscard]] std::size_t CountFaces(const Body& body)
{
    std::size_t faceCount = 0;
    for (const Shell* shell : body.Shells)
    {
        if (!shell)
    {
            continue;
        }
        faceCount += shell->Faces.size();
    }
    return faceCount;
}

[[nodiscard]] Point3d FaceSamplePoint(const Face& face)
{
    if (!face.Surface)
{
        return {};
    }
    return face.Surface->Eval(0.25, 0.25);
}

class PreprocessStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Preprocess;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Preprocess;
        if (!state.BodyA || !state.BodyB || !state.TargetModel)
        {
            stageResult.Diagnostics = "pipeline Preprocess: missing operand or model";
            return stageResult;
        }
        if (state.BodyA->Shells.empty() || state.BodyB->Shells.empty())
        {
            stageResult.Diagnostics = "pipeline Preprocess: operand has no shells";
            return stageResult;
        }
        if (CountFaces(*state.BodyA) == 0 || CountFaces(*state.BodyB) == 0)
        {
            stageResult.Diagnostics = "pipeline Preprocess: operand has no faces";
            return stageResult;
        }
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Preprocess;
        return stageResult;
    }
};

class IntersectStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Intersect;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Intersect;
        IntersectorRegistry registry;
        state.FacePairs = CollectFacePairCandidates(*state.BodyA, *state.BodyB);
        state.Probe = registry.ProbeBodyPair(*state.BodyA, *state.BodyB,
                                             spatial::BuildQuality::Sah, state.Ctx);
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Intersect;
        return stageResult;
    }
};

class ImprintStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Imprint;
    }

    PipelineStageResult Run(PipelineState& /*state*/) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Imprint;
        stageResult.Diagnostics =
            "pipeline Imprint: not implemented yet (P1 — split edges/faces along "
            "intersection graph)";
        return stageResult;
    }
};

class ClassifyStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Classify;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Classify;
        const double eps = std::max(state.Ctx.fuzzy, 1e-9);

        const auto sphereB = RecognizeAnalyticSphere(*state.BodyB, state.Ctx);
        const auto boxB = RecognizeAxisAlignedBox(*state.BodyB, state.Ctx);
        const auto sphereA = RecognizeAnalyticSphere(*state.BodyA, state.Ctx);
        const auto boxA = RecognizeAxisAlignedBox(*state.BodyA, state.Ctx);

        auto classifyVsB = [&](Face* face) -> FaceRegion
        {
            if (!face)
        {
                return FaceRegion::Unknown;
            }
            const Point3d samplePoint = FaceSamplePoint(*face);
            if (sphereB)
            {
                return SolidClassToFaceRegion(
                    ClassifyPointInSphere(*sphereB, samplePoint, eps));
            }
            if (boxB)
            {
                return SolidClassToFaceRegion(
                    ClassifyPointInBox(*boxB, samplePoint, eps));
            }
            return FaceRegion::Unknown;
        };

        auto classifyVsA = [&](Face* face) -> FaceRegion
            {
            if (!face)
            {
                return FaceRegion::Unknown;
            }
            const Point3d samplePoint = FaceSamplePoint(*face);
            if (sphereA)
            {
                return SolidClassToFaceRegion(
                    ClassifyPointInSphere(*sphereA, samplePoint, eps));
            }
            if (boxA)
            {
                return SolidClassToFaceRegion(
                    ClassifyPointInBox(*boxA, samplePoint, eps));
            }
            return FaceRegion::Unknown;
        };

        state.AVsB.clear();
        state.BVsA.clear();
        for (Shell* shell : state.BodyA->Shells)
        {
            if (!shell)
        {
                continue;
            }
            for (Face* face : shell->Faces)
            {
                state.AVsB.push_back(
                    FaceClassification{face, classifyVsB(face)});
            }
        }
        for (Shell* shell : state.BodyB->Shells)
        {
            if (!shell)
        {
                continue;
            }
            for (Face* face : shell->Faces)
            {
                state.BVsA.push_back(FaceClassification{face, classifyVsA(face)});
            }
        }

        const bool haveUnknown =
            std::any_of(state.AVsB.begin(), state.AVsB.end(),
                        [](const FaceClassification& fc)
            {
                            return fc.Region == FaceRegion::Unknown;
                        }) ||
            std::any_of(state.BVsA.begin(), state.BVsA.end(),
                        [](const FaceClassification& fc)
            {
                            return fc.Region == FaceRegion::Unknown;
                        });

        if (haveUnknown)
        {
            stageResult.Diagnostics =
                "pipeline Classify: partial — imprinted face fragments required for "
                "general solids (P1)";
            return stageResult;
        }

        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Classify;
        return stageResult;
    }
};

class SelectStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Select;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Select;
        if (state.AVsB.empty() || state.BVsA.empty())
        {
            stageResult.Diagnostics =
                "pipeline Select: missing classifications (Classify stage required)";
            return stageResult;
        }
        state.Selection = SelectCsgFaces(state.Op, state.AVsB, state.BVsA);
        if (state.Selection.FromA.empty() && state.Selection.FromB.empty())
        {
            stageResult.Diagnostics = "pipeline Select: CSG selection empty";
            return stageResult;
        }
        stageResult.Ok = true;
        state.LastCompleted = PipelineStage::Select;
        return stageResult;
    }
};

class BuildStage final : public IPipelineStage
{
public:
    [[nodiscard]] PipelineStage Id() const override
    {
        return PipelineStage::Build;
    }

    PipelineStageResult Run(PipelineState& state) override
    {
        PipelineStageResult stageResult;
        stageResult.Stage = PipelineStage::Build;
        std::ostringstream oss;
        oss << "pipeline Build: not implemented yet (P1 — assemble "
            << state.Selection.FromA.size() << " faces from A, "
            << state.Selection.FromB.size() << " from B into manifold shell)";
        stageResult.Diagnostics = oss.str();
        return stageResult;
    }
};

}  // namespace

const char* PipelineStageName(PipelineStage stage) noexcept
{
    switch (stage)
{
    case PipelineStage::Preprocess:
        return "Preprocess";
    case PipelineStage::Intersect:
        return "Intersect";
    case PipelineStage::Imprint:
        return "Imprint";
    case PipelineStage::Classify:
        return "Classify";
    case PipelineStage::Select:
        return "Select";
    case PipelineStage::Build:
        return "Build";
    }
    return "Unknown";
}

BooleanPipeline::BooleanPipeline() = default;

void BooleanPipeline::AddStage(std::unique_ptr<IPipelineStage> stage)
{
    if (!stage)
{
        return;
    }
    m_stageOrder.push_back(stage->Id());
    m_stages.push_back(std::move(stage));
}

BooleanResult BooleanPipeline::Evaluate(BooleanOp op, Model& model, const Body& a,
                                        const Body& b, const BooleanContext& ctx)
{
    BooleanResult result;
    result.Mode = BooleanEvalMode::General;

    PipelineState state;
    state.Op = op;
    state.BodyA = &a;
    state.BodyB = &b;
    state.Ctx = ctx;
    state.TargetModel = &model;

    for (const auto& stage : m_stages)
    {
        const PipelineStageResult stageResult = stage->Run(state);
        if (!stageResult.Ok)
        {
            result.Diagnostics =
                stageResult.Diagnostics.empty()
                    ? std::string("pipeline ") + PipelineStageName(stageResult.Stage) +
                          ": failed"
                    : stageResult.Diagnostics;
            if (state.Probe.CandidatePairs > 0 &&
                stageResult.Stage != PipelineStage::Intersect)
            {
                result.Diagnostics += "; " + state.Probe.Summary;
            }
            BREP_WARN("{}", result.Diagnostics);
            return result;
        }
    }

    result.Diagnostics =
        "pipeline: all stages reported ok but no body was produced (internal error)";
    BREP_WARN("{}", result.Diagnostics);
    return result;
}

std::unique_ptr<BooleanPipeline> MakeDefaultBooleanPipeline()
{
    auto pipeline = std::make_unique<BooleanPipeline>();
    pipeline->AddStage(std::make_unique<PreprocessStage>());
    pipeline->AddStage(std::make_unique<IntersectStage>());
    pipeline->AddStage(std::make_unique<ImprintStage>());
    pipeline->AddStage(std::make_unique<ClassifyStage>());
    pipeline->AddStage(std::make_unique<SelectStage>());
    pipeline->AddStage(std::make_unique<BuildStage>());
    return pipeline;
}

}  // namespace brep::boolean
