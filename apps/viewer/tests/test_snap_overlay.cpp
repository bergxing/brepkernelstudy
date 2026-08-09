#include "camera.hpp"
#include "commands/command_types.hpp"
#include "commands/snap/accusnap.hpp"
#include "commands/snap/snap_overlay.hpp"
#include "commands/snap/snap_settings.hpp"
#include "commands/tools/create_box_tool.hpp"
#include "commands/tools/create_sphere_tool.hpp"

#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <limits>

namespace brep::viewer::commands {
namespace {

TEST(SnapOverlay, BuildsDistinctObjectSnapGlyphsAtWinningPoint) {
  const Point3d point{2.0, 3.0, 4.0};

  const EdgeMesh endpoint = make_snap_marker(SnapKind::Endpoint, point);
  const EdgeMesh midpoint = make_snap_marker(SnapKind::Midpoint, point);
  const EdgeMesh center = make_snap_marker(SnapKind::Center, point);
  const EdgeMesh intersection =
      make_snap_marker(SnapKind::Intersection, point);

  EXPECT_EQ(endpoint.positions.size(), 14u);
  EXPECT_EQ(midpoint.positions.size(), 12u);
  EXPECT_GT(center.positions.size(), 22u);
  EXPECT_EQ(intersection.positions.size(), 10u);
  EXPECT_DOUBLE_EQ(endpoint.positions.front().y(), point.y());
  EXPECT_NE(endpoint.positions.front().x(), point.x());
}

TEST(SnapOverlay, ObjectSnapGlyphsSpanAllThreeWorldAxes) {
  const Point3d point{2.0, 3.0, 4.0};
  const SnapKind kinds[] = {
      SnapKind::Endpoint,     SnapKind::Midpoint, SnapKind::Center,
      SnapKind::Intersection, SnapKind::Perpendicular,
      SnapKind::Nearest,      SnapKind::Grid,
  };

  for (const SnapKind kind : kinds) {
    const EdgeMesh marker = make_snap_marker(kind, point);
    bool spans_x = false;
    bool spans_y = false;
    bool spans_z = false;
    for (const Point3d& position : marker.positions) {
      spans_x |= position.x() != point.x();
      spans_y |= position.y() != point.y();
      spans_z |= position.z() != point.z();
    }
    EXPECT_TRUE(spans_x) << static_cast<int>(kind);
    EXPECT_TRUE(spans_y) << static_cast<int>(kind);
    EXPECT_TRUE(spans_z) << static_cast<int>(kind);
  }
}

TEST(SnapOverlay, OmitsMarkerForNoneAndWorkplane) {
  const Point3d point{2.0, 3.0, 4.0};

  EXPECT_TRUE(make_snap_marker(SnapKind::None, point).positions.empty());
  EXPECT_TRUE(
      make_snap_marker(SnapKind::Workplane, point).positions.empty());
}

TEST(SnapOverlay, ProvidesTranslatableObjectSnapNames) {
  EXPECT_EQ(snap_kind_name(SnapKind::Endpoint), QStringLiteral("Endpoint"));
  EXPECT_EQ(snap_kind_name(SnapKind::Midpoint), QStringLiteral("Midpoint"));
  EXPECT_EQ(snap_kind_name(SnapKind::Center), QStringLiteral("Center"));
  EXPECT_EQ(snap_kind_name(SnapKind::Intersection),
            QStringLiteral("Intersection"));
  EXPECT_EQ(snap_kind_name(SnapKind::Perpendicular),
            QStringLiteral("Perpendicular"));
  EXPECT_EQ(snap_kind_name(SnapKind::Nearest), QStringLiteral("Nearest"));
  EXPECT_EQ(snap_kind_name(SnapKind::Grid), QStringLiteral("Grid"));
  EXPECT_TRUE(snap_kind_name(SnapKind::None).isEmpty());
  EXPECT_TRUE(snap_kind_name(SnapKind::Workplane).isEmpty());
}

TEST(SnapFeedback, InitialCreateToolHoverRunsSnapResolution) {
  Camera camera;
  CommandContext ctx;
  ctx.view_camera = &camera;
  ctx.viewport_w = 800;
  ctx.viewport_h = 600;
  int clear_count = 0;
  ctx.clear_snap_overlay = [&] { ++clear_count; };

  CreateBoxTool box;
  box.on_start(ctx);
  box.on_mouse_move(ctx, 400.0f, 300.0f);

  CreateSphereTool sphere;
  sphere.on_start(ctx);
  sphere.on_mouse_move(ctx, 400.0f, 300.0f);

  EXPECT_EQ(clear_count, 2);
}

TEST(SnapFeedback, ClearRemovesOverlayAndCursorTipKind) {
  SnapSession session;
  session.active_snap = SnapKind::Endpoint;
  CommandContext ctx;
  ctx.snap_session = &session;
  int overlay_clears = 0;
  int tip_refreshes = 0;
  ctx.clear_snap_overlay = [&] { ++overlay_clears; };
  ctx.refresh_cursor_tip = [&] { ++tip_refreshes; };

  AccuSnap::clear_feedback(ctx);

  EXPECT_FALSE(session.active_snap.has_value());
  EXPECT_EQ(overlay_clears, 1);
  EXPECT_EQ(tip_refreshes, 1);
}

TEST(AccuSnapGrid, QuantizesWorkplaneCoordinatesToSpacing) {
  const auto candidate =
      make_grid_candidate(Point3d{1.24, 0.0, -2.76}, 0.5);

  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->kind, SnapKind::Grid);
  EXPECT_DOUBLE_EQ(candidate->point.x(), 1.0);
  EXPECT_DOUBLE_EQ(candidate->point.y(), 0.0);
  EXPECT_DOUBLE_EQ(candidate->point.z(), -3.0);
}

TEST(AccuSnapGrid, RejectsInvalidSpacing) {
  EXPECT_FALSE(make_grid_candidate(Point3d{}, 0.0).has_value());
  EXPECT_FALSE(make_grid_candidate(Point3d{}, -1.0).has_value());
  EXPECT_FALSE(
      make_grid_candidate(Point3d{}, std::numeric_limits<double>::infinity())
          .has_value());
}

TEST(AccuSnapGrid, ResolveHonorsGridMasterAndHoldOverride) {
  Camera camera;
  SnapSettings settings;
  settings.grid_enabled = true;
  settings.grid_spacing = 0.5;
  settings.aperture_px = 1000;
  SnapSession session;
  CommandContext ctx;
  ctx.view_camera = &camera;
  ctx.viewport_w = 800;
  ctx.viewport_h = 600;
  ctx.snap_settings = &settings;
  ctx.snap_session = &session;

  const PickResult grid = AccuSnap::resolve(ctx, 400.0f, 300.0f);
  EXPECT_TRUE(grid.snapped);
  EXPECT_EQ(grid.kind, SnapKind::Grid);

  session.hold_override = SnapKind::Endpoint;
  const PickResult endpoint_only = AccuSnap::resolve(ctx, 400.0f, 300.0f);
  EXPECT_FALSE(endpoint_only.snapped);
  EXPECT_EQ(endpoint_only.kind, SnapKind::Workplane);

  settings.grid_enabled = false;
  session.hold_override = SnapKind::Grid;
  const PickResult forced_grid = AccuSnap::resolve(ctx, 400.0f, 300.0f);
  EXPECT_TRUE(forced_grid.snapped);
  EXPECT_EQ(forced_grid.kind, SnapKind::Grid);

  session.hold_override.reset();
  settings.enabled = false;
  const PickResult master_off = AccuSnap::resolve(ctx, 400.0f, 300.0f);
  EXPECT_FALSE(master_off.snapped);
  EXPECT_EQ(master_off.kind, SnapKind::Workplane);
}

TEST(SnapSettings, PersistsAllValuesUnderSnapGroup) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("settings.ini"));

  SnapSettings expected;
  expected.enabled = false;
  expected.kinds = static_cast<std::uint32_t>(SnapKind::Endpoint) |
                   static_cast<std::uint32_t>(SnapKind::Nearest);
  expected.aperture_px = 27;
  expected.grid_enabled = true;
  expected.grid_spacing = 2.5;
  {
    QSettings storage(path, QSettings::IniFormat);
    save_snap_settings(storage, expected);
  }

  QSettings storage(path, QSettings::IniFormat);
  const SnapSettings actual = load_snap_settings(storage);
  EXPECT_EQ(actual.enabled, expected.enabled);
  EXPECT_EQ(actual.kinds, expected.kinds);
  EXPECT_EQ(actual.aperture_px, expected.aperture_px);
  EXPECT_EQ(actual.grid_enabled, expected.grid_enabled);
  EXPECT_DOUBLE_EQ(actual.grid_spacing, expected.grid_spacing);
  EXPECT_TRUE(storage.contains(QStringLiteral("snap/enabled")));
  EXPECT_TRUE(storage.contains(QStringLiteral("snap/kinds")));
  EXPECT_TRUE(storage.contains(QStringLiteral("snap/aperture_px")));
  EXPECT_TRUE(storage.contains(QStringLiteral("snap/grid_enabled")));
  EXPECT_TRUE(storage.contains(QStringLiteral("snap/grid_spacing")));
}

}  // namespace
}  // namespace brep::viewer::commands
