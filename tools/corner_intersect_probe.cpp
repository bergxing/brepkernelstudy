#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/bool/SolidClassifier.h"
#include "brep/build/PrimitiveBuild.h"
#include "brep/ops/Profile.h"

#include <iostream>

int main()
{
  using namespace brep;
  // Reproduce untitled.xl geometry from load log
  Model model;
  ops::ExtrudeSpec spec;
  spec.Name = "Pad";
  spec.Plane = Plane::XzYUp();
  spec.Distance = 0.9996867964130517;
  spec.Symmetric = false;
  spec.Profile.Outer = {
      Point2d{3.53808, 6.51887},
      Point2d{4.03792, 6.51887},
      Point2d{4.03792, 7.51855},
      Point2d{3.53808, 7.51855},
  };
  Body* pad = ops::Extrude(model, spec);
  Body* sphere = MakeSphere(
      model,
      SphereSpec{.Center = {4.03792, 0.999687, 7.01871}, .Radius = 0.499843});
  if (!pad || !sphere)
  {
    return 1;
  }

  const Point3d c{4.03792, 0.999687, 7.01871};
  std::cout << "sphere center classify vs pad="
            << static_cast<int>(boolean::ClassifyPointInBody(*pad, c, 1e-7))
            << " vs sphere="
            << static_cast<int>(boolean::ClassifyPointInBody(*sphere, c, 1e-7))
            << "\n";

  for (Shell* shell : pad->Shells)
  {
    for (Face* face : shell->Faces)
    {
      int in = 0;
      int out = 0;
      int on = 0;
      for (Loop* loop : face->Loops)
      {
        if (!loop || !loop->First)
        {
          continue;
        }
        loop->ForEachCoedge([&](const CoEdge& ce)
        {
          if (ce.From())
          {
            const auto cl =
                boolean::ClassifyPointInBody(*sphere, ce.From()->Position(), 1e-7);
            in += cl == boolean::SolidClass::In;
            out += cl == boolean::SolidClass::Out;
            on += cl == boolean::SolidClass::On;
          }
        });
      }
      std::cout << face->Name << " v(In/On/Out)=" << in << "/" << on << "/"
                << out << "\n";
    }
  }

  auto eval = boolean::MakeDefaultBooleanEvaluator();
  const auto result =
      eval->Evaluate(boolean::BooleanOp::Intersect, model, *sphere, *pad, {});
  std::cout << "intersect ok=" << result.Ok() << " " << result.Diagnostics
            << "\n";
  return result.Ok() ? 0 : 2;
}
