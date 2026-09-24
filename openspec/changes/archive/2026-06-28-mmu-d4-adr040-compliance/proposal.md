## Why

`tools/verify_plugin_decision.sh` (Architecture Gate, ADR-043) 当前在 `ctest` 中失败 — 阻塞 PR 合并。失败由 `ip/mmu/` (mmu-ip-skeleton, untracked) 引入的两类违规导致：(1) D4 检查 #3 命中 `ip/mmu/lib/tlb.h` 的 4 处局部变量 `uint64_t tag`（grep 模式匹配 Bundle 字段名）；(2) ADR-040 检查 #1 命中 `ip/mmu/tlm/MMUPlugin.cpp` 的 7 处 at_stage 回调内 `if (cond) return;` 早返。

修复这两个静态检查违规即可让 `verify_plugin_decision` 重回 PASS，不引入任何运行时行为变更。

## What Changes

- **`ip/mmu/lib/tlb.h`** (4 处): 把 `lookup()` / `insert()` / `insert_from()` / `invalidate_vaddr()` 中的 `const uint64_t tag = ...` 改为 `const typename Entry::tag_type tag = ...` — `Entry::tag_type` 已是 `cf::plugin::uint_t<TAG_BITS>` (HDL-friendly)，与现存的 `entries_[idx].tag = static_cast<typename Entry::tag_type>(tag);` 模式一致，零行为变更。
- **`ip/mmu/tlm/MMUPlugin.cpp`** (5 个 at_stage 闭包, 7 处): 把 `if (!n) return;` / `if (!(*n)(K8::PTW_ACTIVE)) return;` 改为 `if (n) { ... }` / `if ((*n)(K8::PTW_ACTIVE)) { ... }` 包裹主逻辑，符合 ADR-040 Tier-1 §2.1 "at_stage 回调完整执行" 强制。
- 修复后：`ctest --test-dir build` 期望 43/43 通过（含 `verify_plugin_decision`）。

## Capabilities

### New Capabilities
无 — 本次仅为合规修复，不引入新需求/契约。

### Modified Capabilities
无 — REQUIREMENTS 未变（仅类型细化与控制流重构，行为等价）。`openspec/specs/` 下无 mmu 专属 spec 文件，行为契约已隐含在 `ip/mmu/STATUS.md` + `ip/mmu/README.md` + `ip/mmu/docs/architecture.md` 中，无需 delta spec。

## Impact

| 项 | 影响 |
|---|---|
| **修改文件** | `ip/mmu/lib/tlb.h` (+0 / -0 行 net, 4 处 token 替换) ; `ip/mmu/tlm/MMUPlugin.cpp` (+8 / -4 行 net, 5 个闭包重写) |
| **运行时行为** | 零变化（类型相同位宽；控制流等价 `early-return-on-false` ↔ `guard-on-true`） |
| **API/ABI** | 无变化 |
| **依赖** | 无新增 |
| **CI / Architecture Gates** | `verify_plugin_decision.sh` 从 FAIL → PASS ; 其他 ADR-040 检查项 (ch_mem 渗透 / pb.run / array_store) 仍 PASS（已确认） |
| **测试** | 现有 `tests/mmu/` 测试无需修改；通过 `ctest` 全量回归验证 (43/43 期望) |
| **风险** | 低 — 类型替换保证位宽相同（`Entry::tag_type` 在 `tlb_entry.h` 定义）；控制流重构保持 `n == nullptr` 与 `PTW_ACTIVE == false` 时为 no-op 的语义 |