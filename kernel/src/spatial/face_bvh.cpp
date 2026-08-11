#include "brep/spatial/face_bvh.hpp"

#include "brep/geometry.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace brep::spatial {
namespace {

constexpr double k_c_trav = 1.0;
constexpr double k_c_isect = 1.0;

[[nodiscard]] double centroid_component(const Aabb& box, int axis) {
  const Point3d c = box.center();
  if (axis == 0) return c.x();
  if (axis == 1) return c.y();
  return c.z();
}

[[nodiscard]] Aabb bounds_of_range(const std::vector<Aabb>& boxes, int begin,
                                   int end) {
  Aabb out;
  for (int i = begin; i < end; ++i) {
    out.expand(boxes[static_cast<std::size_t>(i)]);
  }
  return out;
}

}  // namespace

Aabb estimate_face_aabb(const Face& face) {
  if (face.surface && face.surface->kind() == SurfaceKind::Sphere) {
    const auto& s = static_cast<const SphereSurface&>(*face.surface);
    const Point3d& c = s.center();
    const double r = s.radius();
    return Aabb{Point3d{c.x() - r, c.y() - r, c.z() - r},
                Point3d{c.x() + r, c.y() + r, c.z() + r}};
  }

  Aabb box;
  bool any = false;
  for (const Loop* loop : face.loops) {
    if (!loop) continue;
    loop->for_each_coedge([&](const CoEdge& ce) {
      if (Vertex* v = ce.from()) {
        box.expand(v->position());
        any = true;
      }
    });
  }

  if (!any && face.surface &&
      face.surface->kind() == SurfaceKind::Cylinder) {
    const auto& cyl = static_cast<const CylinderSurface&>(*face.surface);
    const Point3d& o = cyl.origin();
    const double r = cyl.radius();
    // No loop vertices: conservative cube about origin (infinite cylinder
    // has no height); callers with topology should prefer loop verts.
    box = Aabb{Point3d{o.x() - r, o.y() - r, o.z() - r},
               Point3d{o.x() + r, o.y() + r, o.z() + r}};
    any = true;
  }

  return box;
}

FaceBvh FaceBvh::build(const Body& body, BuildQuality quality, int leaf_max) {
  if (leaf_max < 1) leaf_max = 1;

  FaceBvh bvh;
  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    for (Face* face : shell->faces) {
      if (!face) continue;
      bvh.faces_.push_back(face);
      bvh.face_bounds_.push_back(estimate_face_aabb(*face));
    }
  }

  if (bvh.faces_.empty()) {
    return bvh;
  }

  bvh.root_bounds_ =
      bounds_of_range(bvh.face_bounds_, 0, static_cast<int>(bvh.faces_.size()));
  const int n = static_cast<int>(bvh.faces_.size());
  if (quality == BuildQuality::Sah) {
    bvh.root_ = bvh.build_sah(0, n, leaf_max);
  } else {
    bvh.root_ = bvh.build_median(0, n, leaf_max);
  }
  return bvh;
}

void FaceBvh::apply_order(int begin, int end, const std::vector<int>& order) {
  const int count = end - begin;
  std::vector<Face*> faces_tmp(static_cast<std::size_t>(count));
  std::vector<Aabb> bounds_tmp(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const int src = order[static_cast<std::size_t>(i)];
    faces_tmp[static_cast<std::size_t>(i)] =
        faces_[static_cast<std::size_t>(src)];
    bounds_tmp[static_cast<std::size_t>(i)] =
        face_bounds_[static_cast<std::size_t>(src)];
  }
  for (int i = 0; i < count; ++i) {
    faces_[static_cast<std::size_t>(begin + i)] =
        faces_tmp[static_cast<std::size_t>(i)];
    face_bounds_[static_cast<std::size_t>(begin + i)] =
        bounds_tmp[static_cast<std::size_t>(i)];
  }
}

int FaceBvh::build_median(int begin, int end, int leaf_max) {
  const int count = end - begin;
  FaceBvhNode node;
  node.bounds = bounds_of_range(face_bounds_, begin, end);
  node.face_begin = begin;
  node.face_count = count;

  if (count <= leaf_max) {
    const int idx = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    return idx;
  }

  const int axis = node.bounds.longest_axis();
  const int mid = begin + count / 2;

  std::vector<int> order(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    order[static_cast<std::size_t>(i)] = begin + i;
  }
  std::nth_element(order.begin(), order.begin() + (mid - begin), order.end(),
                   [&](int ia, int ib) {
                     return centroid_component(
                                face_bounds_[static_cast<std::size_t>(ia)],
                                axis) <
                            centroid_component(
                                face_bounds_[static_cast<std::size_t>(ib)],
                                axis);
                   });
  apply_order(begin, end, order);

  const int idx = static_cast<int>(nodes_.size());
  nodes_.push_back(node);
  const int left = build_median(begin, mid, leaf_max);
  const int right = build_median(mid, end, leaf_max);
  nodes_[static_cast<std::size_t>(idx)].left = left;
  nodes_[static_cast<std::size_t>(idx)].right = right;
  nodes_[static_cast<std::size_t>(idx)].face_begin = 0;
  nodes_[static_cast<std::size_t>(idx)].face_count = 0;
  return idx;
}

int FaceBvh::build_sah(int begin, int end, int leaf_max) {
  const int count = end - begin;
  FaceBvhNode node;
  node.bounds = bounds_of_range(face_bounds_, begin, end);
  node.face_begin = begin;
  node.face_count = count;

  const double leaf_cost = k_c_isect * static_cast<double>(count);
  if (count <= leaf_max) {
    const int idx = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    return idx;
  }

  const double parent_sa = node.bounds.surface_area();
  if (!(parent_sa > 0.0)) {
    // Degenerate bounds: fall back to median split by index.
    return build_median(begin, end, leaf_max);
  }

  int best_axis = -1;
  int best_split = -1;  // last left bin index in [0, bins-2]
  double best_cost = std::numeric_limits<double>::infinity();

  for (int axis = 0; axis < 3; ++axis) {
    double cmin = std::numeric_limits<double>::infinity();
    double cmax = -std::numeric_limits<double>::infinity();
    for (int i = begin; i < end; ++i) {
      const double c =
          centroid_component(face_bounds_[static_cast<std::size_t>(i)], axis);
      cmin = std::min(cmin, c);
      cmax = std::max(cmax, c);
    }
    const double extent = cmax - cmin;
    if (!(extent > 1e-30)) continue;

    std::array<int, k_sah_bins> counts{};
    std::array<Aabb, k_sah_bins> bins{};
    counts.fill(0);

    const double inv = static_cast<double>(k_sah_bins) * (1.0 / extent);
    for (int i = begin; i < end; ++i) {
      const Aabb& fb = face_bounds_[static_cast<std::size_t>(i)];
      const double c = centroid_component(fb, axis);
      int bin = static_cast<int>((c - cmin) * inv);
      if (bin < 0) bin = 0;
      if (bin >= k_sah_bins) bin = k_sah_bins - 1;
      ++counts[static_cast<std::size_t>(bin)];
      bins[static_cast<std::size_t>(bin)].expand(fb);
    }

    std::array<int, k_sah_bins> left_count{};
    std::array<Aabb, k_sah_bins> left_bounds{};
    int running = 0;
    Aabb run_bounds;
    for (int b = 0; b < k_sah_bins; ++b) {
      running += counts[static_cast<std::size_t>(b)];
      run_bounds.expand(bins[static_cast<std::size_t>(b)]);
      left_count[static_cast<std::size_t>(b)] = running;
      left_bounds[static_cast<std::size_t>(b)] = run_bounds;
    }

    running = 0;
    run_bounds = Aabb{};
    for (int b = k_sah_bins - 1; b >= 1; --b) {
      running += counts[static_cast<std::size_t>(b)];
      run_bounds.expand(bins[static_cast<std::size_t>(b)]);
      const int n_left = left_count[static_cast<std::size_t>(b - 1)];
      const int n_right = running;
      if (n_left == 0 || n_right == 0) continue;
      const double sa_l =
          left_bounds[static_cast<std::size_t>(b - 1)].surface_area();
      const double sa_r = run_bounds.surface_area();
      const double cost =
          k_c_trav +
          (sa_l / parent_sa) * static_cast<double>(n_left) * k_c_isect +
          (sa_r / parent_sa) * static_cast<double>(n_right) * k_c_isect;
      if (cost < best_cost) {
        best_cost = cost;
        best_axis = axis;
        best_split = b - 1;
      }
    }
  }

  if (best_axis < 0 || !(best_cost < leaf_cost)) {
    // No useful SAH split — keep as leaf (may exceed leaf_max).
    const int idx = static_cast<int>(nodes_.size());
    nodes_.push_back(node);
    return idx;
  }

  double cmin = std::numeric_limits<double>::infinity();
  double cmax = -std::numeric_limits<double>::infinity();
  for (int i = begin; i < end; ++i) {
    const double c = centroid_component(
        face_bounds_[static_cast<std::size_t>(i)], best_axis);
    cmin = std::min(cmin, c);
    cmax = std::max(cmax, c);
  }
  const double extent = cmax - cmin;
  const double inv = static_cast<double>(k_sah_bins) * (1.0 / extent);

  auto bin_of = [&](int index) {
    const double c = centroid_component(
        face_bounds_[static_cast<std::size_t>(index)], best_axis);
    int bin = static_cast<int>((c - cmin) * inv);
    if (bin < 0) bin = 0;
    if (bin >= k_sah_bins) bin = k_sah_bins - 1;
    return bin;
  };

  std::vector<int> order(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    order[static_cast<std::size_t>(i)] = begin + i;
  }
  const auto mid_it = std::partition(
      order.begin(), order.end(),
      [&](int index) { return bin_of(index) <= best_split; });
  const int mid = begin + static_cast<int>(mid_it - order.begin());
  if (mid <= begin || mid >= end) {
    return build_median(begin, end, leaf_max);
  }
  apply_order(begin, end, order);

  const int idx = static_cast<int>(nodes_.size());
  nodes_.push_back(node);
  const int left = build_sah(begin, mid, leaf_max);
  const int right = build_sah(mid, end, leaf_max);
  nodes_[static_cast<std::size_t>(idx)].left = left;
  nodes_[static_cast<std::size_t>(idx)].right = right;
  nodes_[static_cast<std::size_t>(idx)].face_begin = 0;
  nodes_[static_cast<std::size_t>(idx)].face_count = 0;
  return idx;
}

void FaceBvh::query_node(int node_idx, const Aabb& query,
                         std::vector<Face*>& out, QueryStats* stats) const {
  if (node_idx < 0) return;
  if (stats) ++stats->nodes_visited;
  const FaceBvhNode& node = nodes_[static_cast<std::size_t>(node_idx)];
  if (!node.bounds.overlaps(query)) return;
  if (node.is_leaf()) {
    for (int i = 0; i < node.face_count; ++i) {
      const int fi = node.face_begin + i;
      if (face_bounds_[static_cast<std::size_t>(fi)].overlaps(query)) {
        out.push_back(faces_[static_cast<std::size_t>(fi)]);
      }
    }
    return;
  }
  query_node(node.left, query, out, stats);
  query_node(node.right, query, out, stats);
}

std::vector<Face*> FaceBvh::query_overlaps(const Aabb& query,
                                           QueryStats* stats) const {
  std::vector<Face*> out;
  if (root_ < 0) return out;
  query_node(root_, query, out, stats);
  return out;
}

void FaceBvh::visit_pairs(const FaceBvh& a, int na, const FaceBvh& b, int nb,
                          std::vector<std::pair<Face*, Face*>>& out) {
  if (na < 0 || nb < 0) return;
  const FaceBvhNode& A = a.nodes_[static_cast<std::size_t>(na)];
  const FaceBvhNode& B = b.nodes_[static_cast<std::size_t>(nb)];
  if (!A.bounds.overlaps(B.bounds)) return;

  if (A.is_leaf() && B.is_leaf()) {
    for (int i = 0; i < A.face_count; ++i) {
      const int ia = A.face_begin + i;
      for (int j = 0; j < B.face_count; ++j) {
        const int ib = B.face_begin + j;
        if (a.face_bounds_[static_cast<std::size_t>(ia)].overlaps(
                b.face_bounds_[static_cast<std::size_t>(ib)])) {
          out.emplace_back(a.faces_[static_cast<std::size_t>(ia)],
                           b.faces_[static_cast<std::size_t>(ib)]);
        }
      }
    }
    return;
  }

  const bool expand_a =
      !A.is_leaf() &&
      (B.is_leaf() || A.bounds.surface_area() >= B.bounds.surface_area());
  if (expand_a) {
    visit_pairs(a, A.left, b, nb, out);
    visit_pairs(a, A.right, b, nb, out);
  } else {
    visit_pairs(a, na, b, B.left, out);
    visit_pairs(a, na, b, B.right, out);
  }
}

std::vector<std::pair<Face*, Face*>> FaceBvh::candidate_pairs(
    const FaceBvh& a, const FaceBvh& b) {
  std::vector<std::pair<Face*, Face*>> out;
  if (a.root_ < 0 || b.root_ < 0) return out;
  visit_pairs(a, a.root_, b, b.root_, out);
  return out;
}

}  // namespace brep::spatial
