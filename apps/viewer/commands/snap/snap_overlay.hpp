#pragma once

#include "api/mesh.hpp"
#include "api/snap.hpp"

#include <QString>

namespace brep::viewer::commands {

/// Build a small world-space glyph centered at the winning snap point.
[[nodiscard]] EdgeMesh make_snap_marker(SnapKind kind, const Point3d& point);

/// Return the localized object snap name shown in the cursor tip.
[[nodiscard]] QString snap_kind_name(SnapKind kind);

}  // namespace brep::viewer::commands
