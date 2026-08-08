#pragma once

#include "brep/guid.hpp"
#include "brep/param/parameter.hpp"

#include <string>
#include <string_view>

namespace brep {
class Part;
}

namespace brep::feat {

enum class FeatureStatus { Ok, Suppressed, Failed, Dirty };

struct FeatureId {
  Guid guid{};

  [[nodiscard]] friend bool operator==(const FeatureId& a,
                                       const FeatureId& b) noexcept {
    return a.guid == b.guid;
  }
  [[nodiscard]] friend bool operator!=(const FeatureId& a,
                                       const FeatureId& b) noexcept {
    return !(a == b);
  }
  [[nodiscard]] bool is_nil() const noexcept { return guid.is_nil(); }
};

class IFeature {
 public:
  virtual ~IFeature() = default;

  [[nodiscard]] virtual FeatureId id() const = 0;
  [[nodiscard]] virtual std::string_view type_name() const = 0;
  [[nodiscard]] virtual FeatureStatus status() const = 0;
  virtual void set_status(FeatureStatus s) = 0;
  virtual void set_suppressed(bool suppressed) = 0;
  [[nodiscard]] virtual bool suppressed() const = 0;

  virtual void collect_parameters(param::ParameterStore& store) = 0;
  virtual bool rebuild(Part& part, param::ParameterStore& params) = 0;

  [[nodiscard]] virtual Guid body_guid() const = 0;
  virtual void set_body_guid(Guid g) = 0;

  [[nodiscard]] virtual std::string display_name() const = 0;
};

}  // namespace brep::feat

namespace std {
template <>
struct hash<brep::feat::FeatureId> {
  size_t operator()(const brep::feat::FeatureId& id) const noexcept {
    return hash<brep::Guid>{}(id.guid);
  }
};
}  // namespace std
