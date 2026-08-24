#include "brep/param/Parameter.h"

namespace brep::param
{

ParameterId ParameterStore::Add(std::string name, ParamKind kind, double value)
{
  Parameter p;
  p.Id.Guid = Guid::Generate();
  p.Name = std::move(name);
  p.Kind = kind;
  p.Value = value;
  const ParameterId id = p.Id;
  m_index[id.Guid] = m_params.size();
  m_params.push_back(std::move(p));
  m_dirty = true;
  return id;
}

bool ParameterStore::AddWithId(ParameterId id, std::string name, ParamKind kind,
                               double value, bool userDriven)
{
  if (!id.Guid.IsValid() || m_index.count(id.Guid)) return false;
  Parameter p;
  p.Id = id;
  p.Name = std::move(name);
  p.Kind = kind;
  p.Value = value;
  p.UserDriven = userDriven;
  m_index[id.Guid] = m_params.size();
  m_params.push_back(std::move(p));
  m_dirty = true;
  return true;
}

bool ParameterStore::Set(ParameterId id, double value)
{
  Parameter* p = Find(id);
  if (!p) return false;
  if (p->Value == value) return true;
  p->Value = value;
  m_dirty = true;
  return true;
}

bool ParameterStore::SetByName(std::string_view name, double value)
{
  Parameter* p = FindByName(name);
  if (!p) return false;
  return Set(p->Id, value);
}

std::optional<double> ParameterStore::Get(ParameterId id) const
{
  const Parameter* p = Find(id);
  if (!p) return std::nullopt;
  return p->Value;
}

std::optional<double> ParameterStore::GetByName(std::string_view name) const
{
  const Parameter* p = FindByName(name);
  if (!p) return std::nullopt;
  return p->Value;
}

Parameter* ParameterStore::Find(ParameterId id)
{
  const auto it = m_index.find(id.Guid);
  if (it == m_index.end()) return nullptr;
  return &m_params[it->second];
}

const Parameter* ParameterStore::Find(ParameterId id) const
{
  const auto it = m_index.find(id.Guid);
  if (it == m_index.end()) return nullptr;
  return &m_params[it->second];
}

Parameter* ParameterStore::FindByName(std::string_view name)
{
  for (auto& p : m_params)
  {
    if (p.Name == name) return &p;
  }
  return nullptr;
}

const Parameter* ParameterStore::FindByName(std::string_view name) const
{
  for (const auto& p : m_params)
  {
    if (p.Name == name) return &p;
  }
  return nullptr;
}

bool ParameterStore::Remove(ParameterId id)
{
  const auto it = m_index.find(id.Guid);
  if (it == m_index.end()) return false;
  const std::size_t idx = it->second;
  m_index.erase(it);
  m_params.erase(m_params.begin() + static_cast<std::ptrdiff_t>(idx));
  m_index.clear();
  for (std::size_t i = 0; i < m_params.size(); ++i)
  {
    m_index[m_params[i].Id.Guid] = i;
  }
  m_dirty = true;
  return true;
}

}  // namespace brep::param
