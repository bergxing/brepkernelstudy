#pragma once

#include "brep/document.hpp"
#include "brep/guid.hpp"
#include "brep/mesh.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace brep::io {

/// Sidecar mesh cache written next to an .xl as "<stem>.bks.cache".
/// Speeds viewer open by skipping tessellation when Guids still match.
struct BodyMeshCache {
  Guid document_guid{};
  std::unordered_map<Guid, TriangleMesh> triangles;
  std::unordered_map<Guid, EdgeMesh> edges;

  [[nodiscard]] bool has(const Guid& body) const {
    return triangles.count(body) && edges.count(body);
  }
};

[[nodiscard]] std::filesystem::path bks_cache_path_for(
    const std::filesystem::path& xl_path);

struct CacheSaveResult {
  bool ok{false};
  std::string error;
};

struct CacheLoadResult {
  bool ok{false};
  BodyMeshCache cache;
  std::string error;
};

/// Tessellate all bodies in the document and write the sidecar cache.
[[nodiscard]] CacheSaveResult save_bks_cache(
    const Document& doc, const std::filesystem::path& xl_path);

[[nodiscard]] CacheLoadResult load_bks_cache(
    const std::filesystem::path& xl_path, const Guid& expected_document_guid);

}  // namespace brep::io
