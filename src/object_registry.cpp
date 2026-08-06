#include "brep/object_registry.hpp"

#include "brep/log.hpp"

namespace brep {

void ObjectRegistry::add(IObject& object) {
  const Guid& g = object.guid;
  if (g.is_nil()) {
    BREP_WARN("ObjectRegistry::add: refusing nil Guid for '{}'", object.name);
    return;
  }
  auto [it, inserted] = map_.emplace(g, &object);
  if (!inserted) {
    BREP_WARN("ObjectRegistry::add: Guid {} already registered (replacing '{}')",
              g.to_string(), object.name);
    it->second = &object;
  }
}

void ObjectRegistry::remove(const Guid& guid) noexcept { map_.erase(guid); }

void ObjectRegistry::clear() noexcept { map_.clear(); }

IObject* ObjectRegistry::find(const Guid& guid) noexcept {
  const auto it = map_.find(guid);
  return it == map_.end() ? nullptr : it->second;
}

const IObject* ObjectRegistry::find(const Guid& guid) const noexcept {
  const auto it = map_.find(guid);
  return it == map_.end() ? nullptr : it->second;
}

}  // namespace brep
