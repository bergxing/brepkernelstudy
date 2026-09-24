#pragma once

#include "commands/snap/SnapSettings.h"

#include <optional>
#include <vector>

namespace brep::viewer
{
struct Camera;
}

namespace brep::viewer::commands
{

struct CommandContext;

struct PickResult
{
    Point3d Point{};
    SnapKind Kind{SnapKind::None};
    bool Snapped{false};
    std::optional<SnapCandidate> Candidate;
};

/// Lower values win when candidates have the same screen distance.
[[nodiscard]] int SnapKindPriority(SnapKind kind) noexcept;

/// Quantize a default y=0 workplane hit to the configured grid spacing.
[[nodiscard]] std::optional<SnapCandidate> MakeGridCandidate(
    const Point3d& workplanePoint, double gridSpacing);

/// Rank visible candidates by aperture distance, kind priority, then depth.
[[nodiscard]] std::optional<SnapCandidate> PickBestCandidate(
    const std::vector<SnapCandidate>& candidates, const Camera& camera,
    int viewportW, int viewportH, float sx, float sy, int aperturePx,
    std::optional<SnapKind> overrideKind);

class AccuSnap
{
public:
    [[nodiscard]] static PickResult Resolve(CommandContext& ctx, float sx,
                                            float sy);
    static void ClearFeedback(CommandContext& ctx);
};

}  // namespace brep::viewer::commands
