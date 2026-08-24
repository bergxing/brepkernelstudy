#pragma once

#include "brep/Guid.h"

#include <string>
#include <utility>

namespace brep
{

enum class ObjectKind
{
  Document,
  Part,
  Body,
};

/// Document / Part / Body identity. Topology/geometry keep session `Id` only.
class IObject
{
 public:
  Guid Guid{::brep::Guid::Generate()};
  std::string Name;

  virtual ~IObject() = default;

  [[nodiscard]] virtual ObjectKind Kind() const noexcept = 0;

 protected:
  explicit IObject(std::string objectName = {},
                   ::brep::Guid objectGuid = ::brep::Guid::Generate())
      : Guid(std::move(objectGuid)), Name(std::move(objectName))
  {
  }
};

}  // namespace brep
