#pragma once

#include "brep/guid.hpp"
#include "brep/iobject.hpp"

#include <unordered_map>

namespace brep {

/// Document-scoped Guid → IObject* lookup (non-owning).
class ObjectRegistry {
 public:
  void add(IObject& object);
  void remove(const Guid& guid) noexcept;
  void clear() noexcept;

  [[nodiscard]] IObject* find(const Guid& guid) noexcept;
  [[nodiscard]] const IObject* find(const Guid& guid) const noexcept;

  template <class T>
  [[nodiscard]] T* find_as(const Guid& guid) noexcept {
    return dynamic_cast<T*>(find(guid));
  }

  template <class T>
  [[nodiscard]] const T* find_as(const Guid& guid) const noexcept {
    return dynamic_cast<const T*>(find(guid));
  }

  [[nodiscard]] std::size_t size() const noexcept { return map_.size(); }

 private:
  std::unordered_map<Guid, IObject*> map_;
};

}  // namespace brep
