#pragma once

#include "brep/guid.hpp"

#include <string>
#include <utility>

namespace brep {

enum class ObjectKind {
  Document,
  Part,
  Body,
};

/// Document / Part / Body identity. Topology/geometry keep session `Id` only.
class IObject {
 public:
  Guid guid{Guid::generate()};
  std::string name;

  virtual ~IObject() = default;

  [[nodiscard]] virtual ObjectKind kind() const noexcept = 0;

 protected:
  explicit IObject(std::string object_name = {}, Guid id = Guid::generate())
      : guid(std::move(id)), name(std::move(object_name)) {}
};

}  // namespace brep
