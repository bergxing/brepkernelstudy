#pragma once

#include "api/Core.h"
#include "api/Persistence.h"

#include <memory>
#include <string>

namespace brep::viewer::adapter
{

struct LoadResult
{
  bool ok{false};
  std::string error;
  std::unique_ptr<brep::Document> document;
};

struct SaveResult
{
  bool ok{false};
  std::string error;
};

struct MeshCacheLoadResult
{
  bool ok{false};
  brep::io::BodyMeshCache cache;
};

/// Persistence facade for .xl / .bks (Viewer does not call brep::io directly).
class IDocumentService
{
 public:
  virtual ~IDocumentService() = default;

  [[nodiscard]] virtual LoadResult load(const std::string& path) const = 0;
  [[nodiscard]] virtual SaveResult save(const brep::Document& doc,
                                        const std::string& path) const = 0;
  [[nodiscard]] virtual MeshCacheLoadResult load_mesh_cache(
      const std::string& xl_path, const Guid& doc_guid) const = 0;
};

}  // namespace brep::viewer::adapter
