#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/io/XlDocument.h"
#include "brep/Part.h"
#include <iostream>
#include <cmath>
void dump(Body* body) {
  using namespace brep;
  std::cout << "body " << body->Name << "\n";
  Point3d sc; double sr=0; bool hs=false;
  for (Shell* sh : body->Shells) for (Face* f : sh->Faces) {
    if (!f||!f->Surface) continue;
    TriangleMesh m; TessellateFace(*f, m);
    int tris = (int)m.Indices.size()/3;
    if (f->Surface->Kind()==SurfaceKind::Plane) {
      Vector3d n = f->NormalAt(0,0);
      std::cout << " plane " << f->Name << " sense=" << (f->Sense==Orientation::Forward?"F":"R")
                << " tris=" << tris << " n=(" << n.x() << "," << n.y() << "," << n.z() << ")\n";
    } else if (f->Surface->Kind()==SurfaceKind::Sphere) {
      auto* s=(SphereSurface*)f->Surface; sc=s->Center(); sr=s->Radius(); hs=true;
      std::cout << " sphere tris=" << tris << " c=(" << sc.x() << "," << sc.y() << "," << sc.z() << ")\n";
    }
  }
  if (hs) {
    Point3d hint{sc.x()-0.1*sr, sc.y()-0.1*sr, sc.z()-0.1*sr};
    TriangleMesh full = TessellateBody(*body);
    int bad=0, plane_tris=0;
    for (size_t i=0;i+2<full.Indices.size();i+=3) {
      auto& a=full.Vertices[full.Indices[i]];
      auto& b=full.Vertices[full.Indices[i+1]];
      auto& c=full.Vertices[full.Indices[i+2]];
      if (std::abs(a.Position.distance_to(sc)-sr)>0.02 &&
          std::abs(b.Position.distance_to(sc)-sr)>0.02 &&
          std::abs(c.Position.distance_to(sc)-sr)>0.02) {
        plane_tris++;
        Point3d p{(a.Position.x()+b.Position.x()+c.Position.x())/3,
                  (a.Position.y()+b.Position.y()+c.Position.y())/3,
                  (a.Position.z()+b.Position.z()+c.Position.z())/3};
        Vector3d to_in = hint - p;
        if (a.Normal.dot(to_in) > 0) bad++;
      }
    }
    std::cout << "plane_tris_total=" << plane_tris << " normals_point_inward=" << bad << "\n";
  }
}
int main() {
  using namespace brep;
  auto loaded = io::LoadXl("C:/Users/xingbl/Desktop/untitled.xl");
  if (loaded.Ok()) {
    Body* body = loaded.document->MainPart()->FindBody(
        Guid::FromString("b750d152-0acf-4b18-b5ee-041d61777d55"));
    if (body) dump(body);
  }
  Model model;
  Body* box = MakeBox(model, BoxSpec{.min={-2.944,0,1.68354}, .max={-2.13126,0.826297,2.35774}});
  Body* sphere = MakeSphere(model, SphereSpec{.center={-2.13126,0.826297,2.35774}, .radius=0.337101});
  auto r = boolean::make_default_boolean_evaluator()->evaluate(
      boolean::BooleanOp::Intersect, model, *box, *sphere, {});
  if (r.Ok()) dump(r.body);
  return 0;
}
