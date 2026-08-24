#pragma once

#include "brep/spatial/Aabb.h"
#include "brep/Topology.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace brep::spatial
{

enum class BuildQuality
{
  Median,
  Sah,
};

struct FaceBvhNode
{
  Aabb Bounds;
  int Left{-1};
  int Right{-1};
  int FaceBegin{0};
  int FaceCount{0};

  [[nodiscard]] bool IsLeaf() const noexcept
  {
    return Left < 0 && Right < 0;
  }
};

struct QueryStats
{
  std::size_t NodesVisited{0};
};

/// Axis-aligned face BVH for broad-phase candidate generation.
class FaceBvh
{
 public:
  static constexpr int kDefaultLeafMax = 4;
  static constexpr int kSahBins = 16;

  [[nodiscard]] static FaceBvh Build(const Body& body,
                                     BuildQuality quality = BuildQuality::Median,
                                     int leafMax = kDefaultLeafMax);

  [[nodiscard]] std::vector<Face*> QueryOverlaps(
      const Aabb& query, QueryStats* stats = nullptr) const;

  [[nodiscard]] static std::vector<std::pair<Face*, Face*>> CandidatePairs(
      const FaceBvh& a, const FaceBvh& b);

  [[nodiscard]] const Aabb& RootBounds() const noexcept
  {
    return m_rootBounds;
  }
  [[nodiscard]] bool Empty() const noexcept
  {
    return m_faces.empty();
  }
  [[nodiscard]] const std::vector<FaceBvhNode>& Nodes() const noexcept
  {
    return m_nodes;
  }

 private:
  std::vector<Face*> m_faces;
  std::vector<Aabb> m_faceBounds;
  std::vector<FaceBvhNode> m_nodes;
  Aabb m_rootBounds;
  int m_root{-1};

  [[nodiscard]] int BuildMedian(int begin, int end, int leafMax);
  [[nodiscard]] int BuildSah(int begin, int end, int leafMax);
  void ApplyOrder(int begin, int end, const std::vector<int>& order);
  void QueryNode(int node, const Aabb& query, std::vector<Face*>& out,
                 QueryStats* stats) const;
  static void VisitPairs(const FaceBvh& a, int na, const FaceBvh& b, int nb,
                         std::vector<std::pair<Face*, Face*>>& out);
};

[[nodiscard]] Aabb EstimateFaceAabb(const Face& face);

}  // namespace brep::spatial
