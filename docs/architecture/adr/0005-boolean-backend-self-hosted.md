# ADR 0005: 布尔后端继续自研（暂不引入 OCCT）

**状态**：已接受  
**日期**：2026-08-11  
**前置**：设计 [2026-08-10-analytic-sphere-brep-boolean-design.md](../../superpowers/specs/2026-08-10-analytic-sphere-brep-boolean-design.md) Phase 5；评估 [2026-08-11-boolean-backend-evaluation.md](../../superpowers/specs/2026-08-11-boolean-backend-evaluation.md)（T5.1）

## 背景

Phase 1～4 已交付解析球/柱、盒与部分弯曲布尔、面 AABB BVH 与宽相诊断挂钩。Phase 5 需在下列方向中择一写明：

- 继续自研，逐步走向通用曲面 / NURBS；或  
- 在 `IBooleanEvaluator` 后增加可选 OCCT 后端（UI/特征不变）。

产品层此前已锁定「只参考 OCCT 理念、自行实现」；本 ADR 在 Phase 1～4 落地后**再确认**是否改弦更张。

## 决策

1. **继续自研布尔与几何内核**作为唯一默认后端；构建**不链接 OCCT**（维持 TX.1）。  
2. **不实施 T5.2**（OCCT 适配器）于本决策周期内。  
3. NURBS：**延后**到解析曲面求交、通用印记/分类与弯曲扩展（如 T3.7/T3.8）更稳之后再开专项；禁止在通用布尔未通时并行铺开 NURBS 布尔。  
4. 保留 `IBooleanEvaluator` / factory 可替换性；若未来要以 OCCT 做互通或对标，**修订本 ADR** 后再开 T5.2。

## 理由

- 与学习型 B-Rep 内核目标一致：流水线阶段需自研可观测，而非外包给黑盒。  
- 现有特解与 BVH 宽相已形成可扩展骨架；引入 OCCT 会分流维护并触发许可/构建成本，收益主要在「快速复杂布尔」，与当前阶段出口不匹配。  
- 过早 NURBS 与设计风险表「范围膨胀」冲突；先巩固解析 + 通用拓扑路径。

## 后果

- **做**：按自研路线推进缺口（印记/分类、弯曲扩展、再评估 NURBS）。  
- **不做（现周期）**：OCCT submodule/包依赖、拓扑往返适配器、双后端 CI。  
- **文档**：T5.1 评估见上链 specs；设计清单 T5.1/T5.3 勾选完成；T5.2 保持未做/延期。  
- **修订触发**：明确需要 STEP/互通验收、或自研通用布尔成本不可接受时，重开决策。
