#include "adapter/SceneAdapter.h"

#include "api/Mesh.h"
#include "api/Modeling.h"

#include <utility>

namespace brep::viewer::adapter
{

brep::Part* SceneAdapter::main_part() const noexcept
{
    return m_document ? m_document->MainPart() : nullptr;
}

const brep::Part* SceneAdapter::main_part_const() const noexcept
{
    return m_document ? m_document->MainPart() : nullptr;
}

MeshBundle SceneAdapter::mesh_for_body(
    const Guid& body_guid, const brep::io::BodyMeshCache* cache) const
{
    MeshBundle out;
    const Part* part = main_part_const();
    if (!part)
    {
        return out;
    }

    if (cache && cache->Has(body_guid))
    {
        out.faces = cache->Triangles.at(body_guid);
        out.edges = cache->Edges.at(body_guid);
        return out;
    }

    const Body* body = part->FindBody(body_guid);
    if (!body)
    {
        return out;
    }
    out.faces = TessellateBody(*body);
    out.edges = ExtractEdges(*body);
    return out;
}

const feat::IFeature* SceneAdapter::find_box_feature(
    feat::FeatureId id) const
{
    const Part* part = main_part_const();
    if (!part || !id.IsValid())
    {
        return nullptr;
    }
    const auto* f = part->Features().Find(id);
    if (!f || f->TypeName() != "Box")
    {
        return nullptr;
    }
    return f;
}

std::optional<BoxParams> SceneAdapter::box_params_from_feature(
    const feat::IFeature& feature) const
{
    const Part* part = main_part_const();
    if (!part || feature.TypeName() != "Box")
    {
        return std::nullopt;
    }
    const auto& box = static_cast<const feat::BoxFeature&>(feature);
    const auto& params = part->Parameters();
    return BoxParams{
        .length = params.Get(box.LengthId()).value_or(0.0),
        .width = params.Get(box.WidthId()).value_or(0.0),
        .height = params.Get(box.HeightId()).value_or(0.0),
    };
}

std::optional<SceneObject> SceneAdapter::object_for_feature(
    feat::FeatureId id) const
{
    const Part* part = main_part_const();
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
    obj.feature_guid = f->Id().Guid;
    obj.body_guid = f->BodyGuid();
    obj.name = std::string(f->DisplayName());
    obj.type_name = std::string(f->TypeName());
    if (f->TypeName() == "Box")
    {
        obj.box = box_params_from_feature(*f);
    }
    else if (f->TypeName() == "Sphere")
    {
        const auto& sph = static_cast<const feat::SphereFeature&>(*f);
        obj.sphere = SphereParams{
            .radius = part->Parameters().Get(sph.RadiusId()).value_or(0.0),
        };
    }
    else if (f->TypeName() == "Boolean")
    {
        const auto& bf = static_cast<const feat::BooleanFeature&>(*f);
        obj.boolean_info = BooleanParams{.op = bf.Op()};
    }
    return obj;
}

std::optional<SceneObject> SceneAdapter::object_for_body(
    const Guid& body_guid) const
{
    const Part* part = main_part_const();
    if (!part)
    {
        return std::nullopt;
    }
    const auto* f = part->Features().FindByBody(body_guid);
    if (!f)
    {
        const Body* body = part->FindBody(body_guid);
        if (!body)
        {
            return std::nullopt;
        }
        SceneObject obj;
        obj.body_guid = body_guid;
        obj.name = body->Name;
        obj.type_name = "Body";
        return obj;
    }
    return object_for_feature(f->Id());
}

std::optional<BoxParams> SceneAdapter::box_params(feat::FeatureId id) const
{
    const auto* f = find_box_feature(id);
    if (!f)
    {
        return std::nullopt;
    }
    return box_params_from_feature(*f);
}

bool SceneAdapter::set_box_params(feat::FeatureId id, const BoxParams& params)
{
    Part* part = main_part();
    if (!part || !find_box_feature(id))
    {
        return false;
    }
    return part->EditFeatureParams(id, {{"Length", params.length},
                                        {"Width", params.width},
                                        {"Height", params.height}});
}

Body* SceneAdapter::add_box(const BoxSpec& spec)
{
    Part* part = main_part();
    if (!part)
    {
        return nullptr;
    }
    return part->AddBox(spec);
}

void SceneAdapter::record_append_feature(feat::FeatureId id, BoxSpec undo_spec)
{
    Part* part = main_part();
    if (!part || !id.IsValid())
    {
        return;
    }
    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.Feature = id;
    tx.FeatureType = "Box";
    tx.Box = std::move(undo_spec);
    part->FeatureHistory().Record(std::move(tx));
}

Body* SceneAdapter::add_sphere(const SphereSpec& spec)
{
    Part* part = main_part();
    if (!part)
    {
        return nullptr;
    }
    return part->AddSphere(spec);
}

void SceneAdapter::record_append_sphere(feat::FeatureId id,
                                        SphereSpec undo_spec)
{
    Part* part = main_part();
    if (!part || !id.IsValid())
    {
        return;
    }
    feat::FeatureTransaction tx;
    tx.Kind = feat::TxKind::AppendFeature;
    tx.Feature = id;
    tx.FeatureType = "Sphere";
    tx.Sphere = std::move(undo_spec);
    part->FeatureHistory().Record(std::move(tx));
}

Body* SceneAdapter::add_boolean(brep::boolean::BooleanOp op,
                                feat::FeatureId target, feat::FeatureId tool,
                                std::string name)
{
    Part* part = main_part();
    if (!part)
    {
        return nullptr;
    }
    return part->AddBoolean(op, target, tool, std::move(name));
}

std::optional<SphereParams> SceneAdapter::sphere_params(
    feat::FeatureId id) const
{
    const Part* part = main_part_const();
    if (!part || !id.IsValid())
    {
        return std::nullopt;
    }
    const auto* f = part->Features().Find(id);
    if (!f || f->TypeName() != "Sphere")
    {
        return std::nullopt;
    }
    const auto& sph = static_cast<const feat::SphereFeature&>(*f);
    return SphereParams{
        .radius = part->Parameters().Get(sph.RadiusId()).value_or(0.0),
    };
}

bool SceneAdapter::set_sphere_params(feat::FeatureId id,
                                     const SphereParams& params)
{
    Part* part = main_part();
    if (!part)
    {
        return false;
    }
    const auto* f = part->Features().Find(id);
    if (!f || f->TypeName() != "Sphere")
    {
        return false;
    }
    return part->EditFeatureParams(id, {{"Radius", params.radius}});
}

std::optional<BooleanParams> SceneAdapter::boolean_params(
    feat::FeatureId id) const
{
    const Part* part = main_part_const();
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
    return BooleanParams{.op = bf.Op()};
}

std::optional<feat::FeatureId> SceneAdapter::feature_id_for(
    Guid feature_guid, Guid body_guid) const
{
    const Part* part = main_part_const();
    if (!part)
    {
        return std::nullopt;
    }
    const feat::IFeature* f = nullptr;
    if (feature_guid.IsValid())
    {
        f = part->Features().Find(feat::FeatureId{feature_guid});
    }
    if (!f && body_guid.IsValid())
    {
        f = part->Features().FindByBody(body_guid);
    }
    if (!f)
    {
        return std::nullopt;
    }
    return f->Id();
}

bool SceneAdapter::remove_feature(feat::FeatureId id)
{
    Part* part = main_part();
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

    part->FeatureHistory().ApplyAndRecord(*part, std::move(tx));
    return true;
}

void SceneAdapter::undo_feature(int steps)
{
    Part* part = main_part();
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
}

void SceneAdapter::redo_feature(int steps)
{
    Part* part = main_part();
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
}

std::optional<BoxSpec> SceneAdapter::box_spec_for(Guid feature_guid,
                                                  Guid body_guid) const
{
    const Part* part = main_part_const();
    if (!part)
    {
        return std::nullopt;
    }

    const feat::IFeature* f = nullptr;
    if (feature_guid.IsValid())
    {
        f = part->Features().Find(feat::FeatureId{feature_guid});
    }
    if (!f && body_guid.IsValid())
    {
        f = part->Features().FindByBody(body_guid);
    }
    if (!f || f->TypeName() != "Box")
    {
        return std::nullopt;
    }
    const auto& box = static_cast<const feat::BoxFeature&>(*f);
    return box.ToSpec(part->Parameters());
}

std::unique_ptr<brep::Document> SceneAdapter::create_blank(
    const std::string& name)
{
    auto doc = Document::Create(name);
    doc->AddPart("MainPart");
    return doc;
}

}  // namespace brep::viewer::adapter
