#include "brep/Part.h"

#include "brep/Aspect.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/Document.h"
#include "brep/feat/BooleanFeature.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/CopiedBodyFeature.h"
#include "brep/feat/ExtrudeFeature.h"
#include "brep/feat/SketchFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/feat/BezierCurveFeature.h"
#include "brep/Log.h"
#include "brep/ops/Profile.h"

#include <stdexcept>
#include <type_traits>
#include <variant>

namespace brep
{

Part::Part(std::shared_ptr<boolean::IBooleanEvaluator> evaluator,
           std::string part_name)
    : IObject(std::move(part_name)),
      m_booleanEvaluator(std::move(evaluator))
{
  if (!m_booleanEvaluator)
  {
    throw std::invalid_argument("Part requires non-null IBooleanEvaluator");
  }
}

Body* Part::FindBody(const brep::Guid& guid)
{
  for (const auto& body : m_model.Bodies())
{
    if (body && body->Guid == guid) return body.get();
  }
  return nullptr;
}

const Body* Part::FindBody(const brep::Guid& guid) const
{
  for (const auto& body : m_model.Bodies())
{
    if (body && body->Guid == guid) return body.get();
  }
  return nullptr;
}

void Part::RegisterBody(Body& body)
{
  if (!m_document)
{
    BREP_WARN("Part::register_body: part '{}' has no Document; Guid {} not "
              "registered",
              Name, body.Guid.ToString());
    return;
  }
  m_document->Registry().Add(body);
  m_document->MarkDirty();
  BREP_INFO("registered Body '{}' guid={} on Document '{}'", body.Name,
            body.Guid.ToString(), m_document->Name);
}

void Part::UnregisterBody(const brep::Guid& guid)
{
  if (m_document) m_document->Registry().Remove(guid);
}

Body* Part::RebuildBoxBody(brep::Guid keepGuid, const BoxSpec& spec)
{
    if (keepGuid.IsValid())
    {
        UnregisterBody(keepGuid);
        m_model.RemoveBody(keepGuid);
    }
    Body* body = MakeBox(m_model, spec);
    if (keepGuid.IsValid()) body->Guid = keepGuid;
    RegisterBody(*body);
    return body;
}

Body* Part::RebuildSphereBody(brep::Guid keepGuid, const SphereSpec& spec)
{
    if (keepGuid.IsValid())
    {
        UnregisterBody(keepGuid);
        m_model.RemoveBody(keepGuid);
    }
    Body* body = MakeSphere(m_model, spec);
    if (keepGuid.IsValid()) body->Guid = keepGuid;
    RegisterBody(*body);
    return body;
}

Body* Part::RebuildBezierBody(brep::Guid keepGuid, const BezierSpec& spec)
{
    if (keepGuid.IsValid())
    {
        UnregisterBody(keepGuid);
        m_model.RemoveBody(keepGuid);
    }
    Body* body = MakeBezierWire(m_model, spec);
    if (!body) return nullptr;
    if (keepGuid.IsValid()) body->Guid = keepGuid;
    RegisterBody(*body);
    return body;
}

Body* Part::RebuildExtrudeBody(brep::Guid keepGuid, const ops::ExtrudeSpec& spec)
{
    if (keepGuid.IsValid())
    {
        UnregisterBody(keepGuid);
        m_model.RemoveBody(keepGuid);
    }
    Body* body = ops::Extrude(m_model, spec);
    if (!body) return nullptr;
    if (keepGuid.IsValid()) body->Guid = keepGuid;
    RegisterBody(*body);
    return body;
}

Body* Part::RebuildBooleanBody(brep::Guid keepGuid, Body* resultBody)
{
    if (!resultBody) return nullptr;
    if (keepGuid.IsValid() && resultBody->Guid != keepGuid)
    {
        UnregisterBody(keepGuid);
        m_model.RemoveBody(keepGuid);
        resultBody->Guid = keepGuid;
    }
    RegisterBody(*resultBody);
    return resultBody;
}

boolean::IBooleanEvaluator& Part::BooleanEvaluator()
{
  return *m_booleanEvaluator;
}

Body* Part::MaterializeFeatureBody(feat::IFeature& feature,
                                   param::ParameterStore& params,
                                   brep::Model& scratch)
{
  if (Body* live = FindBody(feature.BodyGuid()))
  {
    return live;
  }

  if (std::optional<PrimitiveSpec> spec = feature.ToPrimitiveSpec(params))
  {
    return std::visit(
        [&](const auto& primitive) -> Body*
        {
          using T = std::decay_t<decltype(primitive)>;
          if constexpr (std::is_same_v<T, BoxSpec>)
          {
            return MakeBox(scratch, primitive);
          }
          if constexpr (std::is_same_v<T, SphereSpec>)
          {
            return MakeSphere(scratch, primitive);
          }
          if constexpr (std::is_same_v<T, BezierSpec>)
          {
            return MakeBezierWire(scratch, primitive);
          }
          if constexpr (std::is_same_v<T, NurbsCurveSpec>)
          {
            return MakeNurbsCurveWire(scratch, primitive);
          }
          return nullptr;
        },
        *spec);
  }

  if (feature.TypeName() == "Extrude")
  {
    auto& ext = static_cast<feat::ExtrudeFeature&>(feature);
    feat::IFeature* sketchBase = m_features.Find(ext.SketchFeatureId());
    if (!sketchBase || sketchBase->TypeName() != "Sketch")
    {
      return nullptr;
    }
    auto* sketchFeat = static_cast<feat::SketchFeature*>(sketchBase);
    ops::ExtrudeSpec extrudeSpec;
    extrudeSpec.Profile = ops::ExtractProfile(sketchFeat->Sketch());
    extrudeSpec.Plane = sketchFeat->Sketch().Frame();
    extrudeSpec.Distance = params.Get(ext.DistanceId()).value_or(1.0);
    extrudeSpec.Symmetric = ext.Symmetric();
    extrudeSpec.Name = feature.DisplayName() + "_wk";
    return ops::Extrude(scratch, extrudeSpec);
  }

  if (feature.TypeName() == "CopiedBody")
  {
    auto& copyFeat = static_cast<feat::CopiedBodyFeature&>(feature);
    feat::IFeature* source = m_features.Find(copyFeat.SourceFeatureId());
    if (!source)
    {
      return nullptr;
    }
    Body* sourceBody = MaterializeFeatureBody(*source, params, scratch);
    if (!sourceBody || !copyFeat.Transform().IsTranslation())
    {
      return nullptr;
    }
    boolean::TopologyCopyContext ctx{scratch};
    Body* copied = boolean::CopyBodySubgraph(ctx, *sourceBody, "_wk_copy");
    if (!copied)
    {
      return nullptr;
    }
    if (!boolean::ApplyTransformToCopied(ctx, copyFeat.Transform()))
    {
      scratch.RemoveBody(copied->Guid);
      return nullptr;
    }
    return copied;
  }

  BREP_WARN("Part::MaterializeFeatureBody: unsupported feature '{}'",
            feature.TypeName());
  return nullptr;
}

feat::RegenResult Part::Regenerate()
{
  m_lastRegenError.clear();
  AspectEvent event;
  event.Site = "part.regenerate";
  event.Subject = Name;
  std::string detail;
  return ProcessAspectChain().Invoke(event, [&] {
    feat::RegenResult result = feat::Regenerator::Run(*this, m_features, m_params);
    if (!result.Ok)
    {
      m_lastRegenError = result.Message;
      detail = m_lastRegenError;
      event.Failed = true;
      event.Detail = detail;
    }
    return result;
  });
}

feat::FeatureId Part::AppendFeature(std::unique_ptr<feat::IFeature> feature)
{
  if (!feature)
  {
    return {};
  }
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  const auto result = Regenerate();
  if (!result.Ok)
  {
    BREP_WARN("Part::AppendFeature regenerate failed: {}", result.Message);
    m_features.Remove(fid);
    return {};
  }
  return fid;
}

Body* Part::BodyForFeature(feat::FeatureId id)
{
  const auto* feature = m_features.Find(id);
  if (!feature)
  {
    return nullptr;
  }
  return FindBody(feature->BodyGuid());
}

Body* Part::AddBox(const BoxSpec& spec)
{
  const feat::FeatureId fid =
      AppendFeature(feat::BoxFeature::Create(m_params, spec));
  return BodyForFeature(fid);
}

Body* Part::AddSphere(const SphereSpec& spec)
{
  const feat::FeatureId fid =
      AppendFeature(feat::SphereFeature::Create(m_params, spec));
  return BodyForFeature(fid);
}

Body* Part::AddBezier(const BezierSpec& spec)
{
  const feat::FeatureId fid =
      AppendFeature(feat::BezierCurveFeature::Create(m_params, spec));
  return BodyForFeature(fid);
}

Body* Part::AddNurbsCurve(const NurbsCurveSpec& /*spec*/)
{
  return nullptr;
}

Body* Part::AddPrimitive(const PrimitiveSpec& spec)
{
    return std::visit(
        [this](const auto& s) -> Body*
        {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                return AddBox(s);
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                return AddSphere(s);
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                return AddBezier(s);
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "AddPrimitive: add PrimitiveSpec arm");
                return AddNurbsCurve(s);
            }
        },
        spec);
}

feat::FeatureId Part::AddRectangleSketch(std::string name, Point2d min,
                                           Point2d max)
{
  auto feature =
      feat::SketchFeature::CreateRectangle(m_params, std::move(name), min, max);
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  Regenerate();
  return fid;
}

Body* Part::AddExtrude(feat::FeatureId sketch_feature, double distance,
                        std::string name, bool symmetric)
{
  auto feature = feat::ExtrudeFeature::Create(m_params, std::move(name),
                                              sketch_feature, distance, symmetric);
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  const auto result = Regenerate();
  if (!result.Ok) return nullptr;
  auto* f = m_features.Find(fid);
  if (!f) return nullptr;
  return FindBody(f->BodyGuid());
}

ExtrudePadResult Part::AddExtrudePad(const std::vector<Point2d>& profile,
                                     double distance, bool symmetric,
                                     std::string name)
{
  ExtrudePadResult out;
  if (profile.size() < 3 || std::abs(distance) < 1e-12)
  {
    return out;
  }

  auto sketch =
      feat::SketchFeature::CreatePolyline(name + "_Sketch", profile);
  if (!sketch)
  {
    return out;
  }
  const feat::FeatureId sketch_id = m_features.Append(std::move(sketch));
  if (!Regenerate().Ok)
  {
    m_features.Remove(sketch_id);
    return out;
  }

  auto extrude = feat::ExtrudeFeature::Create(m_params, name, sketch_id,
                                              distance, symmetric);
  const feat::FeatureId extrude_id = m_features.Append(std::move(extrude));
  const auto result = Regenerate();
  if (!result.Ok)
  {
    m_features.Remove(extrude_id);
    m_features.Remove(sketch_id);
    return out;
  }

  feat::FeatureTransaction tx;
  tx.Kind = feat::TxKind::AppendFeature;
  tx.FeatureType = "ExtrudePad";
  tx.Feature = extrude_id;
  tx.ExtrudePadSketchId = sketch_id;
  tx.ExtrudeDistance = distance;
  tx.ExtrudeSymmetric = symmetric;
  tx.SketchPolyline = profile;
  tx.SketchFrame = Plane::XzYUp();
  tx.SketchName = std::move(name);
  m_history.Record(std::move(tx));

  auto* f = m_features.Find(extrude_id);
  if (!f)
  {
    return out;
  }
  out.SketchId = sketch_id;
  out.ExtrudeId = extrude_id;
  out.Body = FindBody(f->BodyGuid());
  return out;
}

Body* Part::AddBoolean(boolean::BooleanOp op, feat::FeatureId target,
                        feat::FeatureId tool, std::string name)
{
  auto* target_f = m_features.Find(target);
  auto* tool_f = m_features.Find(tool);
  if (!target_f || !tool_f)
  {
    BREP_WARN("Part::add_boolean: missing target/tool feature");
    return nullptr;
  }

  feat::FeatureTransaction tx;
  tx.Kind = feat::TxKind::AppendFeature;
  tx.FeatureType = "Boolean";
  tx.Op = op;
  tx.TargetFeatureId = target;
  tx.ToolFeatureId = tool;
  tx.TargetWasSuppressed = target_f->Suppressed();
  tx.ToolWasSuppressed = tool_f->Suppressed();
  tx.SketchName = name;

  auto feature =
      feat::BooleanFeature::Create(op, target, tool, std::move(name));
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  tx.Feature = fid;

  const auto result = Regenerate();
  if (!result.Ok)
  {
    BREP_WARN("Part::add_boolean regenerate failed: {}", result.Message);
    m_features.Remove(fid);
    if (auto* t = m_features.Find(target))
    {
      t->SetSuppressed(tx.TargetWasSuppressed);
    }
    if (auto* t = m_features.Find(tool))
    {
      t->SetSuppressed(tx.ToolWasSuppressed);
    }
    return nullptr;
  }

  m_history.Record(std::move(tx));
  auto* f = m_features.Find(fid);
  if (!f) return nullptr;
  return FindBody(f->BodyGuid());
}

Body* Part::DuplicateBody(brep::Guid sourceBodyGuid, RigidTransform transform,
                          std::string name)
{
    if (!transform.IsTranslation())
    {
        BREP_WARN("Part::DuplicateBody: only translation is supported");
        return nullptr;
    }
    if (!FindBody(sourceBodyGuid))
    {
        BREP_WARN("Part::DuplicateBody: body not found");
        return nullptr;
    }
    auto* sourceFeat = m_features.FindByBody(sourceBodyGuid);
    if (!sourceFeat)
    {
        BREP_WARN("Part::DuplicateBody: body has no feature");
        return nullptr;
    }

    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.FeatureType = "CopiedBody";
    tx.CopiedSource = sourceFeat->Id();
    tx.CopiedTransform = transform;
    tx.SketchName = name;

    auto feature = feat::CopiedBodyFeature::Create(sourceFeat->Id(), transform,
                                                   std::move(name));
    const feat::FeatureId fid = m_features.Append(std::move(feature));
    tx.Feature = fid;

    const auto result = Regenerate();
    if (!result.Ok)
    {
        BREP_WARN("Part::DuplicateBody regenerate failed: {}", result.Message);
        m_features.Remove(fid);
        return nullptr;
    }

    m_history.Record(std::move(tx));
    return BodyForFeature(fid);
}

bool Part::TransformBody(brep::Guid bodyGuid, RigidTransform transform,
                         bool recordHistory)
{
    if (!transform.IsTranslation())
    {
        BREP_WARN("Part::TransformBody: only translation is supported");
        return false;
    }
    auto* feature = m_features.FindByBody(bodyGuid);
    if (!feature)
    {
        return false;
    }
    const Vector3d offset{transform.Translation.x(), transform.Translation.y(),
                          transform.Translation.z()};
    if (feature->TypeName() == "Box")
    {
        auto* box = static_cast<feat::BoxFeature*>(feature);
        box->SetOrigin(box->Origin() + offset);
    }
    else if (feature->TypeName() == "Sphere")
    {
        auto* sphere = static_cast<feat::SphereFeature*>(feature);
        sphere->SetCenter(sphere->Center() + offset);
    }
    else if (feature->TypeName() == "Bezier")
    {
        auto* bezier = static_cast<feat::BezierCurveFeature*>(feature);
        BezierSpec spec = bezier->ToSpec();
        for (Point3d& p : spec.Cvs)
        {
            p = p + offset;
        }
        bezier->SetFromSpec(spec);
    }
    else
    {
        return false;
    }
    m_features.MarkDirtyFrom(feature->Id());
    if (!Regenerate().Ok)
    {
        return false;
    }
    if (recordHistory)
    {
        feat::FeatureTransaction tx;
        tx.Kind = feat::TxKind::TransformBody;
        tx.Feature = feature->Id();
        tx.CopiedTransform = transform;
        m_history.Record(std::move(tx));
    }
    return true;
}

bool Part::RemoveFeature(feat::FeatureId id)
{
  feat::IFeature* f = m_features.Find(id);
  if (!f) return false;

  if (f->TypeName() == "Boolean")
  {
    auto* boolean_f = static_cast<feat::BooleanFeature*>(f);
    if (auto* target = m_features.Find(boolean_f->TargetFeatureId()))
    {
      if (target->Suppressed())
      {
        target->SetSuppressed(false);
        target->SetStatus(feat::FeatureStatus::Dirty);
      }
    }
    if (auto* tool = m_features.Find(boolean_f->ToolFeatureId()))
    {
      if (tool->Suppressed())
      {
        tool->SetSuppressed(false);
        tool->SetStatus(feat::FeatureStatus::Dirty);
      }
    }
  }

  const brep::Guid bodyGuid = f->BodyGuid();
  if (bodyGuid.IsValid())
  {
    UnregisterBody(bodyGuid);
    m_model.RemoveBody(bodyGuid);
  }
  m_features.Remove(id);
  m_features.MarkAllDirty();
  Regenerate();
  if (m_document) m_document->MarkDirty();
  return true;
}

bool Part::EditFeatureParams(
    feat::FeatureId id,
    std::initializer_list<std::pair<std::string_view, double>> named_vals)
    {
  auto* feature = m_features.Find(id);
  if (!feature) return false;

  feat::FeatureTransaction tx;
  tx.Kind = feat::TxKind::EditParameters;
  tx.Feature = id;
  tx.FeatureType = std::string(feature->TypeName());

  if (feature->TypeName() == "Box")
  {
    auto* box = static_cast<feat::BoxFeature*>(feature);
    const param::ParameterId ids[3] = {box->LengthId(), box->WidthId(),
                                       box->HeightId()};
    const char* keys[3] = {"Length", "Width", "Height"};
    for (const auto& [key, value] : named_vals)
    {
      for (int i = 0; i < 3; ++i)
      {
        if (key == keys[i])
        {
          tx.ParamBefore.emplace_back(ids[i],
                                      m_params.Get(ids[i]).value_or(0.0));
          tx.ParamAfter.emplace_back(ids[i], value);
        }
      }
    }
  }
  else if (feature->TypeName() == "Sphere")
  {
    auto* sph = static_cast<feat::SphereFeature*>(feature);
    for (const auto& [key, value] : named_vals)
    {
      if (key == "Radius")
      {
        tx.ParamBefore.emplace_back(
            sph->RadiusId(), m_params.Get(sph->RadiusId()).value_or(0.0));
        tx.ParamAfter.emplace_back(sph->RadiusId(), value);
      }
    }
  }
  else
  {
    for (const auto& [key, value] : named_vals)
    {
      if (auto* p = m_params.FindByName(std::string(feature->DisplayName()) +
                                        "." + std::string(key)))
      {
        tx.ParamBefore.emplace_back(p->Id, p->Value);
        tx.ParamAfter.emplace_back(p->Id, value);
      }
    }
  }

  if (tx.ParamAfter.empty()) return false;
  m_history.ApplyAndRecord(*this, std::move(tx));
  return true;
}

}  // namespace brep
