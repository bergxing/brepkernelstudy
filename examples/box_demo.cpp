#include "brep/brep.hpp"

#include <iostream>

int main() {
  using namespace brep;

  Model model;
  Body* box = make_box(model, BoxSpec{
      .min = {0, 0, 0},
      .max = {2, 1, 3},
      .tolerance = 1e-7,
      .name = "demo_box",
  });

  dump_body(std::cout, *box);

  const ValidationReport report = validate_body(*box);
  std::cout << "\nValidation: " << (report.ok() ? "OK" : "FAILED") << '\n';
  for (const ValidationIssue& issue : report.issues) {
    const char* sev =
        issue.severity == ValidationIssue::Severity::Error     ? "ERROR"
        : issue.severity == ValidationIssue::Severity::Warning ? "WARN"
                                                              : "INFO";
    std::cout << "  [" << sev << "] " << issue.where << ": " << issue.message
              << '\n';
  }

  // Demonstrate adjacency walk: from first coedge to neighbor face
  Shell* shell = box->outer_shell();
  Face* face = shell->faces.front();
  Loop* loop = face->outer_loop();
  CoEdge* c = loop->first;
  std::cout << "\nAdjacency sample:\n";
  std::cout << "  face " << face->name << " coedge -> partner face "
            << c->partner->face()->name << '\n';
  std::cout << "  loop walk:";
  loop->for_each_coedge([&](const CoEdge& ce) {
    std::cout << ' ' << ce.from()->name << "->" << ce.to()->name;
  });
  std::cout << '\n';

  const Vec3 n = face->normal_at(0, 0);
  std::cout << "  " << face->name << " normal = " << n << '\n';

  return report.ok() ? 0 : 1;
}
