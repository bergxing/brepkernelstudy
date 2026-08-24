#pragma once

#include "brep/Document.h"

#include <filesystem>
#include <string>

namespace brep::io
{

/// Binary document format (.xl):
///   magic "XL01" | u32 schema | u32 flags | u64 payload_size | payload | u32 crc32
/// Payload (LE): Document/Parts/parameters/features; schema>=2 also Assembly.
///   Feature types: Box, Sphere, Sketch, Extrude, Boolean.
/// Sidecar mesh cache: "<stem>.bks.cache" (optional, written on save).
/// CRC detects corruption / casual edits; this is integrity, not encryption.

inline constexpr const char* kXlExtension = ".xl";
inline constexpr std::uint32_t kXlSchemaVersion = 2;
inline constexpr std::uint32_t kXlSchemaVersionMin = 1;

struct XlSaveResult
{
    bool Ok{false};
    std::string Error;
};

struct XlLoadResult
{
    std::unique_ptr<Document> document;
    std::string Error;

    [[nodiscard]] bool Ok() const noexcept
    {
        return document != nullptr;
    }
};

[[nodiscard]] XlSaveResult SaveXl(const Document& doc,
                                  const std::filesystem::path& path);

[[nodiscard]] XlLoadResult LoadXl(const std::filesystem::path& path);

[[nodiscard]] const std::string& LastXlError();

}  // namespace brep::io
