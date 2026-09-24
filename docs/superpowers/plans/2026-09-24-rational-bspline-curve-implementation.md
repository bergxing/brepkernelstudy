# 有理 B 样条曲线 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 `part.create_nurbs_curve`：开放夹紧三次有理 B 样条 Wire（CV ≥ 4，Enter 结束），精确求值留在 `NurbsCurve`，屏幕只画采样折线。

**Architecture:** 不新造 Wire 拓扑。贝塞尔已经有 `Body::WireEdges`、`ValidateBody`、`ExtractEdges`、特征历史和 `AddPrimitive`。本计划只加 `NurbsCurve`（有理 Cox–de Boor）、`NurbsCurveSpec`、`NurbsCurveFeature`，以及一条不在第 4 点自动提交的命令。4 个单位权重 CV 的求值必须与现有三次 `BezierCurve` 一致。`part.create_bezier` 保留。

**Tech Stack:** C++20，`brep_core` / `brep_feat` / `brep_io`，Viewer 只 `#include "api/..."`，GoogleTest，MinGW `cmake-build-mingw-debug`。

**设计规格：** [`docs/superpowers/specs/2026-09-24-rational-bspline-curve-requirements.md`](../specs/2026-09-24-rational-bspline-curve-requirements.md)

## Global Constraints

- 次数只接受 `3`。节点缺省为夹紧均匀向量，长度 = `CV 数 + 4`，两端各重复 4 次，内部节点 `j / (interior + 1)`。
- 权重空向量等于全 1；显式权重必须与 CV 等长且均 `> 0`。
- 命令最少 4 个 CV 才能提交；护栏 `kMaxCvs = 256`。第 4 点不自动退出。
- 禁止 `NurbsCurveSpecFor`。Copy / 平移走 `PrimitiveSpec` 里的 `NurbsCurveSpec`。
- 不做曲面、求交、布尔、Fit、周期样条、节点编辑、改次数。不删除 `part.create_bezier`。
- 显示折线不得写回 B-Rep。Wire 不贴材质。
- PascalCase 类型/函数，camelCase 参数，成员 `m_`，4 空格 Allman。Viewer 不 include `brep/bool/**` 或 `brep/build/**`。
- `.xl` 新类型号 `FeatType::NurbsCurve = 10`。不要占用 1–9。

---

## 0. 已经有的，不要重做

| 能力 | 位置 | 本计划怎么用 |
|------|------|----------------|
| Wire 边 | `Body::WireEdges` | `MakeNurbsCurveWire` 只挂一根 Edge |
| 边采样 | `SampleEdgeXyz` 的 `CurveKind::Bezier` 臂 | 旁边加 `Nurbs`，段数 32 |
| 显示 | `ExtractEdges` 已遍历 `WireEdges` | 不改网格写回 |
| 特征事务 | `FeatureTransaction::Bezier` | **另加** `Nurbs` 字段，禁止把节点塞进 `BezierSpec` |
| 权重面板 | `PropertySheet::DescribeBezier` / 字段 `w0`… | 同样字段 id，按 CV 个数生成 |
| 命令骨架 | `CreateBezierTool` | 复制后改提交条件，不改贝塞尔工具 |

`CurveKind::Nurbs` 已在 `kernel/include/brep/Types.h`。不要再加一个枚举值。

---

## 里程碑

| 里程碑 | 可独立合并 | 验收 |
|--------|------------|------|
| **Nb1** 求值 + 节点 | 是 | `cmake-build-mingw-debug\bin\brep_test_nurbs_curve.exe` |
| **Nb2** Wire + 采样 + 曲线拷贝 | 依赖 Nb1 | `brep_test_nurbs_wire.exe` |
| **Nb3** 特征 / 历史 / `.xl` / 属性 | 依赖 Nb2 | `brep_test_nurbs_feature.exe` |
| **Nb4** 命令 + 菜单 | 依赖 Nb3 | 视口：4 点不退出，第 5 点 + Enter 出 Wire |
| **Nb5** Copy + 拖 CV | 依赖 Nb3，拖 CV 依赖 Nb4 | `TestSceneAdapter` 里 Nurbs 权重撤销 |

每步先写失败测试，再写实现。构建：

```powershell
$env:PATH = "C:\Qt6\Tools\mingw1310_64\bin;" + $env:PATH
cmake --build cmake-build-mingw-debug --target brep_test_nurbs_curve --parallel
```

本机 `ctest` 不认 CLion 的 `gtest_discover_tests`。验收跑 `cmake-build-mingw-debug\bin\` 下对应 exe。

---

## Nb1 — `NurbsCurve` 求值

### Task 1: 规格与夹紧节点

**Files:**
- Modify: `kernel/include/brep/feat/PrimitiveSpecs.h`
- Test: `tests/kernel/TestNurbsCurve.cpp`（本任务只放节点与规格断言；求值断言在 Task 2）
- Modify: `tests/CMakeLists.txt`（照 `brep_test_bezier_curve` 增加 `brep_test_nurbs_curve`）

**Interfaces:**
- Produces: `NurbsCurveSpec`、`NurbsCurveSpecValid`、`NurbsWeightAt`、`ClampedUniformKnots(int cvCount, int degree)`

- [ ] **Step 1: 写失败测试**

`tests/kernel/TestNurbsCurve.cpp`：

```cpp
#include "brep/feat/PrimitiveSpecs.h"

#include <gtest/gtest.h>

using namespace brep;

TEST(NurbsKnots, FourCvsIsBezierKnots)
{
    const auto u = ClampedUniformKnots(4, 3);
    ASSERT_EQ(u.size(), 8u);
    EXPECT_DOUBLE_EQ(u[0], 0.0);
    EXPECT_DOUBLE_EQ(u[3], 0.0);
    EXPECT_DOUBLE_EQ(u[4], 1.0);
    EXPECT_DOUBLE_EQ(u[7], 1.0);
}

TEST(NurbsKnots, FiveCvsHasMidKnot)
{
    const auto u = ClampedUniformKnots(5, 3);
    ASSERT_EQ(u.size(), 9u);
    EXPECT_DOUBLE_EQ(u[4], 0.5);
}

TEST(NurbsKnots, SixCvsSplitsThirds)
{
    const auto u = ClampedUniformKnots(6, 3);
    ASSERT_EQ(u.size(), 10u);
    EXPECT_NEAR(u[4], 1.0 / 3.0, 1e-12);
    EXPECT_NEAR(u[5], 2.0 / 3.0, 1e-12);
}

TEST(NurbsSpec, RejectsTooFewCvsBadWeightAndWrongDegree)
{
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{}, Point3d{1, 0, 0}, Point3d{2, 0, 0}};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Cvs.push_back(Point3d{3, 0, 0});
    EXPECT_TRUE(NurbsCurveSpecValid(spec));
    spec.Degree = 2;
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Degree = 3;
    spec.Weights = {1, 1, 0, 1};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
    spec.Weights = {1, 1, 2, 1};
    EXPECT_TRUE(NurbsCurveSpecValid(spec));
    spec.Knots = {0, 0, 0, 1};
    EXPECT_FALSE(NurbsCurveSpecValid(spec));
}
```

- [ ] **Step 2: 跑测试，确认编译失败**（`ClampedUniformKnots` / `NurbsCurveSpec` 尚未声明）

- [ ] **Step 3: 实现**

在 `PrimitiveSpecs.h` 的 `using PrimitiveSpec` **之前** 增加：

```cpp
struct NurbsCurveSpec
{
    std::vector<Point3d> Cvs{};
    std::vector<double> Weights{};
    std::vector<double> Knots{};
    int Degree{3};
    double Tolerance{1e-7};
    std::string Name{"nurbs"};
};

[[nodiscard]] inline double NurbsWeightAt(const NurbsCurveSpec& spec,
                                          std::size_t i) noexcept
{
    if (i < spec.Weights.size() && spec.Weights[i] > 0.0)
    {
        return spec.Weights[i];
    }
    return 1.0;
}

[[nodiscard]] inline std::vector<double> ClampedUniformKnots(int cvCount,
                                                             int degree)
{
    const int n = cvCount - 1;
    const int knotCount = cvCount + degree + 1;
    std::vector<double> u(static_cast<std::size_t>(knotCount), 0.0);
    const int interior = n - degree;
    for (int j = 1; j <= interior; ++j)
    {
        u[static_cast<std::size_t>(degree + j)] =
            static_cast<double>(j) / static_cast<double>(interior + 1);
    }
    for (int i = n + 1; i < knotCount; ++i)
    {
        u[static_cast<std::size_t>(i)] = 1.0;
    }
    return u;
}

[[nodiscard]] inline bool NurbsCurveSpecValid(
    const NurbsCurveSpec& spec) noexcept
{
    if (spec.Degree != 3 || spec.Cvs.size() < 4 || spec.Cvs.size() > 256)
    {
        return false;
    }
    if (!spec.Weights.empty())
    {
        if (spec.Weights.size() != spec.Cvs.size())
        {
            return false;
        }
        for (double w : spec.Weights)
        {
            if (!(w > 0.0))
            {
                return false;
            }
        }
    }
    if (spec.Knots.empty())
    {
        return true;
    }
    if (spec.Knots.size() != spec.Cvs.size() + 4)
    {
        return false;
    }
    for (std::size_t i = 1; i < spec.Knots.size(); ++i)
    {
        if (spec.Knots[i] < spec.Knots[i - 1])
        {
            return false;
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        if (spec.Knots[static_cast<std::size_t>(i)] != 0.0)
        {
            return false;
        }
        if (spec.Knots[spec.Knots.size() - 1 - static_cast<std::size_t>(i)] !=
            1.0)
        {
            return false;
        }
    }
    for (std::size_t i = 4; i + 4 < spec.Knots.size(); ++i)
    {
        int run = 1;
        while (i + static_cast<std::size_t>(run) + 4 < spec.Knots.size() &&
               spec.Knots[i + static_cast<std::size_t>(run)] == spec.Knots[i])
        {
            ++run;
        }
        if (run > spec.Degree)
        {
            return false;
        }
    }
    return true;
}
```

`PrimitiveSpec` 改为：

```cpp
using PrimitiveSpec =
    std::variant<BoxSpec, SphereSpec, BezierSpec, NurbsCurveSpec>;
```

每个 `static_assert(std::is_same_v<T, BezierSpec>, ...)` 改成 `else if constexpr (Bezier)`，再 `else` 里对 `NurbsCurveSpec` 做 **平移 CV、权重与节点不动**（`ApplyTransform`）或 **原样返回 / 调用对应函数**。本任务先让下列文件能编译，Nurbs 臂行为在 Nb3 / Nb5 补全，但不得 `return nullptr` 吞掉合法规格：

| 文件 | 本任务的 Nurbs 臂 |
|------|-------------------|
| `PrimitiveSpecs.h` `ApplyTransform` | 平移每个 CV，返回该 spec |
| `Part.cpp` `MaterializeFeatureBody` | `return nullptr` 会让后续测试失败；Nb2 再改成 `MakeNurbsCurveWire`。本任务用 `if constexpr` 编译通过即可，调用前向声明的 `MakeNurbsCurveWire` **留到 Nb2**。若 Nb1 单独编译 `brep_feat`，`Part.cpp` 的 Nurbs 臂暂时 `return nullptr`，并在 Nb2 的第一步改掉 |
| `Part.cpp` `AddPrimitive` | `return AddNurbsCurve(s)` 留到 Nb3；本任务先声明 `Part::AddNurbsCurve` 返回 `AddBezier` 会类型不符。Nb1 只改 `ApplyTransform` 和 `PropertySheet` 的 **编译臂**：`Describe` 对 Nurbs 走与 Bezier 相同的 `w{i}` 权重字段（把 `DescribeBezier` 抽成按 CV 数 + `WeightAt` 回调，或复制一个 `DescribeNurbs`）。`Apply` 同理，拒绝 `<= 0` |
| `SceneAdapter.cpp` 两处 visit | Nurbs 臂记录 `tx.Nurbs`（字段在 Nb3 加）。本任务若还没有字段，Nurbs 臂先 `static_assert` 通过并 `return` 不写事务——**禁止**写进 `tx.Bezier` |

Nb1 的编译修复以「variant 加臂能链接 `brep_test_nurbs_curve`」为准。`brep_test_nurbs_curve` 只链 `brep`，不要为了它去改 Viewer。Viewer 的 `static_assert` 会在整棵 `xcad_viewer` 编译时爆掉，所以 **同一任务里** 改掉 `PropertySheet.cpp`、`Part.cpp`、`SceneAdapter.cpp` 的 `static_assert`，Nurbs 臂先做安全的最小行为：属性表按权重复制 Bezier 逻辑；`AddPrimitive` 在 `AddNurbsCurve` 出现前不要被生产命令调用。

`Part::AddPrimitive` 的 Nurbs 臂在 `AddNurbsCurve` 落地前会编译失败。处理：Nb1 就在 `Part.h` 声明 `Body* AddNurbsCurve(const NurbsCurveSpec& spec);`，`Part.cpp` 先 `return nullptr;`。Nb3 再改成真正 `AppendFeature`。

- [ ] **Step 4: 跑 `brep_test_nurbs_curve.exe`，四个规格测试通过**

### Task 2: Cox–de Boor 求值

**Files:**
- Modify: `kernel/include/brep/Geometry.h`、`kernel/src/Geometry.cpp`、`kernel/include/brep/Model.h`、`kernel/src/Model.cpp`
- Test: `tests/kernel/TestNurbsCurve.cpp`

**Interfaces:**
- Consumes: `ClampedUniformKnots`、`NurbsCurveSpecValid`
- Produces: `NurbsCurve::Eval`、`NurbsCurve::Tangent`、`NurbsCurve::Domain`、`Model::MakeNurbs`

- [ ] **Step 1: 追加失败测试**

```cpp
#include "brep/Geometry.h"

TEST(NurbsCurve, FourUnitCvsMatchCubicBezier)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{0, 1, 0},
                                   Point3d{1, 1, 0}, Point3d{1, 0, 0}};
    const BezierCurve bezier(cvs);
    const NurbsCurve nurbs(cvs, {}, ClampedUniformKnots(4, 3));
    for (double t : {0.0, 0.25, 0.5, 0.75, 1.0})
    {
        EXPECT_LT(nurbs.Eval(t).distance_to(bezier.Eval(t)), 1e-9) << t;
    }
    EXPECT_LT(nurbs.Eval(0).distance_to(cvs.front()), 1e-12);
    EXPECT_LT(nurbs.Eval(1).distance_to(cvs.back()), 1e-12);
}

TEST(NurbsCurve, FiveCvsAtMidIsNotOnControlPolygon)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{1, 0, 0},
                                   Point3d{2, 1, 0}, Point3d{3, 1, 0},
                                   Point3d{4, 0, 0}};
    const NurbsCurve curve(cvs, {}, ClampedUniformKnots(5, 3));
    const Point3d mid = curve.Eval(0.5);
    EXPECT_NEAR(mid.x(), 2.0, 1e-9);
    EXPECT_NEAR(mid.y(), 0.75, 1e-9);
    EXPECT_NEAR(mid.z(), 0.0, 1e-9);
    EXPECT_GT(mid.distance_to(Point3d{2, 1, 0}), 0.2);
}

TEST(NurbsCurve, HigherWeightPullsTowardThatCv)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{1, 0, 0},
                                   Point3d{2, 1, 0}, Point3d{3, 1, 0},
                                   Point3d{4, 0, 0}};
    const auto knots = ClampedUniformKnots(5, 3);
    const NurbsCurve unit(cvs, {}, knots);
    const NurbsCurve heavy(cvs, {1, 1, 2, 1, 1}, knots);
    EXPECT_GT(heavy.Eval(0.5).y(), unit.Eval(0.5).y());
    EXPECT_LT(heavy.Eval(0).distance_to(cvs.front()), 1e-12);
    EXPECT_LT(heavy.Eval(1).distance_to(cvs.back()), 1e-12);
    EXPECT_NEAR(heavy.Eval(0.5).y(), 1.25 / 1.5, 1e-9);
}

TEST(NurbsCurve, TangentAtZeroMatchesBezier)
{
    const std::vector<Point3d> cvs{Point3d{0, 0, 0}, Point3d{0, 1, 0},
                                   Point3d{1, 1, 0}, Point3d{1, 0, 0}};
    const BezierCurve bezier(cvs);
    const NurbsCurve nurbs(cvs, {}, ClampedUniformKnots(4, 3));
    const Vector3d a = bezier.Tangent(0.0);
    const Vector3d b = nurbs.Tangent(0.0);
    EXPECT_GT(a.dot(b), 0.99);
}
```

手算（写进测试注释，避免以后改期望值）：5 CV、节点 `[0,0,0,0,0.5,1,1,1,1]`、`t=0.5` 的三次基函数是 `N1=0.25, N2=0.5, N3=0.25`。单位权重得点 `(2, 0.75, 0)`。`w2=2` 时 `y = 1.25/1.5`。

- [ ] **Step 2: 跑测试，确认 `NurbsCurve` 未定义而失败**

- [ ] **Step 3: 实现 `NurbsCurve`**

`Geometry.h`，放在 `BezierCurve` 之后：

```cpp
class NurbsCurve final : public Curve
{
public:
    NurbsCurve(std::vector<Point3d> cvs, std::vector<double> weights,
               std::vector<double> knots);

    [[nodiscard]] CurveKind Kind() const noexcept override
    {
        return CurveKind::Nurbs;
    }
    [[nodiscard]] Point3d Eval(double t) const override;
    [[nodiscard]] Vector3d Tangent(double t) const override;
    [[nodiscard]] std::pair<double, double> Domain() const noexcept override
    {
        return {0.0, 1.0};
    }
    [[nodiscard]] int Degree() const noexcept
    {
        return m_degree;
    }
    [[nodiscard]] const std::vector<Point3d>& Cvs() const noexcept
    {
        return m_cvs;
    }
    [[nodiscard]] const std::vector<double>& Weights() const noexcept
    {
        return m_weights;
    }
    [[nodiscard]] const std::vector<double>& Knots() const noexcept
    {
        return m_knots;
    }

private:
    std::vector<Point3d> m_cvs;
    std::vector<double> m_weights;
    std::vector<double> m_knots;
    int m_degree{3};
};
```

构造：`weights` 空则填 `cvs.size()` 个 `1.0`；`knots` 空则 `ClampedUniformKnots`。`cvs.size()<4` 或次数不是 3 时不抛异常到半边图里——`MakeNurbsCurveWire` 会先 `NurbsCurveSpecValid`。曲线对象本身仍按传入数据求值，便于测试直接构造。

`Geometry.cpp` 求值（分母为 0 的基函数项记 0；`t` 夹到 `[0,1]`；`t==1` 时用最后一个非空跨度，即 `span = cvs.size() - 1`）：

```cpp
int FindSpan(const std::vector<double>& u, int n, double t)
{
    if (t >= u[static_cast<std::size_t>(n + 1)])
    {
        return n;
    }
    int low = 3;
    int high = n + 1;
    int mid = (low + high) / 2;
    while (t < u[static_cast<std::size_t>(mid)] ||
           t >= u[static_cast<std::size_t>(mid + 1)])
    {
        if (t < u[static_cast<std::size_t>(mid)])
        {
            high = mid;
        }
        else
        {
            low = mid;
        }
        mid = (low + high) / 2;
    }
    return mid;
}
```

基函数用 Piegl A2.2 的单行三角（只算 `span-p … span`）。有理点：

```text
A = Σ N_i * w_i * P_i
w = Σ N_i * w_i
C = A / w
```

一阶导（次数 `p`）：

```text
N'_i,p = p * ( N_i,p-1 / (u_{i+p}-u_i) - N_{i+1,p-1} / (u_{i+p+1}-u_{i+1}) )
C' = (A' * w - A * w') / w^2
```

`Tangent` 返回 `C'` 的单位向量。`C'` 长度 `< 1e-15` 时回退 `Pn - P0`，再不行回退 `+X`。禁止 NaN。

`Model::MakeNurbs(cvs, weights, knots, name)` 放进与 `MakeBezier` 相同的曲线池，返回 `NurbsCurve*`。

- [ ] **Step 4: `brep_test_nurbs_curve.exe` 全部通过**

**Nb1 DoD：** 不启动 Viewer。4 CV 与三次贝塞尔五点一致。5 CV 中点是 `(2, 0.75, 0)`。

---

## Nb2 — Wire 与显示采样

### Task 3: `MakeNurbsCurveWire`

**Files:**
- Modify: `kernel/include/brep/build/PrimitiveBuild.h`、`kernel/src/Builder.cpp`
- Modify: `kernel/src/Part.cpp` 的 `MaterializeFeatureBody` Nurbs 臂
- Test: `tests/kernel/TestNurbsWire.cpp`、`tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `NurbsCurveSpecValid`、`Model::MakeNurbs`
- Produces: `Body* MakeNurbsCurveWire(Model& model, const NurbsCurveSpec& spec)`

- [ ] **Step 1: 失败测试**

```cpp
TEST(NurbsWire, OneEdgeThroughEnds)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 1, 0}, Point3d{4, 0, 0}};
    spec.Name = "nurbs";
    Body* body = MakeNurbsCurveWire(model, spec);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(body->Type, BodyType::Wire);
    ASSERT_EQ(body->WireEdges.size(), 1u);
    EXPECT_TRUE(body->Shells.empty());
    EXPECT_EQ(body->WireEdges[0]->Curve->Kind(), CurveKind::Nurbs);
    EXPECT_LT(body->WireEdges[0]->V0->Point->Position.distance_to(spec.Cvs.front()),
              1e-12);
    EXPECT_LT(body->WireEdges[0]->V1->Point->Position.distance_to(spec.Cvs.back()),
              1e-12);
    EXPECT_TRUE(ValidateBody(*body).Ok());
}

TEST(NurbsWire, InvalidSpecReturnsNull)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{}, Point3d{1, 0, 0}};
    EXPECT_EQ(MakeNurbsCurveWire(model, spec), nullptr);
}
```

- [ ] **Step 2: 确认链接失败**

- [ ] **Step 3: 实现**

照 `MakeBezierWire` 的 `SegmentCount == 1` 分支，但曲线用 `model.MakeNurbs(spec.Cvs, spec.Weights, knots, name)`。`knots`：`spec.Knots` 空则 `ClampedUniformKnots(spec.Cvs.size(), 3)`。参数域 `0, 1`。不建 Face / Shell。非法规格返回 `nullptr`。

`MaterializeFeatureBody` 的 Nurbs 臂改为 `return MakeNurbsCurveWire(scratch, primitive);`。

- [ ] **Step 4: 测试通过**

### Task 4: 采样与拓扑拷贝

**Files:**
- Modify: `kernel/src/mesh/LoopSample.cpp`（`CurveKind::Nurbs` → `segmentCount = 32`，与 Bezier 相同）
- Modify: `kernel/src/bool/TopologyCopy.cpp` 的 `CopyCurve`
- Modify: `kernel/include/brep/Geometry.h`、`kernel/src/Geometry.cpp`
- Modify: `kernel/include/api/Mesh.h`（导出采样，Viewer 不直接包含 mesh 内部头）
- Test: `tests/kernel/TestNurbsWire.cpp`

**Interfaces:**
- Produces: `std::vector<Point3d> SampleNurbsPolyline(const NurbsCurve& curve, int uniformSegments = 32)`

- [ ] **Step 1: 失败测试**

```cpp
TEST(NurbsWire, ExtractEdgesReachesEnds)
{
    Model model;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{0, 1, 0}, Point3d{1, 1, 0},
                Point3d{1, 0, 0}};
    Body* body = MakeNurbsCurveWire(model, spec);
    const EdgeMesh mesh = ExtractEdges(*body);
    ASSERT_GE(mesh.Positions.size(), 33u);
    EXPECT_LT(mesh.Positions.front().distance_to(spec.Cvs.front()), 1e-6);
    EXPECT_LT(mesh.Positions.back().distance_to(spec.Cvs.back()), 1e-6);
    const TriangleMesh faces = TessellateBody(*body);
    EXPECT_TRUE(faces.Indices.empty());
}
```

再断言 `CopyCurve` 之后副本 `Kind()==Nurbs`，CV、权重、节点与源相同（用 `TopologyCopy` 里已有的 body 拷贝入口；若只有 `CopyCurve` 可测，就直接构 `NurbsCurve` 走该函数）。

- [ ] **Step 2: 确认 `SampleEdgeXyz: unsupported curve kind` 或拷贝返回空**

- [ ] **Step 3: 实现**

`SampleNurbsPolyline`：`Eval(i / N)`，`N=32`，33 个点。`LoopSample.cpp` 的 `switch` 把 `Nurbs` 和 `Bezier` 写成同一个 `segmentCount = 32` 分支。

`CopyCurve`：

```cpp
case CurveKind::Nurbs:
{
    const auto& nurbs = static_cast<const NurbsCurve&>(*src);
    copy = ctx.Target.MakeNurbs(nurbs.Cvs(), nurbs.Weights(), nurbs.Knots(),
                                "nurbs_cp");
    break;
}
```

`api/Mesh.h` 增加 `SampleNurbsPolyline` 声明（与现有 `SampleBezierPolyline` 的导出方式相同）。

- [ ] **Step 4: `brep_test_nurbs_wire.exe` 通过**

**Nb2 DoD：** `MakeNurbsCurveWire` + `ValidateBody` + `ExtractEdges` 绿。Viewer 仍无命令。

---

## Nb3 — 特征、撤销、存盘、属性

### Task 5: `NurbsCurveFeature` 与 `Part::AddNurbsCurve`

**Files:**
- Create: `kernel/include/brep/feat/NurbsCurveFeature.h`、`kernel/src/feat/NurbsCurveFeature.cpp`
- Modify: `cmake/BrepFeat.cmake`、`kernel/include/brep/Part.h`、`kernel/src/Part.cpp`、`kernel/include/api/Modeling.h`
- Test: `tests/kernel/TestNurbsFeature.cpp`

**Interfaces:**
- Produces: `NurbsCurveFeature::Create`、`ToSpec`、`ToPrimitiveSpec`、`SetFromSpec`、`TypeName()=="NurbsCurve"`、`Part::AddNurbsCurve`、`Part::RebuildNurbsCurveBody`

对照 `BezierCurveFeature`。成员是 `m_cvs`、`m_weights`、`m_knots`、`m_degree`、`m_tolerance`。`SetFromSpec` 若 `Knots` 空则存 `ClampedUniformKnots`。`Rebuild` 走与 `RebuildBezierBody` 相同的「按 Guid 换 Wire」路径，函数名 `RebuildNurbsCurveBody`。

- [ ] **Step 1: 失败测试** — `AddNurbsCurve` 得到 1 个 Wire，`TypeName=="NurbsCurve"`，`ToPrimitiveSpec` 命中 `NurbsCurveSpec`，5 个 CV 与节点 `size==9` 仍在。
- [ ] **Step 2: 确认失败**
- [ ] **Step 3: 实现**。`AddPrimitive` 的 Nurbs 臂改为 `return AddNurbsCurve(s);`，删掉 Nb1 的 `return nullptr`。
- [ ] **Step 4: 测试通过**

### Task 6: 历史与 `.xl`

**Files:**
- Modify: `kernel/include/brep/feat/FeatureHistory.h`、`kernel/src/feat/FeatureHistory.cpp`
- Modify: `kernel/src/io/XlDocument.cpp`
- Test: `tests/kernel/TestNurbsFeature.cpp`

**Interfaces:**
- Produces: `FeatureTransaction::Nurbs`、`FeatureTransaction::NurbsBefore`；`FeatType::NurbsCurve = 10`

- [ ] **Step 1: 失败测试**

```cpp
TEST(NurbsFeature, UndoRedoRestoresBody)
{
    Part part;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 0, 0}};
    Body* body = part.AddNurbsCurve(spec);
    ASSERT_NE(body, nullptr);
    const Guid id = body->Guid;
    ASSERT_TRUE(part.History().Undo());
    EXPECT_EQ(part.FindBody(id), nullptr);
    ASSERT_TRUE(part.History().Redo());
    EXPECT_NE(part.FindBody(id), nullptr);
}

TEST(NurbsFeature, XlRoundTripKeepsKnotsAndWeights)
{
    Part part;
    NurbsCurveSpec spec;
    spec.Cvs = {Point3d{0, 0, 0}, Point3d{1, 0, 0}, Point3d{2, 1, 0},
                Point3d{3, 1, 0}, Point3d{4, 0, 0}};
    spec.Weights = {1, 1, 2, 1, 1};
    part.AddNurbsCurve(spec);
    const auto bytes = SaveXl(part);
    Part loaded;
    ASSERT_TRUE(LoadXl(loaded, bytes));
    const auto prim = /* 唯一 NurbsCurve 特征的 ToPrimitiveSpec */;
    const auto* nurbs = std::get_if<NurbsCurveSpec>(&*prim);
    ASSERT_NE(nurbs, nullptr);
    ASSERT_EQ(nurbs->Cvs.size(), 5u);
    EXPECT_NEAR(nurbs->Weights[2], 2.0, 1e-12);
    ASSERT_EQ(nurbs->Knots.size(), 9u);
    EXPECT_DOUBLE_EQ(nurbs->Knots[4], 0.5);
}
```

`SaveXl` / `LoadXl` 用文件里已有的贝塞尔往返测试同名入口，不要新造一套文档 API。

- [ ] **Step 2: 确认失败**
- [ ] **Step 3: 实现**

`FeatureHistory.cpp` 里每一处 `tx.FeatureType == "Bezier"` 旁边加 `"NurbsCurve"`，读写 `tx.Nurbs` / `tx.NurbsBefore`，`static_cast<NurbsCurveFeature*>`。不要复用 `tx.Bezier`。

`.xl`：`FeatType::NurbsCurve = 10`。写入 CV 个数、每个 xyz、权重个数与权重、节点个数与节点、Degree、Tolerance、Name、Suppressed、BodyGuid。读出后 `NurbsCurveFeature::Create` + 进树 + Regenerate。类型 1–9 的旧文件路径不改。

- [ ] **Step 4: 测试通过。** 再跑 `brep_test_bezier_feature.exe`，确认类型 7/8/9 仍能读。

### Task 7: 属性权重

**Files:**
- Modify: `kernel/src/feat/PropertySheet.cpp`
- Test: `tests/kernel/TestPropertySheet.cpp`

- [ ] **Step 1: 失败测试** — 5 CV 的 `NurbsCurveSpec`，`Describe` 得到 `w0`…`w4`；`Apply(..., "w2", 2.0)` 后 `Weights[2]==2`；`Apply(..., "w2", 0.0)` 返回 false 且权重不变；`Apply(..., "w9", 1)` 返回 false。
- [ ] **Step 2: 确认 `Describe` 的 static_assert 或缺少字段而失败**
- [ ] **Step 3: `DescribeNurbs` / `ApplyNurbs`** 与 `DescribeBezier` / `ApplyBezier` 相同，只是类型换成 `NurbsCurveSpec`，权重用 `NurbsWeightAt`。次数和节点不进字段。
- [ ] **Step 4: `brep_test_nurbs_feature.exe` 与现有 `TestPropertySheet` 目标都通过**

**Nb3 DoD：** 无 Qt。Undo/Redo、`.xl`、权重字段绿。

---

## Nb4 — 命令

### Task 8: `CreateNurbsCurveTool`

**Files:**
- Create: `apps/viewer/commands/tools/CreateNurbsCurveTool.h`、`.cpp`
- Modify: `apps/viewer/CMakeLists.txt`、`apps/viewer/commands/BuiltinCommands.cpp`
- Modify: `apps/viewer/ui/ActionCatalog.cpp`、`apps/viewer/ui/RibbonSetup.cpp`、`apps/viewer/ui/MainWindowMenus.cpp`
- Modify: `apps/viewer/i18n/xcad_zh_CN.ts`
- 预览：`apps/viewer/commands/tools/BezierPreview.h` 已有折线 helper。本命令的曲线采样改调 `SampleNurbsPolyline`。CV 少于 4 时预览仍构造 `BezierCurve(prefix+hover)`。

**Interfaces:**
- Consumes: `Part::AddNurbsCurve`（经 `ISceneService::AddPrimitive`，禁止新的 `NurbsCurveSpecFor`）
- Produces: 命令 id `part.create_nurbs_curve`

交互以需求 §5 为准。从 `CreateBezierTool.cpp` 复制状态机，只改下面这些，其余（吸附、过近拒绝、右键菜单、绘制 Undo、弹出层让路）保持原函数体：

| 贝塞尔 | 本命令 |
|--------|--------|
| `kMaxCvs = 4`，收满即 `CommitBezier` | `kMaxCvs = 256`。到达 256 再点击：不收点，状态栏 `At most 256 control points` |
| 第 4 点提交 | 任何左键 / 「确定」都只 `AcceptPoint` |
| `FinishKeepOrAbort`：`>= 2` 提交 | `>= 4` 才 `CommitNurbs`；否则放弃 |
| Enter：贝塞尔没有「点数不足仍留下」 | CV `< 4` 且无 hover：不结束，提示 `Need at least 4 control points` |
| `CommitBezier` 填 `BezierSpec` | `NurbsCurveSpec`：`Degree=3`，`Weights` 空，`Knots` 空，`Name="nurbs"` |

`Prompt()` 源文（`translate("CreateNurbsCurveTool", …)`）：

| 状态 | 英文 |
|------|------|
| `PickFirst` | `Pick first point` |
| CV < 4 | `Pick next point` |
| CV ≥ 4 | `Pick next point, Enter to finish` |

`BuiltinCommands.cpp`：`CreateNurbsCurveCommand`，`kind==Interactive`，`make_tool` → `CreateNurbsCurveTool`，`can_execute` 与贝塞尔相同。`register_builtin_commands` 里 `add`。

菜单 / Ribbon：`Create &NURBS Curve…`，objectName 不要和 Box 的 `&B` 或 Bezier 的 `&z` 抢同一快捷键。Tooltip 源文用需求 §4。`xcad_zh_CN.ts` 给 `CreateNurbsCurveTool` 和菜单各一条中文。

提交后 `mesh_for_body`：edges 非空、faces 空。失败提示 `Cancelled create nurbs curve`。`BREP_INFO` 打出 Guid 与 CV 个数。

- [ ] **Step 1:** 工具类与命令注册能编译。
- [ ] **Step 2:** 手动：3 点 + ESC 无 Body；4 点不退出；第 5 点 + Enter 得到名为 `nurbs` 的 Wire；右键「确定」只加点；Ctrl+Z 只丢最后一个 CV；连续点过近不收点。
- [ ] **Step 3:** 中文界面下状态栏是中文。

**Nb4 DoD：** 视口能创建 5 CV 曲线。撤销后 Body 消失。贝塞尔命令行为不变。

---

## Nb5 — Copy 与拖 CV

### Task 9: 平移与复制

**Files:**
- Modify: `apps/viewer/commands/tools/CopyTool.cpp`（文案里的类型列表加上 NURBS curve）
- Modify: `apps/viewer/adapter/SceneAdapter.cpp`（Nurbs 事务写 `tx.Nurbs`，不要写 `tx.Bezier`）
- Modify: `apps/viewer/ecs/world.cpp`（`get_if<NurbsCurveSpec>` 时把 CV 和权重放进与贝塞尔相同的预览/拖拽组件；节点不进 ECS）
- Test: `apps/viewer/tests/TestSceneAdapter.cpp`

`ApplyTransform` 在 Nb1 已平移 CV。Copy 走现有 `PrimitiveSpec` 路径即可，确认 Nurbs 臂没有被 `continue` 掉。

- [ ] **Step 1: 失败测试** `AddPrimitive(NurbsCurveSpec)` 后 `SpecFor` 得到 4 个 CV；`SetPrimitive` 把 `w1` 改成 2，Undo 回到 1。对照 `SceneAdapter.SetPrimitiveBezierWeightsUndoRedo`。
- [ ] **Step 2: 确认失败**
- [ ] **Step 3: 接上 Nurbs 臂与 `tx.Nurbs`**
- [ ] **Step 4: 测试通过**

### Task 10: 视口拖 CV

**Files:**
- Modify: `apps/viewer/VulkanWindow.cpp`、`apps/viewer/VulkanWindow.h`、`apps/viewer/ui/ViewManager.cpp`

贝塞尔拖 CV 的回调类型是 `BezierSpec`。不要把 NURBS 塞进那个回调。并列一个 `NurbsCurveSpec` 回调：拖动只改被拾取的 CV，`MarkDirty` 后走 `EditParameters`，事务字段 `NurbsBefore` / `Nurbs`。ESC 取消本次拖动时恢复 `NurbsBefore`。权重和节点不动。

预览采样用 `SampleNurbsPolyline`，经 `api/Mesh.h`。

- [ ] **Step 1:** 选中已提交的 5 CV 曲线，拖中间 CV，曲线跟着动，Undo 回到原 CV。
- [ ] **Step 2:** 拖动不改变 `Knots.size()` 与 `Knots[4]`。

**Nb5 DoD：** 复制平移量等于拾取偏移。权重 Undo 回到 1。拖 CV 不改节点。

---

## 验收清单

- [ ] 4 CV、单位权重、默认节点与三次 `BezierCurve` 在 `0, 0.25, 0.5, 0.75, 1` 距离 `< 1e-9`。
- [ ] 5 CV 单位权重 `Eval(0.5)` 为 `(2, 0.75, 0)`；`w2=2` 时 `y = 1.25/1.5`，端点仍是首尾 CV。
- [ ] CV 少于 4、次数不是 3、权重 `<= 0`、节点长度不对：`NurbsCurveSpecValid` 为 false，`MakeNurbsCurveWire` 返回空。
- [ ] Wire：一根 Edge，无 Shell，`ValidateBody` 通过，`ExtractEdges` 端点贴首尾 CV，三角面为空。
- [ ] `.xl` 往返保留 CV、权重、节点。类型 7/8/9 的贝塞尔文件仍能打开。
- [ ] 命令：3 点 ESC 放弃；4 点不自动提交；Enter 在 ≥4 时提交；右键确定只加点。
- [ ] 属性 `w{i}` 可改权重；拖 CV 不改节点。
- [ ] 无 `NurbsCurveSpecFor`。`part.create_bezier` 仍在。
- [ ] `brep_test_nurbs_curve.exe`、`brep_test_nurbs_wire.exe`、`brep_test_nurbs_feature.exe` 通过。

## 回滚

Nb1 只动几何与规格，回滚不影响 Viewer 命令。Nb4 回滚：从 `register_builtin_commands`、Ribbon、菜单去掉 `part.create_nurbs_curve`。不要在本计划里改布尔或贝塞尔求值。
