#include "api/Core.h"
#include "api/Mesh.h"
#include "api/Modeling.h"
#include "brep/bool/Boolean.h"
#include "brep/io/XlDocument.h"
#include "brep/Part.h"
#include <iostream>
int main() {
  using namespace brep;
  // untitled geometry intersect
  {
    Model model;
    Body* box = MakeBox(model, BoxSpec{.Min = {-2.38421, 0, 4.59474}, .Max = {-1.57147, 0.826297, 5.26894}});
    Body* sphere = MakeSphere(model, SphereSpec{.Center = {-1.57147, 0.826297, 4.59474}, .Radius = 0.413149});
    auto eval = boolean::make_default_boolean_evaluator();
    auto result = eval->evaluate(boolean::BooleanOp::Intersect, model, *box, *sphere, {});
    if (!result.Ok()) { std::cout << "fail: " << result.Diagnostics << "\n"; return 1; }
    auto mesh = TessellateBody(*result.body);
    std::cout << "untitled intersect verts=" << mesh.Vertices.size() << " tris=" << (mesh.Indices.size()/3) << "\n";
  }
  auto loaded = io::LoadXl_document("C:/Users/xingbl/Desktop/untitled.xl");
  if (!loaded.Ok()) { std::cout << "xl skip: " << loaded.Error << "\n"; return 0; }
  Part* part = loaded.document->MainPart();
  Body* body = part->FindBody(Guid::FromString("b750d152-0acf-4b18-b5ee-041d61777d55"));
  if (!body) { std::cout << "guid not found\n"; return 0; }
  auto mesh = TessellateBody(*body);
  std::cout << "guid b750 verts=" << mesh.Vertices.size() << " tris=" << (mesh.Indices.size()/3) << " name=" << body->Name << "\n";
  return 0;
}
