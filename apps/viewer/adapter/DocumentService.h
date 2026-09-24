#pragma once

#include "adapter/IDocumentService.h"

#include <string>

namespace brep::viewer::adapter
{

/// Persistence facade for .xl / .bks (Viewer does not call brep::io directly).
class DocumentService final : public IDocumentService
{
 public:
  [[nodiscard]] LoadResult load(const std::string& path) const override;
  [[nodiscard]] SaveResult save(const brep::Document& doc,
                                const std::string& path) const override;
  [[nodiscard]] MeshCacheLoadResult load_mesh_cache(
      const std::string& xl_path, const Guid& doc_guid) const override;
};

}  // namespace brep::viewer::adapter
