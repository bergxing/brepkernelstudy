#include "brep/Brep.h"
#include "brep/Log.h"

#include <iostream>

int main()
{
  using namespace brep;

  try
  {
    InitLogging("brep_demo.log", LogLevel::Debug);
    BREP_INFO("box_demo starting");

    auto doc = Document::Create("box_demo");
    Part& part = doc->AddPart("MainPart");
    Body* box = part.AddBox(BoxSpec{
        .Min = Point3d{0, 0, 0},
        .Max = Point3d{2, 1, 3},
        .Tolerance = 1e-7,
        .Name = "demo_box",
    });

    BREP_INFO("hierarchy Document({}) -> Part({}) -> Body({})",
              doc->Guid.ToString(), part.Guid.ToString(),
              box->Guid.ToString());

    if (doc->Registry().Find(box->Guid) != box)
    {
      BREP_ERROR("Body Guid not registered on Document");
      return 3;
    }

    DumpBody(std::cout, *box);

    const ValidationReport report = ValidateBody(*box);
    BREP_INFO("Validation: {}", report.Ok() ? "OK" : "FAILED");
    for (const ValidationIssue& issue : report.Issues)
    {
      if (issue.Severity == ValidationIssue::IssueSeverity::Error)
      {
        BREP_ERROR("{}: {}", issue.Where, issue.Message);
      }
      else if (issue.Severity == ValidationIssue::IssueSeverity::Warning)
      {
        BREP_WARN("{}: {}", issue.Where, issue.Message);
      }
      else
      {
        BREP_INFO("{}: {}", issue.Where, issue.Message);
      }
    }

    Shell* shell = box->OuterShell();
    Face* face = shell->Faces.front();
    Loop* loop = face->OuterLoop();
    CoEdge* c = loop->First;

    BREP_INFO("Adjacency: face {} coedge -> partner face {}", face->Name,
              c->Partner->GetFace()->Name);

    std::string walk;
    loop->ForEachCoedge([&](const CoEdge& ce)
    {
      walk += ' ';
      walk += ce.From()->Name;
      walk += "->";
      walk += ce.To()->Name;
    });
    BREP_INFO("Loop walk:{}", walk);

    const Vector3d n = face->NormalAt(0, 0);
    BREP_INFO("{} normal = {}", face->Name, n);

    return report.Ok() ? 0 : 1;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "exception: " << ex.what() << '\n';
    return 2;
  }
}
