#include "commands/snap/snap_settings.hpp"

namespace brep::viewer::commands {

std::uint32_t default_snap_kinds() noexcept {
  return static_cast<std::uint32_t>(SnapKind::Endpoint) |
         static_cast<std::uint32_t>(SnapKind::Midpoint) |
         static_cast<std::uint32_t>(SnapKind::Center) |
         static_cast<std::uint32_t>(SnapKind::Intersection) |
         static_cast<std::uint32_t>(SnapKind::Perpendicular) |
         static_cast<std::uint32_t>(SnapKind::Nearest);
}

}  // namespace brep::viewer::commands
