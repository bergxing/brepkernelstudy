#include "brep/solve2d/solver.hpp"

#include <cmath>

namespace brep::solve2d {
namespace {

Point2d& mut_uv(sketch::SketchPoint& p) { return p.p; }

}  // namespace

SolveReport ConstraintSolver::solve(sketch::Sketch& sketch,
                                    std::span<const sketch::Constraint> constraints,
                                    param::ParameterStore* params) {
  SolveReport report;
  // Apply Fixed constraints first.
  for (const auto& c : constraints) {
    if (c.kind == sketch::ConstraintKind::Fixed) {
      if (auto* pt = sketch.point(c.a)) pt->fixed = true;
    }
  }

  // Iterative relaxation for Distance / Horizontal / Vertical / Coincident.
  constexpr int kIters = 40;
  for (int iter = 0; iter < kIters; ++iter) {
    for (const auto& c : constraints) {
      auto* pa = sketch.point(c.a);
      auto* pb = sketch.point(c.b);
      switch (c.kind) {
        case sketch::ConstraintKind::Coincident: {
          if (!pa || !pb) break;
          if (pa->fixed && pb->fixed) break;
          const Point2d mid{(pa->p.u() + pb->p.u()) * 0.5,
                            (pa->p.v() + pb->p.v()) * 0.5};
          if (!pa->fixed) mut_uv(*pa) = mid;
          if (!pb->fixed) mut_uv(*pb) = mid;
          break;
        }
        case sketch::ConstraintKind::Horizontal: {
          if (!pa || !pb) break;
          const double y = (pa->p.v() + pb->p.v()) * 0.5;
          if (!pa->fixed) pa->p.v() = y;
          if (!pb->fixed) pb->p.v() = y;
          break;
        }
        case sketch::ConstraintKind::Vertical: {
          if (!pa || !pb) break;
          const double x = (pa->p.u() + pb->p.u()) * 0.5;
          if (!pa->fixed) pa->p.u() = x;
          if (!pb->fixed) pb->p.u() = x;
          break;
        }
        case sketch::ConstraintKind::Distance: {
          if (!pa || !pb) break;
          double target = c.aux;
          if (params && !c.dim.guid.is_nil()) {
            if (auto v = params->get(c.dim)) target = *v;
          }
          if (target < 0.0) target = 0.0;
          const double du = pb->p.u() - pa->p.u();
          const double dv = pb->p.v() - pa->p.v();
          double dist = std::sqrt(du * du + dv * dv);
          if (dist < 1e-12) {
            if (!pb->fixed) {
              pb->p.u() = pa->p.u() + target;
            } else if (!pa->fixed) {
              pa->p.u() = pb->p.u() - target;
            }
            break;
          }
          const double scale = target / dist;
          const double mu = pa->p.u() + du * 0.5;
          const double mv = pa->p.v() + dv * 0.5;
          const double hu = du * scale * 0.5;
          const double hv = dv * scale * 0.5;
          if (!pa->fixed && !pb->fixed) {
            pa->p = Point2d{mu - hu, mv - hv};
            pb->p = Point2d{mu + hu, mv + hv};
          } else if (!pb->fixed) {
            pb->p = Point2d{pa->p.u() + du * scale, pa->p.v() + dv * scale};
          } else if (!pa->fixed) {
            pa->p = Point2d{pb->p.u() - du * scale, pb->p.v() - dv * scale};
          }
          break;
        }
        case sketch::ConstraintKind::Radius: {
          // Circle radius driven by parameter / aux; update circle record.
          for (auto& circle : sketch.circles()) {
            if (circle.center == c.a) {
              double r = c.aux;
              if (params && !c.dim.guid.is_nil()) {
                if (auto v = params->get(c.dim)) r = *v;
              }
              circle.radius = std::max(0.0, r);
            }
          }
          break;
        }
        default:
          break;
      }
    }
  }

  int free_coords = 0;
  for (const auto& p : sketch.points()) {
    if (!p.fixed) free_coords += 2;
  }
  report.dof = free_coords;
  report.status = SolveStatus::Solved;
  report.message = "ok";
  if (free_coords > 0) {
    report.status = SolveStatus::UnderConstrained;
    report.message = "under-constrained";
  }
  return report;
}

}  // namespace brep::solve2d
