#pragma once

#include "brep/Guid.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace brep::param
{

enum class ParamKind
{
  Length,
  Angle,
  Real,
  Integer,
  Boolean
};

struct ParameterId
{
  Guid Guid{};

  [[nodiscard]] friend bool operator==(const ParameterId& a,
                                       const ParameterId& b) noexcept
  {
    return a.Guid == b.Guid;
  }
  [[nodiscard]] friend bool operator!=(const ParameterId& a,
                                       const ParameterId& b) noexcept
  {
    return !(a == b);
  }
};

struct Parameter
{
  ParameterId Id{};
  std::string Name;
  ParamKind Kind{ParamKind::Length};
  double Value{0.0};
  bool UserDriven{true};
};

class ParameterStore
{
 public:
  ParameterId Add(std::string name, ParamKind kind, double value);
  /// Insert with a stable Guid (document load). Fails if Guid already exists.
  bool AddWithId(ParameterId id, std::string name, ParamKind kind, double value,
                 bool userDriven = true);
  bool Set(ParameterId id, double value);
  bool SetByName(std::string_view name, double value);
  [[nodiscard]] std::optional<double> Get(ParameterId id) const;
  [[nodiscard]] std::optional<double> GetByName(std::string_view name) const;
  [[nodiscard]] Parameter* Find(ParameterId id);
  [[nodiscard]] const Parameter* Find(ParameterId id) const;
  [[nodiscard]] Parameter* FindByName(std::string_view name);
  [[nodiscard]] const Parameter* FindByName(std::string_view name) const;

  [[nodiscard]] bool Dirty() const noexcept
  {
    return m_dirty;
  }
  void ClearDirty() noexcept
  {
    m_dirty = false;
  }
  void MarkDirty() noexcept
  {
    m_dirty = true;
  }

  [[nodiscard]] const std::vector<Parameter>& All() const noexcept
  {
    return m_params;
  }

  bool Remove(ParameterId id);

 private:
  std::vector<Parameter> m_params;
  std::unordered_map<Guid, std::size_t> m_index;
  bool m_dirty{false};
};

}  // namespace brep::param

namespace std
{
template <>
struct hash<brep::param::ParameterId>
{
  size_t operator()(const brep::param::ParameterId& id) const noexcept
  {
    return hash<brep::Guid>{}(id.Guid);
  }
};
}  // namespace std
