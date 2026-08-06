#include "brep/guid.hpp"

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstring>
#include <stdexcept>
#include <string>

namespace brep {

Guid Guid::generate() {
  static boost::uuids::random_generator gen;
  const boost::uuids::uuid u = gen();
  std::array<std::uint8_t, 16> bytes{};
  std::memcpy(bytes.data(), u.begin(), 16);
  return Guid{bytes};
}

Guid Guid::nil() noexcept { return Guid{}; }

Guid Guid::from_string(std::string_view text) {
  try {
    const boost::uuids::uuid u =
        boost::uuids::string_generator()(std::string(text));
    std::array<std::uint8_t, 16> bytes{};
    std::memcpy(bytes.data(), u.begin(), 16);
    return Guid{bytes};
  } catch (const std::exception& ex) {
    throw std::invalid_argument(std::string("Guid::from_string: ") + ex.what());
  }
}

std::string Guid::to_string() const {
  boost::uuids::uuid u{};
  std::memcpy(u.begin(), bytes_.data(), 16);
  return boost::uuids::to_string(u);
}

bool Guid::is_nil() const noexcept {
  for (std::uint8_t b : bytes_) {
    if (b != 0) return false;
  }
  return true;
}

}  // namespace brep
