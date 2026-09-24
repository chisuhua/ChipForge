## Context

`mmu-ip-skeleton` change (2026-06-29) 在 `ip/mmu/` 落地了 MMU IP 的骨架代码，但**未通过**项目自身的两个架构门禁脚本：

| 脚本 | 检查 | 失败位置 | 根因 |
|------|------|---------|------|
| `tools/verify_plugin_decision.sh` (D4 #3) | Bundle 字段必须 `cf::plugin::uint_t<N>` | `ip/mmu/lib/tlb.h:73/98/146/189` | 4 处局部变量 `uint64_t tag` 被 grep 模式误命中为 Bundle 字段 |
| `tools/check_plugin_portability.sh` (ADR-040 §2.1 #1) | at_stage 回调内无 `return;` 早返 | `ip/mmu/tlm/MMUPlugin.cpp:47/73/93/94/102/103/111/112` | 7 处 `if (!n) return;` / `if (!(*n)(K8::PTW_ACTIVE)) return;` |

`ctest --test-dir build` 当前 42/43 通过 — 唯一失败为 `verify_plugin_decision` (Architecture Gate, ADR-043, PR 阻塞)。

## Goals / Non-Goals

**Goals:**
1. 修复 `ip/mmu/lib/tlb.h` 的 4 处类型违规 — 零行为变更
2. 修复 `ip/mmu/tlm/MMUPlugin.cpp` 的 7 处早返违规 — 零行为变更
3. `verify_plugin_decision` 重回 PASS，`ctest` 43/43 全绿
4. 现有 `tests/mmu/` 测试无需修改（已 GREEN 状态保持）

**Non-Goals:**
1. 不重写 `MMUPlugin` 的逻辑 / 不优化 PTW 算法（推迟 `mmu-tlb-ptw-impl`）
2. 不改 verifier 脚本（脚本的 grep 精度是已知限制，business code 适应检查）
3. 不引入新 spec capability（合规修复不改变 REQUIREMENTS）
4. 不修复其他 IP（cache/cpu 等）的 D4/ADR-040 违规（无当前违规）

## Decisions

### Decision 1: 局部变量 `tag` 用 `Entry::tag_type` 替换 `uint64_t`

**Why**:
- `tlb_entry.h` 已定义 `Entry::tag_type` 为 `cf::plugin::uint_t<TAG_BITS>`（HDL-friendly）
- 现有代码 4 处已经在用：`entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);`（line 119/136/164/180）
- 类型位宽保证一致（`uint_t<TAG_BITS>` 在 TAG_BITS=52 时为 `uint64_t` 的子集，转换无损）
- 零运行时差异（编译期类型调整）

**Alternatives considered**:
- ❌ 改 verifier 脚本（用 AST/精确字段定义检测）— 超出范围，verifier 是架构资产
- ❌ 用 `cf::plugin::uint_t<TAG_BITS>` 直接 — `Entry::tag_type` 已是正确别名，更可读
- ❌ 把 tag 提升为成员变量 — 改变函数签名，违反最小修改

### Decision 2: `if (!cond) return;` → `if (cond) { ... }` 包裹主逻辑

**Why**:
- ADR-040 Tier-1 §2.1 强制"at_stage 回调完整执行"（HDL 1:1 映射约束）
- 重构保持语义等价：`cond == false` 时主逻辑跳过，副作用为 no-op
- `n == nullptr` 时原本 `return;` → 现在跳过 `if (n)` 块，行为一致
- `PTW_ACTIVE == false` 时原本 `return;` → 现在跳过 `if (PTW_ACTIVE)` 块，行为一致

**重构模板**（PTW 三级 walk 例）：
```cpp
// before
pb.at_stage("ptw_l0", Phase::NORMAL, [this, &pb]() {
  auto* n = pb.node_of_logic_stage("ptw_l0");
  if (!n) return;
  if (!(*n)(K8::PTW_ACTIVE)) return;
  // ... main
});

// after
pb.at_stage("ptw_l0", Phase::NORMAL, [this, &pb]() {
  auto* n = pb.node_of_logic_stage("ptw_l0");
  if (n && (*n)(K8::PTW_ACTIVE)) {
    // ... main
  }
});
```

**Alternatives considered**:
- ❌ 用 `do { ... } while(0)` + `break` — 仍含 `return/break`，awk 检测器可能命中（虽然脚本只看 `return;`，但语义上 break 是控制流破坏）
- ❌ 提取 `at_stage` 主逻辑到私有方法 `process_ptw_l0()` 然后 `if (cond) process_ptw_l0();` — 增加间接层，对骨架阶段过度工程
- ❌ 强制 `node_of_logic_stage` 永远非 null（断言）— 改变契约，超出范围

### Decision 3: 不创建 OpenSpec spec 文件

**Why**:
- 本次变更为纯合规修复（grep 检查误命中 + 控制流等价重构），不引入新 REQUIREMENTS
- 按 `openspec/specs/` 已有结构（如 `arch-doc-consistency-baseline/spec.md`），spec 文件用于"系统应该做什么"
- 本次"系统应该做什么"未变；唯一变化是"实现如何通过静态检查"
- `proposal.md` 已说明此决策并在 `Impact` 中明确"API/ABI 无变化"

## Risks / Trade-offs

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| `Entry::tag_type` 位宽与 `uint64_t` 不同 → 隐式截断 | Low | `Entry::tag_type = cf::plugin::uint_t<TAG_BITS>`，当 `TAG_BITS <= 64` 时无损转换；现有 `static_cast<typename Entry::tag_type>(tag);` 4 处已在用，无 warning |
| `if (n && (*n)(...) ) { ... }` 重构破坏原 `n == nullptr` 防御 | Low | 条件包裹等价于原早返；可读性更差但行为一致；现有 `tests/mmu/` 测试 GREEN 状态作为最终验证 |
| PTW callback (`ptw_->start_walk`) 在重构后仍正确触发 | Low | `ptw_->start_walk` 在 `if (r.hit) ... else` 内调用，重构仅在最外层包裹，不影响 PTW 启动路径 |
| `node_of_logic_stage` 返回 null 是合法运行时状态 | Medium | 重构保留 `if (n)` 保护，与原行为一致；如未来想强制 non-null，需新 spec 提案 |

## Migration Plan

**无部署步骤** — 本次变更为本地代码修改，CI 流程触发：

1. 修改 `ip/mmu/lib/tlb.h` (4 处 token 替换)
2. 修改 `ip/mmu/tlm/MMUPlugin.cpp` (5 个 at_stage 闭包)
3. 运行 `ctest --test-dir build` 验证 43/43 PASS
4. 提交：`git add` → `git commit` (如用户授权)

**回滚策略**：单一 git revert 即可，无状态迁移。

## Open Questions

无 — 所有变更路径明确。