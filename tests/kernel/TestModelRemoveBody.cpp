#include "api/Core.h"
#include "api/Modeling.h"
#include "brep/Model.h"
#include "brep/Validate.h"
#include "brep/build/PrimitiveBuild.h"

#include <gtest/gtest.h>

namespace brep
{
namespace
{

TEST(ModelRemoveBody, PurgesUnreachableSubgraph)
{
    const BoxSpec boxSpec{
        .Min = {0, 0, 0},
        .Max = {1, 1, 1},
        .Name = "box",
    };

    Model baselineModel;
    Body* baselineBody = MakeBox(baselineModel, boxSpec);
    ASSERT_NE(baselineBody, nullptr);
    const ModelPoolStats baseline = baselineModel.PoolStats();

    Model model;
    Body* bodyA = MakeBox(model, boxSpec);
    Body* bodyB = MakeBox(
        model,
        BoxSpec{.Min = {2, 0, 0}, .Max = {3, 1, 1}, .Name = "box_b"});
    ASSERT_NE(bodyA, nullptr);
    ASSERT_NE(bodyB, nullptr);

    const ModelPoolStats twoBodies = model.PoolStats();
    EXPECT_EQ(twoBodies.Bodies, 2u);
    EXPECT_GT(twoBodies.Vertices, baseline.Vertices);
    EXPECT_GT(twoBodies.Faces, baseline.Faces);

    ASSERT_TRUE(model.RemoveBody(bodyA->Guid));
    EXPECT_EQ(model.PoolStats().Bodies, 1u);
    EXPECT_EQ(model.Bodies().front().get(), bodyB);

    const ModelPoolStats afterRemove = model.PoolStats();
    EXPECT_EQ(afterRemove.Vertices, baseline.Vertices);
    EXPECT_EQ(afterRemove.Edges, baseline.Edges);
    EXPECT_EQ(afterRemove.Faces, baseline.Faces);
    EXPECT_EQ(afterRemove.Shells, baseline.Shells);
    EXPECT_EQ(afterRemove.Coedges, baseline.Coedges);
    EXPECT_EQ(afterRemove.Loops, baseline.Loops);

    EXPECT_TRUE(ValidateBody(*bodyB).Ok());
}

TEST(ModelRemoveBody, UnknownGuidIsNoOp)
{
    Model model;
    Body* body = MakeBox(
        model, BoxSpec{.Min = {0, 0, 0}, .Max = {1, 1, 1}, .Name = "box"});
    ASSERT_NE(body, nullptr);
    const ModelPoolStats before = model.PoolStats();

    EXPECT_FALSE(model.RemoveBody(Guid::Generate()));
    EXPECT_EQ(model.PoolStats().Vertices, before.Vertices);
    EXPECT_EQ(model.Bodies().size(), 1u);
}

}  // namespace
}  // namespace brep
