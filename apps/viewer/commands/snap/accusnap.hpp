#pragma once

#include "commands/snap/snap_settings.hpp"

#include <optional>
#include <vector>

namespace brep::viewer {
struct Camera;
}

namespace brep::viewer::commands {

struct CommandContext;

struct PickResult {
  Point3d point{};
  SnapKind kind{SnapKind::None};
  bool snapped{false};
  std::optional<SnapCandidate> candidate;
};

/// Lower values win when candidates have the same screen distance.
[[nodiscard]] int snap_kind_priority(SnapKind kind) noexcept;

/// Quantize a default y=0 workplane hit to the configured grid spacing.
[[nodiscard]] std::optional<SnapCandidate> make_grid_candidate(
    const Point3d& workplane_point, double grid_spacing);

/// Rank visible candidates by aperture distance, kind priority, then depth.
[[nodiscard]] std::optional<SnapCandidate> pick_best_candidate(
    const std::vector<SnapCandidate>& candidates, const Camera& camera,
    int viewport_w, int viewport_h, float sx, float sy, int aperture_px,
    std::optional<SnapKind> override_kind);

class AccuSnap {
 public:
  [[nodiscard]] static PickResult resolve(CommandContext& ctx, float sx,
                                          float sy);
  static void clear_feedback(CommandContext& ctx);
};

}  // namespace brep::viewer::commands
