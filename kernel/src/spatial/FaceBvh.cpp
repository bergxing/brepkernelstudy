#include "brep/spatial/FaceBvh.h"

#include "brep/Geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace brep::spatial
{
namespace
{

constexpr double k_c_trav = 1.0;
constexpr double k_c_isect = 1.0;

[[nodiscard]] double centroid_component(const Aabb& box, int axis)
{
  const Point3d c = box.Center();
  if (axis == 0) return c.x();
  if (axis == 1) return c.y();
  return c.z();
}

[[nodiscard]] Aabb bounds_of_range(const std::vector<Aabb>& boxes, int begin,
                                   int end)
{
  Aabb out;
  for (int i = begin; i < end; ++i)
  {
    out.Expand(boxes[static_cast<std::size_t>(i)]);
  }
  return out;
}

}  // namespace

Aabb EstimateFaceAabb(const Face& face)
{
  if (face.Surface && face.Surface->Kind() == SurfaceKind::Sphere)
{
    const auto& s = static_cast<const SphereSurface&>(*face.Surface);
    const Point3d& c = s.Center();
    const double r = s.Radius();
    return Aabb{Point3d{c.x() - r, c.y() - r, c.z() - r},
                Point3d{c.x() + r, c.y() + r, c.z() + r}};
  }

  Aabb box;
  bool any = false;
  for (const Loop* loop : face.Loops)
  {
    if (!loop) continue;
    loop->ForEachCoedge([&](const CoEdge& ce)
    {
      if (Vertex* v = ce.From())
    {
        box.Expand(v->Position());
        any = true;
      }
    });
  }

  if (!any && face.Surface &&
      face.Surface->Kind() == SurfaceKind::Cylinder)
  {
    const auto& cyl = static_cast<const CylinderSurface&>(*face.Surface);
    const Point3d& o = cyl.Origin();
    const double r = cyl.Radius();
    // No loop vertices: conservative cube about origin (infinite cylinder
    // has no height); callers with topology should prefer loop verts.
    box = Aabb{Point3d{o.x() - r, o.y() - r, o.z() - r},
               Point3d{o.x() + r, o.y() + r, o.z() + r}};
    any = true;
  }

  return box;
}

FaceBvh FaceBvh::Build(const Body& body, BuildQuality quality, int leaf_max)
{
  if (leaf_max < 1) leaf_max = 1;

  FaceBvh bvh;
  for (const Shell* shell : body.Shells)
  {
    if (!shell) continue;
    for (Face* face : shell->Faces)
    {
      if (!face) continue;
      bvh.m_faces.push_back(face);
      bvh.m_faceBounds.push_back(EstimateFaceAabb(*face));
    }
  }

  if (bvh.m_faces.empty())
  {
    return bvh;
  }

  bvh.m_rootBounds =
      bounds_of_range(bvh.m_faceBounds, 0, static_cast<int>(bvh.m_faces.size()));
  const int n = static_cast<int>(bvh.m_faces.size());
  if (quality == BuildQuality::Sah)
  {
    bvh.m_root = bvh.BuildSah(0, n, leaf_max);
  }
  else
  {
    bvh.m_root = bvh.BuildMedian(0, n, leaf_max);
  }
  return bvh;
}

void FaceBvh::ApplyOrder(int begin, int end, const std::vector<int>& order)
{
  const int count = end - begin;
  std::vector<Face*> faces_tmp(static_cast<std::size_t>(count));
  std::vector<Aabb> bounds_tmp(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i)
  {
    const int src = order[static_cast<std::size_t>(i)];
    faces_tmp[static_cast<std::size_t>(i)] =
        m_faces[static_cast<std::size_t>(src)];
    bounds_tmp[static_cast<std::size_t>(i)] =
        m_faceBounds[static_cast<std::size_t>(src)];
  }
  for (int i = 0; i < count; ++i)
  {
    m_faces[static_cast<std::size_t>(begin + i)] =
        faces_tmp[static_cast<std::size_t>(i)];
    m_faceBounds[static_cast<std::size_t>(begin + i)] =
        bounds_tmp[static_cast<std::size_t>(i)];
  }
}

int FaceBvh::BuildMedian(int begin, int end, int leaf_max)
{
  const int count = end - begin;
  FaceBvhNode node;
  node.Bounds = bounds_of_range(m_faceBounds, begin, end);
  node.FaceBegin = begin;
  node.FaceCount = count;

  if (count <= leaf_max)
  {
    const int idx = static_cast<int>(m_nodes.size());
    m_nodes.push_back(node);
    return idx;
  }

  const int axis = node.Bounds.LongestAxis();
  const int mid = begin + count / 2;

  std::vector<int> order(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i)
  {
    order[static_cast<std::size_t>(i)] = begin + i;
  }
  std::nth_element(order.begin(), order.begin() + (mid - begin), order.end(),
                   [&](int ia, int ib)
  {
                     return centroid_component(
                                m_faceBounds[static_cast<std::size_t>(ia)],
                                axis) <
                            centroid_component(
                                m_faceBounds[static_cast<std::size_t>(ib)],
                                axis);
                   });
  ApplyOrder(begin, end, order);

  const int idx = static_cast<int>(m_nodes.size());
  m_nodes.push_back(node);
  const int left = BuildMedian(begin, mid, leaf_max);
  const int right = BuildMedian(mid, end, leaf_max);
  m_nodes[static_cast<std::size_t>(idx)].Left = left;
  m_nodes[static_cast<std::size_t>(idx)].Right = right;
  m_nodes[static_cast<std::size_t>(idx)].FaceBegin = 0;
  m_nodes[static_cast<std::size_t>(idx)].FaceCount = 0;
  return idx;
}

int FaceBvh::BuildSah(int begin, int end, int leaf_max)
{
  const int count = end - begin;
  FaceBvhNode node;
  node.Bounds = bounds_of_range(m_faceBounds, begin, end);
  node.FaceBegin = begin;
  node.FaceCount = count;

  const double leaf_cost = k_c_isect * static_cast<double>(count);
  if (count <= leaf_max)
  {
    const int idx = static_cast<int>(m_nodes.size());
    m_nodes.push_back(node);
    return idx;
  }

  const double parent_sa = node.Bounds.SurfaceArea();
  if (!(parent_sa > 0.0))
  {
    // Degenerate bounds: fall back to median split by index.
    return BuildMedian(begin, end, leaf_max);
  }

  int best_axis = -1;
  int best_split = -1;  // last left bin index in [0, bins-2]
  double best_cost = std::numeric_limits<double>::infinity();

  for (int axis = 0; axis < 3; ++axis)
  {
    double cmin = std::numeric_limits<double>::infinity();
    double cmax = -std::numeric_limits<double>::infinity();
    for (int i = begin; i < end; ++i)
    {
      const double c =
          centroid_component(m_faceBounds[static_cast<std::size_t>(i)], axis);
      cmin = std::min(cmin, c);
      cmax = std::max(cmax, c);
    }
    const double extent = cmax - cmin;
    if (!(extent > 1e-30)) continue;

    std::array<int, kSahBins> counts{};
    std::array<Aabb, kSahBins> bins{};
    counts.fill(0);

    const double inv = static_cast<double>(kSahBins) * (1.0 / extent);
    for (int i = begin; i < end; ++i)
    {
      const Aabb& fb = m_faceBounds[static_cast<std::size_t>(i)];
      const double c = centroid_component(fb, axis);
      int bin = static_cast<int>((c - cmin) * inv);
      if (bin < 0) bin = 0;
      if (bin >= kSahBins) bin = kSahBins - 1;
      ++counts[static_cast<std::size_t>(bin)];
      bins[static_cast<std::size_t>(bin)].Expand(fb);
    }

    std::array<int, kSahBins> left_count{};
    std::array<Aabb, kSahBins> left_bounds{};
    int running = 0;
    Aabb run_bounds;
    for (int b = 0; b < kSahBins; ++b)
    {
      running += counts[static_cast<std::size_t>(b)];
      run_bounds.Expand(bins[static_cast<std::size_t>(b)]);
      left_count[static_cast<std::size_t>(b)] = running;
      left_bounds[static_cast<std::size_t>(b)] = run_bounds;
    }

    running = 0;
    run_bounds = Aabb{};
    for (int b = kSahBins - 1; b >= 1; --b)
    {
      running += counts[static_cast<std::size_t>(b)];
      run_bounds.Expand(bins[static_cast<std::size_t>(b)]);
      const int n_left = left_count[static_cast<std::size_t>(b - 1)];
      const int n_right = running;
      if (n_left == 0 || n_right == 0) continue;
      const double sa_l =
          left_bounds[static_cast<std::size_t>(b - 1)].SurfaceArea();
      const double sa_r = run_bounds.SurfaceArea();
      const double cost =
          k_c_trav +
          (sa_l / parent_sa) * static_cast<double>(n_left) * k_c_isect +
          (sa_r / parent_sa) * static_cast<double>(n_right) * k_c_isect;
      if (cost < best_cost)
      {
        best_cost = cost;
        best_axis = axis;
        best_split = b - 1;
      }
    }
  }

  if (best_axis < 0 || !(best_cost < leaf_cost))
  {
    // No useful SAH split — keep as leaf (may exceed leaf_max).
    const int idx = static_cast<int>(m_nodes.size());
    m_nodes.push_back(node);
    return idx;
  }

  double cmin = std::numeric_limits<double>::infinity();
  double cmax = -std::numeric_limits<double>::infinity();
  for (int i = begin; i < end; ++i)
  {
    const double c = centroid_component(
        m_faceBounds[static_cast<std::size_t>(i)], best_axis);
    cmin = std::min(cmin, c);
    cmax = std::max(cmax, c);
  }
  const double extent = cmax - cmin;
  const double inv = static_cast<double>(kSahBins) * (1.0 / extent);

  auto bin_of = [&](int index)
  {
    const double c = centroid_component(
        m_faceBounds[static_cast<std::size_t>(index)], best_axis);
    int bin = static_cast<int>((c - cmin) * inv);
    if (bin < 0) bin = 0;
    if (bin >= kSahBins) bin = kSahBins - 1;
    return bin;
  };

  std::vector<int> order(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i)
  {
    order[static_cast<std::size_t>(i)] = begin + i;
  }
  const auto mid_it = std::partition(
      order.begin(), order.end(),
      [&](int index)
      {
          return bin_of(index) <= best_split; 
      });
  const int mid = begin + static_cast<int>(mid_it - order.begin());
  if (mid <= begin || mid >= end)
  {
    return BuildMedian(begin, end, leaf_max);
  }
  ApplyOrder(begin, end, order);

  const int idx = static_cast<int>(m_nodes.size());
  m_nodes.push_back(node);
  const int left = BuildSah(begin, mid, leaf_max);
  const int right = BuildSah(mid, end, leaf_max);
  m_nodes[static_cast<std::size_t>(idx)].Left = left;
  m_nodes[static_cast<std::size_t>(idx)].Right = right;
  m_nodes[static_cast<std::size_t>(idx)].FaceBegin = 0;
  m_nodes[static_cast<std::size_t>(idx)].FaceCount = 0;
  return idx;
}

void FaceBvh::QueryNode(int node_idx, const Aabb& query,
                         std::vector<Face*>& out, QueryStats* stats) const
{
  if (node_idx < 0) return;
  if (stats) ++stats->NodesVisited;
  const FaceBvhNode& node = m_nodes[static_cast<std::size_t>(node_idx)];
  if (!node.Bounds.Overlaps(query)) return;
  if (node.IsLeaf())
  {
    for (int i = 0; i < node.FaceCount; ++i)
  {
      const int fi = node.FaceBegin + i;
      if (m_faceBounds[static_cast<std::size_t>(fi)].Overlaps(query))
      {
        out.push_back(m_faces[static_cast<std::size_t>(fi)]);
      }
    }
    return;
  }
  QueryNode(node.Left, query, out, stats);
  QueryNode(node.Right, query, out, stats);
}

std::vector<Face*> FaceBvh::QueryOverlaps(const Aabb& query,
                                           QueryStats* stats) const
{
  std::vector<Face*> out;
  if (m_root < 0) return out;
  QueryNode(m_root, query, out, stats);
  return out;
}

void FaceBvh::VisitPairs(const FaceBvh& a, int na, const FaceBvh& b, int nb,
                          std::vector<std::pair<Face*, Face*>>& out)
{
  if (na < 0 || nb < 0) return;
  const FaceBvhNode& A = a.m_nodes[static_cast<std::size_t>(na)];
  const FaceBvhNode& B = b.m_nodes[static_cast<std::size_t>(nb)];
  if (!A.Bounds.Overlaps(B.Bounds)) return;

  if (A.IsLeaf() && B.IsLeaf())
  {
    for (int i = 0; i < A.FaceCount; ++i)
  {
      const int ia = A.FaceBegin + i;
      for (int j = 0; j < B.FaceCount; ++j)
      {
        const int ib = B.FaceBegin + j;
        if (a.m_faceBounds[static_cast<std::size_t>(ia)].Overlaps(
                b.m_faceBounds[static_cast<std::size_t>(ib)]))
        {
          out.emplace_back(a.m_faces[static_cast<std::size_t>(ia)],
                           b.m_faces[static_cast<std::size_t>(ib)]);
        }
      }
    }
    return;
  }

  const bool expand_a =
      !A.IsLeaf() &&
      (B.IsLeaf() || A.Bounds.SurfaceArea() >= B.Bounds.SurfaceArea());
  if (expand_a)
  {
    VisitPairs(a, A.Left, b, nb, out);
    VisitPairs(a, A.Right, b, nb, out);
  }
  else
  {
    VisitPairs(a, na, b, B.Left, out);
    VisitPairs(a, na, b, B.Right, out);
  }
}

std::vector<std::pair<Face*, Face*>> FaceBvh::CandidatePairs(
    const FaceBvh& a, const FaceBvh& b)
{
  std::vector<std::pair<Face*, Face*>> out;
  if (a.m_root < 0 || b.m_root < 0) return out;
  VisitPairs(a, a.m_root, b, b.m_root, out);
  return out;
}

}  // namespace brep::spatial
