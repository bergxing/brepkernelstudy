#include <iostream>

#include "brep/Log.h"
#include "brep/Math.h"

int main()
{
  std::cout << "smoke: start\n" << std::flush;
  brep::Point3d p{1, 2, 3};
  brep::Vector3d v{0, 0, 1};
  std::cout << "point=" << p << " vec=" << v << "\n" << std::flush;

  brep::InitLogging({}, brep::LogLevel::Info);
  BREP_INFO("hello from smoke p={} v={}", p, v);
  std::cout << "smoke: done\n" << std::flush;
  return 0;
}
