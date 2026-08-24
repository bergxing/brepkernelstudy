#pragma once

#include "brep/Topology.h"

#include <ostream>

namespace brep
{

void DumpBody(std::ostream& os, const Body& body);

}  // namespace brep
