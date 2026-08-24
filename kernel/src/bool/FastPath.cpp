#include "brep/bool/FastPath.h"

#include "brep/bool/BoxBoolean.h"
#include "brep/bool/BoxRecognize.h"
#include "brep/bool/PlanarBoolean.h"
#include "brep/bool/PlanarRecognize.h"
#include "brep/bool/SphereBoxBoolean.h"
#include "brep/bool/SphereRecognize.h"
#include "brep/bool/SphereSphereBoolean.h"

namespace brep::boolean
{
namespace
{

class BoxBoxFastPath final : public IAnalyticFastPath
{
 public:
  [[nodiscard]] std::string_view Name() const override
  {
    return "BoxBox";
  }

  [[nodiscard]] bool CanHandle(BooleanOp op, const Body& a, const Body& b,
                               const BooleanContext& ctx) const override
  {
    (void)op;
    return RecognizeAxisAlignedBox(a, ctx) && RecognizeAxisAlignedBox(b, ctx);
  }

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override
  {
    return EvaluateBoxBoolean(op, model, a, b, ctx);
  }
};

class PrismBoxFastPath final : public IAnalyticFastPath
{
 public:
  [[nodiscard]] std::string_view Name() const override
  {
    return "PrismBox";
  }

  [[nodiscard]] bool CanHandle(BooleanOp op, const Body& a, const Body& b,
                               const BooleanContext& ctx) const override
  {
    (void)op;
    const auto prismA = RecognizeExtrusionPrism(a, ctx);
    const auto prismB = RecognizeExtrusionPrism(b, ctx);
    const auto boxA = RecognizeAxisAlignedBox(a, ctx);
    const auto boxB = RecognizeAxisAlignedBox(b, ctx);
    return (prismA && boxB) || (boxA && prismB);
  }

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override
  {
    const auto prismA = RecognizeExtrusionPrism(a, ctx);
    const auto prismB = RecognizeExtrusionPrism(b, ctx);
    const auto boxA = RecognizeAxisAlignedBox(a, ctx);
    const auto boxB = RecognizeAxisAlignedBox(b, ctx);
    if (prismA && boxB)
    {
      return EvaluatePrismBoxBoolean(op, model, *prismA, *boxB,
                                     /*prism_is_a=*/true, ctx);
    }
    if (boxA && prismB)
    {
      return EvaluatePrismBoxBoolean(op, model, *prismB, *boxA,
                                     /*prism_is_a=*/false, ctx);
    }
    BooleanResult result;
    result.Diagnostics = "PrismBox fast path: recognition lost after CanHandle";
    return result;
  }
};

class SphereBoxFastPath final : public IAnalyticFastPath
{
 public:
  [[nodiscard]] std::string_view Name() const override
  {
    return "SphereBox";
  }

  [[nodiscard]] bool CanHandle(BooleanOp op, const Body& a, const Body& b,
                               const BooleanContext& ctx) const override
  {
    const auto sphereA = RecognizeAnalyticSphere(a, ctx);
    const auto sphereB = RecognizeAnalyticSphere(b, ctx);
    const auto boxA = RecognizeAxisAlignedBox(a, ctx);
    const auto boxB = RecognizeAxisAlignedBox(b, ctx);
    if (sphereA && boxB)
    {
      return true;
    }
    if (boxA && sphereB)
    {
      return op != BooleanOp::Subtract;
    }
    return false;
  }

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override
  {
    const auto sphereA = RecognizeAnalyticSphere(a, ctx);
    const auto sphereB = RecognizeAnalyticSphere(b, ctx);
    const auto boxA = RecognizeAxisAlignedBox(a, ctx);
    const auto boxB = RecognizeAxisAlignedBox(b, ctx);
    if (sphereA && boxB)
    {
      return EvaluateSphereBoxBoolean(op, model, *sphereA, *boxB,
                                      /*sphere_is_a=*/true, ctx);
    }
    if (boxA && sphereB)
    {
      return EvaluateSphereBoxBoolean(op, model, *sphereB, *boxA,
                                      /*sphere_is_a=*/false, ctx);
    }
    BooleanResult result;
    result.Diagnostics = "SphereBox fast path: recognition lost after CanHandle";
    return result;
  }
};

class SphereSphereFastPath final : public IAnalyticFastPath
{
 public:
  [[nodiscard]] std::string_view Name() const override
  {
    return "SphereSphere";
  }

  [[nodiscard]] bool CanHandle(BooleanOp op, const Body& a, const Body& b,
                               const BooleanContext& ctx) const override
  {
    (void)op;
    return RecognizeAnalyticSphere(a, ctx) && RecognizeAnalyticSphere(b, ctx);
  }

  [[nodiscard]] BooleanResult Evaluate(BooleanOp op, Model& model, const Body& a,
                                     const Body& b,
                                     const BooleanContext& ctx) override
  {
    const auto sphereA = RecognizeAnalyticSphere(a, ctx);
    const auto sphereB = RecognizeAnalyticSphere(b, ctx);
    if (!sphereA || !sphereB)
    {
      BooleanResult result;
      result.Diagnostics =
          "SphereSphere fast path: recognition lost after CanHandle";
      return result;
    }
    return EvaluateSphereSphereBoolean(op, model, *sphereA, *sphereB, ctx);
  }
};

}  // namespace

void AnalyticFastPathRegistry::Add(std::unique_ptr<IAnalyticFastPath> path)
{
  if (path)
  {
    m_paths.push_back(std::move(path));
  }
}

AnalyticFastPathRegistry MakeDefaultFastPathRegistry()
{
  AnalyticFastPathRegistry registry;
  registry.Add(std::make_unique<BoxBoxFastPath>());
  registry.Add(std::make_unique<PrismBoxFastPath>());
  registry.Add(std::make_unique<SphereBoxFastPath>());
  registry.Add(std::make_unique<SphereSphereFastPath>());
  return registry;
}

}  // namespace brep::boolean
