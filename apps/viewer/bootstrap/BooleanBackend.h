#pragma once

namespace brep::viewer::bootstrap
{

/// Selectable boolean evaluator backend (Phase 5). `Occt` is reserved — not implemented.
enum class BooleanBackend
{
  Default,
  Stub,
  Occt,
};

}  // namespace brep::viewer::bootstrap
