#include "brep/asm/Assembly.h"

#include <cmath>

namespace brep::asm_
{

OccurrenceId Assembly::AddOccurrence(Guid partGuid, RigidTransform xf,
                                     std::string name)
{
    Occurrence occ;
    occ.Id.Guid = Guid::Generate();
    occ.PartGuid = partGuid;
    occ.Transform = xf;
    occ.Name = std::move(name);
    const OccurrenceId id = occ.Id;
    m_occurrences.push_back(std::move(occ));
    return id;
}

Guid Assembly::AddMate(Mate mate)
{
    if (!mate.Id.IsValid()) mate.Id = Guid::Generate();
    const Guid id = mate.Id;
    m_mates.push_back(std::move(mate));
    return id;
}

Occurrence* Assembly::Find(OccurrenceId id)
{
    for (auto& o : m_occurrences)
    {
        if (o.Id == id) return &o;
    }
    return nullptr;
}

const Occurrence* Assembly::Find(OccurrenceId id) const
{
    for (const auto& o : m_occurrences)
    {
        if (o.Id == id) return &o;
    }
    return nullptr;
}

solve2d::SolveReport MateSolver::Solve(Assembly& assembly,
                                       param::ParameterStore* params)
{
    solve2d::SolveReport report;
    report.Status = solve2d::SolveStatus::Solved;
    report.Message = "ok";

    // V1: treat first occurrence as fixed ground; apply mates by copying /
    // offsetting translation of the second occurrence.
    if (assembly.Occurrences().empty())
    {
        report.Status = solve2d::SolveStatus::Solved;
        return report;
    }

    for (const auto& mate : assembly.Mates())
    {
        Occurrence* a = assembly.Find(mate.A);
        Occurrence* b = assembly.Find(mate.B);
        if (!a || !b)
        {
            report.Status = solve2d::SolveStatus::Failed;
            report.Message = "mate references missing occurrence";
            return report;
        }

        switch (mate.Kind)
        {
        case MateKind::Coincident:
        case MateKind::Parallel:
            // Align B origin to A origin (simplified face mate).
            b->Transform.Translation = a->Transform.Translation;
            b->Transform.XAxis = a->Transform.XAxis;
            b->Transform.YAxis = a->Transform.YAxis;
            b->Transform.ZAxis = a->Transform.ZAxis;
            break;
        case MateKind::Distance: {
            double dist = mate.Aux;
            if (params && mate.Dim.Guid.IsValid())
            {
                if (auto v = params->Get(mate.Dim)) dist = *v;
            }
            b->Transform.Translation =
                a->Transform.Translation + a->Transform.ZAxis * dist;
            break;
        }
        case MateKind::Angle:
            // Rotate B around A.z by aux radians (simple).
            {
                const double ang = mate.Aux;
                const double c = std::cos(ang);
                const double s = std::sin(ang);
                const Vector3d x = a->Transform.XAxis;
                const Vector3d y = a->Transform.YAxis;
                b->Transform.XAxis = x * c + y * s;
                b->Transform.YAxis = y * c - x * s;
                b->Transform.ZAxis = a->Transform.ZAxis;
                b->Transform.Translation = a->Transform.Translation;
            }
            break;
        case MateKind::Concentric:
            b->Transform.Translation = a->Transform.Translation;
            b->Transform.ZAxis = a->Transform.ZAxis;
            break;
        }
    }

    report.Dof = static_cast<int>(assembly.Occurrences().size());
    return report;
}

}  // namespace brep::asm_
