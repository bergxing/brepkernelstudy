#pragma once

#include <cstdint>

namespace brep::viewer {

struct TriVertexGpu {
  float pos[3];
  float nrm[3];
  float uv[2];
};

struct AxisVertexGpu {
  float pos[3];
  float color[4];
};

}  // namespace brep::viewer
