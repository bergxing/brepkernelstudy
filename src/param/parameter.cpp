#include "brep/param/parameter.hpp"

namespace brep::param {

ParameterId ParameterStore::add(std::string name, ParamKind kind, double value) {
  Parameter p;
  p.id.guid = Guid::generate();
  p.name = std::move(name);
  p.kind = kind;
  p.value = value;
  const ParameterId id = p.id;
  index_[id.guid] = params_.size();
  params_.push_back(std::move(p));
  dirty_ = true;
  return id;
}

bool ParameterStore::add_with_id(ParameterId id, std::string name,
                                 ParamKind kind, double value,
                                 bool user_driven) {
  if (id.guid.is_nil() || index_.count(id.guid)) return false;
  Parameter p;
  p.id = id;
  p.name = std::move(name);
  p.kind = kind;
  p.value = value;
  p.user_driven = user_driven;
  index_[id.guid] = params_.size();
  params_.push_back(std::move(p));
  dirty_ = true;
  return true;
}

bool ParameterStore::set(ParameterId id, double value) {
  Parameter* p = find(id);
  if (!p) return false;
  if (p->value == value) return true;
  p->value = value;
  dirty_ = true;
  return true;
}

bool ParameterStore::set_by_name(std::string_view name, double value) {
  Parameter* p = find_by_name(name);
  if (!p) return false;
  return set(p->id, value);
}

std::optional<double> ParameterStore::get(ParameterId id) const {
  const Parameter* p = find(id);
  if (!p) return std::nullopt;
  return p->value;
}

std::optional<double> ParameterStore::get_by_name(std::string_view name) const {
  const Parameter* p = find_by_name(name);
  if (!p) return std::nullopt;
  return p->value;
}

Parameter* ParameterStore::find(ParameterId id) {
  const auto it = index_.find(id.guid);
  if (it == index_.end()) return nullptr;
  return &params_[it->second];
}

const Parameter* ParameterStore::find(ParameterId id) const {
  const auto it = index_.find(id.guid);
  if (it == index_.end()) return nullptr;
  return &params_[it->second];
}

Parameter* ParameterStore::find_by_name(std::string_view name) {
  for (auto& p : params_) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

const Parameter* ParameterStore::find_by_name(std::string_view name) const {
  for (const auto& p : params_) {
    if (p.name == name) return &p;
  }
  return nullptr;
}

bool ParameterStore::remove(ParameterId id) {
  const auto it = index_.find(id.guid);
  if (it == index_.end()) return false;
  const std::size_t idx = it->second;
  index_.erase(it);
  params_.erase(params_.begin() + static_cast<std::ptrdiff_t>(idx));
  index_.clear();
  for (std::size_t i = 0; i < params_.size(); ++i) {
    index_[params_[i].id.guid] = i;
  }
  dirty_ = true;
  return true;
}

}  // namespace brep::param
