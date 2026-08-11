#include "brep/spatial/face_bvh.hpp"

#include "brep/geometry.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace brep::spatial {
namespace {

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
  if (quality == BuildQuality::Sah) {
    // T4.4.b will replace this; keep Median-only for T4.4.a.
    throw std::invalid_argument(
        "FaceBvh::build: BuildQuality::Sah not implemented yet (T4.4.b)");
  }
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
  bvh.root_ = bvh.build_median(0, static_cast<int>(bvh.faces_.size()), leaf_max);
  return bvh;
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

  // Keep faces_ and face_bounds_ aligned via index nth_element.
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

  const int idx = static_cast<int>(nodes_.size());
  nodes_.push_back(node);  // placeholder; fill children after recursion
  const int left = build_median(begin, mid, leaf_max);
  const int right = build_median(mid, end, leaf_max);
  nodes_[static_cast<std::size_t>(idx)].left = left;
  nodes_[static_cast<std::size_t>(idx)].right = right;
  nodes_[static_cast<std::size_t>(idx)].face_begin = 0;
  nodes_[static_cast<std::size_t>(idx)].face_count = 0;
  return idx;
}

void FaceBvh::query_node(int node_idx, const Aabb& query,
                         std::vector<Face*>& out) const {
  if (node_idx < 0) return;
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
  query_node(node.left, query, out);
  query_node(node.right, query, out);
}

std::vector<Face*> FaceBvh::query_overlaps(const Aabb& query) const {
  std::vector<Face*> out;
  if (root_ < 0) return out;
  query_node(root_, query, out);
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
