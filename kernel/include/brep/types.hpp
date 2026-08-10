#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace brep {

enum class Orientation : std::int8_t {
  Forward = 1,
  Reversed = -1,
};

[[nodiscard]] constexpr Orientation opposite(Orientation o) noexcept {
  return o == Orientation::Forward ? Orientation::Reversed : Orientation::Forward;
}

[[nodiscard]] constexpr int sense_as_int(Orientation o) noexcept {
  return static_cast<int>(o);
}

enum class BodyType { Solid, Sheet, Wire };

enum class LoopType { Outer, Inner };

enum class SurfaceKind { Plane, Sphere, Cylinder, Nurbs };
enum class CurveKind { Line, Circle, Nurbs };

using Id = std::uint64_t;

struct Named {
  Id id{0};
  std::string name;

  [[nodiscard]] std::string_view label() const noexcept {
    return name.empty() ? std::string_view{} : std::string_view{name};
  }
};

}  // namespace brep
