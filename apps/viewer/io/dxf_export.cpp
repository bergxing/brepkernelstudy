#include "io/dxf_export.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace brep::viewer::io {
namespace {

std::string g_last_error;

void write_pair(std::ostream& os, int code, const std::string& value) {
  os << code << '\n' << value << '\n';
}

void write_pair(std::ostream& os, int code, double value) {
  os << code << '\n' << std::setprecision(16) << value << '\n';
}

}  // namespace

void append_edge_segments(const EdgeMesh& edges, const Point3d& offset,
                          std::vector<DxfSegment>& out) {
  const auto& p = edges.positions;
  if (p.size() < 2) return;
  const std::size_t n = p.size() - (p.size() % 2);
  out.reserve(out.size() + n / 2);
  for (std::size_t i = 0; i + 1 < n; i += 2) {
    DxfSegment seg;
    seg.a = Point3d{p[i].x() + offset.x(), p[i].y() + offset.y(),
                    p[i].z() + offset.z()};
    seg.b = Point3d{p[i + 1].x() + offset.x(), p[i + 1].y() + offset.y(),
                    p[i + 1].z() + offset.z()};
    out.push_back(seg);
  }
}

bool write_edges_dxf(const std::filesystem::path& path,
                     const std::vector<DxfSegment>& segments) {
  g_last_error.clear();
  std::ofstream os(path, std::ios::out | std::ios::trunc);
  if (!os) {
    g_last_error = "Failed to open file for writing: " + path.string();
    return false;
  }

  os << std::setprecision(16);

  write_pair(os, 0, "SECTION");
  write_pair(os, 2, "HEADER");
  write_pair(os, 9, "$ACADVER");
  write_pair(os, 1, "AC1009");  // R12
  write_pair(os, 0, "ENDSEC");

  write_pair(os, 0, "SECTION");
  write_pair(os, 2, "ENTITIES");

  for (const auto& seg : segments) {
    write_pair(os, 0, "LINE");
    write_pair(os, 8, "0");
    write_pair(os, 10, seg.a.x());
    write_pair(os, 20, seg.a.y());
    write_pair(os, 30, seg.a.z());
    write_pair(os, 11, seg.b.x());
    write_pair(os, 21, seg.b.y());
    write_pair(os, 31, seg.b.z());
  }

  write_pair(os, 0, "ENDSEC");
  write_pair(os, 0, "EOF");

  if (!os) {
    g_last_error = "Write failed: " + path.string();
    return false;
  }
  return true;
}

std::string last_dxf_error() { return g_last_error; }

}  // namespace brep::viewer::io
