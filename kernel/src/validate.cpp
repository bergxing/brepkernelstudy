#include "brep/validate.hpp"

#include "brep/log.hpp"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace brep {

ValidationReport validate_body(const Body& body) {
  ValidationReport report;
  BREP_INFO("validate_body '{}'", body.name);

  if (body.shells.empty()) {
    report.error(body.name.empty() ? "Body" : body.name, "no shells");
    BREP_ERROR("validate_body '{}': no shells", body.name);
    return report;
  }

  for (const Shell* shell : body.shells) {
    if (!shell) {
      report.error("Body", "null shell pointer");
      continue;
    }
    const std::string sh = shell->name.empty() ? "Shell" : shell->name;

    if (shell->faces.empty()) {
      report.error(sh, "no faces");
      continue;
    }

    for (const Face* face : shell->faces) {
      if (!face) {
        report.error(sh, "null face");
        continue;
      }
      const std::string fn = face->name.empty() ? "Face" : face->name;

      if (!face->surface) {
        report.error(fn, "missing surface geometry");
      }
      if (!face->outer_loop()) {
        report.error(fn, "missing outer loop");
        continue;
      }

      for (const Loop* loop : face->loops) {
        if (!loop) {
          report.error(fn, "null loop");
          continue;
        }
        const std::string ln = loop->name.empty() ? "Loop" : loop->name;
        if (!loop->is_closed()) {
          report.error(ln, "coedge cycle is not closed / next-prev inconsistent");
        }
        if (loop->size() < 3) {
          report.warning(ln, "loop has fewer than 3 coedges");
        }

        // Continuity of vertices around the loop
        loop->for_each_coedge([&](const CoEdge& c) {
          if (!c.edge) {
            report.error(ln, "coedge without edge");
            return;
          }
          if (!c.next) {
            report.error(ln, "coedge missing next");
            return;
          }
          if (c.to() != c.next->from()) {
            report.error(ln, "adjacent coedges do not meet at a vertex");
          }
          if (c.partner) {
            if (c.partner->partner != &c) {
              report.error(ln, "partner is not an involution");
            }
            if (c.partner->edge != c.edge) {
              report.error(ln, "partner does not share the same edge");
            }
          } else if (body.type == BodyType::Solid && shell->closed) {
            report.error(ln, "manifold solid coedge missing partner");
          }
        });
      }
    }

    // Manifold: each edge used by exactly two coedges on a closed shell
    if (shell->closed) {
      std::unordered_set<const Edge*> seen;
      for (const Face* face : shell->faces) {
        if (!face) continue;
        for (const Loop* loop : face->loops) {
          if (!loop) continue;
          loop->for_each_coedge([&](const CoEdge& c) {
            if (!c.edge) return;
            if (!seen.insert(c.edge).second) return;
            if (c.edge->radial.size() != 2) {
              report.error(c.edge->name.empty() ? "Edge" : c.edge->name,
                           "closed shell edge radial degree != 2 (got " +
                               std::to_string(c.edge->radial.size()) + ")");
            }
          });
        }
      }
    }
  }

  // Geometry proximity: edge endpoints vs vertices
  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (const Face* face : shell->faces) {
      if (!face) continue;
      for (const Loop* loop : face->loops) {
        if (!loop) continue;
        loop->for_each_coedge([&](const CoEdge& c) {
          if (!c.edge || !c.edge->curve || !c.edge->v0 || !c.edge->v1) return;
          const Edge& e = *c.edge;
          const Point3d p0 = e.curve->eval(e.t0);
          const Point3d p1 = e.curve->eval(e.t1);
          const double tol =
              std::max(e.tolerance, std::max(e.v0->tolerance, e.v1->tolerance));
          if (p0.distance_to(e.v0->position()) > tol * 10) {
            report.warning(e.name.empty() ? "Edge" : e.name,
                           "curve(t0) far from v0");
          }
          if (p1.distance_to(e.v1->position()) > tol * 10) {
            report.warning(e.name.empty() ? "Edge" : e.name,
                           "curve(t1) far from v1");
          }
        });
      }
    }
  }

  if (report.ok()) {
    BREP_INFO("validate_body '{}': OK ({} issues)", body.name,
              report.issues.size());
  } else {
    BREP_ERROR("validate_body '{}': FAILED ({} issues)", body.name,
               report.issues.size());
  }
  return report;
}

}  // namespace brep
