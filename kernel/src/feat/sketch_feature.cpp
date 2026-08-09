#include "brep/feat/sketch_feature.hpp"

#include "brep/solve2d/solver.hpp"

#include <cmath>

namespace brep::feat {

SketchFeature::SketchFeature(FeatureId id, std::string name,
                             sketch::Sketch sketch)
    : id_(id), name_(std::move(name)), sketch_(std::move(sketch)) {}

void SketchFeature::collect_parameters(param::ParameterStore& /*store*/) {}

bool SketchFeature::rebuild(Part& /*part*/, param::ParameterStore& params) {
  const auto report = solve2d::ConstraintSolver::solve(
      sketch_, sketch_.constraints(), &params);
  return report.status != solve2d::SolveStatus::Failed;
}

std::unique_ptr<SketchFeature> SketchFeature::create_rectangle(
    param::ParameterStore& store, std::string name, Point2d min, Point2d max,
    brep::Plane frame) {
  sketch::Sketch sk;
  sk.set_frame(frame);
  const auto p00 = sk.add_point(min);
  const auto p10 = sk.add_point(Point2d{max.u(), min.v()});
  const auto p11 = sk.add_point(max);
  const auto p01 = sk.add_point(Point2d{min.u(), max.v()});
  sk.add_line(p00, p10);
  sk.add_line(p10, p11);
  sk.add_line(p11, p01);
  sk.add_line(p01, p00);

  const double w = std::abs(max.u() - min.u());
  const double h = std::abs(max.v() - min.v());
  auto width = store.add(name + ".Width", param::ParamKind::Length, w);
  auto height = store.add(name + ".Height", param::ParamKind::Length, h);

  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Fixed, p00, {}, {}, 0.0});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Horizontal, p00, p10, {}, 0.0});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Vertical, p00, p01, {}, 0.0});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Distance, p00, p10, width, w});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Distance, p00, p01, height, h});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Horizontal, p01, p11, {}, 0.0});
  sk.add_constraint(sketch::Constraint{
      {}, sketch::ConstraintKind::Vertical, p10, p11, {}, 0.0});

  FeatureId id{Guid::generate()};
  return std::make_unique<SketchFeature>(id, std::move(name), std::move(sk));
}

}  // namespace brep::feat
