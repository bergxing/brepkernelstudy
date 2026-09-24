#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/io/XlDocument.h"
#include "brep/mesh/LoopSample.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace brep
{
namespace
{

[[nodiscard]] bool point_in_triangle(const Point3d& p, const Point3d& a,
                                     const Point3d& b, const Point3d& c,
                                     double eps = 1e-9)
{
  const Vector3d n = (b - a).cross(c - a);
  if (n.squaredNorm() < 1e-24)
  {
    return false;
  }
  const Vector3d na = (b - a).cross(p - a);
  const Vector3d nb = (c - b).cross(p - b);
  const Vector3d nc = (a - c).cross(p - c);
  return na.dot(n) >= -eps && nb.dot(n) >= -eps && nc.dot(n) >= -eps;
}

[[nodiscard]] bool mesh_covers_point(const TriangleMesh& mesh, const Point3d& p,
                                     double eps = 1e-6)
{
  for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
  {
    const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
    const Point3d& b = mesh.Vertices[mesh.Indices[i + 1]].Position;
    const Point3d& c = mesh.Vertices[mesh.Indices[i + 2]].Position;
    if (point_in_triangle(p, a, b, c, eps)) return true;
  }
  return false;
}

TEST(TessellateOctantBoolean, IntersectIsSmallPatchNotSevenEighthsSphere)
{
  Model model;
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "sphere"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);

  const TriangleMesh mesh = TessellateBody(*result.OutputBody);
  ASSERT_FALSE(mesh.Indices.empty());
  // The selected ⅛-ball must not leak triangles into another octant. The
  // exact triangle count depends on analytic pcurve sampling density.
  EXPECT_LT(mesh.Indices.size() / 3, 700u);
  for (const std::uint32_t index : mesh.Indices)
  {
    const MeshVertex& vertex = mesh.Vertices[index];
    EXPECT_GE(vertex.Position.x(), -2e-4);
    EXPECT_GE(vertex.Position.y(), -2e-4);
    EXPECT_GE(vertex.Position.z(), -2e-4);
  }

  const Point3d inside{0.35, 0.35, 0.35};
  EXPECT_TRUE(mesh_covers_point(mesh, inside, 2e-4))
      << "intersect octant interior should be covered";
}

TEST(TessellateOctantBoolean, SubtractKeepsExteriorSpherePatch)
{
  Model model;
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "sphere"});
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);

  const TriangleMesh mesh = TessellateBody(*result.OutputBody);
  ASSERT_FALSE(mesh.Indices.empty());
  EXPECT_GT(mesh.Indices.size() / 3, 400u);

  const Point3d outside{-0.85, 0.0, 0.0};
  EXPECT_TRUE(mesh_covers_point(mesh, outside, 3e-4))
      << "sphere−box 7/8 patch should cover far exterior samples";
}

TEST(TessellateOctantBoolean, UntitledXlIntersectGuidIfPresent)
{
  const char* xl_path = "C:/Users/xingbl/Desktop/untitled.xl";
  const auto loaded = io::LoadXl(xl_path);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "untitled.xl not available: " << loaded.Error;
  }
  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);
  Body* body = part->FindBody(
      Guid::FromString("1acd5ab1-0db4-4be9-b064-56255dd95003"));
  if (!body)
  {
    GTEST_SKIP() << "intersect guid not in xl";
  }

  Point3d center;
  double radius = 0.0;
  bool have_sphere = false;
  for (Shell* shell : body->Shells)
  {
    for (Face* face : shell->Faces)
    {
      if (face && face->Surface &&
          face->Surface->Kind() == SurfaceKind::Sphere)
      {
        if (const auto* s = dynamic_cast<const SphereSurface*>(face->Surface))
        {
          center = s->Center();
          radius = s->Radius();
          have_sphere = true;
        }
      }
    }
  }
  ASSERT_TRUE(have_sphere);
  const TriangleMesh mesh = TessellateBody(*body);
  ASSERT_FALSE(mesh.Indices.empty());
  EXPECT_LT(mesh.Indices.size() / 3, 300u);

  const Point3d inside{center.x() - 0.35 * radius, center.y() - 0.35 * radius,
                       center.z() - 0.35 * radius};
  EXPECT_TRUE(mesh_covers_point(mesh, inside, 3e-4));
}

TEST(TessellateOctantBoolean, UntitledXlIntersect516a771dNoExtraSphereFaces)
{
  const char* xl_path = "C:/Users/xingbl/Desktop/untitled.xl";
  const auto loaded = io::LoadXl(xl_path);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "untitled.xl not available: " << loaded.Error;
  }
  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);
  Body* body = part->FindBody(
      Guid::FromString("516a771d-edd4-427f-bc72-2dcdf868483e"));
  if (!body)
  {
    GTEST_SKIP() << "intersect guid 516a771d not in xl";
  }

  Point3d center;
  double radius = 0.0;
  bool have_sphere = false;
  for (Shell* shell : body->Shells)
  {
    for (Face* face : shell->Faces)
    {
      if (face && face->Surface &&
          face->Surface->Kind() == SurfaceKind::Sphere)
      {
        if (const auto* s = dynamic_cast<const SphereSurface*>(face->Surface))
        {
          center = s->Center();
          radius = s->Radius();
          have_sphere = true;
        }
      }
    }
  }
  ASSERT_TRUE(have_sphere);
  const TriangleMesh mesh = TessellateBody(*body);
  ASSERT_FALSE(mesh.Indices.empty());
  EXPECT_LT(mesh.Indices.size() / 3, 300u);

  const Point3d inside{center.x() + 0.35 * radius, center.y() + 0.35 * radius,
                       center.z() + 0.35 * radius};
  EXPECT_TRUE(mesh_covers_point(mesh, inside, 3e-4));
  EXPECT_LT(mesh.Indices.size() / 3, 200u)
      << "516a771d should stay a small 1/8-ball tessellation";
}

TEST(TessellateOctantBoolean, CornerTouchSubtractAllPlaneFacesTessellate)
{
  Model model;
  const Point3d center{-2.13126, 0.826297, 2.35774};
  const double radius = 0.337101;
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = center, .Radius = radius, .Name = "sphere"});
  Body* box = MakeBox(model,
                      BoxSpec{.Min = {-2.944, 0.0, 1.68354},
                              .Max = {-2.13126, 0.826297, 2.35774},
                              .Name = "box"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Subtract, model, *sphere, *box, {});
  ASSERT_TRUE(result.Ok()) << result.Diagnostics;
  ASSERT_NE(result.OutputBody, nullptr);

  std::size_t plane_faces = 0;
  for (Shell* shell : result.OutputBody->Shells)
  {
    for (Face* face : shell->Faces)
    {
      if (!face || !face->Surface ||
          face->Surface->Kind() != SurfaceKind::Plane)
      {
        continue;
      }
      ++plane_faces;
      TriangleMesh face_mesh;
      TessellateFace(*face, face_mesh);
      EXPECT_GE(face_mesh.Indices.size() / 3, 20u)
          << "plane face '" << face->Name << "' lost trimmed triangles";
      for (const MeshVertex& vertex : face_mesh.Vertices)
      {
        EXPECT_LE(vertex.Position.distance_to(center), radius + 1e-6)
            << "plane face '" << face->Name
            << "' contains geometry outside the sphere";
      }
    }
  }
  EXPECT_EQ(plane_faces, 3u);
}

[[nodiscard]] double point_to_triangle_distance(const Point3d& p,
                                                const Point3d& a,
                                                const Point3d& b,
                                                const Point3d& c)
{
  const Vector3d ab = b - a;
  const Vector3d ac = c - a;
  const Vector3d ap = p - a;
  const Vector3d n = ab.cross(ac);
  const double nSq = n.squaredNorm();
  if (nSq <= 1e-24)
  {
    return std::min(p.distance_to(a),
                    std::min(p.distance_to(b), p.distance_to(c)));
  }
  const Point3d q = p - n * (ap.dot(n) / nSq);
  if (point_in_triangle(q, a, b, c, 1e-9))
  {
    return p.distance_to(q);
  }
  const auto segmentDist = [](const Point3d& point, const Point3d& u,
                              const Point3d& v)
  {
    const Vector3d uv = v - u;
    const double lenSq = uv.squaredNorm();
    if (lenSq <= 1e-24)
    {
      return point.distance_to(u);
    }
    const double t = std::clamp((point - u).dot(uv) / lenSq, 0.0, 1.0);
    return point.distance_to(u + uv * t);
  };
  return std::min(segmentDist(p, a, b),
                  std::min(segmentDist(p, b, c), segmentDist(p, c, a)));
}

[[nodiscard]] double point_to_mesh_distance(const TriangleMesh& mesh,
                                            const Point3d& p)
{
  double best = std::numeric_limits<double>::infinity();
  for (const MeshVertex& vertex : mesh.Vertices)
  {
    best = std::min(best, p.distance_to(vertex.Position));
  }
  for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
  {
    const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
    const Point3d& b = mesh.Vertices[mesh.Indices[i + 1]].Position;
    const Point3d& c = mesh.Vertices[mesh.Indices[i + 2]].Position;
    best = std::min(best, point_to_triangle_distance(p, a, b, c));
  }
  return best;
}

[[nodiscard]] bool point_in_spherical_polygon_sides(
    const Point3d& p, const Point3d& center,
    const std::vector<Point3d>& boundary, const Point3d& hint)
{
  const Vector3d pd = p - center;
  const Vector3d hd = hint - center;
  if (pd.squaredNorm() < 1e-24 || hd.squaredNorm() < 1e-24 ||
      boundary.size() < 3)
  {
    return false;
  }
  const Vector3d pn = pd.normalized();
  const Vector3d hn = hd.normalized();
  for (std::size_t i = 0; i < boundary.size(); ++i)
  {
    const Vector3d a = boundary[i] - center;
    const Vector3d b = boundary[(i + 1) % boundary.size()] - center;
    const Vector3d n = a.cross(b);
    if (n.squaredNorm() < 1e-24)
    {
      continue;
    }
    const Vector3d nn = n.normalized();
    if (pn.dot(nn) * hn.dot(nn) < 0.0)
    {
      return false;
    }
  }
  return true;
}

void ExpectSpherePatchHugsBoundary(const Face& face)
{
  ASSERT_NE(face.Surface, nullptr);
  ASSERT_EQ(face.Surface->Kind(), SurfaceKind::Sphere);
  const auto* sphere = static_cast<const SphereSurface*>(face.Surface);
  const TessellationOptions opts = TessellationOptions::ForRadius(
      sphere->Radius());
  TriangleMesh mesh;
  TessellateFace(face, mesh, opts);
  ASSERT_GE(mesh.Indices.size() / 3, 1u) << face.Name;
  const brep::mesh::SampledRing ring =
      brep::mesh::SampleLoop(*face.OuterLoop(), *face.Surface, opts);
  ASSERT_GE(ring.Points.size(), 3u) << face.Name;

  std::vector<Point3d> boundary;
  boundary.reserve(ring.Points.size());
  Vector3d hintSum{0.0, 0.0, 0.0};
  for (const brep::mesh::SampledPoint& point : ring.Points)
  {
    boundary.push_back(point.Xyz);
    hintSum += point.Xyz - sphere->Center();
  }
  ASSERT_GT(hintSum.squaredNorm(), 1e-24) << face.Name;
  const Point3d hint =
      sphere->Center() + hintSum.normalized() * sphere->Radius();
  const double hugTol = std::max(0.04 * sphere->Radius(), 1e-3);
  const double outsideTol = std::max(0.02 * sphere->Radius(), 5e-4);

  for (const Point3d& sample : boundary)
  {
    EXPECT_LE(point_to_mesh_distance(mesh, sample), hugTol)
        << face.Name << " gap at " << sample;
  }

  for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
  {
    const Point3d& a = mesh.Vertices[mesh.Indices[i]].Position;
    const Point3d& b = mesh.Vertices[mesh.Indices[i + 1]].Position;
    const Point3d& c = mesh.Vertices[mesh.Indices[i + 2]].Position;
    const Point3d mid{(a.x() + b.x() + c.x()) / 3.0,
                      (a.y() + b.y() + c.y()) / 3.0,
                      (a.z() + b.z() + c.z()) / 3.0};
    const Vector3d radial = mid - sphere->Center();
    if (radial.squaredNorm() < 1e-24)
    {
      continue;
    }
    const Point3d onSphere =
        sphere->Center() + radial.normalized() * sphere->Radius();
    if (point_in_spherical_polygon_sides(onSphere, sphere->Center(), boundary,
                                         hint))
    {
      continue;
    }
    double edgeDist = std::numeric_limits<double>::infinity();
    for (std::size_t k = 0; k < boundary.size(); ++k)
    {
      const Point3d& u = boundary[k];
      const Point3d& v = boundary[(k + 1) % boundary.size()];
      const Vector3d uv = v - u;
      const double lenSq = uv.squaredNorm();
      const double t =
          lenSq <= 1e-24 ? 0.0
                         : std::clamp((onSphere - u).dot(uv) / lenSq, 0.0, 1.0);
      edgeDist = std::min(edgeDist, onSphere.distance_to(u + uv * t));
    }
    EXPECT_LE(edgeDist, outsideTol)
        << face.Name << " extra mesh face outside boundary at " << onSphere;
  }
}

TEST(TessellateOctantBoolean, IntersectSpherePatchHugsBoundary)
{
  Model model;
  Body* box = MakeBox(
      model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
  Body* sphere = MakeSphere(
      model, SphereSpec{.Center = {0, 0, 0}, .Radius = 1.0, .Name = "sphere"});
  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  if (!result.Ok() || result.OutputBody == nullptr)
  {
    GTEST_SKIP() << "intersect body not available: " << result.Diagnostics;
  }

  std::size_t sphereFaces = 0;
  for (Shell* shell : result.OutputBody->Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        continue;
      }
      ++sphereFaces;
      ExpectSpherePatchHugsBoundary(*face);
    }
  }
  EXPECT_GE(sphereFaces, 1u);
}

TEST(TessellateOctantBoolean, UntitledXlIntersect84e5222dHugsBoundary)
{
  const char* xlPath = "C:/Users/xingbl/Desktop/untitled.xl";
  const auto loaded = io::LoadXl(xlPath);
  if (!loaded.Ok())
  {
    GTEST_SKIP() << "untitled.xl not available: " << loaded.Error;
  }
  Part* part = loaded.document->MainPart();
  ASSERT_NE(part, nullptr);
  Body* body = part->FindBody(
      Guid::FromString("84e5222d-cffd-469c-8835-0fa06421fcad"));
  if (body == nullptr)
  {
    GTEST_SKIP() << "intersect guid 84e5222d not in xl";
  }

  std::size_t sphereFaces = 0;
  for (Shell* shell : body->Shells)
  {
    if (shell == nullptr)
    {
      continue;
    }
    for (Face* face : shell->Faces)
    {
      if (face == nullptr || face->Surface == nullptr ||
          face->Surface->Kind() != SurfaceKind::Sphere)
      {
        continue;
      }
      ++sphereFaces;
      ExpectSpherePatchHugsBoundary(*face);
    }
  }
  EXPECT_GE(sphereFaces, 1u);
}

}  // namespace
}  // namespace brep
