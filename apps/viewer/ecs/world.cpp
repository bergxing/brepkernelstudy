#include "ecs/World.h"

#include "adapter/SceneAdapter.h"
#include "ecs/Systems.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <variant>
#include <vector>

namespace brep::viewer::ecs
{
namespace
{

void SyncBezierCvComponent(entt::registry& registry, entt::entity entity,
                           adapter::SceneAdapter& scene, Guid featureGuid,
                           Guid bodyGuid)
{
    auto spec = scene.SpecFor(featureGuid, bodyGuid);
    if (!spec)
    {
        if (registry.all_of<BezierCvComponent>(entity))
        {
            registry.remove<BezierCvComponent>(entity);
        }
        return;
    }
    BezierCvComponent cv;
    if (const auto* bezier = std::get_if<BezierSpec>(&*spec))
    {
        cv.Cvs = bezier->Cvs;
        cv.Weights = bezier->Weights;
        cv.Degree = bezier->Degree;
        cv.SegmentCount = bezier->SegmentCount;
        cv.Corner = bezier->Corner;
    }
    else if (const auto* nurbs = std::get_if<NurbsCurveSpec>(&*spec))
    {
        // Preview / drag reuse BezierCvComponent; knots stay on the feature.
        cv.Cvs = nurbs->Cvs;
        cv.Weights = nurbs->Weights;
        cv.Degree = nurbs->Degree;
        cv.SegmentCount = 1;
    }
    else
    {
        if (registry.all_of<BezierCvComponent>(entity))
        {
            registry.remove<BezierCvComponent>(entity);
        }
        return;
    }
    if (registry.all_of<BezierCvComponent>(entity))
    {
        registry.get<BezierCvComponent>(entity) = std::move(cv);
    }
    else
    {
        registry.emplace<BezierCvComponent>(entity, std::move(cv));
    }
}

}  // namespace

World::World()
{
  m_registry.ctx().emplace<InputState>();
  m_registry.ctx().emplace<SelectionState>();
  m_registry.ctx().emplace<RenderCache>();
}

brep::Model* World::model() noexcept
{
  if (!m_document) return nullptr;
  if (auto* part = m_document->MainPart()) return &part->Model();
  return nullptr;
}

const brep::Model* World::model() const noexcept
{
  if (!m_document) return nullptr;
  if (const auto* part = m_document->MainPart()) return &part->Model();
  return nullptr;
}

void World::ClearScene()
{
  m_registry.clear();
  if (!m_registry.ctx().contains<InputState>())
  {
    m_registry.ctx().emplace<InputState>();
  }
  else
  {
    m_registry.ctx().get<InputState>() = InputState{};
  }
  if (!m_registry.ctx().contains<SelectionState>())
  {
    m_registry.ctx().emplace<SelectionState>();
  }
  else
  {
    m_registry.ctx().get<SelectionState>() = SelectionState{};
  }
  if (!m_registry.ctx().contains<RenderCache>())
  {
    m_registry.ctx().emplace<RenderCache>();
  }
  else
  {
    m_registry.ctx().get<RenderCache>() = RenderCache{};
  }
  m_document.reset();
}

entt::entity World::CreateCamera(Camera camera)
{
  const entt::entity e = m_registry.create();
  m_registry.emplace<Name>(e, Name{"MainCamera"});
  m_registry.emplace<CameraComponent>(e, CameraComponent{std::move(camera)});
  m_registry.emplace<MainCameraTag>(e);
  return e;
}

entt::entity World::CreateRenderable(std::string name, TriangleMesh triangles,
                                      EdgeMesh edges, Material material,
                                      Point3d position)
                                      {
  const entt::entity e = m_registry.create();
  m_registry.emplace<Name>(e, Name{std::move(name)});
  m_registry.emplace<Transform>(e, Transform{position});
  m_registry.emplace<MeshComponent>(
      e, MeshComponent{std::move(triangles), std::move(edges), true});
  m_registry.emplace<MaterialComponent>(
      e, MaterialComponent{std::move(material), true});
  m_registry.emplace<RenderableTag>(e);
  return e;
}

entt::entity World::CreateBodyRenderable(std::string name,
                                           brep::Guid body_guid,
                                           TriangleMesh triangles,
                                           EdgeMesh edges, Material material,
                                           Point3d position,
                                           brep::Guid feature_guid)
                                           {
  const entt::entity e = CreateRenderable(
      std::move(name), std::move(triangles), std::move(edges),
      std::move(material), position);
  m_registry.emplace<BodyRef>(e, BodyRef{body_guid});
  if (feature_guid.IsValid())
  {
    m_registry.emplace<FeatureRef>(e, FeatureRef{feature_guid});
  }
  if (auto* cache = m_registry.ctx().find<RenderCache>())
  {
    cache->force_rebuild = true;
  }
  return e;
}

bool World::UpdateBodyRenderable(const brep::Guid& body_guid,
                                   TriangleMesh triangles, EdgeMesh edges)
{
  const entt::entity e = FindBodyRenderable(body_guid);
  if (e == entt::null) return false;
  auto& mesh = m_registry.get<MeshComponent>(e);
  mesh.triangles = std::move(triangles);
  mesh.edges = std::move(edges);
  mesh.dirty = true;
  if (auto* cache = m_registry.ctx().find<RenderCache>())
  {
    cache->force_rebuild = true;
  }
  return true;
}

void World::SyncPartBodies(brep::Part& part, Material material,
                             const brep::io::BodyMeshCache* cache)
{
  adapter::SceneAdapter scene(m_document.get());
  std::vector<Guid> live;
  for (const auto& body : part.Model().Bodies())
  {
    if (!body) continue;
    live.push_back(body->Guid);
    Guid feature_guid{};
    if (const auto* f = part.Features().FindByBody(body->Guid))
    {
      feature_guid = f->Id().Guid;
    }

    auto mesh = scene.MeshForBody(body->Guid, cache);
    // Face-less bodies (Bézier / wires) are drawn as theme-colored lines.
    const Material bodyMaterial =
        mesh.Faces.Indices.empty() ? Material{} : material;

    const entt::entity existing = FindBodyRenderable(body->Guid);
    if (existing == entt::null)
    {
      const entt::entity created = CreateBodyRenderable(
          body->Name, body->Guid, std::move(mesh.Faces), std::move(mesh.Edges),
          bodyMaterial, Point3d{}, feature_guid);
      SyncBezierCvComponent(m_registry, created, scene, feature_guid,
                            body->Guid);
    }
    else
    {
      UpdateBodyRenderable(body->Guid, std::move(mesh.Faces),
                             std::move(mesh.Edges));
      auto& mat_comp = m_registry.get<MaterialComponent>(existing);
      mat_comp.material = bodyMaterial;
      mat_comp.dirty = true;
      if (feature_guid.IsValid())
      {
        if (m_registry.all_of<FeatureRef>(existing))
        {
          m_registry.get<FeatureRef>(existing).FeatureGuid = feature_guid;
        }
        else
        {
          m_registry.emplace<FeatureRef>(existing, FeatureRef{feature_guid});
        }
      }
      SyncBezierCvComponent(m_registry, existing, scene, feature_guid,
                            body->Guid);
    }
  }

  std::vector<entt::entity> stale;
  auto view = m_registry.view<BodyRef, RenderableTag>();
  for (auto entity : view)
  {
    const Guid g = view.get<BodyRef>(entity).guid;
    bool found = false;
    for (const auto& live_g : live)
    {
      if (live_g == g)
    {
        found = true;
        break;
      }
    }
    if (!found) stale.push_back(entity);
  }
  for (auto entity : stale)
  {
    if (m_registry.all_of<SelectedTag>(entity))
  {
      toggle_selection(m_registry, entity);
    }
    m_registry.destroy(entity);
  }
}

entt::entity World::FindBodyRenderable(const brep::Guid& body_guid) const
{
  auto view = m_registry.view<BodyRef, RenderableTag>();
  for (auto entity : view)
  {
    if (view.get<BodyRef>(entity).guid == body_guid) return entity;
  }
  return entt::null;
}

bool World::DestroyBodyRenderable(const brep::Guid& body_guid)
{
  const entt::entity e = FindBodyRenderable(body_guid);
  if (e == entt::null) return false;
  if (m_registry.all_of<SelectedTag>(e))
  {
    toggle_selection(m_registry, e);
  }
  m_registry.destroy(e);
  return true;
}

void World::CreateBlankScene(
    std::shared_ptr<boolean::IBooleanEvaluator> evaluator)
{
  ClearScene();

  m_document = adapter::SceneAdapter::CreateBlank("Untitled", std::move(evaluator));
  Part* part = m_document->MainPart();

  Camera cam;
  cam.target = Point3d{0.0, 0.0, 0.0};
  cam.distance = 6.0f;
  CreateCamera(cam);

  BREP_INFO("blank scene: Document={} Part={} (no bodies)",
            m_document->Guid.ToString(),
            part ? part->Guid.ToString() : std::string{});
}

void World::CreateDemoBoxScene(
    const std::string& wood_albedo_path,
    std::shared_ptr<boolean::IBooleanEvaluator> evaluator)
{
  ClearScene();

  m_document = adapter::SceneAdapter::CreateBlank("Untitled", std::move(evaluator));
  adapter::SceneAdapter scene(m_document.get());
  Body* body = scene.AddPrimitive(BoxSpec{
      .Min = Point3d{0, 0, 0},
      .Max = Point3d{2, 1, 3},
      .Name = "demo_box",
  });

  Camera cam;
  cam.target = Point3d{1.0, 0.5, 1.5};
  CreateCamera(cam);

  Guid feature_guid{};
  if (body)
  {
    if (auto obj = scene.ObjectForBody(body->Guid))
    {
      feature_guid = obj->FeatureGuid;
    }
    auto mesh = scene.MeshForBody(body->Guid);
    CreateBodyRenderable("demo_box", body->Guid, std::move(mesh.Faces),
                           std::move(mesh.Edges),
                           MakeWoodMaterial(wood_albedo_path), Point3d{},
                           feature_guid);
  }

  BREP_INFO(
      "demo scene: Document={} Part={} Body={} (guid={})",
      m_document->Guid.ToString(),
      m_document->MainPart() ? m_document->MainPart()->Guid.ToString()
                             : std::string{},
      body ? body->Name : std::string{},
      body ? body->Guid.ToString() : std::string{});
}

void World::AdoptDocument(std::unique_ptr<brep::Document> document,
                           Material material,
                           const brep::io::BodyMeshCache* cache)
                           {
  if (!document) return;

  // Keep InputState / Selection / RenderCache; drop entities only.
  m_registry.clear();
  if (!m_registry.ctx().contains<InputState>())
  {
    m_registry.ctx().emplace<InputState>();
  }
  else
  {
    m_registry.ctx().get<InputState>() = InputState{};
  }
  if (!m_registry.ctx().contains<SelectionState>())
  {
    m_registry.ctx().emplace<SelectionState>();
  }
  else
  {
    m_registry.ctx().get<SelectionState>() = SelectionState{};
  }
  if (!m_registry.ctx().contains<RenderCache>())
  {
    m_registry.ctx().emplace<RenderCache>();
  }
  else
  {
    m_registry.ctx().get<RenderCache>() = RenderCache{};
  }

  m_document = std::move(document);

  Camera cam;
  cam.target = Point3d{0.0, 0.0, 0.0};
  cam.distance = 6.0f;
  if (Part* part = m_document->MainPart())
  {
    // Frame roughly around existing bodies.
    if (!part->Model().Bodies().empty() && part->Model().Bodies().front())
    {
      cam.target = Point3d{1.0, 0.5, 1.5};
    }
    CreateCamera(cam);
    SyncPartBodies(*part, std::move(material), cache);
  }
  else
    {
    CreateCamera(cam);
  }

  BREP_INFO("adopted Document={} parts={} occurrences={}",
            m_document->Guid.ToString(), m_document->Parts().size(),
            m_document->Assembly().Occurrences().size());
}

Camera* World::MainCamera() noexcept
{
  auto view = m_registry.view<CameraComponent, MainCameraTag>();
  for (auto entity : view)
  {
    return &view.get<CameraComponent>(entity).camera;
  }
  return nullptr;
}

const Camera* World::MainCamera() const noexcept
  {
  auto view = m_registry.view<CameraComponent, MainCameraTag>();
  for (auto entity : view)
  {
    return &view.get<CameraComponent>(entity).camera;
  }
  return nullptr;
}

}  // namespace brep::viewer::ecs
