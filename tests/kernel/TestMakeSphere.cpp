#include "api/Core.h"
#include "api/Modeling.h"
#include "api/Persistence.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <filesystem>

namespace brep
{
namespace
{

TEST(MakeSphere, AnalyticTopologyAndValidate)
{
  Model model;
  const Point3d center{1, 2, 3};
  const double radius = 4.0;
  Body* body = MakeSphere(
      model, SphereSpec{.Center = center, .Radius = radius, .Name = "s"});
  ASSERT_NE(body, nullptr);

  ASSERT_EQ(body->shells.size(), 1u);
  Shell* shell = body->shells[0];
  ASSERT_NE(shell, nullptr);
  EXPECT_TRUE(shell->closed);
  ASSERT_EQ(shell->faces.size(), 1u);

  Face* face = shell->faces[0];
  ASSERT_NE(face, nullptr);
  ASSERT_NE(face->surface, nullptr);
  EXPECT_EQ(face->surface->kind(), SurfaceKind::Sphere);
  const auto* sphere = static_cast<const SphereSurface*>(face->surface);
  EXPECT_NEAR(sphere->center().x(), center.x(), 1e-12);
  EXPECT_NEAR(sphere->center().y(), center.y(), 1e-12);
  EXPECT_NEAR(sphere->center().z(), center.z(), 1e-12);
  EXPECT_DOUBLE_EQ(sphere->radius(), radius);

  Loop* outer = face->outer_loop();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->size(), 2u);

  Edge* seam = nullptr;
  int coedge_count = 0;
  outer->for_each_coedge([&](const CoEdge& ce)
  {
    ++coedge_count;
    ASSERT_NE(ce.edge, nullptr);
    ASSERT_NE(ce.partner, nullptr);
    EXPECT_EQ(ce.partner->partner, &ce);
    EXPECT_EQ(ce.partner->edge, ce.edge);
    if (!seam) seam = ce.edge;
    EXPECT_EQ(ce.edge, seam);
  });
  EXPECT_EQ(coedge_count, 2);
  ASSERT_NE(seam, nullptr);
  EXPECT_EQ(seam->radial.size(), 2u);
  ASSERT_NE(seam->curve, nullptr);
  EXPECT_EQ(seam->curve->kind(), CurveKind::Circle);
  ASSERT_NE(seam->v0, nullptr);
  ASSERT_NE(seam->v1, nullptr);

  const Point3d south{center.x(), center.y() - radius, center.z()};
  const Point3d north{center.x(), center.y() + radius, center.z()};
  EXPECT_NEAR((seam->v0->position() - south).norm(), 0.0, 1e-9);
  EXPECT_NEAR((seam->v1->position() - north).norm(), 0.0, 1e-9);
  EXPECT_NEAR((seam->curve->eval(seam->t0) - south).norm(), 0.0, 1e-9);
  EXPECT_NEAR((seam->curve->eval(seam->t1) - north).norm(), 0.0, 1e-9);

  // Mid-seam should sit on the sphere at u≈0 (equator +X for default circle).
  const Point3d mid =
      seam->curve->eval(0.5 * (seam->t0 + seam->t1));
  EXPECT_NEAR((mid - center).norm(), radius, 1e-9);

  const ValidationReport report = ValidateBody(*body);
  EXPECT_TRUE(report.Ok());
  for (const auto& issue : report.Issues)
  {
    EXPECT_NE(issue.Severity, ValidationIssue::IssueSeverity::Error)
        << issue.Where << ": " << issue.Message;
  }
}

TEST(MakeSphere, SphereFeatureRebuildValidate)
{
  auto doc = Document::Create("sphere_feat");
  Part& part = doc->AddPart("Main");
  Body* body = part.AddSphere(
      SphereSpec{.Center = {0, 0, 0}, .Radius = 2.5, .Name = "Ball"});
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(ValidateBody(*body).Ok());

  auto* feature = part.features().find_by_body(body->guid);
  ASSERT_NE(feature, nullptr);
  EXPECT_EQ(feature->type_name(), "Sphere");

  ASSERT_TRUE(part.edit_feature_params(feature->id(), {{"Radius", 3.5}}));
  Body* rebuilt = part.find_body(feature->body_guid());
  ASSERT_NE(rebuilt, nullptr);
  EXPECT_TRUE(ValidateBody(*rebuilt).Ok());

  Face* face = rebuilt->shells.at(0)->faces.at(0);
  ASSERT_NE(face->surface, nullptr);
  ASSERT_EQ(face->surface->kind(), SurfaceKind::Sphere);
  EXPECT_DOUBLE_EQ(static_cast<const SphereSurface*>(face->surface)->radius(),
                   3.5);
}

TEST(MakeSphere, XlRoundtripRegeneratesAnalyticSphere)
{
  namespace fs = std::filesystem;
  auto doc = Document::Create("sphere_xl");
  Part& part = doc->AddPart("Main");
  Body* body = part.AddSphere(
      SphereSpec{.Center = {5, -1, 2}, .Radius = 1.25, .Name = "S"});
  ASSERT_NE(body, nullptr);
  const Guid body_guid = body->guid;

  const fs::path path =
      fs::temp_directory_path() / "brep_test_make_sphere_roundtrip.xl";
  auto saved = io::SaveXl(*doc, path);
  ASSERT_TRUE(saved.Ok) << saved.Error;

  auto loaded = io::LoadXl(path);
  ASSERT_TRUE(loaded.Ok()) << loaded.Error;
  Part* p2 = loaded.document->MainPart();
  ASSERT_NE(p2, nullptr);
  Body* body2 = p2->find_body(body_guid);
  ASSERT_NE(body2, nullptr);
  EXPECT_TRUE(ValidateBody(*body2).Ok());

  ASSERT_FALSE(body2->shells.empty());
  ASSERT_FALSE(body2->shells[0]->faces.empty());
  Face* face = body2->shells[0]->faces[0];
  ASSERT_NE(face->surface, nullptr);
  EXPECT_EQ(face->surface->kind(), SurfaceKind::Sphere);
  const auto* sphere = static_cast<const SphereSurface*>(face->surface);
  EXPECT_NEAR(sphere->center().x(), 5.0, 1e-9);
  EXPECT_NEAR(sphere->center().y(), -1.0, 1e-9);
  EXPECT_NEAR(sphere->center().z(), 2.0, 1e-9);
  EXPECT_DOUBLE_EQ(sphere->radius(), 1.25);

  std::error_code ec;
  fs::remove(path, ec);
  fs::remove(io::BksCachePathFor(path), ec);
}

}  // namespace
}  // namespace brep
