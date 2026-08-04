#include "brep/dump.hpp"

#include <string>

namespace brep {

void dump_body(std::ostream& os, const Body& body) {
  os << "Body id=" << body.id << " name=\"" << body.name << "\" type=";
  switch (body.type) {
    case BodyType::Solid:
      os << "Solid";
      break;
    case BodyType::Sheet:
      os << "Sheet";
      break;
    case BodyType::Wire:
      os << "Wire";
      break;
  }
  os << " shells=" << body.shells.size() << '\n';

  for (const Shell* shell : body.shells) {
    if (!shell) continue;
    os << "  Shell id=" << shell->id << " name=\"" << shell->name
       << "\" closed=" << std::boolalpha << shell->closed
       << " faces=" << shell->faces.size() << '\n';

    for (const Face* face : shell->faces) {
      if (!face) continue;
      os << "    Face id=" << face->id << " name=\"" << face->name
         << "\" sense=" << sense_as_int(face->sense)
         << " loops=" << face->loops.size() << '\n';

      for (const Loop* loop : face->loops) {
        if (!loop) continue;
        os << "      Loop id=" << loop->id << " name=\"" << loop->name
           << "\" type=" << (loop->type == LoopType::Outer ? "Outer" : "Inner")
           << " size=" << loop->size() << '\n';

        loop->for_each_coedge([&](const CoEdge& c) {
          os << "        CoEdge id=" << c.id << " edge=\""
             << (c.edge ? c.edge->name : "?") << "\" sense="
             << sense_as_int(c.sense) << " from="
             << (c.from() ? c.from()->name : "?") << " to="
             << (c.to() ? c.to()->name : "?") << " partner="
             << (c.partner ? std::to_string(c.partner->id) : "null") << '\n';
        });
      }
    }
  }
}

}  // namespace brep
