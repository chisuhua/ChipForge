---
name: "verify-architecture"
description: "验证 CppTLM/CppHDL/ChipForge 架构设计是否与代码实现对齐 — 融合机械检查（脚本）+ 设计意图审查（语义）+ 战略复查（架构适配性）三层"
when_to_use: "当用户询问'架构是否漂移'、'设计意图是否还对'、'Plugin 模型是否合适'、'ADR 该升级吗'、'重构前评估'、'升级 CppTLM/CppHDL 后是否还对齐'、'新功能是否违反原架构'，或'PR 涉及架构变更'时"
---

# 架构验证技能（Verify Architecture）

## 概述

本技能提供**三层递进**的架构验证流程，从机械到语义到战略：

```
┌─────────────────────────────────────────────────────────────┐
│ Layer 3: 战略复查 (Strategic Review)                        │
│   - 设计是否仍适合新需求？                                    │
│   - 是否有更好的替代方案出现？                                 │
│   - 非目标是否被新代码违反？                                   │
│   频率：每季度 / 大版本升级前                                 │
├─────────────────────────────────────────────────────────────┤
│ Layer 2: 设计意图审查 (Design-Intent Review)                │
│   - 抽象层级是否合理？                                         │
│   - 命名是否清晰一致？                                         │
│   - 依赖方向是否正确？                                         │
│   - 性能假设是否仍成立？                                       │
│   频率：每月 / 重大 ADR 变更后                                │
├─────────────────────────────────────────────────────────────┤
│ Layer 1: 机械漂移检测 (Mechanical Drift Detection)         │
│   - 路径/类名/宏是否仍正确？                                  │
│   - 注册表是否完整？                                           │
│   - 继承关系是否仍对？                                         │
│   频率：每次 PR / CI                                          │
└─────────────────────────────────────────────────────────────┘
```

**Layer 1 由 `tools/verify_adr.sh` 自动执行**。**Layer 2/3 必须由 AI 代理（你）执行**，
因为它们涉及阅读代码、判断意图、推理权衡。

---

## 何时使用本技能

**触发场景**（按频率）：

| 场景 | 必需层级 | 期望动作 |
|------|----------|----------|
| PR 包含 `CppTLM/` 或 `CppHDL/` 修改 | L1 | 仅运行 `verify_adr.sh` |
| PR 包含 `docs/architecture/adr.md` 修改 | L1 + L2 | 脚本 + 评审 ADR 改动 |
| 升级 CppTLM/CppHDL 子模块 | L1 + L2 | 脚本 + 检查新 API 引入的设计变化 |
| 季度架构审查 | L1 + L2 + L3 | 全部三层 |
| 新增 IP 类型（如新 Cache 一致性协议） | L1 + L2 | 脚本 + 评估 Plugin 模型适配性 |
| 评估"重写 Plugin 模型"提案 | L2 + L3 | 重点审查设计意图 + 战略 |
| 收到"X 设计不再合理"的反馈 | L2 + L3 | 重点审查特定 ADR |

**不要使用本技能的场景**：
- 用户要求"快速 grep 一下文件"
- 用户要求"列出所有 TODO"
- 单文件 bug 修复（与架构无关）

---

## 必需的前置资源

- `docs/architecture/adr.md` — 38 条 ADR 记录
- `docs/architecture/code-framework-mapping.md` — 框架层 API 权威清单
- `docs/architecture/declarative-hybrid-framework.md` — 应用层设计意图
- `tools/verify_adr.sh` — 机械验证脚本（已存在）

---

## 工作流（三层）

### Phase 0：上下文收集

在执行任何验证前，先建立上下文：

```bash
# 1. 确认 ADR 文档存在且最新
test -f docs/architecture/adr.md && wc -l docs/architecture/adr.md

# 2. 确认 verify_adr.sh 可执行
test -x tools/verify_adr.sh

# 3. 快速浏览 ADR 文档结构
grep -E "^##? " docs/architecture/adr.md

# 4. 了解 CppTLM/CppHDL 当前 HEAD
cd /workspace/project/CppTLM && git log --oneline -5
cd /workspace/project/CppHDL && git log --oneline -5
```

**输出**：上下文摘要（ADR 总数、脚本可用性、最近变更）

---

### Phase 1：机械漂移检测（L1）

```bash
cd /workspace/project/ChipForge
bash tools/verify_adr.sh --verbose 2>&1 | tee /tmp/adr_verify_output.txt
```

**解读结果**（4 种状态）：

| 状态 | 含义 | 后续动作 |
|------|------|----------|
| ✅ PASS | 代码与 ADR 一致 | 无需动作 |
| 🚧 EXPECTED_MISSING | Phase 1 提案，缺失符合预期 | 记录进度（可选） |
| ⚠️ STALE | 代码已先于文档实现 | 立即更新文档 |
| ❌ FAILED | Critical 漂移 | 立即诊断（Phase 2-A） |

**Phase 1 完成后产出**：
- 漂移报告（机械部分）
- 失败 / 陈旧 ADR 清单
- 退出码（0=健康，1=Critical 漂移）

---

### Phase 2-A：诊断（针对 L1 失败）

对每个 ❌ FAILED ADR：

1. **读取 ADR 详情**：
   ```bash
   # 查看具体 ADR
   sed -n '/^#### ADR-XXX/,/^#### /p' docs/architecture/adr.md
   ```

2. **运行 ADR 内置验证命令**：
   ```bash
   # ADR 文档每个记录都包含"验证命令"代码块
   # 直接复制执行
   ```

3. **诊断根因**（三种可能）：
   - **代码改动**（最常见）：上游重构，类/宏/路径变更 → 回滚或适配
   - **文档笔误**（少见）：ADR 路径写错 → 修正 ADR（同时修正脚本）
   - **验证命令 bug**（罕见）：脚本正则错误 → 修正脚本

4. **判断是否影响架构**：
   - 若仅是机械漂移（路径变更）→ 修复并继续
   - 若触及设计意图（类删除、API 替换）→ 升级到 Phase 2-B

**输出**：每条 FAILED ADR 的根因 + 修复方案

---

### Phase 2-B：设计意图审查（针对重大变更）

**触发条件**（满足任一）：

- L1 输出 FAILED 数量 > 5
- 升级 CppTLM/CppHDL 后
- 新增/废弃主要 API
- 用户明确询问"X 设计是否还对"

**审查清单**（11 项）：

#### 1. 抽象层级（Abstraction Level）

```bash
# 找：一个开发者实现新 IP 需要理解多少层抽象？
# 对比：CppTLM SimObject → TLMModule → ChStreamModuleBase 是几层？
# 对比：ChStreamModuleBase::set_stream_adapter 是低层还是高层 API？
```

**判断标准**：
- ✅ 合理：每层抽象有清晰边界，新 IP 开发者无需理解全部
- ❌ 过高：开发者需学习 5+ 层才能写新 IP
- ❌ 过低：抽象层只做包装，无附加价值

**ChipForge 现状评估**：
- 框架层（CppTLM/CppHDL）抽象合理
- 应用层（Plugin 模型）尚未实现，抽象层级不可评估

#### 2. 命名一致性（Naming Consistency）

```bash
# 检查命名约定是否统一
# CppTLM: CacheTLM, MemoryTLM, CrossbarTLM, CPUTLM（驼峰 + TLM 后缀）
# CppHDL: ch_uint, ch_reg, ch_mem, ch_bool（小写 + 下划线）
# 应用层: Plugin, PipeNode, PipeBuilder（驼峰无后缀）
```

**检查项**：
- [ ] 同类组件命名约定是否一致？
- [ ] CppTLM 的类名与 CppHDL 的类型名风格是否协调？
- [ ] Plugin 模型类名（`Plugin` / `PipeNode` / `CtrlLink`）是否易理解？
- [ ] 是否需要术语表（GLOSSARY.md 已存在，需检查覆盖度）？

#### 3. 依赖方向（Dependency Direction）

```bash
# CppHDL 不应依赖 CppTLM
grep -r "include.*CppTLM\|include.*cpptlm" /workspace/project/CppHDL/include 2>/dev/null
# 输出应为 0 行

# ChipForge 不应被 CppTLM/CppHDL 引用
grep -r "include.*ChipForge\|include.*chipforge" /workspace/project/CppTLM /workspace/project/CppHDL 2>/dev/null
# 输出应为 0 行
```

**判断标准**：
- ✅ CppTLM → CppHDL 单向依赖（合理）
- ✅ ChipForge → CppTLM + CppHDL 单向依赖（合理）
- ❌ 反向依赖（违反分层）

#### 4. 性能假设（Performance Assumptions）

```bash
# 检查 ADR 中是否有性能数字声称
grep -E "[0-9]+\s*MIPS|MHz|ns|cycle" docs/architecture/*.md
```

**评估**：
- "10-100 MIPS" 是否有 benchmark 支撑？
- 如果从未 benchmark 过，应标注"TBD"或删除
- 不要保留未经测试的性能数字

#### 5. API 人体工学（API Ergonomics）

```bash
# 计算：实现一个新 IP 需要多少行样板代码？
# 找到 CppTLM 标准 IP 模板（CacheTLM）的最小完整示例
wc -l /workspace/project/CppTLM/include/tlm/cache_tlm.hh
```

**评估**：
- 一个 100 行的样板才能写一个简单 IP？→ 可能太复杂
- 一个 30 行的样板就能写？→ 简洁
- 同一类操作有多种 API？→ 减少选择

#### 6. 非目标合规（Non-Goal Compliance）

检查架构的隐式非目标：

| 隐式非目标 | 验证方法 |
|------------|----------|
| 不使用 RTTI（运行时类型信息） | `grep "dynamic_cast" CppTLM/src` — 应仅出现在必要的边界 |
| 不引入外部依赖（除 C++ std） | `find -name "CMakeLists.txt" \| xargs grep "find_package"` |
| 头文件尽量自包含 | `grep "include.*\\.\\." include/**/*.hh` |
| 不使用宏编程做用户 API | 已违反：CH_BUNDLE_FIELDS_T 宏族 |

#### 7. 边界清洁度（Boundary Cleanliness）

```bash
# CppTLM 不应暴露 CppHDL 类型，反之亦然
grep -E "include.*CppHDL|ch_uint|ch_reg" /workspace/project/CppTLM/include/core 2>/dev/null | head -5
# 允许：tlm_bundle_converter.h 引用 CppHDL（设计允许）

# 检查头文件自包含性
for f in /workspace/project/CppTLM/include/core/*.hh; do
  g++ -I CppTLM/include -c "$f" 2>/dev/null && echo "OK: $f" || echo "BROKEN: $f"
done | grep BROKEN
```

#### 8. 复杂度正当性（Complexity Justification）

对每个 Phase 1 提案，问：
- 这个抽象的复杂度值得吗？
- 不引入这个抽象，开发者会少写多少代码？
- 引入后调试/理解成本增加多少？

例如：`PipeBuilder` / `Plugin` / `Payload<T>` 三件套带来的复杂度，
是否被"用 JSON 配置即可切换流水线深度"的价值所抵消？

#### 9. 演化能力（Evolution Capacity）

- [ ] 设计能否容纳新的 IP 类型（GPU、DSP、加速器）？
- [ ] 设计能否容纳多 ISA（RISC-V + ARM，已在 `ip/cpu/docs/multi_isa_architecture.md` 提及）？
- [ ] 设计能否支持新的仿真模式（QEMU 集成、FPGA 仿真）？
- [ ] 设计能否扩展到大 SoC（>100 IP 实例）？

#### 10. 替代方案评估（Alternative Evaluation）

**当前 ChipForge 设计的替代方案**：

| 替代 | 优势 | 劣势 | 当前选择 |
|------|------|------|----------|
| SystemC TLM 2.0 | 行业标准 | 学习曲线陡、Python 集成差 | ❌ 未采用 |
| SpinalHDL | HDL 表达力强 | Scala 工具链、与 C++ 集成难 | ❌ 未采用 |
| Bluespec | 高阶 HDL | 工具链封闭、文档少 | ❌ 未采用 |
| Chisel/FIRRTL | 生成式、Scala 生态 | 需要 Scala 工具链 | ❌ 未采用 |
| 纯 C++ 自研 | 简单、可控 | 重新造轮子 | ✅ 当前选择 |

**问题**：当前选择是否仍合理？有更好的方案出现吗？

#### 11. 错误处理与调试能力

```bash
# 框架是否提供良好的错误消息？
grep -rE "CHERROR|assert\(" /workspace/project/CppHDL/include 2>/dev/null | wc -l
# 0 命中可能意味着错误处理不足
```

**评估**：
- 编译错误是否友好？（模板错误信息晦涩？）
- 运行时错误是否定位精确？
- 是否有 trace / debug 工具？（已有 DebugTracker）

---

### Phase 2-B 完成后产出

- 设计意图审查报告（11 项清单的结果）
- 重大设计问题的清单（如有）
- ADR 升级建议（哪些 🚧 应改为 ❌ 或 ✅）

---

### Phase 3：战略复查（L3）

**触发条件**（满足任一）：

- 用户要求"季度架构审查"
- 评估"重写 Plugin 模型"
- 评估"切换到 SpinalHDL"
- 新增重大需求（如多 ISA 协同仿真）

**审查问题**（5 个战略性问题）：

1. **三年视角**：当前架构在 3 年后是否仍合理？
2. **采用成本**：一个新开发者需要多久才能上手？1 周？1 月？3 月？
3. **失败模式**：架构的最薄弱环节是什么？Plugin 模型未实现？文档与代码背离？
4. **退出成本**：如果决定重写，迁移成本多大？
5. **生态位**：ChipForge 在 RISC-V 虚拟原型工具中定位是什么？与 Gem5、Spike、ETISS 的差异？

**输出**：战略评估报告 + 行动建议

---

## 输出模板

### 完整验证报告结构

```markdown
# 架构验证报告

**日期**：YYYY-MM-DD
**触发**：<场景>
**ADR 版本**：v1.0
**脚本版本**：v1.0

## Phase 1：机械检查
- ✅ PASS：N
- 🚧 EXPECTED_MISSING：N（Phase 1 提案）
- ⚠️ STALE：N
- ❌ FAILED：N

[详细列表]

## Phase 2-A：失败诊断
### ❌ FAILED ADR-XXX
- 根因：[代码改动/文档笔误/验证命令 bug]
- 影响：[架构层面/仅机械]
- 修复方案：[具体动作]

## Phase 2-B：设计意图审查（如适用）
### 11 项检查结果
1. 抽象层级：[✅ 合理 / ⚠️ 需关注 / ❌ 问题]
2. 命名一致性：[...]
...

### 重大设计问题
- [问题描述 + 建议]

## Phase 3：战略复查（如适用）
### 战略性问题回答
1. 三年视角：[回答]
...

### 行动建议
- [优先级 + 描述]

## 下次审查
- 时间：YYYY-MM-DD
- 触发：<场景>
```

---

## 使用示例

### 示例 1：PR 包含架构变更

```
用户：我刚修改了 `CppHDL/include/component.h`，加了 `ch_async_reset` 支持

AI（使用本技能）：
1. Phase 0：确认上下文（component.h 存在，最近有修改）
2. Phase 1：运行 verify_adr.sh
   → 24 PASS, 14 EXPECTED_MISSING, 0 STALE, 0 FAILED
3. Phase 2-A：0 个 FAILED，无需诊断
4. Phase 2-B：因为引入了新 API，需要评估：
   - ch_async_reset 是否对应 ADR-010（ch_reg<T>）的扩展？
   - 文档 ADR-010 是否需要更新以反映新 API？
5. 输出建议：
   - ADR-010 状态保持 ✅
   - 但 ADR 文档应补充"ch_reg 扩展点"小节
   - 添加 ADR-039 记录 `ch_async_reset` 新增决策
```

### 示例 2：季度架构审查

```
用户：开始季度架构审查

AI（使用本技能）：
1. Phase 0：确认上下文
2. Phase 1：运行 verify_adr.sh（baseline）
3. Phase 2-B：执行 11 项检查清单：
   - 抽象层级：合理（CppTLM 3 层，开发者 1 周可上手）
   - 命名一致性：发现问题 — CppTLM 用 CamelCase，CppHDL 用 snake_case，
     Plugin 模型用 CamelCase 但无后缀。需统一约定。
   - 依赖方向：正确
   - 性能假设：TBD — 无 benchmark 数据
   - 复杂度：Plugin 模型未实现，复杂度不可评估
   - 演化能力：可容纳新 IP，多 ISA 设计草案存在
   - 替代方案评估：自研路线仍合理，但需关注 SpinalHDL 生态
4. Phase 3：战略评估
   - 三年视角：Plugin 模型是关键，如果再不实施，3 年后将面临架构代际差距
   - 失败模式：文档与代码背离、Plugin 模型缺位
5. 输出完整报告 + 行动建议
```

### 示例 3：评估重写 Plugin 模型

```
用户：我们在考虑用 LLVM MLIR 重写 Plugin 模型，你怎么看？

AI（使用本技能）：
1. Phase 0：上下文
2. Phase 1：baseline
3. Phase 2-B：重点审查 Plugin 模型的复杂度正当性
   - 当前 Plugin 模型复杂度：5 个新类（Plugin/PipeNode/PipeLink/PipeBuilder/Payload）
   - 价值：声明式 IP 描述
   - 替代方案：LLVM MLIR
     - 优势：成熟生态、多 dialect、可视化工具
     - 劣势：构建复杂度高、学习曲线陡
4. 输出对比表 + 建议
```

---

## 与其他技能的关系

- **`cpp-architecture`**：本技能的"机械检查"部分借鉴了 `cpp-architecture` 的依赖分析方法
- **`regression-bisect`**：当 L1 检查发现"昨天还能过，今天就 FAILED"时，使用 `regression-bisect` 定位漂移引入点
- **`planning-with-files`**：Phase 2-B 的审查报告建议写入 `state/adr-review-YYYYMMDD.md`

---

## 维护规则

1. **本技能**与 `tools/verify_adr.sh` 配套使用
2. **新增 ADR 时**：在 `docs/architecture/adr.md` 中添加详细记录 + 在技能中添加对应的 Phase 2-B 检查项
3. **废弃 ADR 时**：从 `docs/architecture/adr.md` 移除（不保留历史），从技能检查清单中移除对应项
4. **每月审计**：跑一次完整三层流程，更新 ADR 状态
5. **CI 集成**：仅 Phase 1 接入 CI；Phase 2/3 由 PR 评审或季度审查触发

---

## 参考

- `docs/architecture/adr.md` — 38 条 ADR 详细记录
- `docs/architecture/code-framework-mapping.md` — 框架层 API 清单
- `docs/architecture/declarative-hybrid-framework.md` — 应用层设计
- `tools/verify_adr.sh` — Phase 1 机械验证脚本
- `~/.config/opencode/skills/cpp-architecture/SKILL.md` — 通用 C++ 架构分析
