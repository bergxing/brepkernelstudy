#pragma once

#include "brep/bool/planar_recognize.hpp"
#include "brep/builder.hpp"
#include "brep/math.hpp"

namespace brep::boolean {

enum class SolidClass { In, Out, On };

[[nodiscard]] SolidClass classify_point_in_box(const BoxSpec& box,
                                               const Point3d& p, double eps);

[[nodiscard]] SolidClass classify_point_in_prism(const PlanarPrismSpec& prism,
                                                 const Point3d& p, double eps);

[[nodiscard]] SolidClass classify_point_in_sphere(const SphereSpec& sphere,
                                                  const Point3d& p, double eps);

}  // namespace brep::boolean
