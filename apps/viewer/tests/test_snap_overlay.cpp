#include "commands/snap/snap_overlay.hpp"

#include <gtest/gtest.h>

namespace brep::viewer::commands {
namespace {

TEST(SnapOverlay, BuildsDistinctObjectSnapGlyphsAtWinningPoint) {
  const Point3d point{2.0, 3.0, 4.0};

  const EdgeMesh endpoint = make_snap_marker(SnapKind::Endpoint, point);
  const EdgeMesh midpoint = make_snap_marker(SnapKind::Midpoint, point);
  const EdgeMesh center = make_snap_marker(SnapKind::Center, point);
  const EdgeMesh intersection =
      make_snap_marker(SnapKind::Intersection, point);

  EXPECT_EQ(endpoint.positions.size(), 8u);
  EXPECT_EQ(midpoint.positions.size(), 6u);
  EXPECT_GT(center.positions.size(), 16u);
  EXPECT_EQ(intersection.positions.size(), 4u);
  EXPECT_DOUBLE_EQ(endpoint.positions.front().y(), point.y());
  EXPECT_NE(endpoint.positions.front().x(), point.x());
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

}  // namespace
}  // namespace brep::viewer::commands
