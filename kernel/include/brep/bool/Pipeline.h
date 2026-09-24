#pragma once

#include "brep/bool/Broadphase.h"
#include "brep/bool/Context.h"
#include "brep/bool/FaceSelector.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/Result.h"
#include "brep/bool/Types.h"
#include "brep/Model.h"
#include "brep/Topology.h"

#include <memory>
#include <string>
#include <vector>

namespace brep::boolean
{

enum class PipelineStage : std::uint8_t
{
  Preprocess = 0,
  Intersect,
  Imprint,
  Classify,
  Select,
  Build,
};

[[nodiscard]] const char* PipelineStageName(PipelineStage stage) noexcept;

struct PipelineStageResult
{
  bool Ok{false};
  PipelineStage Stage{PipelineStage::Preprocess};
  std::string Diagnostics;
};

/// Shared mutable state passed through pipeline stages.
struct PipelineState
{
  BooleanOp Op{BooleanOp::Union};
  const Body* BodyA{nullptr};
  const Body* BodyB{nullptr};
  BooleanContext Ctx{};
  Model* TargetModel{nullptr};
  Body* WorkingBodyA{nullptr};
  Body* WorkingBodyB{nullptr};
  std::vector<FacePairCandidate> FacePairs;
  BroadphaseProbe Probe{};
  IntersectionGraph IntersectionGraph;
  std::vector<FaceClassification> AVsB;
  std::vector<FaceClassification> BVsA;
  FaceSelection Selection{};
  Body* OutputBody{nullptr};
  bool NoVolumeOverlap{false};
  PipelineStage LastCompleted{PipelineStage::Preprocess};
};

class IPipelineStage
{
 public:
  virtual ~IPipelineStage() = default;

  [[nodiscard]] virtual PipelineStage Id() const = 0;
  virtual PipelineStageResult Run(PipelineState& state) = 0;
};

/// Classify a face against the other operand solid (imprint-aware).
[[nodiscard]] FaceRegion ClassifyFaceAgainstBody(const Face& face,
                                                 const Body& solid,
                                                 const Body& owner,
                                                 double eps,
                                                 BooleanOp op,
                                                 bool fromA = true);

/// General boolean CSG pipeline (ADR 0006).
class BooleanPipeline
{
 public:
  BooleanPipeline();

  void AddStage(std::unique_ptr<IPipelineStage> stage);

  [[nodiscard]] const std::vector<PipelineStage>& StageOrder() const noexcept
  {
    return m_stageOrder;
  }

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                       const Body& b, const BooleanContext& ctx);

 private:
  std::vector<std::unique_ptr<IPipelineStage>> m_stages;
  std::vector<PipelineStage> m_stageOrder;
};

[[nodiscard]] std::unique_ptr<BooleanPipeline> MakeDefaultBooleanPipeline();

}  // namespace brep::boolean
