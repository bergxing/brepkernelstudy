#include "api/core.hpp"
#include "api/modeling.hpp"
#include "api/snap.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

namespace brep {
namespace {

bool has_kind_near(const std::vector<SnapCandidate>& cands, SnapKind kind,
                   const Point3d& p, double eps = 1e-6) {
  return std::any_of(cands.begin(), cands.end(), [&](const SnapCandidate& c) {
    return c.kind == kind && (c.point - p).norm() < eps;
  });
}

TEST(SnapQuery, BoxEndpointsAndMidpoints) {
  Model model;
  Body* body = make_box(model, BoxSpec{.min = {0, 0, 0}, .max = {2, 1, 3}, .name = "b"});
  ASSERT_NE(body, nullptr);

  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Endpoint) |
            static_cast<std::uint32_t>(SnapKind::Midpoint);
  auto cands = query_snap_candidates(std::span<Body* const>{&body, 1}, q);

  EXPECT_TRUE(has_kind_near(cands, SnapKind::Endpoint, {0, 0, 0}));
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Endpoint, {2, 1, 3}));
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Midpoint, {1, 0, 0}));  // bottom edge mid
}

TEST(SnapQuery, SphereCenter) {
  Model model;
  Body* body = make_sphere(model, SphereSpec{.center = {1, 2, 3}, .radius = 2.0, .name = "s"});
  ASSERT_NE(body, nullptr);

  SnapQuery q;
  q.kinds = static_cast<std::uint32_t>(SnapKind::Center);
  auto cands = query_snap_candidates(std::span<Body* const>{&body, 1}, q);
  EXPECT_TRUE(has_kind_near(cands, SnapKind::Center, {1, 2, 3}, 1e-3));
}

}  // namespace
}  // namespace brep
