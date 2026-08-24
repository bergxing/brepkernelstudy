#include "commands/snap/SnapOverlay.h"

#include <QCoreApplication>

#include <cmath>
#include <numbers>

namespace brep::viewer::commands
{
namespace
{

constexpr double kMarkerRadius = 0.14;

Point3d OffsetPoint(const Point3d& point, double x, double z)
{
    return {point.x() + x, point.y(), point.z() + z};
}

void PushSegment(EdgeMesh& mesh, const Point3d& a, const Point3d& b)
{
    mesh.Positions.push_back(a);
    mesh.Positions.push_back(b);
}

void PushPolyline(EdgeMesh& mesh, const Point3d& point,
                  const std::initializer_list<std::pair<double, double>>& xy,
                  bool close)
{
    if (xy.size() < 2)
    {
        return;
    }
    auto current = xy.begin();
    const auto first = *current;
    auto previous = current++;
    for (; current != xy.end(); ++current)
    {
        PushSegment(mesh, OffsetPoint(point, previous->first, previous->second),
                    OffsetPoint(point, current->first, current->second));
        previous = current;
    }
    if (close)
    {
        PushSegment(mesh, OffsetPoint(point, previous->first, previous->second),
                    OffsetPoint(point, first.first, first.second));
    }
}

}  // namespace

EdgeMesh MakeSnapMarker(SnapKind kind, const Point3d& point)
{
    EdgeMesh mesh;
    const double radius = kMarkerRadius;
    if (kind != SnapKind::None && kind != SnapKind::Workplane)
    {
        PushSegment(mesh, {point.x() - radius, point.y(), point.z()},
                    {point.x() + radius, point.y(), point.z()});
        PushSegment(mesh, {point.x(), point.y() - radius, point.z()},
                    {point.x(), point.y() + radius, point.z()});
        PushSegment(mesh, {point.x(), point.y(), point.z() - radius},
                    {point.x(), point.y(), point.z() + radius});
    }
    switch (kind)
    {
        case SnapKind::Endpoint:
            PushPolyline(mesh, point,
                         {{-radius, -radius}, {radius, -radius}, {radius, radius},
                          {-radius, radius}},
                         true);
            break;
        case SnapKind::Midpoint:
            PushPolyline(mesh, point, {{0.0, -radius}, {radius, radius}, {-radius, radius}},
                         true);
            break;
        case SnapKind::Center:
        {
            constexpr int kSegments = 16;
            for (int i = 0; i < kSegments; ++i)
            {
                const double angle0 = 2.0 * std::numbers::pi * i / kSegments;
                const double angle1 = 2.0 * std::numbers::pi * (i + 1) / kSegments;
                PushSegment(mesh,
                            OffsetPoint(point, radius * std::cos(angle0),
                                        radius * std::sin(angle0)),
                            OffsetPoint(point, radius * std::cos(angle1),
                                        radius * std::sin(angle1)));
            }
            break;
        }
        case SnapKind::Intersection:
            PushSegment(mesh, OffsetPoint(point, -radius, -radius),
                        OffsetPoint(point, radius, radius));
            PushSegment(mesh, OffsetPoint(point, -radius, radius),
                        OffsetPoint(point, radius, -radius));
            break;
        case SnapKind::Perpendicular:
            PushSegment(mesh, OffsetPoint(point, -radius, radius),
                        OffsetPoint(point, -radius, -radius));
            PushSegment(mesh, OffsetPoint(point, -radius, -radius),
                        OffsetPoint(point, radius, -radius));
            PushSegment(mesh, OffsetPoint(point, 0.0, -radius),
                        OffsetPoint(point, 0.0, 0.0));
            PushSegment(mesh, OffsetPoint(point, 0.0, 0.0),
                        OffsetPoint(point, radius, 0.0));
            break;
        case SnapKind::Nearest:
            PushSegment(mesh, OffsetPoint(point, -radius, 0.0),
                        OffsetPoint(point, radius, 0.0));
            break;
        case SnapKind::Grid:
            PushSegment(mesh, OffsetPoint(point, -radius, 0.0),
                        OffsetPoint(point, radius, 0.0));
            PushSegment(mesh, OffsetPoint(point, 0.0, -radius),
                        OffsetPoint(point, 0.0, radius));
            break;
        case SnapKind::None:
        case SnapKind::Workplane:
            break;
    }
    return mesh;
}

QString SnapKindName(SnapKind kind)
{
    switch (kind)
    {
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
