#pragma once

#include "brep/spatial/aabb.hpp"
#include "brep/topology.hpp"

#include <utility>
#include <vector>

namespace brep::spatial {

enum class BuildQuality {
  Median,
  Sah,  // T4.4.b — declared now; Median-only until SAH lands
};

struct FaceBvhNode {
  Aabb bounds;
  int left{-1};
  int right{-1};
  int face_begin{0};
  int face_count{0};

  [[nodiscard]] bool is_leaf() const noexcept { return left < 0 && right < 0; }
};

/// Axis-aligned face BVH for broad-phase candidate generation.
class FaceBvh {
 public:
  static constexpr int k_default_leaf_max = 4;

  [[nodiscard]] static FaceBvh build(const Body& body,
                                     BuildQuality quality = BuildQuality::Median,
                                     int leaf_max = k_default_leaf_max);

  [[nodiscard]] std::vector<Face*> query_overlaps(const Aabb& query) const;

  [[nodiscard]] static std::vector<std::pair<Face*, Face*>> candidate_pairs(
      const FaceBvh& a, const FaceBvh& b);

  [[nodiscard]] const Aabb& root_bounds() const noexcept { return root_bounds_; }
  [[nodiscard]] bool empty() const noexcept { return faces_.empty(); }
  [[nodiscard]] const std::vector<FaceBvhNode>& nodes() const noexcept {
    return nodes_;
  }

 private:
  std::vector<Face*> faces_;
  std::vector<Aabb> face_bounds_;
  std::vector<FaceBvhNode> nodes_;
  Aabb root_bounds_;
  int root_{-1};

  [[nodiscard]] int build_median(int begin, int end, int leaf_max);
  void query_node(int node, const Aabb& query, std::vector<Face*>& out) const;
  static void visit_pairs(const FaceBvh& a, int na, const FaceBvh& b, int nb,
                          std::vector<std::pair<Face*, Face*>>& out);
};

[[nodiscard]] Aabb estimate_face_aabb(const Face& face);

}  // namespace brep::spatial
