#include "brep/bool/BoxRecognize.h"

#include "brep/Geometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{
namespace
{

[[nodiscard]] bool near_abs(double a, double b, double tol)
{
  return std::abs(a - b) <= tol;
}

[[nodiscard]] bool is_unit_axis(const Vector3d& n, double tol)
{
  const double ax = std::abs(n.x());
  const double ay = std::abs(n.y());
  const double az = std::abs(n.z());
  const double sum = ax + ay + az;
  if (!near_abs(sum, 1.0, tol * 10.0)) return false;
  return (ax > 0.5 && ay <= tol * 10.0 && az <= tol * 10.0) ||
         (ay > 0.5 && ax <= tol * 10.0 && az <= tol * 10.0) ||
         (az > 0.5 && ax <= tol * 10.0 && ay <= tol * 10.0);
}

}  // namespace

std::optional<BoxSpec> RecognizeAxisAlignedBox(const Body& body,
                                                  const BooleanContext& ctx)
{
  const double tol = std::max(ctx.fuzzy, 1e-12);
  if (body.Shells.size() != 1 || !body.Shells[0]) return std::nullopt;
  const Shell& shell = *body.Shells[0];
  if (!shell.Closed || shell.Faces.size() != 6) return std::nullopt;

  Point3d mn{1e300, 1e300, 1e300};
  Point3d mx{-1e300, -1e300, -1e300};
  std::unordered_set<std::uint64_t> vert_ids;
  std::vector<Point3d> positions;

  for (const Face* face : shell.Faces)
  {
    if (!face || !face->Surface) return std::nullopt;
    if (face->Surface->Kind() != SurfaceKind::Plane) return std::nullopt;
    if (!is_unit_axis(face->NormalAt(0, 0), tol)) return std::nullopt;
    const Loop* loop = face->OuterLoop();
    if (!loop) return std::nullopt;
    std::size_t count = 0;
    loop->ForEachCoedge([&](const CoEdge& ce)
    {
      ++count;
      if (Vertex* v = ce.From())
      {
        if (vert_ids.insert(static_cast<std::uint64_t>(v->Id)).second)
      {
          positions.push_back(v->Position());
        }
        const Point3d& p = v->Position();
        mn = Point3d{std::min(mn.x(), p.x()), std::min(mn.y(), p.y()),
                     std::min(mn.z(), p.z())};
        mx = Point3d{std::max(mx.x(), p.x()), std::max(mx.y(), p.y()),
                     std::max(mx.z(), p.z())};
      }
    });
    if (count != 4) return std::nullopt;
  }

  if (vert_ids.size() != 8) return std::nullopt;
  if (!(mx.x() > mn.x() + tol && mx.y() > mn.y() + tol &&
        mx.z() > mn.z() + tol))
  {
    return std::nullopt;
  }

  for (const Point3d& p : positions)
  {
    const bool on_x =
        near_abs(p.x(), mn.x(), tol) || near_abs(p.x(), mx.x(), tol);
    const bool on_y =
        near_abs(p.y(), mn.y(), tol) || near_abs(p.y(), mx.y(), tol);
    const bool on_z =
        near_abs(p.z(), mn.z(), tol) || near_abs(p.z(), mx.z(), tol);
    if (!(on_x && on_y && on_z)) return std::nullopt;
  }

  BoxSpec spec;
  spec.Min = mn;
  spec.Max = mx;
  spec.Name = body.Name.empty() ? "box" : body.Name;
  return spec;
}

}  // namespace brep::boolean
