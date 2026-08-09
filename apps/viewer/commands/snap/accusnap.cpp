#include "commands/snap/accusnap.hpp"

#include "commands/command_types.hpp"
#include "commands/picking.hpp"
#include "commands/snap/snap_overlay.hpp"

#include "api/modeling.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

namespace brep::viewer::commands {
namespace {

constexpr std::uint32_t kKernelKinds =
    static_cast<std::uint32_t>(SnapKind::Endpoint) |
    static_cast<std::uint32_t>(SnapKind::Midpoint) |
    static_cast<std::uint32_t>(SnapKind::Center) |
    static_cast<std::uint32_t>(SnapKind::Intersection) |
    static_cast<std::uint32_t>(SnapKind::Perpendicular) |
    static_cast<std::uint32_t>(SnapKind::Nearest);

void clear_snap_feedback(CommandContext& ctx) {
  if (ctx.snap_session) ctx.snap_session->active_snap.reset();
  if (ctx.clear_snap_overlay) ctx.clear_snap_overlay();
  if (ctx.refresh_cursor_tip) ctx.refresh_cursor_tip();
}

std::uint32_t effective_kernel_kinds(
    const SnapSettings& settings,
    const std::optional<SnapKind>& override_kind) {
  if (!settings.enabled) return 0;
  if (override_kind) {
    return static_cast<std::uint32_t>(*override_kind) & kKernelKinds;
  }
  return settings.kinds & kKernelKinds;
}

PickResult finish_resolve(CommandContext& ctx, PickResult result) {
  const bool show_snap = result.snapped && result.kind != SnapKind::Workplane;
  if (!show_snap) {
    clear_snap_feedback(ctx);
    return result;
  }

  if (ctx.snap_session) ctx.snap_session->active_snap = result.kind;
  if (ctx.set_snap_overlay) {
    ctx.set_snap_overlay(make_snap_marker(result.kind, result.point));
  }
  if (ctx.refresh_cursor_tip) ctx.refresh_cursor_tip();
  return result;
}

}  // namespace

int snap_kind_priority(SnapKind kind) noexcept {
  switch (kind) {
    case SnapKind::Endpoint:
      return 0;
    case SnapKind::Midpoint:
      return 1;
    case SnapKind::Intersection:
      return 2;
    case SnapKind::Center:
      return 3;
    case SnapKind::Perpendicular:
      return 4;
    case SnapKind::Nearest:
      return 5;
    case SnapKind::Grid:
      return 6;
    case SnapKind::Workplane:
      return 7;
    case SnapKind::None:
      return 8;
  }
  return 8;
}

std::optional<SnapCandidate> pick_best_candidate(
    const std::vector<SnapCandidate>& candidates, const Camera& camera,
    int viewport_w, int viewport_h, float sx, float sy, int aperture_px,
    std::optional<SnapKind> override_kind) {
  if (viewport_w <= 0 || viewport_h <= 0 || aperture_px < 0) {
    return std::nullopt;
  }

  Point3d ray_origin;
  Vector3d ray_direction;
  if (!screen_to_ray(camera, viewport_w, viewport_h, sx, sy, ray_origin,
                     ray_direction)) {
    return std::nullopt;
  }
  const double aperture_squared =
      static_cast<double>(aperture_px) * aperture_px;

  const SnapCandidate* best = nullptr;
  std::tuple<double, int, double> best_score{
      std::numeric_limits<double>::infinity(),
      std::numeric_limits<int>::max(),
      std::numeric_limits<double>::infinity()};

  for (const SnapCandidate& candidate : candidates) {
    if (override_kind && candidate.kind != *override_kind) continue;

    float candidate_sx = 0.0f;
    float candidate_sy = 0.0f;
    if (!world_to_screen(camera, viewport_w, viewport_h, candidate.point,
                         candidate_sx, candidate_sy)) {
      continue;
    }

    const double dx = static_cast<double>(candidate_sx - sx);
    const double dy = static_cast<double>(candidate_sy - sy);
    const double distance_squared = dx * dx + dy * dy;
    if (distance_squared > aperture_squared) continue;

    const double depth =
        (candidate.point - ray_origin).dot(ray_direction);
    if (depth < 0.0) continue;
    const auto score = std::tuple{distance_squared,
                                  snap_kind_priority(candidate.kind), depth};
    if (score < best_score) {
      best = &candidate;
      best_score = score;
    }
  }

  if (!best) return std::nullopt;
  return *best;
}

PickResult AccuSnap::resolve(CommandContext& ctx, float sx, float sy) {
  const Camera* camera =
      ctx.view_camera ? ctx.view_camera
                      : (ctx.world ? ctx.world->main_camera() : nullptr);
  if (!camera) return finish_resolve(ctx, {});

  Point3d ray_origin;
  Vector3d ray_direction;
  if (!screen_to_ray(*camera, ctx.viewport_w, ctx.viewport_h, sx, sy,
                     ray_origin, ray_direction)) {
    return finish_resolve(ctx, {});
  }

  Point3d workplane_point;
  const bool have_workplane =
      intersect_plane_y(ray_origin, ray_direction, 0.0, workplane_point);

  const SnapSettings* settings = ctx.snap_settings;
  SnapSession* session = ctx.snap_session;
  const std::optional<SnapKind> override_kind =
      session ? session->hold_override : std::nullopt;

  if (settings && settings->enabled && ctx.world && ctx.world->document()) {
    Part* part = ctx.world->document()->main_part();
    if (part) {
      std::vector<Body*> bodies;
      bodies.reserve(part->model().bodies().size());
      for (const auto& body : part->model().bodies()) {
        if (body) bodies.push_back(body.get());
      }

      SnapQuery query;
      query.kinds = effective_kernel_kinds(*settings, override_kind);
      if (have_workplane) query.near_point = workplane_point;
      if (session) query.reference_point = session->last_point;

      if (query.kinds != 0 && !bodies.empty()) {
        const auto candidates = query_snap_candidates(bodies, query);
        if (auto best = pick_best_candidate(
                candidates, *camera, ctx.viewport_w, ctx.viewport_h, sx, sy,
                std::max(0, settings->aperture_px), override_kind)) {
          return finish_resolve(
              ctx, {.point = best->point,
                    .kind = best->kind,
                    .snapped = true,
                    .candidate = std::move(best)});
        }
      }
    }
  }

  if (have_workplane) {
    return finish_resolve(ctx, {.point = workplane_point,
                                .kind = SnapKind::Workplane,
                                .snapped = false,
                                .candidate = std::nullopt});
  }
  return finish_resolve(ctx, {});
}

void AccuSnap::clear_feedback(CommandContext& ctx) {
  clear_snap_feedback(ctx);
}

}  // namespace brep::viewer::commands
