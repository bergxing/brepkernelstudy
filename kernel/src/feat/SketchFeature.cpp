#include "brep/feat/SketchFeature.h"

#include "brep/solve2d/Solver.h"

#include <cmath>

namespace brep::feat
{

SketchFeature::SketchFeature(FeatureId id, std::string name, sketch::Sketch sketch)
    : m_id(id), m_name(std::move(name)), m_sketch(std::move(sketch))
{
}

void SketchFeature::CollectParameters(param::ParameterStore& /*store*/)
{
}

bool SketchFeature::Rebuild(Part& /*part*/, param::ParameterStore& params)
{
    const auto report =
        solve2d::ConstraintSolver::Solve(m_sketch, m_sketch.Constraints(), &params);
    return report.Status != solve2d::SolveStatus::Failed;
}

std::unique_ptr<SketchFeature> SketchFeature::CreateRectangle(param::ParameterStore& store,
                                                              std::string name, Point2d min,
                                                              Point2d max, brep::Plane frame)
                                                              {
    sketch::Sketch sk;
    sk.SetFrame(frame);
    const auto p00 = sk.AddPoint(min);
    const auto p10 = sk.AddPoint(Point2d{max.u(), min.v()});
    const auto p11 = sk.AddPoint(max);
    const auto p01 = sk.AddPoint(Point2d{min.u(), max.v()});
    sk.AddLine(p00, p10);
    sk.AddLine(p10, p11);
    sk.AddLine(p11, p01);
    sk.AddLine(p01, p00);

    const double w = std::abs(max.u() - min.u());
    const double h = std::abs(max.v() - min.v());
    auto width = store.Add(name + ".Width", param::ParamKind::Length, w);
    auto height = store.Add(name + ".Height", param::ParamKind::Length, h);

    sk.AddConstraint(sketch::Constraint{{}, sketch::ConstraintKind::Fixed, p00, {}, {}, 0.0});
    sk.AddConstraint(
        sketch::Constraint{{}, sketch::ConstraintKind::Horizontal, p00, p10, {}, 0.0});
    sk.AddConstraint(sketch::Constraint{{}, sketch::ConstraintKind::Vertical, p00, p01, {}, 0.0});
    sk.AddConstraint(
        sketch::Constraint{{}, sketch::ConstraintKind::Distance, p00, p10, width, w});
    sk.AddConstraint(
        sketch::Constraint{{}, sketch::ConstraintKind::Distance, p00, p01, height, h});
    sk.AddConstraint(
        sketch::Constraint{{}, sketch::ConstraintKind::Horizontal, p01, p11, {}, 0.0});
    sk.AddConstraint(
        sketch::Constraint{{}, sketch::ConstraintKind::Vertical, p10, p11, {}, 0.0});

    FeatureId id{Guid::Generate()};
    return std::make_unique<SketchFeature>(id, std::move(name), std::move(sk));
}

}  // namespace brep::feat
