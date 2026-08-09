#include "brep/feat/sphere_feature.hpp"

#include "brep/document.hpp"
#include "brep/part.hpp"

#include <cmath>

namespace brep::feat {
namespace {

std::string unique_param_name(param::ParameterStore& store, std::string base) {
  if (!store.find_by_name(base)) return base;
  for (int i = 2; i < 10000; ++i) {
    std::string candidate = base + "_" + std::to_string(i);
    if (!store.find_by_name(candidate)) return candidate;
  }
  return base + "_" + Guid::generate().to_string();
}

}  // namespace

SphereFeature::SphereFeature(FeatureId id, std::string name, Point3d center,
                             param::ParameterId radius)
    : id_(id), name_(std::move(name)), center_(center), radius_(radius) {}

std::unique_ptr<SphereFeature> SphereFeature::create(
    param::ParameterStore& store, const SphereSpec& spec) {
  const std::string base = spec.name.empty() ? "Sphere" : spec.name;
  FeatureId fid{Guid::generate()};
  auto radius = store.add(unique_param_name(store, base + ".Radius"),
                          param::ParamKind::Length, std::abs(spec.radius));
  return std::make_unique<SphereFeature>(fid, base, spec.center, radius);
}

void SphereFeature::collect_parameters(param::ParameterStore& /*store*/) {}

SphereSpec SphereFeature::to_spec(const param::ParameterStore& params) const {
  SphereSpec spec;
  spec.center = center_;
  spec.radius = params.get(radius_).value_or(1.0);
  spec.name = name_;
  return spec;
}

bool SphereFeature::rebuild(Part& part, param::ParameterStore& params) {
  const SphereSpec spec = to_spec(params);
  if (!(spec.radius > 1e-9)) return false;

  Body* body = part.rebuild_sphere_body(body_guid_, spec);
  if (!body) return false;
  body_guid_ = body->guid;
  return true;
}

}  // namespace brep::feat
