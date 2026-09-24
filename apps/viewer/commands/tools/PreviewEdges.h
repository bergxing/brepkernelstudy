#pragma once

#include "api/Core.h"
#include "api/Mesh.h"

#include <cstdint>
#include <set>
#include <utility>

namespace brep::viewer::commands
{

inline void PushSegment(EdgeMesh& mesh, const Point3d& a, const Point3d& b)
{
    mesh.Positions.push_back(a);
    mesh.Positions.push_back(b);
}

[[nodiscard]] inline EdgeMesh MakePointMarker(const Point3d& p, double s = 0.12)
{
    EdgeMesh mesh;
    PushSegment(mesh, Point3d{p.x() - s, p.y(), p.z()},
                Point3d{p.x() + s, p.y(), p.z()});
    PushSegment(mesh, Point3d{p.x(), p.y(), p.z() - s},
                Point3d{p.x(), p.y(), p.z() + s});
    PushSegment(mesh, Point3d{p.x(), p.y() - s, p.z()},
                Point3d{p.x(), p.y() + s, p.z()});
    return mesh;
}

[[nodiscard]] inline EdgeMesh TranslatedEdges(const EdgeMesh& src,
                                              const Vector3d& offset)
{
    EdgeMesh out = src;
    for (auto& p : out.Positions)
    {
        p += offset;
    }
    return out;
}

/// Unique triangle edges as a wire. Type-agnostic fallback when ExtractEdges
/// returns empty (e.g. analytic sphere seams hidden by default).
[[nodiscard]] inline EdgeMesh WireFromFaces(const TriangleMesh& faces)
{
    EdgeMesh out;
    const auto& verts = faces.Vertices;
    const auto& idx = faces.Indices;
    if (verts.empty() || idx.size() < 3)
    {
        return out;
    }

    std::set<std::pair<std::uint32_t, std::uint32_t>> seen;
    auto addEdge = [&](std::uint32_t a, std::uint32_t b)
    {
        if (a == b || a >= verts.size() || b >= verts.size())
        {
            return;
        }
        if (a > b)
        {
            std::swap(a, b);
        }
        if (!seen.insert({a, b}).second)
        {
            return;
        }
        PushSegment(out, verts[a].Position, verts[b].Position);
    };

    for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
    {
        addEdge(idx[i], idx[i + 1]);
        addEdge(idx[i + 1], idx[i + 2]);
        addEdge(idx[i + 2], idx[i]);
    }
    return out;
}

/// Prefer topological edges; if none, derive a wire from the face mesh.
[[nodiscard]] inline EdgeMesh PreviewWire(const EdgeMesh& edges,
                                          const TriangleMesh& faces)
{
    if (!edges.Positions.empty())
    {
        return edges;
    }
    return WireFromFaces(faces);
}

}  // namespace brep::viewer::commands
