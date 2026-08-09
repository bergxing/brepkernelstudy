#include "adapter/document_service.hpp"

#include "brep/io/xl_document.hpp"

namespace brep::viewer::adapter {

LoadResult DocumentService::load(const std::string& path) const {
  auto loaded = brep::io::load_xl(path);
  LoadResult out;
  out.ok = loaded.ok();
  out.error = loaded.error;
  out.document = std::move(loaded.document);
  return out;
}

SaveResult DocumentService::save(const brep::Document& doc,
                                 const std::string& path) const {
  auto result = brep::io::save_xl(doc, path);
  return SaveResult{.ok = result.ok, .error = std::move(result.error)};
}

MeshCacheLoadResult DocumentService::load_mesh_cache(
    const std::string& xl_path, const Guid& doc_guid) const {
  auto loaded = brep::io::load_bks_cache(xl_path, doc_guid);
  MeshCacheLoadResult out;
  out.ok = loaded.ok;
  if (loaded.ok) out.cache = std::move(loaded.cache);
  return out;
}

}  // namespace brep::viewer::adapter
