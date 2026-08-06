#include "brep/brep.hpp"
#include "brep/log.hpp"

#include <iostream>

int main() {
  using namespace brep;

  try {
    init_logging("brep_demo.log", LogLevel::Debug);
    BREP_INFO("box_demo starting");

    auto doc = Document::create("box_demo");
    Part& part = doc->add_part("MainPart");
    Body* box = part.add_box(BoxSpec{
        .min = Point3d{0, 0, 0},
        .max = Point3d{2, 1, 3},
        .tolerance = 1e-7,
        .name = "demo_box",
    });

    BREP_INFO("hierarchy Document({}) -> Part({}) -> Body({})",
              doc->guid.to_string(), part.guid.to_string(),
              box->guid.to_string());

    if (doc->registry().find(box->guid) != box) {
      BREP_ERROR("Body Guid not registered on Document");
      return 3;
    }

    dump_body(std::cout, *box);

    const ValidationReport report = validate_body(*box);
    BREP_INFO("Validation: {}", report.ok() ? "OK" : "FAILED");
    for (const ValidationIssue& issue : report.issues) {
      if (issue.severity == ValidationIssue::Severity::Error) {
        BREP_ERROR("{}: {}", issue.where, issue.message);
      } else if (issue.severity == ValidationIssue::Severity::Warning) {
        BREP_WARN("{}: {}", issue.where, issue.message);
      } else {
        BREP_INFO("{}: {}", issue.where, issue.message);
      }
    }

    Shell* shell = box->outer_shell();
    Face* face = shell->faces.front();
    Loop* loop = face->outer_loop();
    CoEdge* c = loop->first;

    BREP_INFO("Adjacency: face {} coedge -> partner face {}", face->name,
              c->partner->face()->name);

    std::string walk;
    loop->for_each_coedge([&](const CoEdge& ce) {
      walk += ' ';
      walk += ce.from()->name;
      walk += "->";
      walk += ce.to()->name;
    });
    BREP_INFO("Loop walk:{}", walk);

    const Vector3d n = face->normal_at(0, 0);
    BREP_INFO("{} normal = {}", face->name, n);

    return report.ok() ? 0 : 1;
  } catch (const std::exception& ex) {
    std::cerr << "exception: " << ex.what() << '\n';
    return 2;
  }
}
