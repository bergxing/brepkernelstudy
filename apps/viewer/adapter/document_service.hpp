#pragma once

#include "brep/document.hpp"
#include "brep/guid.hpp"
#include "brep/io/bks_cache.hpp"

#include <memory>
#include <string>

namespace brep::viewer::adapter {

struct LoadResult {
  bool ok{false};
  std::string error;
  std::unique_ptr<brep::Document> document;
};

struct SaveResult {
  bool ok{false};
  std::string error;
};

struct MeshCacheLoadResult {
  bool ok{false};
  brep::io::BodyMeshCache cache;
};

/// Persistence facade for .xl / .bks (Viewer does not call brep::io directly).
class DocumentService {
 public:
  [[nodiscard]] LoadResult load(const std::string& path) const;
  [[nodiscard]] SaveResult save(const brep::Document& doc,
                                const std::string& path) const;
  [[nodiscard]] MeshCacheLoadResult load_mesh_cache(
      const std::string& xl_path, const Guid& doc_guid) const;
};

}  // namespace brep::viewer::adapter
