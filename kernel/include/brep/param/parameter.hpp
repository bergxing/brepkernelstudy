#pragma once

#include "brep/guid.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace brep::param {

enum class ParamKind { Length, Angle, Real, Integer, Boolean };

struct ParameterId {
  Guid guid{};

  [[nodiscard]] friend bool operator==(const ParameterId& a,
                                       const ParameterId& b) noexcept {
    return a.guid == b.guid;
  }
  [[nodiscard]] friend bool operator!=(const ParameterId& a,
                                       const ParameterId& b) noexcept {
    return !(a == b);
  }
};

struct Parameter {
  ParameterId id{};
  std::string name;
  ParamKind kind{ParamKind::Length};
  double value{0.0};
  bool user_driven{true};
};

class ParameterStore {
 public:
  ParameterId add(std::string name, ParamKind kind, double value);
  /// Insert with a stable Guid (document load). Fails if Guid already exists.
  bool add_with_id(ParameterId id, std::string name, ParamKind kind,
                   double value, bool user_driven = true);
  bool set(ParameterId id, double value);
  bool set_by_name(std::string_view name, double value);
  [[nodiscard]] std::optional<double> get(ParameterId id) const;
  [[nodiscard]] std::optional<double> get_by_name(std::string_view name) const;
  [[nodiscard]] Parameter* find(ParameterId id);
  [[nodiscard]] const Parameter* find(ParameterId id) const;
  [[nodiscard]] Parameter* find_by_name(std::string_view name);
  [[nodiscard]] const Parameter* find_by_name(std::string_view name) const;

  [[nodiscard]] bool dirty() const noexcept { return dirty_; }
  void clear_dirty() noexcept { dirty_ = false; }
  void mark_dirty() noexcept { dirty_ = true; }

  [[nodiscard]] const std::vector<Parameter>& all() const noexcept {
    return params_;
  }

  bool remove(ParameterId id);

 private:
  std::vector<Parameter> params_;
  std::unordered_map<Guid, std::size_t> index_;
  bool dirty_{false};
};

}  // namespace brep::param

namespace std {
template <>
struct hash<brep::param::ParameterId> {
  size_t operator()(const brep::param::ParameterId& id) const noexcept {
    return hash<brep::Guid>{}(id.guid);
  }
};
}  // namespace std
