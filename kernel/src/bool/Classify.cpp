#include "brep/bool/Classify.h"

#include <algorithm>
#include <cmath>

namespace brep::boolean
{
namespace
{

[[nodiscard]] bool point_in_ring_2d(const Point2d& p,
                                    const std::vector<Point2d>& ring)
{
  if (ring.size() < 3) return false;
  // Even-odd ray cast along +u.
  bool inside = false;
  for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
  {
    const Point2d& a = ring[j];
    const Point2d& b = ring[i];
    const bool cross_v =
        ((a.v() > p.v()) != (b.v() > p.v())) &&
        (p.u() < (b.u() - a.u()) * (p.v() - a.v()) / (b.v() - a.v() + 0.0) + a.u());
    if (cross_v) inside = !inside;
  }
  return inside;
}

[[nodiscard]] double dist_to_segment_2d(const Point2d& p, const Point2d& a,
                                        const Point2d& b)
{
  const double ab_u = b.u() - a.u();
  const double ab_v = b.v() - a.v();
  const double len2 = ab_u * ab_u + ab_v * ab_v;
  if (len2 < 1e-30)
  {
    const double du = p.u() - a.u();
    const double dv = p.v() - a.v();
    return std::sqrt(du * du + dv * dv);
  }
  double t = ((p.u() - a.u()) * ab_u + (p.v() - a.v()) * ab_v) / len2;
  t = std::clamp(t, 0.0, 1.0);
  const double qu = a.u() + t * ab_u;
  const double qv = a.v() + t * ab_v;
  const double du = p.u() - qu;
  const double dv = p.v() - qv;
  return std::sqrt(du * du + dv * dv);
}

[[nodiscard]] bool on_ring_boundary_2d(const Point2d& p,
                                       const std::vector<Point2d>& ring,
                                       double eps)
                                       {
  for (std::size_t i = 0; i < ring.size(); ++i)
                                       {
    const Point2d& a = ring[i];
    const Point2d& b = ring[(i + 1) % ring.size()];
    if (dist_to_segment_2d(p, a, b) <= eps) return true;
  }
  return false;
}

}  // namespace

SolidClass ClassifyPointInBox(const BoxSpec& box, const Point3d& p,
                                 double eps)
{
  const bool xin = p.x() >= box.Min.x() - eps && p.x() <= box.Max.x() + eps;
  const bool yin = p.y() >= box.Min.y() - eps && p.y() <= box.Max.y() + eps;
  const bool zin = p.z() >= box.Min.z() - eps && p.z() <= box.Max.z() + eps;
  if (!(xin && yin && zin)) return SolidClass::Out;

  const bool on_x =
      std::abs(p.x() - box.Min.x()) <= eps || std::abs(p.x() - box.Max.x()) <= eps;
  const bool on_y =
      std::abs(p.y() - box.Min.y()) <= eps || std::abs(p.y() - box.Max.y()) <= eps;
  const bool on_z =
      std::abs(p.z() - box.Min.z()) <= eps || std::abs(p.z() - box.Max.z()) <= eps;
  if (on_x || on_y || on_z) return SolidClass::On;
  return SolidClass::In;
}

SolidClass ClassifyPointInPrism(const PlanarPrismSpec& prism, const Point3d& p,
                                   double eps)
{
  const Vector3d d = p - prism.Plane.Origin;
  const double height = d.dot(prism.Plane.Normal);
  const double h0 = std::min(prism.d0, prism.d1);
  const double h1 = std::max(prism.d0, prism.d1);
  if (height < h0 - eps || height > h1 + eps) return SolidClass::Out;

  const Point2d uv{d.dot(prism.Plane.UAxis), d.dot(prism.Plane.VAxis)};

  if (on_ring_boundary_2d(uv, prism.Outer, eps))
  {
    if (height >= h0 - eps && height <= h1 + eps) return SolidClass::On;
  }
  for (const auto& hole : prism.Holes)
  {
    if (on_ring_boundary_2d(uv, hole, eps)) return SolidClass::On;
  }

  const bool on_cap =
      std::abs(height - h0) <= eps || std::abs(height - h1) <= eps;

  if (!point_in_ring_2d(uv, prism.Outer)) return SolidClass::Out;
  for (const auto& hole : prism.Holes)
  {
    if (point_in_ring_2d(uv, hole)) return SolidClass::Out;
  }

  if (on_cap) return SolidClass::On;
  return SolidClass::In;
}

SolidClass ClassifyPointInSphere(const SphereSpec& sphere, const Point3d& p,
                                    double eps)
{
  const double dist = (p - sphere.Center).norm();
  if (dist > sphere.Radius + eps) return SolidClass::Out;
  if (std::abs(dist - sphere.Radius) <= eps) return SolidClass::On;
  return SolidClass::In;
}

}  // namespace brep::boolean
