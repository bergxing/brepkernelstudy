#pragma once

namespace brep::viewer
{

/// Application entry used by the thin exe. Lives in viewer_ui so Qt
/// signal/slot connections stay inside one shared library (MinGW-safe).
int run_viewer(int argc, char* argv[]);

}  // namespace brep::viewer
