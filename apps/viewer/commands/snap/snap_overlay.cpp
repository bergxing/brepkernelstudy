#include "commands/snap/snap_overlay.hpp"

#include <QCoreApplication>

#include <cmath>

namespace brep::viewer::commands {
namespace {

constexpr double kMarkerRadius = 0.14;
constexpr double kPi = 3.14159265358979323846;

Point3d offset(const Point3d& point, double x, double z) {
  return {point.x() + x, point.y(), point.z() + z};
}

void push_segment(EdgeMesh& mesh, const Point3d& a, const Point3d& b) {
  mesh.positions.push_back(a);
  mesh.positions.push_back(b);
}

void push_polyline(EdgeMesh& mesh, const Point3d& point,
                   const std::initializer_list<std::pair<double, double>>& xy,
                   bool close) {
  if (xy.size() < 2) return;
  auto current = xy.begin();
  const auto first = *current;
  auto previous = current++;
  for (; current != xy.end(); ++current) {
    push_segment(mesh, offset(point, previous->first, previous->second),
                 offset(point, current->first, current->second));
    previous = current;
  }
  if (close) {
    push_segment(mesh, offset(point, previous->first, previous->second),
                 offset(point, first.first, first.second));
  }
}

}  // namespace

EdgeMesh make_snap_marker(SnapKind kind, const Point3d& point) {
  EdgeMesh mesh;
  const double r = kMarkerRadius;
  if (kind != SnapKind::None && kind != SnapKind::Workplane) {
    push_segment(mesh, {point.x() - r, point.y(), point.z()},
                 {point.x() + r, point.y(), point.z()});
    push_segment(mesh, {point.x(), point.y() - r, point.z()},
                 {point.x(), point.y() + r, point.z()});
    push_segment(mesh, {point.x(), point.y(), point.z() - r},
                 {point.x(), point.y(), point.z() + r});
  }
  switch (kind) {
    case SnapKind::Endpoint:
      push_polyline(mesh, point, {{-r, -r}, {r, -r}, {r, r}, {-r, r}},
                    true);
      break;
    case SnapKind::Midpoint:
      push_polyline(mesh, point, {{0.0, -r}, {r, r}, {-r, r}}, true);
      break;
    case SnapKind::Center: {
      constexpr int kSegments = 16;
      for (int i = 0; i < kSegments; ++i) {
        const double a0 = 2.0 * kPi * i / kSegments;
        const double a1 = 2.0 * kPi * (i + 1) / kSegments;
        push_segment(mesh, offset(point, r * std::cos(a0), r * std::sin(a0)),
                     offset(point, r * std::cos(a1), r * std::sin(a1)));
      }
      break;
    }
    case SnapKind::Intersection:
      push_segment(mesh, offset(point, -r, -r), offset(point, r, r));
      push_segment(mesh, offset(point, -r, r), offset(point, r, -r));
      break;
    case SnapKind::Perpendicular:
      push_segment(mesh, offset(point, -r, r), offset(point, -r, -r));
      push_segment(mesh, offset(point, -r, -r), offset(point, r, -r));
      push_segment(mesh, offset(point, 0.0, -r), offset(point, 0.0, 0.0));
      push_segment(mesh, offset(point, 0.0, 0.0), offset(point, r, 0.0));
      break;
    case SnapKind::Nearest:
      push_segment(mesh, offset(point, -r, 0.0), offset(point, r, 0.0));
      break;
    case SnapKind::Grid:
      push_segment(mesh, offset(point, -r, 0.0), offset(point, r, 0.0));
      push_segment(mesh, offset(point, 0.0, -r), offset(point, 0.0, r));
      break;
    case SnapKind::None:
    case SnapKind::Workplane:
      break;
  }
  return mesh;
}

QString snap_kind_name(SnapKind kind) {
  switch (kind) {
    case SnapKind::Endpoint:
      return QCoreApplication::translate("SnapKind", "Endpoint");
    case SnapKind::Midpoint:
      return QCoreApplication::translate("SnapKind", "Midpoint");
    case SnapKind::Center:
      return QCoreApplication::translate("SnapKind", "Center");
    case SnapKind::Intersection:
      return QCoreApplication::translate("SnapKind", "Intersection");
    case SnapKind::Perpendicular:
      return QCoreApplication::translate("SnapKind", "Perpendicular");
    case SnapKind::Nearest:
      return QCoreApplication::translate("SnapKind", "Nearest");
    case SnapKind::Grid:
      return QCoreApplication::translate("SnapKind", "Grid");
    case SnapKind::None:
    case SnapKind::Workplane:
      return {};
  }
  return {};
}

}  // namespace brep::viewer::commands
