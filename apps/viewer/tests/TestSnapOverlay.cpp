#include "Camera.h"
#include "commands/CommandTypes.h"
#include "commands/snap/Accusnap.h"
#include "commands/snap/SnapOverlay.h"
#include "commands/snap/SnapSettings.h"
#include "commands/tools/CreateBoxTool.h"
#include "commands/tools/CreateSphereTool.h"

#include <QSettings>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include <limits>

namespace brep::viewer::commands
{
namespace
{

TEST(SnapOverlay, BuildsDistinctObjectSnapGlyphsAtWinningPoint)
{
  const Point3d point{2.0, 3.0, 4.0};

  const EdgeMesh endpoint = MakeSnapMarker(SnapKind::Endpoint, point);
  const EdgeMesh midpoint = MakeSnapMarker(SnapKind::Midpoint, point);
  const EdgeMesh center = MakeSnapMarker(SnapKind::Center, point);
  const EdgeMesh intersection =
      MakeSnapMarker(SnapKind::Intersection, point);

  EXPECT_EQ(endpoint.Positions.size(), 14u);
  EXPECT_EQ(midpoint.Positions.size(), 12u);
  EXPECT_GT(center.Positions.size(), 22u);
  EXPECT_EQ(intersection.Positions.size(), 10u);
  EXPECT_DOUBLE_EQ(endpoint.Positions.front().y(), point.y());
  EXPECT_NE(endpoint.Positions.front().x(), point.x());
}

TEST(SnapOverlay, ObjectSnapGlyphsSpanAllThreeWorldAxes)
{
  const Point3d point{2.0, 3.0, 4.0};
  const SnapKind kinds[] = {
      SnapKind::Endpoint,     SnapKind::Midpoint, SnapKind::Center,
      SnapKind::Intersection, SnapKind::Perpendicular,
      SnapKind::Nearest,      SnapKind::Grid,
  };

  for (const SnapKind kind : kinds)
  {
    const EdgeMesh marker = MakeSnapMarker(kind, point);
    bool spans_x = false;
    bool spans_y = false;
    bool spans_z = false;
    for (const Point3d& position : marker.Positions)
    {
      spans_x |= position.x() != point.x();
      spans_y |= position.y() != point.y();
      spans_z |= position.z() != point.z();
    }
    EXPECT_TRUE(spans_x) << static_cast<int>(kind);
    EXPECT_TRUE(spans_y) << static_cast<int>(kind);
    EXPECT_TRUE(spans_z) << static_cast<int>(kind);
  }
}

TEST(SnapOverlay, OmitsMarkerForNoneAndWorkplane)
{
  const Point3d point{2.0, 3.0, 4.0};

  EXPECT_TRUE(MakeSnapMarker(SnapKind::None, point).Positions.empty());
  EXPECT_TRUE(
      MakeSnapMarker(SnapKind::Workplane, point).Positions.empty());
}

TEST(SnapOverlay, ProvidesTranslatableObjectSnapNames)
{
  EXPECT_EQ(SnapKindName(SnapKind::Endpoint), QStringLiteral("Endpoint"));
  EXPECT_EQ(SnapKindName(SnapKind::Midpoint), QStringLiteral("Midpoint"));
  EXPECT_EQ(SnapKindName(SnapKind::Center), QStringLiteral("Center"));
  EXPECT_EQ(SnapKindName(SnapKind::Intersection),
            QStringLiteral("Intersection"));
  EXPECT_EQ(SnapKindName(SnapKind::Perpendicular),
            QStringLiteral("Perpendicular"));
  EXPECT_EQ(SnapKindName(SnapKind::Nearest), QStringLiteral("Nearest"));
  EXPECT_EQ(SnapKindName(SnapKind::Grid), QStringLiteral("Grid"));
  EXPECT_TRUE(SnapKindName(SnapKind::None).isEmpty());
  EXPECT_TRUE(SnapKindName(SnapKind::Workplane).isEmpty());
}

TEST(SnapFeedback, InitialCreateToolHoverRunsSnapResolution)
{
  Camera camera;
  CommandContext ctx;
  ctx.ViewCamera = &camera;
  ctx.ViewportWidth = 800;
  ctx.ViewportHeight = 600;
  int clear_count = 0;
  ctx.ClearSnapOverlay = [&] { ++clear_count; };

  CreateBoxTool box;
  box.on_start(ctx);
  box.on_mouse_move(ctx, 400.0f, 300.0f);

  CreateSphereTool sphere;
  sphere.on_start(ctx);
  sphere.on_mouse_move(ctx, 400.0f, 300.0f);

  EXPECT_EQ(clear_count, 2);
}

TEST(SnapFeedback, ClearRemovesOverlayAndCursorTipKind)
{
  SnapSession session;
  session.active_snap = SnapKind::Endpoint;
  CommandContext ctx;
  ctx.SnapSessionRef = &session;
  int overlay_clears = 0;
  int tip_refreshes = 0;
  ctx.ClearSnapOverlay = [&] { ++overlay_clears; };
  ctx.RefreshCursorTip = [&] { ++tip_refreshes; };

  AccuSnap::clear_feedback(ctx);

  EXPECT_FALSE(session.active_snap.has_value());
  EXPECT_EQ(overlay_clears, 1);
  EXPECT_EQ(tip_refreshes, 1);
}

TEST(AccuSnapGrid, QuantizesWorkplaneCoordinatesToSpacing)
{
  const auto candidate =
      make_grid_candidate(Point3d{1.24, 0.0, -2.76}, 0.5);

  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->kind, SnapKind::Grid);
  EXPECT_DOUBLE_EQ(candidate->point.x(), 1.0);
  EXPECT_DOUBLE_EQ(candidate->point.y(), 0.0);
  EXPECT_DOUBLE_EQ(candidate->point.z(), -3.0);
}

TEST(AccuSnapGrid, RejectsInvalidSpacing)
{
  EXPECT_FALSE(make_grid_candidate(Point3d{}, 0.0).has_value());
  EXPECT_FALSE(make_grid_candidate(Point3d{}, -1.0).has_value());
  EXPECT_FALSE(
      make_grid_candidate(Point3d{}, std::numeric_limits<double>::infinity())
          .has_value());
}

TEST(AccuSnapGrid, ResolveHonorsGridMasterAndHoldOverride)
{
  Camera camera;
  SnapSettings settings;
  settings.grid_enabled = true;
  settings.grid_spacing = 0.5;
  settings.aperture_px = 1000;
  SnapSession session;
  CommandContext ctx;
  ctx.ViewCamera = &camera;
  ctx.ViewportWidth = 800;
  ctx.ViewportHeight = 600;
  ctx.SnapSettingsRef = &settings;
  ctx.SnapSessionRef = &session;

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

TEST(SnapSettings, PersistsAllValuesUnderSnapGroup)
{
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
