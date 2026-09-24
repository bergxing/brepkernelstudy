#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/IntersectorRegistry.h"

#include <gtest/gtest.h>

#include <cmath>

namespace brep
{
namespace
{

[[nodiscard]] double SegmentLength(const boolean::IntersectionSegment& segment)
{
  return (segment.End - segment.Start).norm();
}

[[nodiscard]] bool SegmentMatchesPoints(const boolean::IntersectionSegment& segment,
                                        const Point3d& expectedA,
                                        const Point3d& expectedB, double eps)
{
  const double dDirect =
      (segment.Start - expectedA).norm() + (segment.End - expectedB).norm();
  const double dReverse =
      (segment.Start - expectedB).norm() + (segment.End - expectedA).norm();
  return std::min(dDirect, dReverse) <= eps * 2.0;
}

[[nodiscard]] const Face* FindFaceOnPlane(const Body& body, double x, double y,
                                          double z, const Vector3d& normal)
{
  for (const Shell* shell : body.Shells)
  {
    if (!shell)
    {
      continue;
    }
    for (const Face* face : shell->Faces)
    {
      if (!face || !face->Surface || face->Surface->Kind() != SurfaceKind::Plane)
      {
        continue;
      }
      const auto& plane = static_cast<const PlaneSurface&>(*face->Surface);
      const Vector3d n = plane.Normal(0.0, 0.0).normalized();
      if (std::abs(n.dot(normal.normalized())) < 0.99)
      {
        continue;
      }
      const Point3d sample = plane.Eval(0.0, 0.0);
      if (std::abs(normal.x()) > 0.5 && std::abs(sample.x() - x) > 1e-6)
      {
        continue;
      }
      if (std::abs(normal.y()) > 0.5 && std::abs(sample.y() - y) > 1e-6)
      {
        continue;
      }
      if (std::abs(normal.z()) > 0.5 && std::abs(sample.z() - z) > 1e-6)
      {
        continue;
      }
      return face;
    }
  }
  return nullptr;
}

}  // namespace

TEST(IntersectionGraph, OrthogonalBoxFacesShareEdgeSegment)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(boxA, nullptr);
  ASSERT_NE(boxB, nullptr);

  const Face* faceX = FindFaceOnPlane(*boxA, 2.0, 0.0, 0.0, Vector3d{1, 0, 0});
  const Face* faceY = FindFaceOnPlane(*boxB, 0.0, 1.0, 0.0, Vector3d{0, 1, 0});
  ASSERT_NE(faceX, nullptr);
  ASSERT_NE(faceY, nullptr);

  boolean::IntersectorRegistry registry;
  const auto segments = registry.Intersect(*faceX, *faceY, {});
  ASSERT_EQ(segments.size(), 1u);
  EXPECT_NEAR(SegmentLength(segments[0]), 1.0, 1e-6);
  EXPECT_TRUE(SegmentMatchesPoints(segments[0], Point3d{2, 1, 1}, Point3d{2, 1, 2}, 1e-6));
  EXPECT_EQ(segments[0].CurveKind, CurveKind::Line);
}

TEST(IntersectionGraph, BuildGraphFindsSharedEdgeAmongCandidates)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(boxA, nullptr);
  ASSERT_NE(boxB, nullptr);

  const boolean::IntersectionGraph graph = boolean::BuildIntersectionGraph(*boxA, *boxB);
  ASSERT_FALSE(graph.Segments.empty());

  bool foundUnitSegment = false;
  for (const boolean::IntersectionSegment& segment : graph.Segments)
  {
    const double len = SegmentLength(segment);
    if (len > 0.5 && len < 1.5)
    {
      foundUnitSegment = true;
      break;
    }
  }
  EXPECT_TRUE(foundUnitSegment);
}

TEST(IntersectionGraph, ParallelFacePairsProduceNoSegments)
{
  Model model;
  Body* boxA = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "A"});
  Body* boxB = MakeBox(model, BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "B"});
  ASSERT_NE(boxA, nullptr);
  ASSERT_NE(boxB, nullptr);

  const Face* faceA = FindFaceOnPlane(*boxA, 1.0, 0.0, 0.0, Vector3d{1, 0, 0});
  const Face* faceB = FindFaceOnPlane(*boxB, 2.0, 0.0, 0.0, Vector3d{1, 0, 0});
  ASSERT_NE(faceA, nullptr);
  ASSERT_NE(faceB, nullptr);

  boolean::IntersectorRegistry registry;
  EXPECT_TRUE(registry.Intersect(*faceA, *faceB, {}).empty());
}

TEST(ClipLineToPlanarFace, ClipsInfiniteLineToSquareFace)
{
  Model model;
  Body* box = MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}});
  ASSERT_NE(box, nullptr);

  const Face* face = FindFaceOnPlane(*box, 2.0, 0.0, 0.0, Vector3d{1, 0, 0});
  ASSERT_NE(face, nullptr);

  const Point3d origin{2, 1, 0};
  const Vector3d direction{0, 0, 1};
  const auto interval = boolean::ClipLineToPlanarFace(*face, origin, direction, 1e-9);
  ASSERT_TRUE(interval.has_value());
  EXPECT_NEAR(interval->first, 0.0, 1e-6);
  EXPECT_NEAR(interval->second, 2.0, 1e-6);
}

TEST(IntersectionGraph, PlaneSphereOctantProducesCircleSegments)
{
  Model model;
  Body* sphere =
      MakeSphere(model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "S"});
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(sphere, nullptr);
  ASSERT_NE(box, nullptr);

  const Face* planeFace = FindFaceOnPlane(*box, 0.0, 0.0, 0.0, Vector3d{0, 0, -1});
  const Face* sphereFace = sphere->OuterShell()->Faces.front();
  ASSERT_NE(planeFace, nullptr);
  ASSERT_NE(sphereFace, nullptr);

  boolean::IntersectorRegistry registry;
  const auto segments = registry.Intersect(*planeFace, *sphereFace, {});
  EXPECT_GE(segments.size(), 4u);
  for (const boolean::IntersectionSegment& segment : segments)
  {
    EXPECT_EQ(segment.CurveKind, CurveKind::Circle);
    EXPECT_GT(SegmentLength(segment), 1e-9);
  }
}

}  // namespace brep
