#include "brep/Part.h"

#include "brep/Document.h"
#include "brep/feat/BooleanFeature.h"
#include "brep/feat/BoxFeature.h"
#include "brep/feat/ExtrudeFeature.h"
#include "brep/feat/SketchFeature.h"
#include "brep/feat/SphereFeature.h"
#include "brep/Log.h"
#include "brep/ops/Profile.h"

namespace brep
{

Part::Part(std::string part_name) : IObject(std::move(part_name))
{
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

void Part::SetBooleanEvaluator(
    std::shared_ptr<boolean::IBooleanEvaluator> evaluator)
{
  m_booleanEvaluator = std::move(evaluator);
}

boolean::IBooleanEvaluator& Part::BooleanEvaluator()
{
  if (!m_booleanEvaluator)
{
    m_booleanEvaluator = boolean::MakeDefaultBooleanEvaluator();
  }
  return *m_booleanEvaluator;
}

feat::RegenResult Part::Regenerate()
{
  return feat::Regenerator::Run(*this, m_features, m_params);
}

Body* Part::AddBox(const BoxSpec& spec)
{
  auto feature = feat::BoxFeature::Create(m_params, spec);
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  const auto result = Regenerate();
  if (!result.Ok)
  {
    BREP_WARN("Part::add_box regenerate failed: {}", result.Message);
    return nullptr;
  }
  auto* f = m_features.Find(fid);
  if (!f) return nullptr;
  return FindBody(f->BodyGuid());
}

Body* Part::AddSphere(const SphereSpec& spec)
{
  auto feature = feat::SphereFeature::Create(m_params, spec);
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  const auto result = Regenerate();
  if (!result.Ok)
  {
    BREP_WARN("Part::add_sphere regenerate failed: {}", result.Message);
    return nullptr;
  }
  auto* f = m_features.Find(fid);
  if (!f) return nullptr;
  return FindBody(f->BodyGuid());
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
                        std::string name)
{
  auto feature = feat::ExtrudeFeature::Create(m_params, std::move(name),
                                              sketch_feature, distance);
  const feat::FeatureId fid = m_features.Append(std::move(feature));
  const auto result = Regenerate();
  if (!result.Ok) return nullptr;
  auto* f = m_features.Find(fid);
  if (!f) return nullptr;
  return FindBody(f->BodyGuid());
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
