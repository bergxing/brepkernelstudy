#pragma once

#include "brep/topology.hpp"

#include <ostream>

namespace brep {

void dump_body(std::ostream& os, const Body& body);

}  // namespace brep
