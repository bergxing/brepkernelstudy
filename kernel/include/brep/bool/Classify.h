#pragma once

#include "brep/bool/PlanarRecognize.h"
#include "brep/Builder.h"
#include "brep/Math.h"

namespace brep::boolean
{

enum class SolidClass
{
    In, Out, On 
};

[[nodiscard]] SolidClass ClassifyPointInBox(const BoxSpec& box,
                                               const Point3d& p, double eps);

[[nodiscard]] SolidClass ClassifyPointInPrism(const PlanarPrismSpec& prism,
                                                 const Point3d& p, double eps);

[[nodiscard]] SolidClass ClassifyPointInSphere(const SphereSpec& sphere,
                                                  const Point3d& p, double eps);

}  // namespace brep::boolean
