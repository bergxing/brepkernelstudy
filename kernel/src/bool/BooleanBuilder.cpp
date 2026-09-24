#include "brep/bool/BooleanBuilder.h"

#include "brep/bool/TopologyCopy.h"
#include "brep/internal/Fuzzy.h"
#include "brep/Validate.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace brep::boolean
{
namespace
{

struct QuantKey
{
    long long x;
    long long y;
    long long z;

    bool operator==(const QuantKey& other) const = default;
};

struct QuantKeyHash
{
    [[nodiscard]] std::size_t operator()(const QuantKey& key) const noexcept
    {
        const std::size_t hx = static_cast<std::size_t>(key.x);
        const std::size_t hy = static_cast<std::size_t>(key.y);
        const std::size_t hz = static_cast<std::size_t>(key.z);
        return hx ^ (hy << 11U) ^ (hz << 22U);
    }
};

[[nodiscard]] QuantKey QuantizePoint(const Point3d& point, double invEps)
{
    return QuantKey{static_cast<long long>(std::llround(point.x() * invEps)),
                    static_cast<long long>(std::llround(point.y() * invEps)),
                    static_cast<long long>(std::llround(point.z() * invEps))};
}

void CollectShellEdges(const Shell& shell, std::vector<Edge*>& edges)
{
    std::unordered_set<Edge*> seen;
    for (const Face* face : shell.Faces)
    {
        if (!face)
        {
            continue;
        }
        for (const Loop* loop : face->Loops)
        {
            if (!loop || !loop->First)
            {
                continue;
            }
            CoEdge* coedge = loop->First;
            std::size_t guard = 0;
            do
            {
                if (++guard > 1024U)
                {
                    break;
                }
                if (coedge->Edge && seen.insert(coedge->Edge).second)
                {
                    edges.push_back(coedge->Edge);
                }
                coedge = coedge->Next;
            } while (coedge && coedge != loop->First);
        }
    }
}

void WeldVertices(Shell& shell, double eps)
{
    const double snapEps = brep::internal::SnapTolerance(eps);
    const double invEps = 1.0 / snapEps;
    std::vector<Edge*> edges;
    CollectShellEdges(shell, edges);

    std::unordered_map<QuantKey, Vertex*, QuantKeyHash> canonical;
    std::unordered_map<Vertex*, Vertex*> remap;

    for (Edge* edge : edges)
    {
        if (!edge)
        {
            continue;
        }
        for (Vertex* vertex : {edge->V0, edge->V1})
        {
            if (!vertex)
            {
                continue;
            }
            const QuantKey key = QuantizePoint(vertex->Position(), invEps);
            const auto found = canonical.find(key);
            if (found == canonical.end())
            {
                canonical.emplace(key, vertex);
            }
            else if (found->second != vertex)
            {
                remap.emplace(vertex, found->second);
            }
        }
    }

    for (Edge* edge : edges)
    {
        if (!edge)
        {
            continue;
        }
        if (Vertex* mapped = remap.count(edge->V0) ? remap[edge->V0] : nullptr)
        {
            edge->V0 = mapped;
        }
        if (Vertex* mapped = remap.count(edge->V1) ? remap[edge->V1] : nullptr)
        {
            edge->V1 = mapped;
        }
    }
}

struct GeometricEdgeKey
{
    QuantKey a;
    QuantKey b;
    QuantKey mid;

    bool operator==(const GeometricEdgeKey& other) const = default;

    bool operator<(const GeometricEdgeKey& other) const noexcept
    {
        if (a.x != other.a.x)
        {
            return a.x < other.a.x;
        }
        if (a.y != other.a.y)
        {
            return a.y < other.a.y;
        }
        if (a.z != other.a.z)
        {
            return a.z < other.a.z;
        }
        if (b.x != other.b.x)
        {
            return b.x < other.b.x;
        }
        if (b.y != other.b.y)
        {
            return b.y < other.b.y;
        }
        if (b.z != other.b.z)
        {
            return b.z < other.b.z;
        }
        if (mid.x != other.mid.x)
        {
            return mid.x < other.mid.x;
        }
        if (mid.y != other.mid.y)
        {
            return mid.y < other.mid.y;
        }
        return mid.z < other.mid.z;
    }
};

[[nodiscard]] GeometricEdgeKey MakeGeometricKey(const Edge& edge, double invEps)
{
    QuantKey k0 = QuantizePoint(edge.V0->Position(), invEps);
    QuantKey k1 = QuantizePoint(edge.V1->Position(), invEps);
    if (k1.x < k0.x || (k1.x == k0.x && (k1.y < k0.y || (k1.y == k0.y && k1.z < k0.z))))
    {
        std::swap(k0, k1);
    }
    QuantKey mid{};
    if (edge.Curve != nullptr)
    {
        const Point3d midpoint = edge.Curve->Eval(0.5 * (edge.T0 + edge.T1));
        mid = QuantizePoint(midpoint, invEps);
    }
    return GeometricEdgeKey{k0, k1, mid};
}

[[nodiscard]] bool SameGeometricEdge(const Edge& a, const Edge& b, double invEps)
{
    return MakeGeometricKey(a, invEps) == MakeGeometricKey(b, invEps);
}

[[nodiscard]] bool SameGeometricCurve(const Edge& a, const Edge& b,
                                      double eps)
{
    const double snapEps = brep::internal::SnapTolerance(eps);
    if (!a.Curve || !b.Curve || a.Curve->Kind() != b.Curve->Kind() ||
        !a.V0 || !a.V1 || !b.V0 || !b.V1)
    {
        return false;
    }
    const Point3d a0 = a.V0->Position();
    const Point3d a1 = a.V1->Position();
    const Point3d b0 = b.V0->Position();
    const Point3d b1 = b.V1->Position();
    const auto close = [&](const Point3d& p, const Point3d& q)
    {
        return (p - q).norm() <= snapEps;
    };
    const bool bothImprint =
        a.Name.find("_imprint") != std::string::npos &&
        b.Name.find("_imprint") != std::string::npos;
    if (a.Curve->Kind() == CurveKind::Circle && bothImprint)
    {
        const auto& circleA = static_cast<const CircleCurve&>(*a.Curve);
        const auto& circleB = static_cast<const CircleCurve&>(*b.Curve);
        if (!close(circleA.Center(), circleB.Center()) ||
            std::abs(circleA.Radius() - circleB.Radius()) > snapEps ||
            std::abs(std::abs(circleA.Normal().dot(circleB.Normal())) - 1.0) >
                snapEps)
        {
            return false;
        }
        const bool bothClosed = (a.V0 == a.V1) && (b.V0 == b.V1);
        if (bothClosed)
        {
            return true;
        }
        if ((a.V0 == a.V1) != (b.V0 == b.V1))
        {
            return false;
        }
        const bool endsMatch =
            (close(a0, b0) && close(a1, b1)) || (close(a0, b1) && close(a1, b0));
        if (endsMatch)
        {
            return true;
        }
        const Point3d midpointA = circleA.Eval(0.5 * (a.T0 + a.T1));
        const Point3d midpointB = circleB.Eval(0.5 * (b.T0 + b.T1));
        return close(midpointA, midpointB);
    }
    if (!((close(a0, b0) && close(a1, b1)) || (close(a0, b1) && close(a1, b0))))
    {
        return false;
    }
    if (a.Curve->Kind() == CurveKind::Line)
    {
        return true;
    }
    if (a.Curve->Kind() != CurveKind::Circle)
    {
        return false;
    }
    const auto& circleA = static_cast<const CircleCurve&>(*a.Curve);
    const auto& circleB = static_cast<const CircleCurve&>(*b.Curve);
    if (!close(circleA.Center(), circleB.Center()) ||
        std::abs(circleA.Radius() - circleB.Radius()) > snapEps ||
        std::abs(std::abs(circleA.Normal().dot(circleB.Normal())) - 1.0) > snapEps)
    {
        return false;
    }
    if (a.Name.find("_imprint") != std::string::npos &&
        b.Name.find("_imprint") != std::string::npos)
    {
        return true;
    }
    const Point3d midpointA = circleA.Eval(0.5 * (a.T0 + a.T1));
    const Point3d midpointB = circleB.Eval(0.5 * (b.T0 + b.T1));
    return close(midpointA, midpointB);
}

[[nodiscard]] bool ReassignImprintEdgeByGeometry(CoEdge& coedge, Edge& edge,
                                               double eps)
{
    const double snapEps = brep::internal::SnapTolerance(eps);
    const Vertex* from = coedge.From();
    const Vertex* to = coedge.To();
    if (from == nullptr || to == nullptr || edge.V0 == nullptr ||
        edge.V1 == nullptr)
    {
        return false;
    }
    const auto close = [&](const Point3d& p, const Point3d& q)
    {
        return (p - q).norm() <= snapEps;
    };
    const Point3d fromPos = from->Position();
    const Point3d toPos = to->Position();
    const Point3d startPos = edge.V0->Position();
    const Point3d endPos = edge.V1->Position();
    if (close(fromPos, startPos) && close(toPos, endPos))
    {
        coedge.Edge = &edge;
        coedge.Sense = Orientation::Forward;
        return true;
    }
    if (close(fromPos, endPos) && close(toPos, startPos))
    {
        coedge.Edge = &edge;
        coedge.Sense = Orientation::Reversed;
        return true;
    }
    return false;
}

[[nodiscard]] bool ReassignEdgePreservingDirection(CoEdge& coedge, Edge& edge)
{
    Vertex* from = coedge.From();
    Vertex* to = coedge.To();
    if (from == nullptr || to == nullptr)
    {
        return false;
    }
    Orientation sense = coedge.Sense;
    if (edge.Start(sense) != from || edge.End(sense) != to)
    {
        sense = opposite(sense);
        if (edge.Start(sense) != from || edge.End(sense) != to)
        {
            return false;
        }
    }
    coedge.Edge = &edge;
    coedge.Sense = sense;
    return true;
}

[[nodiscard]] bool MergeCoedgeOntoEdge(CoEdge& coedge, Edge& keep, Edge& drop,
                                       double eps)
{
    if (&keep == &drop)
    {
        return true;
    }
    if (!ReassignEdgePreservingDirection(coedge, keep) &&
        !ReassignImprintEdgeByGeometry(coedge, keep, eps))
    {
        return false;
    }
    keep.Radial.push_back(&coedge);
    auto& dropRadial = drop.Radial;
    dropRadial.erase(std::remove(dropRadial.begin(), dropRadial.end(), &coedge),
                     dropRadial.end());
    return true;
}

void RebuildEdgeRadial(Shell& shell)
{
    std::vector<Edge*> edges;
    CollectShellEdges(shell, edges);
    for (Edge* edge : edges)
    {
        if (edge)
        {
            edge->Radial.clear();
        }
    }
    for (Face* face : shell.Faces)
    {
        if (!face)
        {
            continue;
        }
        for (Loop* loop : face->Loops)
        {
            if (!loop || !loop->First)
            {
                continue;
            }
            CoEdge* coedge = loop->First;
            std::size_t guard = 0;
            do
            {
                if (++guard > 1024U)
                {
                    break;
                }
                if (coedge->Edge)
                {
                    coedge->Edge->Radial.push_back(coedge);
                }
                coedge = coedge->Next;
            } while (coedge && coedge != loop->First);
        }
    }
}

[[nodiscard]] bool IsImprintEdge(const Edge& edge)
{
    return edge.Name.find("_imprint") != std::string::npos ||
           edge.Name.find("_circle_imprint") != std::string::npos;
}

void MergeCoincidentEdges(Shell& shell, double eps)
{
    const double snapEps = brep::internal::SnapTolerance(eps);
    const double invEps = 1.0 / snapEps;
    for (int pass = 0; pass < 8; ++pass)
    {
        std::vector<Edge*> edges;
        CollectShellEdges(shell, edges);
        bool mergedAny = false;
        std::map<GeometricEdgeKey, Edge*> primary;
        for (Edge* edge : edges)
        {
            if (!edge || !edge->V0 || !edge->V1 || IsImprintEdge(*edge))
            {
                continue;
            }
            const GeometricEdgeKey key = MakeGeometricKey(*edge, invEps);
            const auto found = primary.find(key);
            if (found == primary.end())
            {
                primary.emplace(key, edge);
                continue;
            }
            Edge* keep = found->second;
            if (IsImprintEdge(*keep))
            {
                continue;
            }
            if (!SameGeometricCurve(*keep, *edge, eps))
            {
                continue;
            }
            if (keep->Radial.size() + edge->Radial.size() > 2U)
            {
                continue;
            }
            for (CoEdge* coedge : edge->Radial)
            {
                if (coedge == nullptr ||
                    !ReassignEdgePreservingDirection(*coedge, *keep))
                {
                    continue;
                }
                keep->Radial.push_back(coedge);
            }
            edge->Radial.clear();
            mergedAny = true;
        }
        if (!mergedAny)
        {
            break;
        }
    }
}

void MergeSingletonCutEdges(Shell& shell, double eps)
{
    const double invEps = 1.0 / eps;
    std::vector<Edge*> edges;
    CollectShellEdges(shell, edges);

    std::map<GeometricEdgeKey, std::vector<Edge*>> singletons;
    for (Edge* edge : edges)
    {
        if (!edge || !edge->V0 || !edge->V1 || edge->Radial.size() != 1U)
        {
            continue;
        }
        singletons[MakeGeometricKey(*edge, invEps)].push_back(edge);
    }

    for (auto& [key, group] : singletons)
    {
        if (group.size() != 2U)
        {
            continue;
        }
        Edge* keep = group[0];
        Edge* other = group[1];
        if (keep == other || !SameGeometricCurve(*keep, *other, eps))
        {
            continue;
        }
        CoEdge* coedgeA = keep->Radial.empty() ? nullptr : keep->Radial[0];
        CoEdge* coedgeB = other->Radial.empty() ? nullptr : other->Radial[0];
        if (coedgeA == nullptr || coedgeB == nullptr || coedgeA->Partner != nullptr ||
            coedgeB->Partner != nullptr)
        {
            continue;
        }
        if (!MergeCoedgeOntoEdge(*coedgeB, *keep, *other, eps))
        {
            continue;
        }
        Model::PairPartners(coedgeA, coedgeB);
        (void)key;
    }
}

void PairUnpartneredByGeometry(Shell& shell, double eps)
{
    const double invEps = 1.0 / eps;
    std::vector<CoEdge*> unpartnered;
    for (Face* face : shell.Faces)
    {
        if (!face)
        {
            continue;
        }
        for (Loop* loop : face->Loops)
        {
            if (!loop || !loop->First)
            {
                continue;
            }
            CoEdge* coedge = loop->First;
            std::size_t guard = 0;
            do
            {
                if (++guard > 1024U)
                {
                    break;
                }
                if (coedge->Partner == nullptr && coedge->Edge != nullptr &&
                    coedge->Edge->V0 != nullptr && coedge->Edge->V1 != nullptr)
                {
                    unpartnered.push_back(coedge);
                }
                coedge = coedge->Next;
            } while (coedge && coedge != loop->First);
        }
    }

    std::map<GeometricEdgeKey, std::vector<CoEdge*>> groups;
    for (CoEdge* coedge : unpartnered)
    {
        groups[MakeGeometricKey(*coedge->Edge, invEps)].push_back(coedge);
    }

    for (auto& [key, coedges] : groups)
    {
        const auto tryPair = [&](CoEdge* coedgeA, CoEdge* coedgeB) -> bool
        {
            if (coedgeA == nullptr || coedgeB == nullptr ||
                coedgeA->Partner != nullptr || coedgeB->Partner != nullptr)
            {
                return false;
            }
            Edge* edgeA = coedgeA->Edge;
            Edge* edgeB = coedgeB->Edge;
            if (edgeA == nullptr || edgeB == nullptr)
            {
                return false;
            }
            if (!SameGeometricCurve(*edgeA, *edgeB, eps))
            {
                return false;
            }
            if (edgeA != edgeB)
            {
                if (!MergeCoedgeOntoEdge(*coedgeB, *edgeA, *edgeB, eps))
                {
                    return false;
                }
            }
            Model::PairPartners(coedgeA, coedgeB);
            return true;
        };

        for (std::size_t i = 0; i < coedges.size(); ++i)
        {
            CoEdge* coedgeA = coedges[i];
            if (coedgeA == nullptr || coedgeA->Partner != nullptr)
            {
                continue;
            }
            std::size_t match = coedges.size();
            for (std::size_t j = i + 1; j < coedges.size(); ++j)
            {
                CoEdge* coedgeB = coedges[j];
                if (coedgeB == nullptr || coedgeB->Partner != nullptr)
                {
                    continue;
                }
                if (coedgeA->GetFace() == coedgeB->GetFace())
                {
                    continue;
                }
                if (SameGeometricCurve(*coedgeA->Edge, *coedgeB->Edge, eps))
                {
                    match = j;
                    break;
                }
            }
            if (match == coedges.size())
            {
                for (std::size_t j = i + 1; j < coedges.size(); ++j)
                {
                    CoEdge* coedgeB = coedges[j];
                    if (coedgeB == nullptr || coedgeB->Partner != nullptr)
                    {
                        continue;
                    }
                    if (SameGeometricCurve(*coedgeA->Edge, *coedgeB->Edge, eps))
                    {
                        match = j;
                        break;
                    }
                }
            }
            if (match < coedges.size())
            {
                (void)tryPair(coedgeA, coedges[match]);
            }
        }
        (void)key;
    }
}

void PairManifoldCoedges(Shell& shell)
{
    std::vector<Edge*> edges;
    CollectShellEdges(shell, edges);
    for (Edge* edge : edges)
    {
        if (!edge || edge->Radial.size() != 2U)
        {
            continue;
        }
        CoEdge* first = edge->Radial[0];
        CoEdge* second = edge->Radial[1];
        if (!first || !second || first->Partner || second->Partner)
        {
            continue;
        }
        Model::PairPartners(first, second);
    }
}

void PairCircleImprintContacts(Shell& shell, double eps)
{
    const double imprintEps = std::max(eps, 1e-3);
    std::vector<CoEdge*> contacts;
    for (Face* face : shell.Faces)
    {
        if (face == nullptr)
        {
            continue;
        }
        for (Loop* loop : face->Loops)
        {
            if (loop == nullptr)
            {
                continue;
            }
            loop->ForEachCoedge([&](CoEdge& coedge)
            {
                if (coedge.Partner != nullptr || coedge.Edge == nullptr)
                {
                    return;
                }
                const bool imprintContact =
                    coedge.Name.find("_circle_imprint") != std::string::npos ||
                    coedge.Name.find("_sphere_imprint") != std::string::npos;
                if (!imprintContact)
                {
                    return;
                }
                contacts.push_back(&coedge);
            });
        }
    }

    const auto tryPair = [&](CoEdge* coedgeA, CoEdge* coedgeB) -> bool
    {
        if (coedgeA == nullptr || coedgeB == nullptr || coedgeA == coedgeB ||
            coedgeA->Partner != nullptr || coedgeB->Partner != nullptr)
        {
            return false;
        }
        if (coedgeA->GetFace() == coedgeB->GetFace())
        {
            return false;
        }
        Edge* edgeA = coedgeA->Edge;
        Edge* edgeB = coedgeB->Edge;
        if (edgeA == nullptr || edgeB == nullptr)
        {
            return false;
        }
        if (!SameGeometricCurve(*edgeA, *edgeB, imprintEps))
        {
            return false;
        }
        Edge* keep = edgeA;
        Edge* drop = edgeB;
        CoEdge* moving = coedgeB;
        const auto isPlanarImprint = [](const CoEdge& coedge) -> bool
        {
            return coedge.Name.find("_circle_imprint") != std::string::npos;
        };
        const auto isOpenEdge = [](const Edge& edge) -> bool
        {
            return edge.V0 != nullptr && edge.V1 != nullptr && edge.V0 != edge.V1;
        };
        if ((isPlanarImprint(*coedgeB) && !isPlanarImprint(*coedgeA)) ||
            (isOpenEdge(*edgeB) && !isOpenEdge(*edgeA)))
        {
            keep = edgeB;
            drop = edgeA;
            moving = coedgeA;
        }
        if (keep != drop &&
            !MergeCoedgeOntoEdge(*moving, *keep, *drop, imprintEps))
        {
            return false;
        }
        if (coedgeA->Edge == nullptr || coedgeA->Edge != coedgeB->Edge)
        {
            return false;
        }
        Model::PairPartners(coedgeA, coedgeB);
        return true;
    };

    for (std::size_t i = 0; i < contacts.size(); ++i)
    {
        CoEdge* coedgeA = contacts[i];
        if (coedgeA == nullptr || coedgeA->Partner != nullptr)
        {
            continue;
        }
        for (std::size_t j = i + 1U; j < contacts.size(); ++j)
        {
            if (tryPair(coedgeA, contacts[j]))
            {
                break;
            }
        }
    }
}

void RepairLoopVertexGaps(Shell& shell, double eps)
{
    const double snapEps = brep::internal::SnapTolerance(eps);
    std::vector<Edge*> edges;
    CollectShellEdges(shell, edges);

    const auto mergeVertices = [&](Vertex* keep, Vertex* drop)
    {
        if (keep == nullptr || drop == nullptr || keep == drop)
        {
            return;
        }
        for (Edge* edge : edges)
        {
            if (edge == nullptr)
            {
                continue;
            }
            if (edge->V0 == drop)
            {
                edge->V0 = keep;
            }
            if (edge->V1 == drop)
            {
                edge->V1 = keep;
            }
        }
    };

    for (Face* face : shell.Faces)
    {
        if (face == nullptr)
        {
            continue;
        }
        for (Loop* loop : face->Loops)
        {
            if (loop == nullptr)
            {
                continue;
            }
            loop->ForEachCoedge([&](CoEdge& coedge)
            {
                if (coedge.Next == nullptr || coedge.To() == nullptr ||
                    coedge.Next->From() == nullptr)
                {
                    return;
                }
                Vertex* tail = coedge.To();
                Vertex* head = coedge.Next->From();
                if (tail != head && brep::internal::PointsEqual(tail->Position(),
                                                                head->Position(),
                                                                snapEps))
                {
                    mergeVertices(tail, head);
                }
            });
        }
    }
}

}  // namespace

void WeldShellVertices(Shell& shell, double eps)
{
    RebuildEdgeRadial(shell);
    WeldVertices(shell, eps);
    RebuildEdgeRadial(shell);
}

void StitchCopiedShell(Shell& shell, double eps)
{
    const double weldEps = std::max(eps, 1e-5);
    RebuildEdgeRadial(shell);
    WeldVertices(shell, weldEps);
    RepairLoopVertexGaps(shell, weldEps);
    WeldVertices(shell, weldEps);
    RepairLoopVertexGaps(shell, weldEps);
    RebuildEdgeRadial(shell);
    MergeCoincidentEdges(shell, eps);
    MergeSingletonCutEdges(shell, eps);
    PairUnpartneredByGeometry(shell, eps);
    PairCircleImprintContacts(shell, eps);
    RepairLoopVertexGaps(shell, std::max(eps, 1e-3));
    WeldVertices(shell, weldEps);
    RebuildEdgeRadial(shell);
    PairManifoldCoedges(shell);
}

BooleanBuildResult BuildBooleanBody(Model& model, BooleanOp /*op*/,
                                    const FaceSelection& selection,
                                    const std::string& name)
{
    BooleanBuildResult result;
    if (selection.FromA.empty() && selection.FromB.empty())
    {
        result.Diagnostics = "BuildBooleanBody: empty selection";
        return result;
    }

    std::unordered_set<const Face*> selectedFaces;
    selectedFaces.reserve(selection.FromA.size() + selection.FromB.size());
    for (Face* face : selection.FromA)
    {
        if (face != nullptr)
        {
            selectedFaces.insert(face);
        }
    }
    for (Face* face : selection.FromB)
    {
        if (face != nullptr)
        {
            selectedFaces.insert(face);
        }
    }

    TopologyCopyContext copyCtx{model};
    Body* body = model.MakeBody(BodyType::Solid, name);
    Shell* shell = model.MakeShell(true, name + "_shell");
    body->Shells.push_back(shell);

    for (Face* face : selection.FromA)
    {
        if (!face)
        {
            continue;
        }
        Face* copied = CopyFaceSubgraph(copyCtx, *face, false, &selectedFaces);
        if (!copied)
        {
            result.Diagnostics = "BuildBooleanBody: failed to copy face from A";
            model.RemoveBody(body->Guid);
            return result;
        }
        shell->Faces.push_back(copied);
    }
    for (Face* face : selection.FromB)
    {
        if (!face)
        {
            continue;
        }
        Face* copied = CopyFaceSubgraph(copyCtx, *face, selection.ReverseB, &selectedFaces);
        if (!copied)
        {
            result.Diagnostics = "BuildBooleanBody: failed to copy face from B";
            model.RemoveBody(body->Guid);
            return result;
        }
        shell->Faces.push_back(copied);
    }

    PairAllCopiedCoedgePartners(copyCtx);

    const double eps = 1e-7;
    StitchCopiedShell(*shell, eps);

    const ValidationReport report = ValidateBody(*body);
    if (!report.Ok())
    {
        const SeamExpansionReport seam = ReportSeamExpansion(selection);
        const ImprintCopyPairingReport pairing =
            ReportImprintCopyPairing(selection, copyCtx);
        std::ostringstream diag;
        diag << "BuildBooleanBody: ValidateBody failed ("
             << report.Issues.size() << " issues); "
             << "imprint pair bothSelected=" << pairing.BothSelectedImprintCoedges
             << " mapped=" << pairing.MappedBothEnds
             << " paired=" << pairing.PairedCopiedCoedges
             << " edgeMismatch=" << pairing.EdgeMismatch
             << "; seam unselected=" << seam.PartnerUnselected
             << " expanded=" << seam.Expanded
             << " fallback=" << seam.CopiedImprintFallback;
        for (const ValidationIssue& issue : report.Issues)
        {
            if (issue.Severity != ValidationIssue::IssueSeverity::Error)
            {
                continue;
            }
            diag << "; [" << issue.Where << "] " << issue.Message;
        }
        result.Diagnostics = diag.str();
        model.RemoveBody(body->Guid);
        return result;
    }

    result.OutputBody = body;
    return result;
}

}  // namespace brep::boolean
