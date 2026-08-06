#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace brep {

/// Stable 128-bit identifier (Boost.Uuid under the hood; Boost types not exposed).
class Guid {
 public:
  Guid() = default;

  [[nodiscard]] static Guid generate();
  [[nodiscard]] static Guid nil() noexcept;
  [[nodiscard]] static Guid from_string(std::string_view text);

  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] bool is_nil() const noexcept;

  [[nodiscard]] const std::array<std::uint8_t, 16>& bytes() const noexcept {
    return bytes_;
  }

  [[nodiscard]] friend bool operator==(const Guid& a, const Guid& b) noexcept {
    return a.bytes_ == b.bytes_;
  }
  [[nodiscard]] friend bool operator!=(const Guid& a, const Guid& b) noexcept {
    return !(a == b);
  }
  [[nodiscard]] friend bool operator<(const Guid& a, const Guid& b) noexcept {
    return a.bytes_ < b.bytes_;
  }

 private:
  explicit Guid(std::array<std::uint8_t, 16> bytes) : bytes_(bytes) {}

  std::array<std::uint8_t, 16> bytes_{};
};

}  // namespace brep

namespace std {
template <>
struct hash<brep::Guid> {
  size_t operator()(const brep::Guid& g) const noexcept {
    const auto& b = g.bytes();
    std::uint64_t lo = 0;
    std::uint64_t hi = 0;
    for (int i = 0; i < 8; ++i) {
      lo |= static_cast<std::uint64_t>(b[static_cast<std::size_t>(i)])
            << (8 * i);
      hi |= static_cast<std::uint64_t>(b[static_cast<std::size_t>(8 + i)])
            << (8 * i);
    }
    return hash<std::uint64_t>{}(lo) ^ (hash<std::uint64_t>{}(hi) << 1);
  }
};
}  // namespace std
