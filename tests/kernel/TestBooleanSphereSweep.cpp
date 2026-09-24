#include "api/Core.h"
#include "api/Modeling.h"

#include "brep/build/PrimitiveBuild.h"
#include "brep/bool/Boolean.h"
#include "brep/Mesh.h"
#include "brep/Validate.h"

#include <gtest/gtest.h>

#include <cmath>
#include <ostream>
#include <string>
#include <vector>

namespace brep
{
namespace
{

// Sphere × sphere matrix — see
// docs/superpowers/specs/2026-09-18-sphere-sphere-boolean-test-matrix.md
// Do not delete a red case. Expect*=false must carry SkipReason.

enum class SpherePose
{
    Overlap,
    Contained,
    Separate,
    ExternalTangent,
    InternalTangent,
};

struct SphereSphereCase
{
    const char* Name{};
    Point3d CenterA{};
    double RadiusA{1.0};
    Point3d CenterB{};
    double RadiusB{1.0};
    SpherePose Pose{SpherePose::Overlap};
    bool ExpectUnion{true};
    bool ExpectIntersect{true};
    bool ExpectAMinusB{true};
    bool ExpectBMinusA{true};
    const char* SkipReason{};
};

void PrintTo(const SphereSphereCase& value, std::ostream* out)
{
    *out << value.Name;
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

[[nodiscard]] std::size_t CountDistinctSphereCenters(const Body& body,
                                                     double eps)
{
    std::vector<Point3d> centers;
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
            const auto& sphere =
                static_cast<const SphereSurface&>(*face->Surface);
            bool seen = false;
            for (const Point3d& existing : centers)
            {
                if ((existing - sphere.Center()).norm() <= eps)
                {
                    seen = true;
                    break;
                }
            }
            if (!seen)
            {
                centers.push_back(sphere.Center());
            }
        }
    }
    return centers.size();
}

void ExpectValidSphereBoolean(boolean::BooleanOp op, Model& model,
                              const Body& a, const Body& b, const char* label,
                              bool expectTwoCenters)
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
    EXPECT_GE(CountSphereFaces(*result.OutputBody), 1U) << label;
    EXPECT_GT(CountSphereTriangles(*result.OutputBody), 0U)
        << label << ": sphere faces have no triangles";
    if (expectTwoCenters)
    {
        EXPECT_EQ(CountDistinctSphereCenters(*result.OutputBody, 1e-6), 2U)
            << label << ": overlap CSG must keep both sphere centers";
        EXPECT_EQ(CountSphereFaces(*result.OutputBody), 2U)
            << label << ": overlap CSG must be two spherical faces";
    }
}

class SphereSphereSweep : public ::testing::TestWithParam<SphereSphereCase>
{
};

TEST_P(SphereSphereSweep, UnionIntersectSubtractValidate)
{
    const SphereSphereCase spec = GetParam();
    SCOPED_TRACE(spec.Name);

    const bool anySkip = !spec.ExpectUnion || !spec.ExpectIntersect ||
                         !spec.ExpectAMinusB || !spec.ExpectBMinusA;
    if (anySkip)
    {
        ASSERT_TRUE(spec.SkipReason != nullptr && spec.SkipReason[0] != '\0')
            << spec.Name
            << ": Expect*=false requires SkipReason "
               "(EmptyCsg or KNOWN_GAP:id)";
    }

    Model model;
    Body* sphereA = MakeSphere(
        model, SphereSpec{.Center = spec.CenterA,
                          .Radius = spec.RadiusA,
                          .Name = "sphereA"});
    Body* sphereB = MakeSphere(
        model, SphereSpec{.Center = spec.CenterB,
                          .Radius = spec.RadiusB,
                          .Name = "sphereB"});
    ASSERT_NE(sphereA, nullptr);
    ASSERT_NE(sphereB, nullptr);

    const bool twoCenters = spec.Pose == SpherePose::Overlap;
    if (spec.ExpectUnion)
    {
        ExpectValidSphereBoolean(boolean::BooleanOp::Union, model, *sphereA,
                                 *sphereB, "union A∪B", twoCenters);
    }
    if (spec.ExpectIntersect)
    {
        ExpectValidSphereBoolean(boolean::BooleanOp::Intersect, model, *sphereA,
                                 *sphereB, "intersect A∩B", twoCenters);
    }
    if (spec.ExpectAMinusB)
    {
        ExpectValidSphereBoolean(boolean::BooleanOp::Subtract, model, *sphereA,
                                 *sphereB, "subtract A−B", twoCenters);
    }
    if (spec.ExpectBMinusA)
    {
        ExpectValidSphereBoolean(boolean::BooleanOp::Subtract, model, *sphereB,
                                 *sphereA, "subtract B−A", twoCenters);
    }
}

INSTANTIATE_TEST_SUITE_P(
    Configs, SphereSphereSweep,
    ::testing::Values(
        SphereSphereCase{
            .Name = "EqualOffsetZ",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {0.0, 1.0, 0.0},
            .RadiusB = 1.0,
        },
        SphereSphereCase{
            .Name = "EqualOffsetX",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {1.0, 0.0, 0.0},
            .RadiusB = 1.0,
        },
        SphereSphereCase{
            .Name = "EqualDeep",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {0.5, 0.0, 0.0},
            .RadiusB = 1.0,
        },
        SphereSphereCase{
            .Name = "EqualShallow",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {1.8, 0.0, 0.0},
            .RadiusB = 1.0,
        },
        SphereSphereCase{
            .Name = "UnequalOverlap",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {1.5, 0.0, 0.0},
            .RadiusB = 2.0,
        },
        SphereSphereCase{
            .Name = "UntitledUnequalOverlap",
            .CenterA = {-2.9938904332874152, 1.2315022069431985,
                        -0.66607390034937941},
            .RadiusA = 1.3263314607869463,
            .CenterB = {-0.41971032900437599, 1.2315022069431985,
                        -0.70031663571947766},
            .RadiusB = 2.5348365237903314,
        },
        SphereSphereCase{
            .Name = "Contained",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 2.0,
            .CenterB = {0.2, 0.0, 0.0},
            .RadiusB = 0.5,
            .Pose = SpherePose::Contained,
            .ExpectBMinusA = false,
            .SkipReason = "EmptyCsg",
        },
        SphereSphereCase{
            .Name = "Separate",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {5.0, 0.0, 0.0},
            .RadiusB = 1.0,
            .Pose = SpherePose::Separate,
            .ExpectUnion = false,
            .ExpectIntersect = false,
            .ExpectAMinusB = false,
            .ExpectBMinusA = false,
            .SkipReason = "EmptyCsg",
        },
        SphereSphereCase{
            .Name = "ExternalTangent",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 1.0,
            .CenterB = {2.0, 0.0, 0.0},
            .RadiusB = 1.0,
            .Pose = SpherePose::ExternalTangent,
            .ExpectUnion = false,
            .ExpectIntersect = false,
            .ExpectAMinusB = false,
            .ExpectBMinusA = false,
            .SkipReason = "EmptyCsg",
        },
        SphereSphereCase{
            .Name = "InternalTangent",
            .CenterA = {0.0, 0.0, 0.0},
            .RadiusA = 2.0,
            .CenterB = {1.0, 0.0, 0.0},
            .RadiusB = 1.0,
            .Pose = SpherePose::InternalTangent,
            .ExpectBMinusA = false,
            .SkipReason = "EmptyCsg",
        }),
    [](const ::testing::TestParamInfo<SphereSphereCase>& info)
    {
        return std::string(info.param.Name);
    });

}  // namespace
}  // namespace brep
