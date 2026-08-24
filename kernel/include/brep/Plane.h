#pragma once

#include "brep/Math.h"

namespace brep
{

/// Oriented plane for sketch frames / extrude paths.
struct Plane
{
  Point3d Origin{0, 0, 0};
  Vector3d Normal{0, 1, 0};
  Vector3d UAxis{1, 0, 0};
  Vector3d VAxis{0, 0, 1};

  [[nodiscard]] Point3d ToWorld(const Point2d& uv) const
  {
    return Origin + UAxis * uv.u() + VAxis * uv.v();
  }

  static Plane Xy()
  {
    return Plane{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, Vector3d{1, 0, 0},
                 Vector3d{0, 1, 0}};
  }

  /// Ground plane used by the viewer create-box tool (Y up).
  static Plane XzYUp()
  {
    return Plane{Point3d{0, 0, 0}, Vector3d{0, 1, 0}, Vector3d{1, 0, 0},
                 Vector3d{0, 0, 1}};
  }
};

struct RigidTransform
{
    Point3d Translation{0, 0, 0};
    // Simple axis-angle / basis; identity by default.
    Vector3d XAxis{1, 0, 0};
    Vector3d YAxis{0, 1, 0};
    Vector3d ZAxis{0, 0, 1};

    [[nodiscard]] Point3d TransformPoint(const Point3d& p) const
    {
        return Translation + XAxis * p.x() + YAxis * p.y() + ZAxis * p.z();
    }
};

}  // namespace brep
