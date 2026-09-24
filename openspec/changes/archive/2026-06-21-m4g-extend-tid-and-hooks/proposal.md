## Why
M4G (commit 89892aa) shipped D.1-D.4 in ~108 lines of header churn, locking 4 critical decisions. But 3 plumbing/doc gaps remain that will block M5-DSE 2-wide superscalar. Total cost ~80 LOC, 0 behavior change. This is the second-stage forward-compat lock.

## What Changes
- **Gap A (tid plumbing)**: 4 plugins (reg_file, hazard, branch_predictor) get a `set_tid(uint8_t)` method; `cpu_factory.h::build_cpu` iterates `tid=0..N_THREADS-1` to dispatch per-thread cycles
- **Gap B (commit_hook doc)**: 6 lines added to `pipe_builder.h:104-138` documenting commit_hook as OoO commit primitive and flush_when as mispredict-squash primitive
- **Gap C (COMMIT stage)**: 2 lines added to `multi_isa_architecture.md §2.4` adding `COMMIT` stage to 5-stage list

## Capabilities
### New Capabilities
- `m4g-extend-tid-and-hooks`: M4G extension — second-stage forward-compatibility locks (tid plumbing, commit_hook documentation, COMMIT stage naming). Bridges D.1-D.4 (M4G) to Phase 5+ OoO infrastructure.

### Modified Capabilities
(无)

## Impact
- 影响文件:
  - 修改: `ip/cpu/plugins/reg_file.h` (~25 LOC: set_tid method + 2 closure changes)
  - 修改: `ip/cpu/plugins/hazard.h` (~10 LOC: set_tid + 1 closure change)
  - 修改: `ip/cpu/plugins/branch_predictor.h` (~10 LOC: set_tid + 1 closure change)
  - 修改: `ip/cpu/cpu_factory.h` (~5 LOC: tid loop in build_cpu)
  - 修改: `include/cf/plugin/pipe_builder.h` (~6 lines: documentation)
  - 修改: `ip/cpu/docs/multi_isa_architecture.md` (~2 lines: COMMIT stage)
- 依赖与时序: 本 change 是 M4G (✅ 已完成) 的扩展;M5-DSE 2-wide superscalar (待启动) 的硬前置
- 基线影响: ctest 36/36 PASS (无行为变化,N_THREADS=1 时 tid 仍 0);verify_adr.sh PASS;verify_no_ghost_refs.sh PASS
- 运行时影响: 零 (N_THREADS=1 默认,行为不变)
- API 影响: 新增 `PluginBase::set_tid(uint8_t) = 0` 虚函数;0 breaking
- breaking 变更: 零

## Alternatives Considered
### Alternative A: 不补这 3 个,直接 M5-DSE 时再补
放弃理由: M5-DSE 2-wide superscalar 第一周就会遇到 tid plumbing 缺失,需要回 4 个 plugin 闭包 + factory wiring,~200 LOC 重写。**不放弃**:1-2 天现在补,省 M5-DSE 1 周。

### Alternative B: 一次性补完所有 10 个 OoO 缺口 (ROB/IQ/PRF/LSQ/Rename/...)
放弃理由: ~2700-3300 LOC 新代码,需要 in-order baseline 跑通才能验证。M4-DSE 还没跑 ELF,顺序错。**不放弃**:只补 3 个零/低成本,OoO 主体留 Phase 5+。

### Alternative C: 补 D.5 stage-name 成员
放弃理由: Oracle 2026-06-17 明确否决 ("superscalar lanes 不是 stage 名称"),165 LOC 跨 11 plugin。**不放弃**:用 factory 层 `set_tid` 模式 (10x 更便宜)。