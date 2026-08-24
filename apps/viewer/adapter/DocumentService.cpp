#include "adapter/DocumentService.h"

#include "api/Persistence.h"

namespace brep::viewer::adapter
{

LoadResult DocumentService::load(const std::string& path) const
{
  auto loaded = brep::io::LoadXl(path);
  LoadResult out;
  out.ok = loaded.Ok();
  out.error = loaded.Error;
  out.document = std::move(loaded.document);
  return out;
}

SaveResult DocumentService::save(const brep::Document& doc,
                                 const std::string& path) const
{
  auto result = brep::io::SaveXl(doc, path);
  return SaveResult{.ok = result.Ok, .error = std::move(result.Error)};
}

MeshCacheLoadResult DocumentService::load_mesh_cache(
    const std::string& xl_path, const Guid& doc_guid) const
{
  auto loaded = brep::io::LoadBksCache(xl_path, doc_guid);
  MeshCacheLoadResult out;
  out.ok = loaded.Ok;
  if (loaded.Ok) out.cache = std::move(loaded.Cache);
  return out;
}

}  // namespace brep::viewer::adapter
