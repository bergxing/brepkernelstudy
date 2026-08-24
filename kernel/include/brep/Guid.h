#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace brep
{

/// Stable 128-bit identifier (Boost.Uuid under the hood; Boost types not exposed).
class Guid
{
 public:
  Guid() = default;

  [[nodiscard]] static Guid Generate();
  [[nodiscard]] static Guid Nil() noexcept;
  [[nodiscard]] static Guid FromString(std::string_view text);
  [[nodiscard]] static Guid FromBytes(
      const std::array<std::uint8_t, 16>& bytes) noexcept;

  [[nodiscard]] std::string ToString() const;
  [[nodiscard]] bool IsValid() const noexcept;

  [[nodiscard]] const std::array<std::uint8_t, 16>& Bytes() const noexcept
  {
    return m_bytes;
  }

  [[nodiscard]] friend bool operator==(const Guid& a, const Guid& b) noexcept
  {
    return a.m_bytes == b.m_bytes;
  }
  [[nodiscard]] friend bool operator!=(const Guid& a, const Guid& b) noexcept
  {
    return !(a == b);
  }
  [[nodiscard]] friend bool operator<(const Guid& a, const Guid& b) noexcept
  {
    return a.m_bytes < b.m_bytes;
  }

 private:
  explicit Guid(std::array<std::uint8_t, 16> bytes) : m_bytes(bytes)
  {
  }

  std::array<std::uint8_t, 16> m_bytes{};
};

}  // namespace brep

namespace std
{
template <>
struct hash<brep::Guid>
{
  size_t operator()(const brep::Guid& g) const noexcept
  {
    const auto& b = g.Bytes();
    std::uint64_t lo = 0;
    std::uint64_t hi = 0;
    for (int i = 0; i < 8; ++i)
    {
      lo |= static_cast<std::uint64_t>(b[static_cast<std::size_t>(i)])
            << (8 * i);
      hi |= static_cast<std::uint64_t>(b[static_cast<std::size_t>(8 + i)])
            << (8 * i);
    }
    return hash<std::uint64_t>{}(lo) ^ (hash<std::uint64_t>{}(hi) << 1);
  }
};
}  // namespace std
