#include "brep/ObjectRegistry.h"

#include "brep/Log.h"

namespace brep
{

void ObjectRegistry::Add(IObject& object)
{
  const Guid& g = object.Guid;
  if (!g.IsValid())
  {
    BREP_WARN("ObjectRegistry::Add: refusing nil Guid for '{}'", object.Name);
    return;
  }
  auto [it, inserted] = m_map.emplace(g, &object);
  if (!inserted)
  {
    BREP_WARN("ObjectRegistry::Add: Guid {} already registered (replacing '{}')",
              g.ToString(), object.Name);
    it->second = &object;
  }
}

void ObjectRegistry::Remove(const Guid& guid) noexcept
{
    m_map.erase(guid); 
}

void ObjectRegistry::Clear() noexcept
{
  m_map.clear();
}

IObject* ObjectRegistry::Find(const Guid& guid) noexcept
{
  const auto it = m_map.find(guid);
  return it == m_map.end() ? nullptr : it->second;
}

const IObject* ObjectRegistry::Find(const Guid& guid) const noexcept
{
  const auto it = m_map.find(guid);
  return it == m_map.end() ? nullptr : it->second;
}

}  // namespace brep
