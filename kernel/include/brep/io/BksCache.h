#pragma once

#include "brep/Document.h"
#include "brep/Guid.h"
#include "brep/Mesh.h"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace brep::io
{

/// Sidecar mesh cache written next to an .xl as "<stem>.bks.cache".
/// Speeds viewer open by skipping tessellation when Guids still match.
struct BodyMeshCache
{
    Guid DocumentGuid{};
    std::unordered_map<Guid, TriangleMesh> Triangles;
    std::unordered_map<Guid, EdgeMesh> Edges;

    [[nodiscard]] bool Has(const Guid& body) const
    {
        return Triangles.count(body) && Edges.count(body);
    }
};

[[nodiscard]] std::filesystem::path BksCachePathFor(
    const std::filesystem::path& xlPath);

struct CacheSaveResult
{
    bool Ok{false};
    std::string Error;
};

struct CacheLoadResult
{
    bool Ok{false};
    BodyMeshCache Cache;
    std::string Error;
};

/// Tessellate all bodies in the document and write the sidecar cache.
[[nodiscard]] CacheSaveResult SaveBksCache(
    const Document& doc, const std::filesystem::path& xlPath);

[[nodiscard]] CacheLoadResult LoadBksCache(
    const std::filesystem::path& xlPath, const Guid& expectedDocumentGuid);

}  // namespace brep::io
