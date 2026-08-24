#pragma once

#include "api/Mesh.h"
#include "api/Snap.h"

#include <QString>

namespace brep::viewer::commands
{

/// Build a small world-space glyph centered at the winning snap point.
[[nodiscard]] EdgeMesh MakeSnapMarker(SnapKind kind, const Point3d& point);

/// Return the localized object snap name shown in the cursor tip.
[[nodiscard]] QString SnapKindName(SnapKind kind);

}  // namespace brep::viewer::commands
