#pragma once

#include "api/snap.hpp"

#include <cstdint>
#include <optional>

namespace brep::viewer::commands {

[[nodiscard]] std::uint32_t default_snap_kinds() noexcept;

struct SnapSettings {
  bool enabled{true};
  std::uint32_t kinds{default_snap_kinds()};
  int aperture_px{12};
  bool grid_enabled{false};
  double grid_spacing{1.0};
};

struct SnapSession {
  std::optional<Point3d> last_point;

  // Reserved for AccuDraw; unused in Phase 1.
  std::optional<Point3d> origin;
  std::optional<Vector3d> axis_x;
  std::optional<Vector3d> axis_y;
  int dynamic_input_mode{0};

  std::optional<SnapKind> hold_override;
  std::optional<SnapKind> active_snap;
};

}  // namespace brep::viewer::commands
