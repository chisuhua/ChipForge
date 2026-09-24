# Tasks: cache-policy-foundation

> **总工时**: ~6h
> **执行模式**: 主分支直接改（避免 worktree 隔离，理由：变更范围小且前向兼容）
> **依赖**: CHANGE-002 (ip-catalog-status-correct) 完成后 cache 状态描述稳定；v0.0.5 (empty-directory-cleanup) 已落地
> **基线**: 当前 16/16 ctest PASS（CHANGELOG v0.0.5 + Phase 1.3d-extras 后）

## 0. 预执行（已落地）

- [x] 0.1 修正 `ip/cache/tlm/L1CachePlugin.h` L18 注释：`256 sets × 64-byte = 32KB` → `256 sets × 1 way × 64B = 16KB`（原 32KB 是 8-way 误算）
- [x] 0.2 archive v1 原稿到 `openspec/changes/archive/2026-06-18-cache-policy-foundation-v1-original/`

## 1. 实现 ReplacementPolicy 抽象接口

- [x] 1.1 创建 `ip/cache/policies/replacement_policy.h`（4 虚方法 + 1 工厂方法 + Doxygen 注释 + 命名空间 `cf::ip::cache::policies`）
- [x] 1.2 验证 `wc -l ip/cache/policies/replacement_policy.h` ~50 LOC
- [x] 1.3 验证 `grep -c "virtual" ip/cache/policies/replacement_policy.h` = 4

## 2. 实现具体策略

- [x] 2.1 创建 `ip/cache/policies/no_replacement_policy.h`（3 方法 no-op + name() 返回 "None"）
- [x] 2.2 创建 `ip/cache/policies/lru_policy.h`，文件顶部必须包含 Doxygen 注释块：
  ```
  /** @file lru_policy.h
   *  @brief REFERENCE IMPLEMENTATION ONLY
   *
   *  适用于 L1CachePlugin 1-way 直接映射（256 sets × 1 way）。
   *  Phase 1.5 L2CachePlugin 实施时此文件将被 8-way 完整 LRU 整体替换，
   *  不做向前兼容迁移。如需 L2 LRU 请等待 Phase 1.5。
   */
  ```
- [x] 2.3 LRUPolicy 实现：`select_victim(set)` 返回 0；`on_access` 增加全局 counter；`on_insert` 记录 timestamp；`name()` 返回 "LRU"
- [x] 2.4 验证两个策略都能编译：`g++ -c -std=c++17 -I ip/cache/policies no_replacement_policy.h lru_policy.h` 不报错（虽然 .h 无 .cpp）

## 3. 集成到 L1CachePlugin

- [x] 3.1 修改 `ip/cache/tlm/L1CachePlugin.h` 构造签名：`explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr)`
- [x] 3.2 添加 `#include "ip/cache/policies/replacement_policy.h"` 和 `std::unique_ptr<ReplacementPolicy> policy_` 成员
- [x] 3.3 修改 `L1CachePlugin.cpp` 在 ctor 中：`if (!policy_) policy_ = std::make_unique<NoReplacementPolicy>();`
- [x] 3.4 在 `lookup` 阶段 `at_stage` 回调中调用 `policy_->on_access(set, way)`（D4 合规：在 at_stage 内调用）
- [x] 3.5 验证现有 4 个 L1Cache 测试 (`tests/cache/test_l1_cache_*.cpp`) 100% PASS

## 4. 单元测试

- [x] 4.1 创建 `tests/cache/test_replacement_policy.cpp`（注意：`tests/cache/` 不是 `ip/cache/test/`，遵守 v0.0.5 约定）
- [x] 4.2 5 个测试：factory-create-LRU / factory-create-None / factory-unknown-throws / LRU-on-access-increments / NoReplacement-victim-returns-zero
- [x] 4.3 在 `tests/cache/CMakeLists.txt`（如果不存在则创建）注册测试
- [x] 4.4 验证 `ctest --test-dir build -R replacement` 5/5 PASS
- [x] 4.5 验证全量 ctest 基线：现有 16/16 + 新增 5 = **21/21** PASS（不要用 v1 的 "51 plugin + 4 L1Cache + 5 replacement = 60/60"，该数字无来源）

## 5. 文档同步

- [x] 5.1 **同步修正** `ip/cache/README.md §4` "可插拔策略"表格：将"256 sets × 64-byte cache line = 32KB L1"改为"256 × 1 way × 64B = 16KB L1 (direct-mapped)"
- [x] 5.2 **同步修正** `ip/cache/README.md §5` "配置参数"表格：`capacity_kb` 默认值从 32 改为 16
- [x] 5.3 **更新** `ip/cache/README.md §4` 表格下方加注："✅ Phase 1.4 落地：NoReplacementPolicy + LRUPolicy 接口契约（详见 `ip/cache/policies/`）"
- [x] 5.4 **更新** `docs/architecture/overview.md §"可插拔策略模式"` L1/L2 Cache 行加 "✅ Phase 1.4 落地 (NoReplacementPolicy + LRUPolicy)"
- [x] 5.5 **不要修改** `ip/README.md`（v0.0.5 STATUS 约定段已含 cache 状态描述；本 change 不应污染全局约定）
- [x] 5.6 **不要创建** `docs/templates/IP_README_TEMPLATE.md`（v0.0.5 已建 `IP_STATUS_TEMPLATE.md` 覆盖零代码 IP 场景）

## 6. 更新验证基线

- [x] 6.1 在 `CHANGELOG.md` 顶部 `## v0.0.4` 段添加条目："L1CachePlugin 容量注释修正 (32KB→16KB) + ReplacementPolicy 接口落地"
- [x] 6.2 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [x] 6.3 验证 `tools/verify_adr.sh` 仍 PASS（新增 ADR 不需要）
- [x] 6.4 完整 ctest 验证：16 (existing) + 5 (replacement) = **21/21** PASS

## 7. PR 准备

- [x] 7.1 `git diff --stat` 报告变更规模
- [x] 7.2 PR description 说明：默认行为零变化（向后兼容）+ 5 个新测试 + 1 个注释错误修正 + 引用 v0.0.5 (empty-directory-cleanup) 已删除 `ip/cache/test/` 避免冲突
- [x] 7.3 PR description 引用 `archive/2026-06-18-cache-policy-foundation-v1-original/` 中的 9 项 v1 问题已在本版本修复
- [x] 7.4 Request review