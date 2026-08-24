#include "brep/solve2d/Solver.h"

#include <cmath>

namespace brep::solve2d
{
namespace
{

Point2d& MutUv(sketch::SketchPoint& p)
{
    return p.P;
}

}  // namespace

SolveReport ConstraintSolver::Solve(sketch::Sketch& sketch,
                                    std::span<const sketch::Constraint> constraints,
                                    param::ParameterStore* params)
{
    SolveReport report;
    // Apply Fixed constraints first.
    for (const auto& c : constraints)
    {
        if (c.Kind == sketch::ConstraintKind::Fixed)
        {
            if (auto* pt = sketch.Point(c.A)) pt->Fixed = true;
        }
    }

    // Iterative relaxation for Distance / Horizontal / Vertical / Coincident.
    constexpr int kIters = 40;
    for (int iter = 0; iter < kIters; ++iter)
    {
        for (const auto& c : constraints)
        {
            auto* pa = sketch.Point(c.A);
            auto* pb = sketch.Point(c.B);
            switch (c.Kind)
            {
            case sketch::ConstraintKind::Coincident: {
                if (!pa || !pb) break;
                if (pa->Fixed && pb->Fixed) break;
                const Point2d mid{(pa->P.u() + pb->P.u()) * 0.5,
                                  (pa->P.v() + pb->P.v()) * 0.5};
                if (!pa->Fixed) MutUv(*pa) = mid;
                if (!pb->Fixed) MutUv(*pb) = mid;
                break;
            }
            case sketch::ConstraintKind::Horizontal: {
                if (!pa || !pb) break;
                const double y = (pa->P.v() + pb->P.v()) * 0.5;
                if (!pa->Fixed) pa->P.v() = y;
                if (!pb->Fixed) pb->P.v() = y;
                break;
            }
            case sketch::ConstraintKind::Vertical: {
                if (!pa || !pb) break;
                const double x = (pa->P.u() + pb->P.u()) * 0.5;
                if (!pa->Fixed) pa->P.u() = x;
                if (!pb->Fixed) pb->P.u() = x;
                break;
            }
            case sketch::ConstraintKind::Distance: {
                if (!pa || !pb) break;
                double target = c.Aux;
                if (params && c.Dim.Guid.IsValid())
                {
                    if (auto v = params->Get(c.Dim)) target = *v;
                }
                if (target < 0.0) target = 0.0;
                const double du = pb->P.u() - pa->P.u();
                const double dv = pb->P.v() - pa->P.v();
                double dist = std::sqrt(du * du + dv * dv);
                if (dist < 1e-12)
                {
                    if (!pb->Fixed)
                    {
                        pb->P.u() = pa->P.u() + target;
                    }
                    else if (!pa->Fixed)
                    {
                        pa->P.u() = pb->P.u() - target;
                    }
                    break;
                }
                const double scale = target / dist;
                const double mu = pa->P.u() + du * 0.5;
                const double mv = pa->P.v() + dv * 0.5;
                const double hu = du * scale * 0.5;
                const double hv = dv * scale * 0.5;
                if (!pa->Fixed && !pb->Fixed)
                {
                    pa->P = Point2d{mu - hu, mv - hv};
                    pb->P = Point2d{mu + hu, mv + hv};
                }
                else if (!pb->Fixed)
                {
                    pb->P = Point2d{pa->P.u() + du * scale, pa->P.v() + dv * scale};
                }
                else if (!pa->Fixed)
                {
                    pa->P = Point2d{pb->P.u() - du * scale, pb->P.v() - dv * scale};
                }
                break;
            }
            case sketch::ConstraintKind::Radius: {
                // Circle radius driven by parameter / aux; update circle record.
                for (auto& circle : sketch.Circles())
                {
                    if (circle.Center == c.A)
                    {
                        double r = c.Aux;
                        if (params && c.Dim.Guid.IsValid())
                        {
                            if (auto v = params->Get(c.Dim)) r = *v;
                        }
                        circle.Radius = std::max(0.0, r);
                    }
                }
                break;
            }
            default:
                break;
            }
        }
    }

    int freeCoords = 0;
    for (const auto& p : sketch.Points())
    {
        if (!p.Fixed) freeCoords += 2;
    }
    report.Dof = freeCoords;
    report.Status = SolveStatus::Solved;
    report.Message = "ok";
    if (freeCoords > 0)
    {
        report.Status = SolveStatus::UnderConstrained;
        report.Message = "under-constrained";
    }
    return report;
}

}  // namespace brep::solve2d
