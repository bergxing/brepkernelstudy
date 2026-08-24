#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/spatial/Aabb.h"
#include "brep/spatial/FaceBvh.h"

#include <gtest/gtest.h>

#include <set>
#include <utility>
#include <vector>

namespace brep
{
namespace
{

using FacePair = std::pair<const Face*, const Face*>;

[[nodiscard]] std::set<FacePair> naive_overlapping_pairs(const Body& a,
                                                         const Body& b)
{
  std::set<FacePair> out;
  for (const Shell* sa : a.shells)
  {
    if (!sa) continue;
    for (Face* fa : sa->faces)
    {
      if (!fa) continue;
      const spatial::Aabb ba = spatial::estimate_face_aabb(*fa);
      for (const Shell* sb : b.shells)
      {
        if (!sb) continue;
        for (Face* fb : sb->faces)
        {
          if (!fb) continue;
          const spatial::Aabb bb = spatial::estimate_face_aabb(*fb);
          if (ba.overlaps(bb))
          {
            out.insert(FacePair{fa, fb});
          }
        }
      }
    }
  }
  return out;
}

[[nodiscard]] std::set<FacePair> to_set(
    const std::vector<std::pair<Face*, Face*>>& pairs)
{
  std::set<FacePair> out;
  for (const auto& p : pairs)
  {
    out.insert(FacePair{p.first, p.second});
  }
  return out;
}

TEST(Aabb, OverlapsAndSurfaceArea)
{
  const spatial::Aabb a{{0, 0, 0}, {1, 1, 1}};
  const spatial::Aabb b{{0.5, 0.5, 0.5}, {2, 2, 2}};
  const spatial::Aabb c{{2, 0, 0}, {3, 1, 1}};
  EXPECT_TRUE(a.overlaps(b));
  EXPECT_FALSE(a.overlaps(c));
  EXPECT_NEAR(a.surface_area(), 6.0, 1e-12);
  const spatial::Aabb m = spatial::Aabb::merge(a, c);
  EXPECT_NEAR(m.min.x(), 0.0, 1e-12);
  EXPECT_NEAR(m.max.x(), 3.0, 1e-12);
}

TEST(FaceBvh, SeparatedBoxesHaveNoCandidatePairs)
{
  Model model;
  Body* left =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "L"});
  Body* right =
      MakeBox(model, BoxSpec{.Min = {3, 0, 0}, .Max = {4, 1, 1}, .Name = "R"});
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);

  spatial::FaceBvh a =
      spatial::FaceBvh::build(*left, spatial::BuildQuality::Median);
  spatial::FaceBvh b =
      spatial::FaceBvh::build(*right, spatial::BuildQuality::Median);
  const auto pairs = spatial::FaceBvh::candidate_pairs(a, b);
  EXPECT_TRUE(pairs.empty());
  EXPECT_TRUE(naive_overlapping_pairs(*left, *right).empty());
}

TEST(FaceBvh, OverlappingBoxesCandidatesSubsetOfNaive)
{
  Model model;
  Body* a_body =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* b_body =
      MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(a_body, nullptr);
  ASSERT_NE(b_body, nullptr);

  spatial::FaceBvh a =
      spatial::FaceBvh::build(*a_body, spatial::BuildQuality::Median);
  spatial::FaceBvh b =
      spatial::FaceBvh::build(*b_body, spatial::BuildQuality::Median);
  const auto pairs = spatial::FaceBvh::candidate_pairs(a, b);
  EXPECT_FALSE(pairs.empty());

  const auto naive = naive_overlapping_pairs(*a_body, *b_body);
  const auto got = to_set(pairs);
  for (const FacePair& p : got)
  {
    EXPECT_TRUE(naive.count(p) > 0)
        << "BVH pair not in naive AABB overlap set";
  }
  EXPECT_EQ(got.size(), naive.size());
}

TEST(FaceBvh, QueryOverlapsHitsNearbyFaces)
{
  Model model;
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "B"});
  ASSERT_NE(box, nullptr);
  spatial::FaceBvh bvh =
      spatial::FaceBvh::build(*box, spatial::BuildQuality::Median);

  const spatial::Aabb query{{0.9, 0.4, 0.4}, {1.1, 0.6, 0.6}};
  const auto hits = bvh.query_overlaps(query);
  EXPECT_FALSE(hits.empty());
  for (const Face* f : hits)
  {
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(spatial::estimate_face_aabb(*f).overlaps(query));
  }

  const spatial::Aabb far_q{{10, 10, 10}, {11, 11, 11}};
  EXPECT_TRUE(bvh.query_overlaps(far_q).empty());
}

TEST(FaceBvh, SphereFaceAabbIsCenterPlusMinusRadius)
{
  Model model;
  Body* sph = MakeSphere(
      model, SphereSpec{.Center = {1, 2, 3}, .Radius = 4.0, .Name = "S"});
  ASSERT_NE(sph, nullptr);
  ASSERT_NE(sph->outer_shell(), nullptr);
  ASSERT_FALSE(sph->outer_shell()->faces.empty());
  Face* face = sph->outer_shell()->faces.front();
  ASSERT_NE(face, nullptr);

  const spatial::Aabb box = spatial::estimate_face_aabb(*face);
  EXPECT_NEAR(box.min.x(), -3.0, 1e-9);
  EXPECT_NEAR(box.min.y(), -2.0, 1e-9);
  EXPECT_NEAR(box.min.z(), -1.0, 1e-9);
  EXPECT_NEAR(box.max.x(), 5.0, 1e-9);
  EXPECT_NEAR(box.max.y(), 6.0, 1e-9);
  EXPECT_NEAR(box.max.z(), 7.0, 1e-9);

  spatial::FaceBvh bvh =
      spatial::FaceBvh::build(*sph, spatial::BuildQuality::Median);
  EXPECT_FALSE(bvh.query_overlaps(box).empty());
}

TEST(FaceBvh, SahSeparatedBoxesHaveNoCandidatePairs)
{
  Model model;
  Body* left =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "L"});
  Body* right =
      MakeBox(model, BoxSpec{.Min = {5, 0, 0}, .Max = {6, 1, 1}, .Name = "R"});
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);

  spatial::FaceBvh a =
      spatial::FaceBvh::build(*left, spatial::BuildQuality::Sah);
  spatial::FaceBvh b =
      spatial::FaceBvh::build(*right, spatial::BuildQuality::Sah);
  EXPECT_TRUE(spatial::FaceBvh::candidate_pairs(a, b).empty());
}

TEST(FaceBvh, SahOverlappingBoxesCandidatesMatchNaive)
{
  Model model;
  Body* a_body =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 2, 2}, .Name = "A"});
  Body* b_body =
      MakeBox(model, BoxSpec{.Min = {1, 1, 1}, .Max = {3, 3, 3}, .Name = "B"});
  ASSERT_NE(a_body, nullptr);
  ASSERT_NE(b_body, nullptr);

  spatial::FaceBvh a =
      spatial::FaceBvh::build(*a_body, spatial::BuildQuality::Sah);
  spatial::FaceBvh b =
      spatial::FaceBvh::build(*b_body, spatial::BuildQuality::Sah);
  const auto pairs = spatial::FaceBvh::candidate_pairs(a, b);
  EXPECT_FALSE(pairs.empty());
  EXPECT_EQ(to_set(pairs), naive_overlapping_pairs(*a_body, *b_body));
}

TEST(FaceBvh, SahQueryAgreesWithMedianAndTracksVisits)
{
  Model model;
  Body* box =
      MakeBox(model, BoxSpec{.Min = {0, 0, 0}, .Max = {2, 1, 1}, .Name = "B"});
  ASSERT_NE(box, nullptr);

  spatial::FaceBvh median =
      spatial::FaceBvh::build(*box, spatial::BuildQuality::Median);
  spatial::FaceBvh sah =
      spatial::FaceBvh::build(*box, spatial::BuildQuality::Sah);

  const spatial::Aabb query{{1.9, 0.4, 0.4}, {2.1, 0.6, 0.6}};
  spatial::QueryStats med_stats;
  spatial::QueryStats sah_stats;
  auto med_hits = median.query_overlaps(query, &med_stats);
  auto sah_hits = sah.query_overlaps(query, &sah_stats);

  std::set<const Face*> med_set(med_hits.begin(), med_hits.end());
  std::set<const Face*> sah_set(sah_hits.begin(), sah_hits.end());
  EXPECT_EQ(med_set, sah_set);
  EXPECT_GT(med_stats.nodes_visited, 0u);
  EXPECT_GT(sah_stats.nodes_visited, 0u);
  // Weak quality check: SAH should not visit wildly more nodes than Median.
  EXPECT_LE(sah_stats.nodes_visited, med_stats.nodes_visited + 8u);
}

}  // namespace
}  // namespace brep
