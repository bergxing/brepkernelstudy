#pragma once

#include "brep/Guid.h"
#include "brep/IObject.h"

#include <unordered_map>

namespace brep
{

/// Document-scoped Guid → IObject* lookup (non-owning).
class ObjectRegistry
{
 public:
  void Add(IObject& object);
  void Remove(const Guid& guid) noexcept;
  void Clear() noexcept;

  [[nodiscard]] IObject* Find(const Guid& guid) noexcept;
  [[nodiscard]] const IObject* Find(const Guid& guid) const noexcept;

  template <class T>
  [[nodiscard]] T* FindAs(const Guid& guid) noexcept
  {
    return dynamic_cast<T*>(Find(guid));
  }

  template <class T>
  [[nodiscard]] const T* FindAs(const Guid& guid) const noexcept
  {
    return dynamic_cast<const T*>(Find(guid));
  }

  [[nodiscard]] std::size_t Size() const noexcept
  {
    return m_map.size();
  }

 private:
  std::unordered_map<Guid, IObject*> m_map;
};

}  // namespace brep
