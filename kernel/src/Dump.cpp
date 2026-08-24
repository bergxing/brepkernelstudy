#include "brep/Dump.h"

#include <string>

namespace brep
{

void DumpBody(std::ostream& os, const Body& body)
{
  os << "Body id=" << body.id << " name=\"" << body.Name << "\" type=";
  switch (body.Type)
  {
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
  os << " shells=" << body.Shells.size() << '\n';

  for (const Shell* shell : body.Shells)
  {
    if (!shell) continue;
    os << "  Shell id=" << shell->Id << " name=\"" << shell->Name
       << "\" closed=" << std::boolalpha << shell->Closed
       << " faces=" << shell->Faces.size() << '\n';

    for (const Face* face : shell->Faces)
    {
      if (!face) continue;
      os << "    Face id=" << face->Id << " name=\"" << face->Name
         << "\" sense=" << sense_as_int(face->Sense)
         << " loops=" << face->Loops.size() << '\n';

      for (const Loop* loop : face->Loops)
      {
        if (!loop) continue;
        os << "      Loop id=" << loop->Id << " name=\"" << loop->Name
           << "\" type=" << (loop->Type == LoopType::Outer ? "Outer" : "Inner")
           << " size=" << loop->CoedgeCount() << '\n';

        loop->ForEachCoedge([&](const CoEdge& c)
        {
          os << "        CoEdge id=" << c.Id << " edge=\""
             << (c.Edge ? c.Edge->Name : "?") << "\" sense="
             << sense_as_int(c.Sense) << " from="
             << (c.From() ? c.From()->Name : "?") << " to="
             << (c.To() ? c.To()->Name : "?") << " partner="
             << (c.Partner ? std::to_string(c.Partner->Id) : "null") << '\n';
        });
      }
    }
  }
}

}  // namespace brep
