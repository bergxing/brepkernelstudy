#include "brep/Builder.h"
#include "brep/bool/ImprintEngine.h"
#include "brep/bool/IntersectionGraph.h"
#include "brep/bool/TopologyCopy.h"
#include "brep/Validate.h"
#include <iostream>
int main() {
  brep::Model model;
  brep::Body* a = brep::MakeBox(model, {.Min={0,0,0}, .Max={2,1,1}, .Name="A"});
  brep::Body* b = brep::MakeBox(model, {.Min={1,0,0}, .Max={3,1,1}, .Name="B"});
  brep::boolean::TopologyCopyContext ctxA{model};
  brep::Body* wA = brep::boolean::CopyBodySubgraph(ctxA, *a, "_wkA");
  brep::boolean::TopologyCopyContext ctxB{model};
  brep::Body* wB = brep::boolean::CopyBodySubgraph(ctxB, *b, "_wkB");
  auto graph = brep::boolean::BuildIntersectionGraph(*wA, *wB);
  std::cout << "segments=" << graph.Segments.size() << "\n";
  int idx=0;
  for (const auto& seg : graph.Segments) {
    std::string fa = seg.FaceA ? seg.FaceA->Name : "-";
    std::string fb = seg.FaceB ? seg.FaceB->Name : "-";
    std::cout << idx++ << " fa=" << fa << " fb=" << fb << "\n";
  }
  brep::boolean::PipelineState st;
  st.BodyA=a; st.BodyB=b; st.WorkingBodyA=wA; st.WorkingBodyB=wB; st.TargetModel=&model;
  st.IntersectionGraph=graph;
  brep::boolean::RunTopologicalImprint(st);
  auto rep = brep::ValidateBody(*wB);
  std::cout << "B issues=" << rep.Issues.size() << "\n";
  return rep.Ok() ? 0 : 1;
}
