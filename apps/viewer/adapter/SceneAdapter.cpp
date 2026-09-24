#include "adapter/SceneAdapter.h"

#include "api/Base.h"
#include "api/Mesh.h"
#include "api/Modeling.h"

#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace brep::viewer::adapter
{
namespace
{

template <class F>
decltype(auto) TraceScene(const char* site, const std::string& subject, F&& body)
{
    brep::AspectEvent event;
    event.Site = site;
    event.Subject = subject;
    return brep::ProcessAspectChain().Invoke(event, std::forward<F>(body));
}

std::string PrimitiveName(const PrimitiveSpec& spec)
{
    return std::visit([](const auto& arm) { return arm.Name; }, spec);
}

}  // namespace

brep::Part* SceneAdapter::MainPart() const noexcept
{
    return m_document ? m_document->MainPart() : nullptr;
}

const brep::Part* SceneAdapter::MainPartConst() const noexcept
{
    return m_document ? m_document->MainPart() : nullptr;
}

MeshBundle SceneAdapter::MeshForBody(
    const Guid& bodyGuid, const brep::io::BodyMeshCache* cache) const
{
    MeshBundle out;
    const Part* part = MainPartConst();
    if (!part)
    {
        return out;
    }

    if (cache && cache->Has(bodyGuid))
    {
        out.Faces = cache->Triangles.at(bodyGuid);
        out.Edges = cache->Edges.at(bodyGuid);
        return out;
    }

    const Body* body = part->FindBody(bodyGuid);
    if (!body)
    {
        return out;
    }
    out.Faces = TessellateBody(*body);
    out.Edges = ExtractEdges(*body);
    return out;
}

std::optional<SceneObject> SceneAdapter::ObjectForFeature(
    feat::FeatureId id) const
{
    const Part* part = MainPartConst();
    if (!part || !id.IsValid())
    {
        return std::nullopt;
    }
    const auto* f = part->Features().Find(id);
    if (!f)
    {
        return std::nullopt;
    }

    SceneObject obj;
    obj.FeatureGuid = f->Id().Guid;
    obj.BodyGuid = f->BodyGuid();
    obj.Name = std::string(f->DisplayName());
    obj.TypeName = std::string(f->TypeName());
    return obj;
}

std::optional<SceneObject> SceneAdapter::ObjectForBody(
    const Guid& bodyGuid) const
{
    const Part* part = MainPartConst();
    if (!part)
    {
        return std::nullopt;
    }
    const auto* f = part->Features().FindByBody(bodyGuid);
    if (!f)
    {
        const Body* body = part->FindBody(bodyGuid);
        if (!body)
        {
            return std::nullopt;
        }
        SceneObject obj;
        obj.BodyGuid = bodyGuid;
        obj.Name = body->Name;
        obj.TypeName = "Body";
        return obj;
    }
    return ObjectForFeature(f->Id());
}

Body* SceneAdapter::AddPrimitive(const PrimitiveSpec& spec)
{
    const std::string subject = PrimitiveName(spec);
    return TraceScene("scene.addPrimitive", subject, [&]() -> Body* {
        Part* part = MainPart();
        if (!part)
        {
            return nullptr;
        }
        return part->AddPrimitive(spec);
    });
}

void SceneAdapter::RecordAppendPrimitive(feat::FeatureId id, PrimitiveSpec undo)
{
    Part* part = MainPart();
    if (!part || !id.IsValid())
    {
        return;
    }
    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.Feature = id;
    std::visit(
        [&](auto&& spec)
        {
            using T = std::decay_t<decltype(spec)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                tx.FeatureType = "Box";
                tx.Box = std::forward<decltype(spec)>(spec);
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                tx.FeatureType = "Sphere";
                tx.Sphere = std::forward<decltype(spec)>(spec);
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                tx.FeatureType = "Bezier";
                tx.Bezier = std::forward<decltype(spec)>(spec);
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "RecordAppendPrimitive: add PrimitiveSpec arm");
                tx.FeatureType = "NurbsCurve";
                tx.Nurbs = std::forward<decltype(spec)>(spec);
            }
        },
        undo);
    part->FeatureHistory().Record(std::move(tx));
}

bool SceneAdapter::SetPrimitive(feat::FeatureId id, const PrimitiveSpec& spec)
{
    Part* part = MainPart();
    if (!part || !id.IsValid())
    {
        return false;
    }
    feat::IFeature* feature = part->Features().Find(id);
    if (!feature)
    {
        return false;
    }

    return std::visit(
        [&](const auto& value) -> bool
        {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, BoxSpec>)
            {
                if (feature->TypeName() != "Box")
                {
                    return false;
                }
                auto* box = static_cast<feat::BoxFeature*>(feature);
                const double length = value.Max.x() - value.Min.x();
                const double height = value.Max.y() - value.Min.y();
                const double width = value.Max.z() - value.Min.z();
                if (length <= 1e-12 || width <= 1e-12 || height <= 1e-12)
                {
                    return false;
                }
                box->SetOrigin(value.Min);
                return part->EditFeatureParams(id, {{"Length", length},
                                                    {"Width", width},
                                                    {"Height", height}});
            }
            else if constexpr (std::is_same_v<T, SphereSpec>)
            {
                if (feature->TypeName() != "Sphere")
                {
                    return false;
                }
                auto* sphere = static_cast<feat::SphereFeature*>(feature);
                if (!(value.Radius > 1e-12))
                {
                    return false;
                }
                sphere->SetCenter(value.Center);
                return part->EditFeatureParams(id, {{"Radius", value.Radius}});
            }
            else if constexpr (std::is_same_v<T, BezierSpec>)
            {
                if (feature->TypeName() != "Bezier")
                {
                    return false;
                }
                auto* bezier = static_cast<feat::BezierCurveFeature*>(feature);
                feat::FeatureTransaction tx;
                tx.Kind = feat::TxKind::EditParameters;
                tx.Feature = id;
                tx.FeatureType = "Bezier";
                tx.BezierBefore = bezier->ToSpec();
                tx.Bezier = value;
                if (!value.Name.empty())
                {
                    tx.Bezier.Name = value.Name;
                }
                else
                {
                    tx.Bezier.Name = tx.BezierBefore.Name;
                }
                part->FeatureHistory().ApplyAndRecord(*part, std::move(tx));
                return true;
            }
            else
            {
                static_assert(std::is_same_v<T, NurbsCurveSpec>,
                              "SetPrimitive: add PrimitiveSpec arm");
                if (feature->TypeName() != "NurbsCurve")
                {
                    return false;
                }
                auto* nurbs = static_cast<feat::NurbsCurveFeature*>(feature);
                feat::FeatureTransaction tx;
                tx.Kind = feat::TxKind::EditParameters;
                tx.Feature = id;
                tx.FeatureType = "NurbsCurve";
                tx.NurbsBefore = nurbs->ToSpec();
                tx.Nurbs = value;
                if (!value.Name.empty())
                {
                    tx.Nurbs.Name = value.Name;
                }
                else
                {
                    tx.Nurbs.Name = tx.NurbsBefore.Name;
                }
                part->FeatureHistory().ApplyAndRecord(*part, std::move(tx));
                return true;
            }
        },
        spec);
}

Body* SceneAdapter::AddBoolean(brep::boolean::BooleanOp op,
                               feat::FeatureId target, feat::FeatureId tool,
                               std::string name)
{
    const std::string subject = name;
    return TraceScene("scene.addBoolean", subject, [&]() -> Body* {
        Part* part = MainPart();
        if (!part)
        {
            return nullptr;
        }
        return part->AddBoolean(op, target, tool, name);
    });
}

Body* SceneAdapter::AddExtrudePad(const std::vector<Point2d>& profile,
                                  double distance, bool symmetric,
                                  std::string name)
{
    const std::string subject = name;
    return TraceScene("scene.addExtrude", subject, [&]() -> Body* {
        Part* part = MainPart();
        if (!part)
        {
            return nullptr;
        }
        const ExtrudePadResult result =
            part->AddExtrudePad(profile, distance, symmetric, name);
        return result.Body;
    });
}

std::optional<BooleanParams> SceneAdapter::BooleanParamsFor(
    feat::FeatureId id) const
{
    const Part* part = MainPartConst();
    if (!part || !id.IsValid())
    {
        return std::nullopt;
    }
    const auto* f = part->Features().Find(id);
    if (!f || f->TypeName() != "Boolean")
    {
        return std::nullopt;
    }
    const auto& bf = static_cast<const feat::BooleanFeature&>(*f);
    return BooleanParams{.Op = bf.Op()};
}

std::optional<feat::FeatureId> SceneAdapter::FeatureIdFor(
    Guid featureGuid, Guid bodyGuid) const
{
    const feat::IFeature* f = FindFeature(featureGuid, bodyGuid);
    if (!f)
    {
        return std::nullopt;
    }
    return f->Id();
}

bool SceneAdapter::RemoveFeature(feat::FeatureId id)
{
    const std::string subject = id.Guid.ToString();
    return TraceScene("scene.removeFeature", subject, [&] {
    Part* part = MainPart();
    if (!part || !id.IsValid())
    {
        return false;
    }
    feat::IFeature* f = part->Features().Find(id);
    if (!f)
    {
        return false;
    }

    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::RemoveFeature;
    tx.Feature = id;
    tx.FeatureType = std::string(f->TypeName());
    tx.SketchName = f->DisplayName();

    if (tx.FeatureType == "Box")
    {
        const auto& box = static_cast<const feat::BoxFeature&>(*f);
        tx.Box = box.ToSpec(part->Parameters());
        tx.BoxOrigin = box.Origin();
    }
    else if (tx.FeatureType == "Sphere")
    {
        const auto& sph = static_cast<const feat::SphereFeature&>(*f);
        tx.Sphere = sph.ToSpec(part->Parameters());
    }
    else if (tx.FeatureType == "Extrude")
    {
        const auto& ext = static_cast<const feat::ExtrudeFeature&>(*f);
        tx.SketchFeatureId = ext.SketchFeatureId();
        tx.ExtrudeDistance =
            part->Parameters().Get(ext.DistanceId()).value_or(1.0);
    }
    else if (tx.FeatureType == "Boolean")
    {
        const auto& bf = static_cast<const feat::BooleanFeature&>(*f);
        tx.Op = bf.Op();
        tx.TargetFeatureId = bf.TargetFeatureId();
        tx.ToolFeatureId = bf.ToolFeatureId();
    }
    else if (tx.FeatureType == "Bezier")
    {
        const auto& bez = static_cast<const feat::BezierCurveFeature&>(*f);
        tx.Bezier = bez.ToSpec();
    }
    else if (tx.FeatureType == "NurbsCurve")
    {
        const auto& nurbs = static_cast<const feat::NurbsCurveFeature&>(*f);
        tx.Nurbs = nurbs.ToSpec();
    }

    part->FeatureHistory().ApplyAndRecord(*part, std::move(tx));
    return true;
    });
}

void SceneAdapter::UndoFeature(int steps)
{
    TraceScene("scene.undo", std::to_string(steps), [&] {
    Part* part = MainPart();
    if (!part)
    {
        return;
    }
    for (int i = 0; i < steps; ++i)
    {
        if (!part->FeatureHistory().Undo(*part))
        {
            break;
        }
    }
    });
}

void SceneAdapter::RedoFeature(int steps)
{
    TraceScene("scene.redo", std::to_string(steps), [&] {
    Part* part = MainPart();
    if (!part)
    {
        return;
    }
    for (int i = 0; i < steps; ++i)
    {
        if (!part->FeatureHistory().Redo(*part))
        {
            break;
        }
    }
    });
}

std::optional<PrimitiveSpec> SceneAdapter::SpecFor(Guid featureGuid,
                                                   Guid bodyGuid) const
{
    const Part* part = MainPartConst();
    const feat::IFeature* f = FindFeature(featureGuid, bodyGuid);
    if (!part || !f)
    {
        return std::nullopt;
    }
    return f->ToPrimitiveSpec(part->Parameters());
}

Body* SceneAdapter::DuplicateBody(Guid bodyGuid, RigidTransform transform)
{
    const std::string subject = bodyGuid.ToString();
    return TraceScene("scene.duplicate", subject, [&]() -> Body* {
        Part* part = MainPart();
        if (!part)
        {
            return nullptr;
        }
        return part->DuplicateBody(bodyGuid, transform);
    });
}

bool SceneAdapter::TransformBody(Guid bodyGuid, RigidTransform transform)
{
    const std::string subject = bodyGuid.ToString();
    return TraceScene("scene.transform", subject, [&] {
        Part* part = MainPart();
        if (!part)
        {
            return false;
        }
        return part->TransformBody(bodyGuid, transform);
    });
}

const feat::IFeature* SceneAdapter::FindFeature(Guid featureGuid,
                                               Guid bodyGuid) const
{
    const Part* part = MainPartConst();
    if (!part)
    {
        return nullptr;
    }
    const feat::IFeature* f = nullptr;
    if (featureGuid.IsValid())
    {
        f = part->Features().Find(feat::FeatureId{featureGuid});
        if (f && bodyGuid.IsValid() && f->BodyGuid().IsValid() &&
            f->BodyGuid() != bodyGuid)
        {
            f = nullptr;
        }
    }
    if (!f && bodyGuid.IsValid())
    {
        f = part->Features().FindByBody(bodyGuid);
    }
    return f;
}

std::unique_ptr<brep::Document> SceneAdapter::CreateBlank(
    const std::string& name,
    std::shared_ptr<brep::boolean::IBooleanEvaluator> evaluator)
{
    auto doc = brep::Document::Create(name, std::move(evaluator));
    doc->AddPart("MainPart");
    return doc;
}

}  // namespace brep::viewer::adapter
