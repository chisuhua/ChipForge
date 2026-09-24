# Tasks: m4g-extend-tid-and-hooks

> **总工时**: 1-2 天 (1 person, ~80 LOC)
> **执行模式**: 主分支直接改 (无新 capability 实质变更,纯扩展)
> **依赖**: M4G 已完成 ✅ (commit 89892aa, 2026-06-19)
> **Oracle 评审**: 由背景研究综合 (post-m4g-strategic-decision-2026-06-20.md)
> **作为前置**: M5-DSE 2-wide superscalar 必须先经本 change

## 1. Gap A: tid plumbing (4 plugins + factory)

- [x] 1.1 在 `ip/cpu/plugins/reg_file.h` 添加 `set_tid(uint8_t)` 方法 + tid_ 成员 (~15 LOC)
- [x] 1.2 在 `reg_file.h:146` (decode read) 闭包读取 `this->tid_` 替代 `tid=0` (~5 LOC)
- [x] 1.3 在 `reg_file.h:165` (writeback write) 闭包读取 `this->tid_` (~5 LOC)
- [x] 1.4 在 `ip/cpu/plugins/hazard.h` 添加 `set_tid` 方法 + 改 `build` 闭包读取 `tid_` (~10 LOC)
- [x] 1.5 在 `ip/cpu/plugins/branch_predictor.h` 添加 `set_tid` + 改 `update` 闭包 (~10 LOC)
- [x] 1.6 在 `ip/cpu/cpu_factory.h` `build_cpu` 添加 tid 循环 (via PipeBuilder n_threads_ + run() 循环) (~5 LOC)
- [x] 1.7 在 `include/cf/plugin/plugin_base.h` 添加 `virtual void set_tid(uint8_t /*tid*/) {}` 虚函数 (默认 no-op,避免 3rd-party plugin break) (~3 LOC)

## 2. Gap B: commit_hook documentation (6 lines)

- [x] 2.1 在 `include/cf/plugin/pipe_builder.h:104-138` 添加 6 行注释: commit_hook = OoO 提交原语;CtrlLink::flush_when = mispredict 恢复原语
- [x] 2.2 引用 `dse_architecture_v2_design_research.md §3 E.1` (ROB 设计) 作为 Phase 5 起点

## 3. Gap C: COMMIT stage naming (2 lines)

- [x] 3.1 在 `ip/cpu/docs/multi_isa_architecture.md §2.4` 5-stage 表后添加 `COMMIT` 行 (~2 lines)
- [x] 3.2 注释: "in-order 隐含; OoO 显式阶段, 用 commit_hook 原语"

## 4. 验证

- [x] 4.1 验证 `cd build && ctest` 仍 36/36 PASS (N_THREADS=1 默认,行为不变)
- [x] 4.2 验证 `tools/verify_adr.sh` 仍 PASS (新增能力不冲突现有 ADR)
- [x] 4.3 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [x] 4.4 验证 `openspec validate --changes` 1 passed

## 5. git 验收

- [x] 5.1 commit 1: `feat(m4g-extend): add tid plumbing + commit_hook docs + COMMIT stage` (~80 LOC across 6 files) — commit `ec6ee4f` 落地 (2026-06-21 11:07 +0800, 8 文件 +217/-27)

## 6. PR 准备

- [x] 6.1 PR description 引用 `post-m4g-strategic-decision-2026-06-20.md` 作为决策依据 — 已包含在 commit `ec6ee4f` body
- [x] 6.2 PR description 标注: 本 change 是 M5-DSE 2-wide superscalar 硬前置 — 已包含在 commit `ec6ee4f` body
- [x] 6.3 PR description 标注: 推迟的 7 个 OoO 缺口 (ROB/IQ/PRF/LSQ/Rename/MUL-latency/Cache-latency) 留给 Phase 5+ — 已包含在 commit `ec6ee4f` body

> **PR 创建说明**: PR description = commit `ec6ee4f` body 全文. 实际 PR 在 GitHub/GitLab 上手动创建（使用 `git push` 后在 UI 操作, 或 `gh pr create`）