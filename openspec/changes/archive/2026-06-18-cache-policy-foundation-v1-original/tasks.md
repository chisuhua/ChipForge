# Tasks: cache-policy-foundation

> **总工时**: ~6h
> **执行模式**: git worktree `change/cache-policy-foundation`
> **依赖**: CHANGE-002 (ip-catalog-status) 完成后 cache 状态稳定

## 1. 实现 ReplacementPolicy 抽象接口

- [ ] 1.1 创建 `ip/cache/policies/replacement_policy.h`（4 虚方法 + 1 工厂方法 + Doxygen 注释）
- [ ] 1.2 验证 `wc -l ip/cache/policies/replacement_policy.h` ~50 LOC

## 2. 实现具体策略

- [ ] 2.1 创建 `ip/cache/policies/no_replacement_policy.h`（3 方法 no-op + name() 返回 "None"）
- [ ] 2.2 创建 `ip/cache/policies/lru_policy.h`（1-way LRU 简化实现：access counter + victim=0 + name() 返回 "LRU"）
- [ ] 2.3 验证两个策略都能编译

## 3. 集成到 L1CachePlugin

- [ ] 3.1 修改 `ip/cache/tlm/L1CachePlugin.h` 构造签名：`explicit L1CachePlugin(std::unique_ptr<ReplacementPolicy> policy = nullptr)`
- [ ] 3.2 添加 `std::unique_ptr<ReplacementPolicy> policy_` 成员
- [ ] 3.3 修改 `L1CachePlugin.cpp` 在 ctor 中：`if (!policy_) policy_ = std::make_unique<NoReplacementPolicy>();`
- [ ] 3.4 在 `lookup` 阶段 `at_stage` 回调中调用 `policy_->on_access(set, way)`（D4 合规：在 at_stage 内调用）
- [ ] 3.5 验证现有 4 个 L1Cache 测试 (`tests/cache/test_l1_cache_*.cpp`) 100% PASS

## 4. 单元测试

- [ ] 4.1 创建 `tests/cache/test_replacement_policy.cpp`
- [ ] 4.2 5 个测试：factory/LRU-create/none-create/unknown-name-throws/LRU-basic
- [ ] 4.3 在 `src/cf_plugin/CMakeLists.txt` 或独立 `tests/cache/CMakeLists.txt` 注册测试
- [ ] 4.4 验证 `ctest --test-dir build -R replacement` 5/5 PASS

## 5. 文档同步

- [ ] 5.1 更新 `docs/architecture/overview.md §"可插拔策略模式"` 在 L1/L2 Cache 行加 "✅ Phase 1.4 落地 LRU + NoReplacement" 标记
- [ ] 5.2 在 `bundles/README.md` 加交叉引用段："策略接口见 `ip/cache/policies/`"
- [ ] 5.3 更新 `ip/README.md` 强调 `policies/` 子目录的契约
- [ ] 5.4 更新 `ip/cache/README.md` 加 "策略接口" 段指向新文件

## 6. 更新验证基线

- [ ] 6.1 在 `CHANGELOG.md` 顶部添加 v0.0.4 条目
- [ ] 6.2 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [ ] 6.3 验证 `tools/verify_adr.sh` 仍 PASS（新增 ADR 不需要，ADR-041 在 CHANGE-001）
- [ ] 6.4 完整 ctest 验证：51 plugin + 4 L1Cache (existing) + 5 replacement (new) = 60/60 PASS

## 7. PR 准备

- [ ] 7.1 `git diff --stat` 报告变更规模
- [ ] 7.2 PR description 说明：默认行为零变化（向后兼容）+ 5 个新测试
- [ ] 7.3 Request review
