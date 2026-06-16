# M1 CPU 框架层实施 Lessons (M1.1-M1.8)

> **状态**: 🟢 完成 (2026-06-16, M1 收官)
> **作者**: ChipForge Plugin Team
> **最后修改日期**: 2026-06-16
> **关联**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md` §0, `tests/README.md`
> **范围**: M1 实施中的 B2 摩擦 + 解决方案 + 决策调整

---

## 1. 基线复用 (M1 估时 3-4 d → 实际 1.1 d)

### 1.1 发现过程

M1 启动前进行代码探索 (C1 commit), 发现 cf_plugin Phase 0 已大量实现 M1 任务:

| M1 任务 | 计划描述 | 实际状态 | 节省工作量 |
|---------|----------|----------|------------|
| M1.1 | 扩展 PipeBuilder::at_stage | ✅ 已存在 `pipe_builder.h:69-76` | 1.0 d |
| M1.2 | 扩展 PipeBuilder::declare_substage | ✅ 已存在 `pipe_builder.h:78-86` | 0.5 d |
| M1.3 | PipeLink StageLink | ⚠️ CtrlLink 部分覆盖 | 0.5 d |
| M1.4 | PipeLink DirectLink | ⚠️ CtrlLink 部分覆盖 | 0.3 d |
| M1.5 | PipeArbitration | ❌ 全新 | 0 d |
| M1.6 | PipeNode 集成 arb_ | ⚠️ 5 态方法已存在 | 0.1 d |
| M1.7 | payload_common.h | ❌ 全新 | 0 d |
| M1.8 | 4 框架测试 | ⚠️ 3/4 已存在 | 0 d |

**节省总计**: 2-3 d (从 3-4 d 降到 1.1 d)

### 1.2 教训

- **启动前探索** 是 M1 的关键步骤: 20-30 分钟读 `include/cf/plugin/*.h` 避免 2-3 d 重复工作
- **cf_plugin Phase 0** 是"隐形的已完成 M1 子任务": at_stage/declare_substage 在 Phase 0 已落地, 但 v2.0 M1 任务清单没有标记
- **CtrlLink 的"部分覆盖"**: 蓝图说 PipeLink 是独立基类, 但 CtrlLink 已有 halt/throw/flush/bypass API。 实际兼容路径是"CtrlLink 不动 + PipeArbitration 独立", 不是蓝图预期的 PipeLink 重命名

### 1.3 后续建议

- 每个新 M 阶段启动前, 做 15-20 分钟探索 (看现有代码 + 已有测试)
- 在 M-cpu-skeleton.md 顶部加 "实际状态" 段 (C1 模式), 记录基线复用
- 将基线复用标记为 🟢 基线复用 (而非 🟢 PASS), 区分 "0 实施量" vs "100% 新代码"

---

## 2. PipeNode 5 态方法 + arb_ 字段并存 (M1.6)

### 2.1 设计冲突

蓝图 §6.2.3: "集成 PipeArbitration 到 PipeNode: PipeNode 持有 `PipeArbitration arb_` 成员"

两种实现路径:
- **路径 A**: 5 态方法委托到 arb_ (统一 API, 老代码自动升级)
- **路径 B**: 5 态方法 + arb_ 字段并存 (互不委托, 自由选择)

### 2.2 选择路径 B 的理由

1. **test_pipe_node 14/14 PASS** 用 5 态方法 (assert_valid/assert_ready/...), 改路径 A 会破坏测试
2. **L1CachePlugin 4/4 PASS** 不依赖 PipeNode 状态字段 (用 helper API), 但第三方 Plugin 可能用 5 态方法
3. **5 态与 3 态语义不同**: 5 态是 State 枚举 (IDLE/FIRING/MOVING/BLOCKED/CANCELING), 3 态是 bool 组合。 直接委托会导致语义不一致 (如 `blocked()` 对应 State::BLOCKED, 但 `fired()` 对应 State::MOVING, 不是同一映射)
4. **蓝图只要求 "arb_ 字段"**: 没有要求 "5 态方法委托到 arb_"

### 2.3 实现

```cpp
struct PipeNode {
  // 5 态方法保留 (原状)
  void assert_valid() { state_ = State::FIRING; }
  void assert_ready() { /* ... */ }
  // ...

  // 新增 arb_ 字段 (M1.6)
  const PipeArbitration& arb() const noexcept { return arb_; }
  PipeArbitration& arb_mut() noexcept { return arb_; }
  PipeArbitration& arbitration() noexcept { return arb_; }

 private:
  State state_ = State::IDLE;  // 5 态字段
  PipeArbitration arb_;        // 3 态字段 (M1.6 新增)
};
```

### 2.4 测试验证

`test_pipe_arbitration.cpp` Test 6:
- `n->arb().idle()` → 默认 idle
- `n->arb_mut().set_fired()` → 可写
- `n->arb().fired()` → 读回
- `n->state() == PipeNode::State::IDLE` → 5 态未受影响

### 2.5 教训

- **蓝图是意图, 不是实现约束**: 蓝图说"集成", 但实现方式有多种。 选择最兼容的
- **并存模式** 在框架层是常见策略: 老 API 冻结 + 新 API 并行, 不强制迁移
- 未来如需统一, 可以在 Phase 6+ 做 "deprecated" 标记, 逐步淘汰 5 态方法

---

## 3. D-1 决策修正 (测试目录)

### 3.1 原始决策 (C1 commit)

> **D-1**: 测试结构不与 IP 目录绑定 — 所有测试在 `src/cf_plugin/tests/`, **不是** `ip/ccpu/tests/`。 L1CachePlugin 4/4 测试在 `src/cf_plugin/tests/test_l1_cache_plugin_unit.cpp`。

理由 (C1 时):
- M1 阶段测试全属 cf_plugin (framework 扩展)
- 与 cf_plugin 头文件同区, 便于 include
- 避免新建 `ip/cpu/tlm/tests/` 目录

### 3.2 修正触发

M1 收官后用户反馈: "src/cf_plugin/tests/ 是否要移动到项目根目录, 并按家族划分子目录?"

根本原因: M1 阶段 17 个测试分布 4 家族:
- framework: 9 (Phase 0+1.5 框架)
- cpu: 1 (payload_common M1.7)
- cache: 4 (L1Cache 1.2-1.3d)
- soc: 2 (SoC JSON 1.3b-1.3c)
- bundles: 1 (mem_bundles 1.1)

### 3.3 修正后决策

> **D-1 (修正)**: 测试按家族 (framework/cpu/cache/soc/bundles) 划分子目录, 顶层 `tests/` 是统一入口, 与 `src/` 物理隔离。 详见 `tests/README.md`。

物理路径:
```
tests/
├── framework/  (9 个, cf_plugin 框架)
├── cpu/        (1 个, CPU IP 业务, M2-M5 增长)
├── cache/      (4 个, L1Cache 业务)
├── soc/        (2 个, SoC 集成)
└── bundles/    (1 个, 共享 Bundle)
```

### 3.4 修正过程

1. 创建 5 个子目录 + 6 个 README
2. `git mv` 17 个文件到新位置
3. 改 `src/cf_plugin/CMakeLists.txt` 中 17 个源文件路径 (加 `${CMAKE_SOURCE_DIR}` 前缀)
4. 处理一个 bug: `test_plugin_lifecycle` 没 `if EXISTS` 守卫, 路径漏改
5. 处理另一个 bug: `EXISTS` 路径双前缀 (sed 误替换)

### 3.5 验证

- cmake reconfigure + build 100% 通过
- ctest 18/18 PASS (17 迁移 + 1 既有)
- 17 个 rename 全部由 git 识别

### 3.6 教训

- **启动时决策 vs 过程中决策**: C1 的 D-1 在启动时合理, 但 M1 收官后 17 个测试的分布证明它不适合长期
- **用户反馈是重要信号**: 用户提 "是否要移动" 时, 我应立刻做影响分析, 而不是等 "M2 启动后再说"
- **sed 批量替换需谨慎**: 2 个 sed bug (漏改守卫、双前缀) 本可通过更精确的正则避免
- **git commit --only 的风险**: 只 stage 部分路径, 漏了 delete 的 stage, amend 修正

---

## 4. PipeArbitration 命名空间与位置 (M1.5)

### 4.1 蓝图原意 vs 实际选择

蓝图 §6.2.3: "新建 `ip/cpu/core/pipe_arbitration.h` (独立文件, 避免与 cf_plugin 头文件耦合)"

实际选择: `include/cf/plugin/pipe_arbitration.h` (cf_plugin 框架层)

### 4.2 选择 cf_plugin 层的理由

1. **仲裁是通用框架概念**: 任何 Plugin (CPU/Cache/Memory) 都需仲裁
2. **不应耦合到 IP 层**: 否则每个 IP 都要重新 include 自己的仲裁文件
3. **与 PipeBuilder/PipeNode/CtrlLink 同区**: 保持 cf_plugin 的完整性
4. **L1CachePlugin 未来也需要**: 如果仲裁在 ip/cpu/, L1Cache 无法复用

### 4.3 蓝图与实际的关系

蓝图意图: "独立文件, 不耦合 cf_plugin" → 实际理解为 "不耦合到特定 IP 的 core/ 目录"
实际实现: "放在 cf_plugin 层, 让所有 IP 共享" → 符合蓝图 "独立" 的意图, 只是位置不同

### 4.4 教训

- **蓝图是意图指南, 不是硬编码路径**: "独立文件" ≠ "必须在 ip/cpu/core/"
- **命名空间比文件路径更重要**: `cf::plugin::PipeArbitration` 在框架命名空间, 位置自然应在 cf_plugin 层
- 如果未来需要 ISA 特有仲裁, 可以在 `ip/cpu/core/` 做派生/封装

---

## 5. 编译期 static_assert 陷阱

### 5.1 问题

`test_payload_common.cpp` 初始用 `static_assert` 验证 `Payload::name()`:
```cpp
static_assert(payload::keys_rv32::PC.name() == "cpu.pc", ...);
```

编译失败: `std::string operator==` 不是 `constexpr`。

### 5.2 解决方案

改为运行时 `assert`:
```cpp
assert(payload::keys_rv32::PC.name() == "cpu.pc");
```

### 5.3 教训

- `std::string` 在 C++17 中大部分操作不是 `constexpr` (C++20 部分支持)
- 编译期验证应限于 `sizeof` / `type_traits` / `constexpr` 字面量
- 运行时 `assert` 在单元测试中同样有效
- 头文件 API 的 `name()` 返回 `const std::string&`, 是运行时信息

---

## 6. ctest 基线管理

### 6.1 基线策略

M1 实施中严格遵守 "每步 commit 后 ctest 不退化":
- C1 (文档): 16/16 PASS (0 代码改动)
- C2 (PipeArbitration): 16/16 PASS (新增头文件, 未引用)
- C3 (PipeNode arb_): 16/16 PASS (加字段, 不破坏 5 态方法)
- C4 (payload_common): 16/16 PASS (新增头文件, 未引用)
- C5 (2 测试): 18/18 PASS (2 新 + 16 既有)
- C6 (M1 收官): 18/18 PASS
- 重构 (test mv): 18/18 PASS

### 6.2 测试增长预测

| 阶段 | 新增测试 | 目录 | 预测总测试 |
|------|----------|------|------------|
| M1 收官 | 2 (pipe_arbitration, payload_common) | framework/, cpu/ | 18 |
| M2 | 5 (RegFile/Hazard/IBus/DBus/BranchPredictor) | cpu/ | 23 |
| M3 | 6 (RiscvDecode/IntAlu/Mul/Branch/Lsu/Csr) | cpu/ | 29 |
| M4 | 2 (CpuFactory + 集成) | cpu/ | 31 |
| M5 | 1 (demo_soc e2e) | soc/ | 32 |
| **M5 收官** | | | **~32** |

### 6.3 教训

- **基线不退化** 是 M1 成功的关键: 每次 commit 后跑 ctest, 发现问题立即回退
- `chmod +x` 权限问题: `test_l1_cache_plugin_unit` 在 cmake rebuild 后失去可执行权限, 需要 `chmod +x` 修复。 这是 build 环境问题, 不是代码问题
- 新测试必须加入 `src/cf_plugin/CMakeLists.txt` 的 `add_executable` + `add_test`, 否则 ctest 不识别

---

## 7. 决策编号约定 (D-α/β/γ)

### 7.1 使用模式

M1 实施中使用了 D-α/D-β/D-γ 编号标记关键决策:

- **D-α**: M1 启动授权 (是/否)
- **D-β**: PipeLink 命名 (兼容 CtrlLink vs 重命名)
- **D-γ**: PipeArbitration 位置 (独立小类 vs PipeNode state façade)

### 7.2 效果

- 决策清晰可追踪: 每个决策有编号、选项、理由、最终选择
- 便于回查: 在 commit message 和 lessons 文档中引用
- 用户参与: 3 个决策都经过用户确认 (A+A+A)

### 7.3 建议

后续 M2-M5 继续使用 D-编号标记关键决策, 格式:
```
D-<阶段编号>.<序号>: <决策主题>
  选项 A: ...
  选项 B: ...
  选择: <选项>
  理由: ...
```

---

## 8. 相关文档

- **M1 实施计划**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
- **测试架构**: `tests/README.md`
- **D-1 修正**: `tests/README.md` §2 "与之前架构的对比"
- **M2 详细**: `ip/cpu/docs/implementation-plan/M2-core-plugins.md`
- **M3 详细**: `ip/cpu/docs/implementation-plan/M3-riscv-plugins.md`
- **总体实施规划**: `ip/cpu/docs/implementation-plan/README.md`
- **L1Cache Lessons**: `docs/lessons/phase-1.2-l1cacheplugin.md`
