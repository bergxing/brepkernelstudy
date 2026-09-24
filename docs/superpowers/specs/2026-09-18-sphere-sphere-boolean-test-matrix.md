# 球 × 球 布尔测试矩阵

**日期：** 2026-09-18  
**状态：** Phase A 全绿（2026-09-18）  
**实现入口：** [`tests/kernel/TestBooleanSphereSweep.cpp`](../../../tests/kernel/TestBooleanSphereSweep.cpp)  
**关联：** [棱柱 × 球矩阵](./2026-09-18-prism-sphere-boolean-test-matrix.md)、[ADR 0008](../../architecture/adr/0008-boolean-general-pipeline-only.md)

**Goal：** 用姿态 × 半径比 × 操作矩阵驱动球−球 CSG。失败用例留在表里，禁止删红充绿。`AllowOperandSwap=false`。

桌面复现（`untitled.xl`）：

| Guid | 角色 | 圆心 | 半径 |
|------|------|------|------|
| `223cdc9d-8455-46bf-a24f-49644c568c09` | 小球 A | `(-2.9938904332874152, 1.2315022069431985, -0.66607390034937941)` | `1.3263314607869463` |
| `0a680dea-ef99-4f03-944b-8252d7d16ee9` | 大球 B | `(-0.41971032900437599, 1.2315022069431985, -0.70031663571947766)` | `2.5348365237903314` |

两心距 ≈ 2.574，`|R_B − R_A| < d < R_A + R_B`：不等径相交，非包含。用户图：∪ 只留一带孔球面、∩ 只留一片、B−A 缺凹腔球面。

---

## 1. 一条用例

一条 = **球 A + 球 B + 姿态 + 四条操作期望**。四条：`∪`、`∩`、`A−B`、`B−A`。

`Expect*=false` 必须带 `SkipReason`（`EmptyCsg` 或 `KNOWN_GAP:id`）。空 CSG 与 `ValidateBody` 失败分开。

相交（有体积重叠、非包含）的正确结果：

| 操作 | 拓扑 | 网格 |
|------|------|------|
| ∪ | 2 个球面（各球在对方**外**的剩余），交线缝合，无洞 | 两面都有三角 |
| ∩ | 2 个球面（各球在对方**内**的帽）= 透镜 | 两面都有三角 |
| A−B | A 的外剩余 + B 的内帽（反向） | 凹腔面有三角 |
| B−A | 对称 | 凹腔面有三角 |

结果里应出现**两个不同的球心**（各来自一个操作体）。

---

## 2. Phase A 姿态

| ID | 姿态 | 规格 | 四条 |
|----|------|------|------|
| EqualOffsetZ | 等径中等重叠 | r=1，d=1 沿 +Y | 全开 |
| EqualOffsetX | 等径中等重叠 | r=1，d=1 沿 +X | 全开 |
| EqualDeep | 等径深重叠 | r=1，d=0.5 | 全开 |
| EqualShallow | 等径浅重叠 | r=1，d=1.8 | 全开 |
| UnequalOverlap | 不等径相交 | r=1 与 r=2，d=1.5 | 全开 |
| UntitledUnequalOverlap | 桌面烘焙 | 上表 Guid | 全开 |
| Contained | 大包小 | R=2，r=0.5，d=0.2 | ∪/∩ 开；小−大 = `EmptyCsg` |
| Separate | 分离 | r=1，d=5 | 全关 `EmptyCsg` |
| ExternalTangent | 外切 | r=1，d=2 | 全关 `EmptyCsg` |
| InternalTangent | 内切 | R=2，r=1，d=1 | ∪/∩ 开；小−大 = `EmptyCsg` |

Guid 锁（有 `untitled.xl` 才跑）在 `UntitledSphereSphereBoolean`，与烘焙矩阵分开，不替代矩阵。

拓扑 + 几何指纹锁（不依赖桌面文件）在 `TestBooleanSphereLock.cpp`（`BooleanSphereLock.*`）：烘焙圆心/半径、拓扑计数、AABB、cap 必须为 `SphereSurface`、网格法向与径向对齐（防止“圆锥”显示）。

---

## 3. 缺口台账

| GapId | 症状 | 触发 | 现状 |
|-------|------|------|------|
| `GAP-ss-missing-cap` | ∪ 带孔 / ∩ 单片 / 差缺凹腔 | 不等径相交 | **已关**：`complement_inner` 分类 + 球面环带 lat-long 三角化 |
| `GAP-ss-void` | 大−小应出空腔 | Contained | 无交线，暂不强制空腔 |

新失败先登记再允许 `Expect*=false`。

---

## 4. 验收

- [x] 矩阵每格有用例名；绿或挂 `EmptyCsg` / `KNOWN_GAP`
- [x] untitled 两球烘焙在矩阵里，不依赖桌面文件
- [x] `brep_test_boolean_size_sweep`（84）与 `BooleanLock` 不回退
- [x] 不删红 case
