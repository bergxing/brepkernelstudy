#include "api/Core.h"


#include "api/Mesh.h"


#include "api/Modeling.h"


#include "brep/io/XlDocument.h"


#include <gtest/gtest.h>


#include <map>


#include <tuple>


namespace brep


{


namespace


{


struct EdgeKey


{


  std::uint32_t a;


  std::uint32_t b;


  bool operator<(const EdgeKey& o) const noexcept
  {


    return std::tie(a, b) < std::tie(o.a, o.b);


  }


};


[[nodiscard]] EdgeKey make_edge(std::uint32_t i, std::uint32_t j)


{


  return i < j ? EdgeKey{i, j} : EdgeKey{j, i};


}


struct MeshDiag


{


  std::size_t boundary_edges = 0;


  std::size_t dup_vert_pairs = 0;


  std::size_t degenerate_tris = 0;


};


[[nodiscard]] MeshDiag analyze_mesh(const TriangleMesh& mesh)


{


  MeshDiag d;


  std::map<EdgeKey, int> edge_use;


  for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)


  {


    const std::uint32_t a = mesh.Indices[i];


    const std::uint32_t b = mesh.Indices[i + 1];


    const std::uint32_t c = mesh.Indices[i + 2];


    ++edge_use[make_edge(a, b)];


    ++edge_use[make_edge(b, c)];


    ++edge_use[make_edge(c, a)];


    const Vector3d area =


        (mesh.Vertices[b].Position - mesh.Vertices[a].Position)


            .cross(mesh.Vertices[c].Position - mesh.Vertices[a].Position);


    if (area.squaredNorm() < 1e-24)


    {


      ++d.degenerate_tris;


    }


  }


  for (const auto& [ek, count] : edge_use)


  {


    if (count == 1)


    {


      ++d.boundary_edges;


    }


  }


  for (std::size_t i = 0; i < mesh.Vertices.size(); ++i)


  {


    for (std::size_t j = i + 1; j < mesh.Vertices.size(); ++j)


    {


      if (mesh.Vertices[i].Position.distance_to(mesh.Vertices[j].Position) <


          1e-6)


          {


        ++d.dup_vert_pairs;


        break;


      }


    }


  }


  return d;


}


[[nodiscard]] bool is_analytic_sphere_seam_outer(const Face& face)


{


  if (!face.OuterLoop() || face.OuterLoop()->CoedgeCount() != 2)


  {


    return false;


  }


  const CoEdge* c0 = face.OuterLoop()->First;


  const CoEdge* c1 = c0 ? c0->Next : nullptr;


  return c0 && c1 && c0->Edge == c1->Edge;


}


void expect_closed_sphere_watertight(const TriangleMesh& mesh, const Point3d& center,


                                     double radius, std::size_t expected_tris,


                                     std::size_t expected_verts)


                                     {


  const MeshDiag diag = analyze_mesh(mesh);


  EXPECT_EQ(diag.boundary_edges, 0u)


      << "closed sphere must have no boundary edges (mesh holes)";


  EXPECT_EQ(diag.dup_vert_pairs, 0u)


      << "seam/pole vertices must be welded (no duplicate positions)";


  EXPECT_EQ(diag.degenerate_tris, 0u);


  EXPECT_EQ(mesh.Indices.size() / 3, expected_tris);


  EXPECT_EQ(mesh.Vertices.size(), expected_verts);


  for (const MeshVertex& mv : mesh.Vertices)


  {


    EXPECT_NEAR(mv.Position.distance_to(center), radius, 0.02);


  }


}


TEST(TessellateClosedSphere, SeamOuterGridIsWatertight)


{


  Model model;


  const Point3d Center{-1.57147, 0.826297, 4.59474};


  const double radius = 0.413149;


  Body* body = MakeSphere(


      model, SphereSpec{.Center = center, .Radius = radius, .Name = "sphere"});


  ASSERT_NE(body, nullptr);


  Face* face = body->Shells.at(0)->Faces.at(0);


  ASSERT_NE(face, nullptr);


  EXPECT_TRUE(is_analytic_sphere_seam_outer(*face));


  const TriangleMesh mesh = TessellateBody(*body);


  expect_closed_sphere_watertight(mesh, center, radius, 528u, 266u);


}


TEST(TessellateClosedSphere, UntitledXlGuidIsWatertightIfPresent)


{


  const char* xl_path = "C:/Users/xingbl/Desktop/untitled.xl";


  const auto loaded = io::LoadXl(xl_path);


  if (!loaded.Ok())


  {


    GTEST_SKIP() << "untitled.xl not available: " << loaded.Error;


  }


  Part* part = loaded.document->MainPart();


  ASSERT_NE(part, nullptr);


  const Guid target = Guid::FromString("22cd2ef1-df12-4968-a2c5-9f6cb9665478");


  Body* body = part->FindBody(target);


  if (!body)


  {


    GTEST_SKIP() << "target sphere guid not in xl";


  }


  double radius = 0.0;


  Point3d center;


  bool have_center = false;


  for (Shell* shell : body->Shells)


  {


    for (Face* face : shell->Faces)


    {


      if (face && face->Surface &&


          face->Surface->Kind() == SurfaceKind::Sphere)


          {


        if (const auto* s = dynamic_cast<const SphereSurface*>(face->Surface))


        {


          radius = s->Radius();


          center = s->Center();


          have_center = true;


        }


        EXPECT_TRUE(is_analytic_sphere_seam_outer(*face));


      }


    }


  }


  ASSERT_TRUE(have_center);


  ASSERT_GT(radius, 0.0);


  const TriangleMesh mesh = TessellateBody(*body);


  expect_closed_sphere_watertight(mesh, center, radius, 528u, 266u);


}


}  // namespace


}  // namespace brep
