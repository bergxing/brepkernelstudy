#pragma once

#include "brep/asm/assembly.hpp"
#include "brep/iobject.hpp"
#include "brep/object_registry.hpp"
#include "brep/part.hpp"

#include <memory>
#include <string>
#include <vector>

namespace brep {

/// Document owns Parts, an Assembly, and the Guid registry.
class Document final : public IObject {
 public:
  [[nodiscard]] static std::unique_ptr<Document> create(
      std::string title = "Untitled");

  explicit Document(std::string title = "Untitled");

  [[nodiscard]] ObjectKind kind() const noexcept override {
    return ObjectKind::Document;
  }

  [[nodiscard]] ObjectRegistry& registry() noexcept { return registry_; }
  [[nodiscard]] const ObjectRegistry& registry() const noexcept {
    return registry_;
  }

  [[nodiscard]] bool dirty() const noexcept { return dirty_; }
  void mark_dirty() noexcept { dirty_ = true; }
  void mark_clean() noexcept { dirty_ = false; }

  [[nodiscard]] const std::string& path() const noexcept { return path_; }
  void set_path(std::string path) { path_ = std::move(path); }

  Part& add_part(std::string part_name = "MainPart");
  [[nodiscard]] Part* main_part() noexcept;
  [[nodiscard]] const Part* main_part() const noexcept;

  [[nodiscard]] const std::vector<std::unique_ptr<Part>>& parts() const noexcept {
    return parts_;
  }

  [[nodiscard]] asm_::Assembly& assembly() noexcept { return assembly_; }
  [[nodiscard]] const asm_::Assembly& assembly() const noexcept {
    return assembly_;
  }

  /// Drop all parts/assembly and registry entries; keep this Document's Guid/name.
  void clear();

 private:
  ObjectRegistry registry_;
  std::vector<std::unique_ptr<Part>> parts_;
  asm_::Assembly assembly_;
  std::string path_;
  bool dirty_{false};
};

}  // namespace brep
