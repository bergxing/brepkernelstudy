#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/Mesh.h"
#include "brep/ops/Profile.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <ostream>
#include <string>
#include <vector>

namespace brep
{
namespace
{

// Prism × sphere matrix — see
// docs/superpowers/specs/2026-09-18-prism-sphere-boolean-test-matrix.md
// Do not delete a red case. Expect*=false must carry SkipReason.

enum class ProfileKind
{
    Rect,
    Regular,
    Explicit,
};

enum class PoseClass
{
    Legacy,
    ThroughCentered,
    TopBite,
    SideBite,
    EdgeBite,
    VertexNick,
    Contains,
    IrregularFile,
};

enum class SphereOffset
{
    Center,
    TowardSide,
    TowardVertex,
};

constexpr double kPhaseAHeight = 1.2;
constexpr double kPhaseACircum = 2.0;

struct PadSphereSizeCase
{
    const char* Name{};
    ProfileKind Profile{ProfileKind::Rect};
    int Sides{4};
    PoseClass Pose{PoseClass::Legacy};
    double Width{2.0};
    double Depth{2.0};
    double Height{1.0};
    double Radius{2.0};
    std::vector<Point2d> ProfileUv{};
    Point3d SphereCenter{1.0, 0.5, 1.0};
    double SphereRadius{0.8};
    bool ExpectUnion{true};
    bool ExpectIntersect{true};
    bool ExpectPadMinusSphere{true};
    bool ExpectSphereMinusPad{true};
    const char* SkipReason{};
};

void PrintTo(const PadSphereSizeCase& value, std::ostream* out)
{
    *out << value.Name;
}

[[nodiscard]] Point2d RegularUv(int sides, double radius, int index)
{
    const double angle = 2.0 * std::numbers::pi * static_cast<double>(index) /
                         static_cast<double>(sides);
    return Point2d{radius * std::cos(angle), radius * std::sin(angle)};
}

[[nodiscard]] Body* MakeRectPad(Model& model, const PadSphereSizeCase& spec)
{
    ops::ExtrudeSpec extrude;
    extrude.Name = "pad";
    extrude.Plane = Plane::XzYUp();
    extrude.Distance = spec.Height;
    extrude.Profile.Outer = {
        Point2d{0.0, 0.0},
        Point2d{spec.Width, 0.0},
        Point2d{spec.Width, spec.Depth},
        Point2d{0.0, spec.Depth},
    };
    return ops::Extrude(model, extrude);
}

[[nodiscard]] Body* MakeRegularPrism(Model& model,
                                     const PadSphereSizeCase& spec)
{
    ops::ExtrudeSpec extrude;
    extrude.Name = "Pad";
    extrude.Plane = Plane::XzYUp();
    extrude.Distance = spec.Height;
    extrude.Profile.Outer.reserve(static_cast<std::size_t>(spec.Sides));
    for (int i = 0; i < spec.Sides; ++i)
    {
        extrude.Profile.Outer.push_back(
            RegularUv(spec.Sides, spec.Radius, i));
    }
    return ops::Extrude(model, extrude);
}

[[nodiscard]] Body* MakeExplicitPrism(Model& model,
                                      const PadSphereSizeCase& spec)
{
    ops::ExtrudeSpec extrude;
    extrude.Name = "Pad";
    extrude.Plane = Plane::XzYUp();
    extrude.Distance = spec.Height;
    extrude.Profile.Outer = spec.ProfileUv;
    return ops::Extrude(model, extrude);
}

[[nodiscard]] Body* MakePad(Model& model, const PadSphereSizeCase& spec)
{
    if (spec.Profile == ProfileKind::Regular)
    {
        return MakeRegularPrism(model, spec);
    }
    if (spec.Profile == ProfileKind::Explicit)
    {
        return MakeExplicitPrism(model, spec);
    }
    return MakeRectPad(model, spec);
}

[[nodiscard]] double DefaultSphereRadius(PoseClass pose)
{
    switch (pose)
    {
    case PoseClass::ThroughCentered:
        return 0.7;
    case PoseClass::TopBite:
    case PoseClass::SideBite:
        return 0.55;
    case PoseClass::EdgeBite:
    case PoseClass::VertexNick:
        return 0.45;
    case PoseClass::Contains:
        return 2.5;
    case PoseClass::Legacy:
    case PoseClass::IrregularFile:
        return 0.7;
    }
    return 0.7;
}

[[nodiscard]] PadSphereSizeCase MakeRegularPose(
    const char* name, int sides, PoseClass pose,
    double height = kPhaseAHeight, double sphereRadius = -1.0,
    SphereOffset offset = SphereOffset::Center)
{
    PadSphereSizeCase spec;
    spec.Name = name;
    spec.Profile = ProfileKind::Regular;
    spec.Sides = sides;
    spec.Pose = pose;
    spec.Height = height;
    spec.Radius = kPhaseACircum;

    const Point2d v0 = RegularUv(sides, kPhaseACircum, 0);
    const Point2d v1 = RegularUv(sides, kPhaseACircum, 1);
    const double midU = 0.5 * (v0.u() + v1.u());
    const double midV = 0.5 * (v0.v() + v1.v());
    const double midLen = std::hypot(midU, midV);
    const double radius =
        sphereRadius > 0.0 ? sphereRadius : DefaultSphereRadius(pose);

    auto shiftXz = [&](double u, double v) -> Point3d
    {
        if (offset == SphereOffset::TowardSide && midLen > 0.0)
        {
            return Point3d{u + 0.45 * midU / midLen, 0.0,
                           v + 0.45 * midV / midLen};
        }
        if (offset == SphereOffset::TowardVertex)
        {
            return Point3d{u + 0.70 * v0.u() / kPhaseACircum, 0.0,
                           v + 0.70 * v0.v() / kPhaseACircum};
        }
        return Point3d{u, 0.0, v};
    };

    switch (pose)
    {
    case PoseClass::ThroughCentered:
    {
        spec.SphereRadius = radius;
        const Point3d xz = shiftXz(0.0, 0.0);
        spec.SphereCenter = Point3d{xz.x(), 0.5 * height, xz.z()};
        const double halfH = 0.5 * height;
        const double vertexDist =
            std::sqrt(kPhaseACircum * kPhaseACircum + halfH * halfH);
        if (radius + 1.0e-9 >= vertexDist)
        {
            spec.Pose = PoseClass::Contains;
            spec.ExpectPadMinusSphere = false;
            spec.SkipReason = "EmptyCsg";
        }
        break;
    }
    case PoseClass::TopBite:
    {
        spec.SphereRadius = radius;
        const Point3d xz = shiftXz(0.0, 0.0);
        spec.SphereCenter = Point3d{xz.x(), height + 0.15, xz.z()};
        break;
    }
    case PoseClass::SideBite:
    {
        spec.SphereRadius = radius;
        const double scale = (midLen + 0.15) / midLen;
        spec.SphereCenter =
            Point3d{midU * scale, 0.5 * height, midV * scale};
        break;
    }
    case PoseClass::EdgeBite:
        spec.SphereRadius = radius;
        spec.SphereCenter =
            Point3d{v0.u() + 0.20, 0.5 * height, v0.v() + 0.12};
        break;
    case PoseClass::VertexNick:
        spec.SphereRadius = radius;
        spec.SphereCenter = Point3d{v0.u(), 0.5 * height, v0.v()};
        break;
    case PoseClass::IrregularFile:
        break;
    case PoseClass::Contains:
        spec.Height = height > 0.0 ? height : 0.8;
        if (sphereRadius <= 0.0)
        {
            spec.Height = 0.8;
            spec.Radius = 1.0;
            spec.SphereCenter = Point3d{0.0, 0.4, 0.0};
            spec.SphereRadius = 2.5;
        }
        else
        {
            spec.SphereRadius = radius;
            spec.SphereCenter = Point3d{0.0, 0.5 * spec.Height, 0.0};
        }
        spec.ExpectPadMinusSphere = false;
        spec.SkipReason = "EmptyCsg";
        break;
    case PoseClass::Legacy:
        break;
    }
    return spec;
}

[[nodiscard]] Point2d PolygonCentroid(const std::vector<Point2d>& uv)
{
    double sumU = 0.0;
    double sumV = 0.0;
    for (const Point2d& p : uv)
    {
        sumU += p.u();
        sumV += p.v();
    }
    const double n = static_cast<double>(uv.size());
    return Point2d{sumU / n, sumV / n};
}

// untitled.xl Pad 11de7787… / sphere 0a680dea… (irregular hex, not regular).
[[nodiscard]] std::vector<Point2d> UntitledHexUv()
{
    return {
        Point2d{-1.6271359128663692, -0.73192507438205467},
        Point2d{-1.2165929224231791, -1.6303806780834618},
        Point2d{-3.8798258963231582, -0.94253380167824963},
        Point2d{-5.5618712318402643, 1.1250780853602658},
        Point2d{-3.2462516583975134, 1.7636817815004244},
        Point2d{-2.4814244712129527, 0.31253090872995548},
    };
}

[[nodiscard]] std::vector<Point2d> TrapezoidUv()
{
    return {
        Point2d{0.0, 0.0},
        Point2d{3.0, 0.0},
        Point2d{2.2, 1.8},
        Point2d{0.6, 1.8},
    };
}

[[nodiscard]] std::vector<Point2d> IrregularPentUv()
{
    std::vector<Point2d> uv;
    uv.reserve(5U);
    for (int i = 0; i < 5; ++i)
    {
        uv.push_back(RegularUv(5, kPhaseACircum, i));
    }
    uv[0] = Point2d{2.55, 0.18};
    uv[2] = Point2d{uv[2].u() * 0.72, uv[2].v() * 0.72};
    return uv;
}

[[nodiscard]] PadSphereSizeCase MakeExplicitPose(const char* name,
                                                 std::vector<Point2d> uv,
                                                 PoseClass pose,
                                                 double height = kPhaseAHeight,
                                                 double sphereRadius = -1.0)
{
    PadSphereSizeCase spec;
    spec.Name = name;
    spec.Profile = ProfileKind::Explicit;
    spec.Sides = static_cast<int>(uv.size());
    spec.Pose = pose;
    spec.Height = height;
    spec.ProfileUv = std::move(uv);
    const double radius =
        sphereRadius > 0.0 ? sphereRadius : DefaultSphereRadius(pose);
    spec.SphereRadius = radius;

    const Point2d centroid = PolygonCentroid(spec.ProfileUv);
    switch (pose)
    {
    case PoseClass::ThroughCentered:
        spec.SphereCenter =
            Point3d{centroid.u(), 0.5 * height, centroid.v()};
        break;
    case PoseClass::TopBite:
        spec.SphereCenter =
            Point3d{centroid.u(), height + 0.15, centroid.v()};
        break;
    case PoseClass::SideBite:
    {
        const Point2d a = spec.ProfileUv.front();
        const Point2d b = spec.ProfileUv[1];
        const double midU = 0.5 * (a.u() + b.u());
        const double midV = 0.5 * (a.v() + b.v());
        double outU = -(b.v() - a.v());
        double outV = b.u() - a.u();
        if (outU * (midU - centroid.u()) + outV * (midV - centroid.v()) <
            0.0)
        {
            outU = -outU;
            outV = -outV;
        }
        const double len = std::hypot(outU, outV);
        spec.SphereCenter = Point3d{midU + 0.15 * outU / len, 0.5 * height,
                                    midV + 0.15 * outV / len};
        break;
    }
    default:
        spec.SphereCenter =
            Point3d{centroid.u(), 0.5 * height, centroid.v()};
        break;
    }
    return spec;
}

[[nodiscard]] std::size_t CountSphereTriangles(const Body& body)
{
    std::size_t triangles = 0;
    for (const Shell* shell : body.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* face : shell->Faces)
        {
            if (face == nullptr || face->Surface == nullptr ||
                face->Surface->Kind() != SurfaceKind::Sphere)
            {
                continue;
            }
            TriangleMesh mesh;
            TessellateFace(*face, mesh);
            triangles += mesh.Indices.size() / 3U;
        }
    }
    return triangles;
}

[[nodiscard]] std::size_t CountSphereFaces(const Body& body)
{
    std::size_t faces = 0;
    for (const Shell* shell : body.Shells)
    {
        if (shell == nullptr)
        {
            continue;
        }
        for (const Face* face : shell->Faces)
        {
            if (face != nullptr && face->Surface != nullptr &&
                face->Surface->Kind() == SurfaceKind::Sphere)
            {
                ++faces;
            }
        }
    }
    return faces;
}

void ExpectValidBoolean(boolean::BooleanOp op, Model& model, const Body& a,
                        const Body& b, const char* label,
                        bool checkSphereMesh)
{
    boolean::BooleanContext ctx;
    ctx.AllowOperandSwap = false;
    auto eval = boolean::MakeDefaultBooleanEvaluator();
    const auto result = eval->Evaluate(op, model, a, b, ctx);
    ASSERT_TRUE(result.Ok()) << label << ": " << result.Diagnostics;
    ASSERT_NE(result.OutputBody, nullptr) << label;
    const ValidationReport report = ValidateBody(*result.OutputBody);
    if (!report.Ok())
    {
        std::string issues;
        for (const ValidationIssue& issue : report.Issues)
        {
            if (issue.Severity == ValidationIssue::IssueSeverity::Error)
            {
                issues += issue.Where + ": " + issue.Message + "\n";
            }
        }
        ADD_FAILURE() << label << ":\n" << issues;
    }
    if (checkSphereMesh && CountSphereFaces(*result.OutputBody) > 0U)
    {
        EXPECT_GT(CountSphereTriangles(*result.OutputBody), 0U)
            << label << ": sphere contact faces have no triangles";
    }
}

class PadSphereSizeSweep : public ::testing::TestWithParam<PadSphereSizeCase>
{
};

TEST_P(PadSphereSizeSweep, UnionIntersectSubtractValidate)
{
    const PadSphereSizeCase spec = GetParam();
    SCOPED_TRACE(spec.Name);

    const bool anySkip = !spec.ExpectUnion || !spec.ExpectIntersect ||
                         !spec.ExpectPadMinusSphere ||
                         !spec.ExpectSphereMinusPad;
    if (anySkip)
    {
        ASSERT_TRUE(spec.SkipReason != nullptr && spec.SkipReason[0] != '\0')
            << spec.Name
            << ": Expect*=false requires SkipReason "
               "(EmptyCsg or KNOWN_GAP:id)";
    }

    Model model;
    Body* pad = MakePad(model, spec);
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = spec.SphereCenter,
                          .Radius = spec.SphereRadius,
                          .Name = "sphere"});
    ASSERT_NE(pad, nullptr);
    ASSERT_NE(sphere, nullptr);

    if (spec.ExpectUnion)
    {
        ExpectValidBoolean(boolean::BooleanOp::Union, model, *pad, *sphere,
                           "union pad∪sphere", false);
    }
    if (spec.ExpectIntersect)
    {
        ExpectValidBoolean(boolean::BooleanOp::Intersect, model, *pad, *sphere,
                           "intersect pad∩sphere", false);
    }
    if (spec.ExpectPadMinusSphere)
    {
        ExpectValidBoolean(boolean::BooleanOp::Subtract, model, *pad, *sphere,
                           "subtract pad−sphere", true);
    }
    if (spec.ExpectSphereMinusPad)
    {
        ExpectValidBoolean(boolean::BooleanOp::Subtract, model, *sphere, *pad,
                           "subtract sphere−pad", false);
    }
}

constexpr double kHexH = 1.2315022069431985;

INSTANTIATE_TEST_SUITE_P(
    Configs, PadSphereSizeSweep,
    ::testing::Values(
        PadSphereSizeCase{
            .Name = "RectThroughBaseline",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {1.0, 0.5, 1.0},
            .SphereRadius = 0.8,
        },
        PadSphereSizeCase{
            .Name = "RectThroughThin",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 0.45,
            .SphereCenter = {1.0, 0.225, 1.0},
            .SphereRadius = 0.5,
        },
        PadSphereSizeCase{
            .Name = "RectThroughTaller",
            .Profile = ProfileKind::Rect,
            .Width = 3.0,
            .Depth = 3.0,
            .Height = 2.4,
            .SphereCenter = {1.5, 1.2, 1.5},
            .SphereRadius = 1.35,
        },
        PadSphereSizeCase{
            .Name = "RectBuriedSphere",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {1.0, 0.5, 1.0},
            .SphereRadius = 0.35,
            .ExpectSphereMinusPad = false,
            .SkipReason = "EmptyCsg",
        },
        PadSphereSizeCase{
            .Name = "RectThroughLarge",
            .Profile = ProfileKind::Rect,
            .Width = 3.0,
            .Depth = 3.0,
            .Height = 1.0,
            .SphereCenter = {1.5, 0.5, 1.5},
            .SphereRadius = 1.2,
        },
        PadSphereSizeCase{
            .Name = "RectThroughOffset",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {1.55, 0.5, 1.55},
            .SphereRadius = 0.6,
        },
        PadSphereSizeCase{
            .Name = "RectThroughThickTangent",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 2.4,
            .SphereCenter = {1.0, 1.2, 1.0},
            .SphereRadius = 1.0,
            // Sphere is inside the pad and tangent to four walls.
            .ExpectSphereMinusPad = false,
            .SkipReason = "EmptyCsg",
        },
        PadSphereSizeCase{
            .Name = "RectTopCapBite",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {1.0, 1.15, 1.0},
            .SphereRadius = 0.55,
        },
        PadSphereSizeCase{
            .Name = "RectBottomCapBite",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {1.0, -0.15, 1.0},
            .SphereRadius = 0.55,
        },
        PadSphereSizeCase{
            .Name = "RectSideBite",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {-0.15, 0.5, 1.0},
            .SphereRadius = 0.55,
        },
        PadSphereSizeCase{
            .Name = "RectCornerBite",
            .Profile = ProfileKind::Rect,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {-0.2, -0.2, -0.2},
            .SphereRadius = 0.7,
        },
        PadSphereSizeCase{
            .Name = "RectEdgeBite",
            .Profile = ProfileKind::Rect,
            .Pose = PoseClass::EdgeBite,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {-0.15, 0.5, 0.12},
            .SphereRadius = 0.45,
        },
        PadSphereSizeCase{
            .Name = "RectVertexNick",
            .Profile = ProfileKind::Rect,
            .Pose = PoseClass::VertexNick,
            .Width = 2.0,
            .Depth = 2.0,
            .Height = 1.0,
            .SphereCenter = {0.0, 0.5, 0.0},
            .SphereRadius = 0.45,
        },
        PadSphereSizeCase{
            .Name = "HexUntitledLike",
            .Profile = ProfileKind::Regular,
            .Sides = 6,
            .Height = kHexH,
            .Radius = 2.0,
            .SphereCenter = {-1.76672, 1.2315, -0.954155},
            .SphereRadius = 2.53484,
        },
        PadSphereSizeCase{
            .Name = "HexScaleHalf",
            .Profile = ProfileKind::Regular,
            .Sides = 6,
            .Height = 0.5 * kHexH,
            .Radius = 1.0,
            .SphereCenter = {-0.88336, 0.61575, -0.4770775},
            .SphereRadius = 1.26742,
        },
        PadSphereSizeCase{
            .Name = "HexTopNick",
            .Profile = ProfileKind::Regular,
            .Sides = 6,
            .Pose = PoseClass::TopBite,
            .Height = 1.2,
            .Radius = 2.0,
            .SphereCenter = {0.0, 1.2, 0.0},
            .SphereRadius = 0.7,
        },
        PadSphereSizeCase{
            .Name = "HexVertexNick",
            .Profile = ProfileKind::Regular,
            .Sides = 6,
            .Pose = PoseClass::VertexNick,
            .Height = 1.2,
            .Radius = 2.0,
            .SphereCenter = {2.0, 0.6, 0.0},
            .SphereRadius = 0.45,
        },
        PadSphereSizeCase{
            .Name = "SphereContainsHex",
            .Profile = ProfileKind::Regular,
            .Sides = 6,
            .Pose = PoseClass::Contains,
            .Height = 0.8,
            .Radius = 1.0,
            .SphereCenter = {0.0, 0.4, 0.0},
            .SphereRadius = 2.5,
            .ExpectPadMinusSphere = false,
            .ExpectSphereMinusPad = true,
            .SkipReason = "EmptyCsg",
        },
        MakeRegularPose("TriRegularThroughCentered", 3,
                        PoseClass::ThroughCentered),
        MakeRegularPose("TriRegularTopBite", 3, PoseClass::TopBite),
        MakeRegularPose("TriRegularSideBite", 3, PoseClass::SideBite),
        MakeRegularPose("TriRegularEdgeBite", 3, PoseClass::EdgeBite),
        MakeRegularPose("TriRegularVertexNick", 3, PoseClass::VertexNick),
        MakeRegularPose("TriRegularContains", 3, PoseClass::Contains),
        MakeRegularPose("QuadRegularThroughCentered", 4,
                        PoseClass::ThroughCentered),
        MakeRegularPose("QuadRegularTopBite", 4, PoseClass::TopBite),
        MakeRegularPose("QuadRegularSideBite", 4, PoseClass::SideBite),
        MakeRegularPose("QuadRegularEdgeBite", 4, PoseClass::EdgeBite),
        MakeRegularPose("QuadRegularVertexNick", 4, PoseClass::VertexNick),
        MakeRegularPose("QuadRegularContains", 4, PoseClass::Contains),
        MakeRegularPose("PentRegularThroughCentered", 5,
                        PoseClass::ThroughCentered),
        MakeRegularPose("PentRegularTopBite", 5, PoseClass::TopBite),
        MakeRegularPose("PentRegularSideBite", 5, PoseClass::SideBite),
        MakeRegularPose("PentRegularEdgeBite", 5, PoseClass::EdgeBite),
        MakeRegularPose("PentRegularVertexNick", 5, PoseClass::VertexNick),
        MakeRegularPose("PentRegularContains", 5, PoseClass::Contains),
        MakeRegularPose("HexRegularThroughCentered", 6,
                        PoseClass::ThroughCentered),
        MakeRegularPose("HexRegularTopBite", 6, PoseClass::TopBite),
        MakeRegularPose("HexRegularSideBite", 6, PoseClass::SideBite),
        MakeRegularPose("HexRegularEdgeBite", 6, PoseClass::EdgeBite),
        // Phase B: size / offset on Phase A green poses (n=3 and n=5).
        // VertexNick and n=3 EdgeBite stay at Phase A (known gaps).
        MakeRegularPose("TriRegularThroughThin", 3, PoseClass::ThroughCentered,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("TriRegularThroughTall", 3, PoseClass::ThroughCentered,
                        2.0 * kPhaseAHeight, 0.7 * kPhaseACircum),
        MakeRegularPose("TriRegularThroughR04", 3, PoseClass::ThroughCentered,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("TriRegularThroughR07", 3, PoseClass::ThroughCentered,
                        kPhaseAHeight, 0.7 * kPhaseACircum),
        MakeRegularPose("TriRegularThroughR12", 3, PoseClass::ThroughCentered,
                        kPhaseAHeight, 1.2 * kPhaseACircum),
        MakeRegularPose("TriRegularThroughOffsetSide", 3,
                        PoseClass::ThroughCentered, kPhaseAHeight,
                        0.4 * kPhaseACircum, SphereOffset::TowardSide),
        MakeRegularPose("TriRegularThroughOffsetVertex", 3,
                        PoseClass::ThroughCentered, kPhaseAHeight, 0.7,
                        SphereOffset::TowardVertex),
        MakeRegularPose("PentRegularThroughThin", 5, PoseClass::ThroughCentered,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("PentRegularThroughTall", 5, PoseClass::ThroughCentered,
                        2.0 * kPhaseAHeight, 0.7 * kPhaseACircum),
        MakeRegularPose("PentRegularThroughR04", 5, PoseClass::ThroughCentered,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("PentRegularThroughR07", 5, PoseClass::ThroughCentered,
                        kPhaseAHeight, 0.7 * kPhaseACircum),
        MakeRegularPose("PentRegularThroughR12", 5, PoseClass::ThroughCentered,
                        kPhaseAHeight, 1.2 * kPhaseACircum),
        MakeRegularPose("PentRegularThroughOffsetSide", 5,
                        PoseClass::ThroughCentered, kPhaseAHeight,
                        0.4 * kPhaseACircum, SphereOffset::TowardSide),
        MakeRegularPose("PentRegularThroughOffsetVertex", 5,
                        PoseClass::ThroughCentered, kPhaseAHeight, 0.7,
                        SphereOffset::TowardVertex),
        MakeRegularPose("TriRegularTopBiteThin", 3, PoseClass::TopBite,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("TriRegularTopBiteTall", 3, PoseClass::TopBite,
                        2.0 * kPhaseAHeight),
        MakeRegularPose("TriRegularTopBiteLarge", 3, PoseClass::TopBite,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("PentRegularTopBiteThin", 5, PoseClass::TopBite,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("PentRegularTopBiteTall", 5, PoseClass::TopBite,
                        2.0 * kPhaseAHeight),
        MakeRegularPose("PentRegularTopBiteLarge", 5, PoseClass::TopBite,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("TriRegularSideBiteThin", 3, PoseClass::SideBite,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("TriRegularSideBiteTall", 3, PoseClass::SideBite,
                        2.0 * kPhaseAHeight),
        MakeRegularPose("TriRegularSideBiteLarge", 3, PoseClass::SideBite,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("PentRegularSideBiteThin", 5, PoseClass::SideBite,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("PentRegularSideBiteTall", 5, PoseClass::SideBite,
                        2.0 * kPhaseAHeight),
        MakeRegularPose("PentRegularSideBiteLarge", 5, PoseClass::SideBite,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        MakeRegularPose("PentRegularEdgeBiteThin", 5, PoseClass::EdgeBite,
                        0.4 * kPhaseAHeight),
        MakeRegularPose("PentRegularEdgeBiteTall", 5, PoseClass::EdgeBite,
                        2.0 * kPhaseAHeight),
        MakeRegularPose("PentRegularEdgeBiteLarge", 5, PoseClass::EdgeBite,
                        kPhaseAHeight, 0.4 * kPhaseACircum),
        // Phase C: irregular profiles. Untitled verts are from
        // untitled.xl Pad 11de7787 / sphere 0a680dea (not a regular hex).
        PadSphereSizeCase{
            .Name = "HexIrregularUntitled",
            .Profile = ProfileKind::Explicit,
            .Sides = 6,
            .Pose = PoseClass::IrregularFile,
            .Height = 1.2315022069431985,
            .ProfileUv = UntitledHexUv(),
            .SphereCenter = {-1.7667168123615866, 1.2315022069431985,
                             -0.95415473448792976},
            .SphereRadius = 2.5348365237903314,
        },
        MakeExplicitPose("QuadTrapezoidThrough", TrapezoidUv(),
                         PoseClass::ThroughCentered),
        MakeExplicitPose("QuadTrapezoidTopBite", TrapezoidUv(),
                         PoseClass::TopBite),
        MakeExplicitPose("QuadTrapezoidSideBite", TrapezoidUv(),
                         PoseClass::SideBite),
        MakeExplicitPose("PentIrregularThrough", IrregularPentUv(),
                         PoseClass::ThroughCentered),
        MakeExplicitPose("PentIrregularTopBite", IrregularPentUv(),
                         PoseClass::TopBite),
        MakeExplicitPose("PentIrregularSideBite", IrregularPentUv(),
                         PoseClass::SideBite)),
    [](const ::testing::TestParamInfo<PadSphereSizeCase>& info)
    {
        return std::string(info.param.Name);
    });

struct BoxSphereSizeCase
{
    const char* Name{};
    Point3d BoxMin{0.0, 0.0, 0.0};
    Point3d BoxMax{1.0, 1.0, 1.0};
    Point3d SphereCenter{0.0, 0.0, 0.0};
    double SphereRadius{1.0};
};

void PrintTo(const BoxSphereSizeCase& value, std::ostream* out)
{
    *out << value.Name;
}

class BoxSphereSizeSweep : public ::testing::TestWithParam<BoxSphereSizeCase>
{
};

TEST_P(BoxSphereSizeSweep, UnionIntersectSubtractValidate)
{
    const BoxSphereSizeCase spec = GetParam();
    SCOPED_TRACE(spec.Name);

    Model model;
    Body* box = MakeBox(model, BoxSpec{.Min = spec.BoxMin,
                                       .Max = spec.BoxMax,
                                       .Name = "box"});
    Body* sphere = MakeSphere(
        model, SphereSpec{.Center = spec.SphereCenter,
                          .Radius = spec.SphereRadius,
                          .Name = "sphere"});
    ASSERT_NE(box, nullptr);
    ASSERT_NE(sphere, nullptr);

    ExpectValidBoolean(boolean::BooleanOp::Union, model, *box, *sphere,
                       "union box∪sphere", false);
    ExpectValidBoolean(boolean::BooleanOp::Intersect, model, *box, *sphere,
                       "intersect box∩sphere", false);
    ExpectValidBoolean(boolean::BooleanOp::Subtract, model, *box, *sphere,
                       "subtract box−sphere", true);
    ExpectValidBoolean(boolean::BooleanOp::Subtract, model, *sphere, *box,
                       "subtract sphere−box", true);
}

INSTANTIATE_TEST_SUITE_P(
    Configs, BoxSphereSizeSweep,
    ::testing::Values(
        BoxSphereSizeCase{
            .Name = "OctantUnit",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {1.0, 1.0, 1.0},
            .SphereCenter = {0.0, 0.0, 0.0},
            .SphereRadius = 1.0,
        },
        BoxSphereSizeCase{
            .Name = "OctantHalf",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {0.5, 0.5, 0.5},
            .SphereCenter = {0.0, 0.0, 0.0},
            .SphereRadius = 0.5,
        },
        BoxSphereSizeCase{
            .Name = "OctantDouble",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {2.0, 2.0, 2.0},
            .SphereCenter = {0.0, 0.0, 0.0},
            .SphereRadius = 2.0,
        },
        BoxSphereSizeCase{
            .Name = "BoxThroughSphere",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {2.0, 1.0, 2.0},
            .SphereCenter = {1.0, 0.5, 1.0},
            .SphereRadius = 0.8,
        },
        BoxSphereSizeCase{
            .Name = "BoxThroughHalf",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {1.0, 0.5, 1.0},
            .SphereCenter = {0.5, 0.25, 0.5},
            .SphereRadius = 0.4,
        },
        BoxSphereSizeCase{
            .Name = "BoxThroughDouble",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {4.0, 2.0, 4.0},
            .SphereCenter = {2.0, 1.0, 2.0},
            .SphereRadius = 1.6,
        },
        BoxSphereSizeCase{
            .Name = "BoxCornerClip",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {2.0, 2.0, 2.0},
            .SphereCenter = {0.0, 0.0, 0.0},
            .SphereRadius = 1.0,
        },
        BoxSphereSizeCase{
            .Name = "BoxFaceBite",
            .BoxMin = {0.0, 0.0, 0.0},
            .BoxMax = {2.0, 2.0, 2.0},
            .SphereCenter = {1.0, 1.0, 2.3},
            .SphereRadius = 0.8,
        }),
    [](const ::testing::TestParamInfo<BoxSphereSizeCase>& info)
    {
        return std::string(info.param.Name);
    });

}  // namespace
}  // namespace brep
