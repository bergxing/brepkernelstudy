#include "brep/feat/box_feature.hpp"

#include "brep/document.hpp"
#include "brep/part.hpp"

#include <algorithm>
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

BoxFeature::BoxFeature(FeatureId id, std::string name, Point3d origin,
                       param::ParameterId length, param::ParameterId width,
                       param::ParameterId height)
    : id_(id),
      name_(std::move(name)),
      origin_(origin),
      length_(length),
      width_(width),
      height_(height) {}

std::unique_ptr<BoxFeature> BoxFeature::create(param::ParameterStore& store,
                                               const BoxSpec& spec) {
  const double lx = std::abs(spec.max.x() - spec.min.x());
  const double hy = std::abs(spec.max.y() - spec.min.y());
  const double wz = std::abs(spec.max.z() - spec.min.z());
  const Point3d origin{std::min(spec.min.x(), spec.max.x()),
                       std::min(spec.min.y(), spec.max.y()),
                       std::min(spec.min.z(), spec.max.z())};

  const std::string base = spec.name.empty() ? "Box" : spec.name;
  FeatureId fid{Guid::generate()};
  auto length = store.add(unique_param_name(store, base + ".Length"),
                          param::ParamKind::Length, lx);
  auto width = store.add(unique_param_name(store, base + ".Width"),
                         param::ParamKind::Length, wz);
  auto height = store.add(unique_param_name(store, base + ".Height"),
                          param::ParamKind::Length, hy);
  return std::make_unique<BoxFeature>(fid, base, origin, length, width, height);
}

void BoxFeature::collect_parameters(param::ParameterStore& /*store*/) {
  // Parameters are owned by ParameterStore at creation time.
}

BoxSpec BoxFeature::to_spec(const param::ParameterStore& params) const {
  const double lx = params.get(length_).value_or(1.0);
  const double wz = params.get(width_).value_or(1.0);
  const double hy = params.get(height_).value_or(1.0);
  BoxSpec spec;
  spec.min = origin_;
  spec.max = Point3d{origin_.x() + lx, origin_.y() + hy, origin_.z() + wz};
  spec.name = name_;
  return spec;
}

bool BoxFeature::rebuild(Part& part, param::ParameterStore& params) {
  const BoxSpec spec = to_spec(params);
  if (std::abs(spec.max.x() - spec.min.x()) < 1e-9 ||
      std::abs(spec.max.y() - spec.min.y()) < 1e-9 ||
      std::abs(spec.max.z() - spec.min.z()) < 1e-9) {
    return false;
  }

  Body* body = part.rebuild_box_body(body_guid_, spec);
  if (!body) return false;
  body_guid_ = body->guid;
  return true;
}

}  // namespace brep::feat
