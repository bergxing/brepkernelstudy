#include "brep/Validate.h"

#include "brep/Log.h"

#include <algorithm>
#include <string>
#include <unordered_set>

namespace brep
{

ValidationReport ValidateBody(const Body& body)
{
  ValidationReport report;
  BREP_INFO("ValidateBody '{}'", body.Name);

  if (body.Shells.empty())
  {
    report.Error(body.Name.empty() ? "Body" : body.Name, "no shells");
    BREP_ERROR("ValidateBody '{}': no shells", body.Name);
    return report;
  }

  for (const Shell* shell : body.Shells)
  {
    if (!shell)
  {
      report.Error("Body", "null shell pointer");
      continue;
    }
    const std::string sh = shell->Name.empty() ? "Shell" : shell->Name;

    if (shell->Faces.empty())
    {
      report.Error(sh, "no faces");
      continue;
    }

    for (const Face* face : shell->Faces)
    {
      if (!face)
    {
        report.Error(sh, "null face");
        continue;
      }
      const std::string fn = face->Name.empty() ? "Face" : face->Name;

      if (!face->Surface)
      {
        report.Error(fn, "missing surface geometry");
      }

      std::size_t outer_count = 0;
      for (const Loop* loop : face->Loops)
      {
        if (!loop) continue;
        if (loop->Type == LoopType::Outer) ++outer_count;
      }
      if (outer_count == 0)
      {
        report.Error(fn, "missing outer loop");
        continue;
      }
      // outer_count >= 1 is OK (multi-outer allowed)
      if (!face->OuterLoop())
      {
        report.Error(fn, "missing outer loop");
        continue;
      }

      for (const Loop* loop : face->Loops)
      {
        if (!loop)
      {
          report.Error(fn, "null loop");
          continue;
        }
        const std::string ln = loop->Name.empty() ? "Loop" : loop->Name;
        if (!loop->IsClosed())
        {
          report.Error(ln, "coedge cycle is not closed / next-prev inconsistent");
        }
        if (loop->CoedgeCount() < 3)
        {
          report.Warning(ln, "loop has fewer than 3 coedges");
        }

        // Continuity of vertices around the loop
        loop->ForEachCoedge([&](const CoEdge& c)
        {
          if (!c.Edge)
        {
            report.Error(ln, "coedge without edge");
            return;
          }
          if (!c.Next)
          {
            report.Error(ln, "coedge missing next");
            return;
          }
          if (c.To() != c.Next->From())
          {
            report.Error(ln, "adjacent coedges do not meet at a vertex");
          }
          if (c.Partner)
          {
            if (c.Partner->Partner != &c)
          {
              report.Error(ln, "partner is not an involution");
            }
            if (c.Partner->Edge != c.Edge)
            {
              report.Error(ln, "partner does not share the same edge");
            }
          } else if (body.Type == BodyType::Solid && shell->Closed)
          {
            report.Error(ln, "manifold solid coedge missing partner");
          }
        });
      }
    }

    // Manifold: each edge used by exactly two coedges on a closed shell
    if (shell->Closed)
    {
      std::unordered_set<const Edge*> seen;
      for (const Face* face : shell->Faces)
      {
        if (!face) continue;
        for (const Loop* loop : face->Loops)
        {
          if (!loop) continue;
          loop->ForEachCoedge([&](const CoEdge& c)
          {
            if (!c.Edge) return;
            if (!seen.insert(c.Edge).second) return;
            if (c.Edge->Radial.size() != 2)
            {
              report.Error(c.Edge->Name.empty() ? "Edge" : c.Edge->Name,
                           "closed shell edge radial degree != 2 (got " +
                               std::to_string(c.Edge->Radial.size()) + ")");
            }
          });
        }
      }
    }
  }

  // Geometry proximity: edge endpoints vs vertices
  for (const Shell* shell : body.Shells)
  {
    if (!shell) continue;
    for (const Face* face : shell->Faces)
    {
      if (!face) continue;
      for (const Loop* loop : face->Loops)
      {
        if (!loop) continue;
        loop->ForEachCoedge([&](const CoEdge& c)
        {
          if (!c.Edge || !c.Edge->Curve || !c.Edge->V0 || !c.Edge->V1) return;
          const Edge& e = *c.Edge;
          const Point3d p0 = e.Curve->Eval(e.T0);
          const Point3d p1 = e.Curve->Eval(e.T1);
          const double tol =
              std::max(e.Tolerance, std::max(e.V0->Tolerance, e.V1->Tolerance));
          if (p0.distance_to(e.V0->Position()) > tol * 10)
          {
            report.Warning(e.Name.empty() ? "Edge" : e.Name,
                           "curve(t0) far from v0");
          }
          if (p1.distance_to(e.V1->Position()) > tol * 10)
          {
            report.Warning(e.Name.empty() ? "Edge" : e.Name,
                           "curve(t1) far from v1");
          }
        });
      }
    }
  }

  if (report.Ok())
  {
    BREP_INFO("ValidateBody '{}': OK ({} issues)", body.Name,
              report.Issues.size());
  }
  else
  {
    BREP_ERROR("ValidateBody '{}': FAILED ({} issues)", body.Name,
               report.Issues.size());
  }
  return report;
}

}  // namespace brep
