#pragma once

#include "api/Core.h"
#include "api/Mesh.h"

#include <filesystem>
#include <string>
#include <vector>

namespace brep::viewer::io
{

struct DxfSegment
{
  Point3d a{};
  Point3d b{};
};

/// Collect world-space segments from an EdgeMesh + translation.
void append_edge_segments(const EdgeMesh& edges, const Point3d& offset,
                          std::vector<DxfSegment>& out);

/// Write ASCII DXF R12 with LINE entities. Returns false on I/O error.
[[nodiscard]] bool write_edges_dxf(const std::filesystem::path& path,
                                   const std::vector<DxfSegment>& segments);

[[nodiscard]] std::string last_dxf_error();

}  // namespace brep::viewer::io
