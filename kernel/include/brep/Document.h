#pragma once

#include "brep/asm/Assembly.h"
#include "brep/IObject.h"
#include "brep/ObjectRegistry.h"
#include "brep/Part.h"

#include <memory>
#include <string>
#include <vector>

namespace brep
{

/// Document owns Parts, an Assembly, and the Guid registry.
class Document final : public IObject
{
 public:
  [[nodiscard]] static std::unique_ptr<Document> Create(
      std::string title = "Untitled");

  explicit Document(std::string title = "Untitled");

  [[nodiscard]] ObjectKind Kind() const noexcept override
  {
    return ObjectKind::Document;
  }

  [[nodiscard]] ObjectRegistry& Registry() noexcept
  {
    return m_registry;
  }
  [[nodiscard]] const ObjectRegistry& Registry() const noexcept
  {
    return m_registry;
  }

  [[nodiscard]] bool Dirty() const noexcept
  {
    return m_dirty;
  }
  void MarkDirty() noexcept
  {
    m_dirty = true;
  }
  void MarkClean() noexcept
  {
    m_dirty = false;
  }

  [[nodiscard]] const std::string& Path() const noexcept
  {
    return m_path;
  }
  void SetPath(std::string path)
  {
    m_path = std::move(path);
  }

  Part& AddPart(std::string part_name = "MainPart");
  [[nodiscard]] Part* MainPart() noexcept;
  [[nodiscard]] const Part* MainPart() const noexcept;

  [[nodiscard]] const std::vector<std::unique_ptr<Part>>& Parts() const noexcept
  {
    return m_parts;
  }

  [[nodiscard]] asm_::Assembly& Assembly() noexcept
  {
    return m_assembly;
  }
  [[nodiscard]] const asm_::Assembly& Assembly() const noexcept
  {
    return m_assembly;
  }

  /// Drop all parts/assembly and registry entries; keep this Document's Guid/name.
  void Clear();

 private:
  ObjectRegistry m_registry;
  std::vector<std::unique_ptr<Part>> m_parts;
  asm_::Assembly m_assembly;
  std::string m_path;
  bool m_dirty{false};
};

}  // namespace brep
