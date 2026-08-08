#pragma once

#include "brep/math.hpp"

namespace brep {

/// Oriented plane for sketch frames / extrude paths.
struct Plane {
  Point3d origin{0, 0, 0};
  Vector3d normal{0, 1, 0};
  Vector3d u_axis{1, 0, 0};
  Vector3d v_axis{0, 0, 1};

  [[nodiscard]] Point3d to_world(const Point2d& uv) const {
    return origin + u_axis * uv.u() + v_axis * uv.v();
  }

  static Plane xy() {
    return Plane{Point3d{0, 0, 0}, Vector3d{0, 0, 1}, Vector3d{1, 0, 0},
                 Vector3d{0, 1, 0}};
  }

  /// Ground plane used by the viewer create-box tool (Y up).
  static Plane xz_y_up() {
    return Plane{Point3d{0, 0, 0}, Vector3d{0, 1, 0}, Vector3d{1, 0, 0},
                 Vector3d{0, 0, 1}};
  }
};

struct RigidTransform {
  Point3d translation{0, 0, 0};
  // Simple axis-angle / basis; identity by default.
  Vector3d x_axis{1, 0, 0};
  Vector3d y_axis{0, 1, 0};
  Vector3d z_axis{0, 0, 1};

  [[nodiscard]] Point3d transform_point(const Point3d& p) const {
    return translation + x_axis * p.x() + y_axis * p.y() + z_axis * p.z();
  }
};

}  // namespace brep
