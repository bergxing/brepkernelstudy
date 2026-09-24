#include "brep/Guid.h"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

namespace brep
{

Guid Guid::Generate()
{
  static boost::uuids::random_generator gen;
  const boost::uuids::uuid u = gen();
  std::array<std::uint8_t, 16> bytes{};
  std::memcpy(bytes.data(), u.begin(), 16);
  return Guid{bytes};
}

Guid Guid::Nil() noexcept
{
    return Guid{}; 
}

Guid Guid::FromBytes(const std::array<std::uint8_t, 16>& bytes) noexcept
{
  return Guid{bytes};
}

Guid Guid::FromString(std::string_view text)
{
  try {
    const boost::uuids::uuid u =
        boost::uuids::string_generator()(std::string(text));
    std::array<std::uint8_t, 16> bytes{};
    std::memcpy(bytes.data(), u.begin(), 16);
    return Guid{bytes};
  } catch (const std::exception& ex)
  {
    throw std::invalid_argument(std::string("Guid::FromString: ") + ex.what());
  }
}

std::string Guid::ToString() const
{
  boost::uuids::uuid u{};
  std::memcpy(u.begin(), m_bytes.data(), 16);
  return boost::uuids::to_string(u);
}

bool Guid::IsValid() const noexcept
{
  for (std::uint8_t b : m_bytes)
  {
    if (b != 0) return true;
  }
  return false;
}

}  // namespace brep
