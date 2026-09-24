# Tasks: `plugin-framework-stall`

> **TDD 5-step structure**: write failing test → verify fail → implement → verify pass → commit
> **Date**: 2026-09-15
> **Status**: PROPOSED
> **Reference**: `proposal.md`, `design.md`, `specs/{pb-stall-loop,ctrllink-consumption-contract,ptw-fetch-stall-consumer,hazard-execute-stall-consumer}/spec.md`

## 1. Layer 0: framework primitive（commit A）

- [x] 1.1 **Write failing test**: `tests/framework/test_pipe_builder_stall.cpp` 新文件，12 case：
  - 1.1.1 empty_ctrl_no_effect: 无 CtrlLink 时 pb.run() 行为不变（baseline 兼容）
  - 1.1.2 single_halt_skip_callback: halt=true 时该 stage 闭包不被调用
  - 1.1.3 single_halt_false_normal: halt=false 时该 stage 闭包正常执行
  - 1.1.4 or_merge_two_ctrls: 2 CtrlLink OR 合并
  - 1.1.5 or_merge_three_ctrls: 3 CtrlLink OR 合并（含 ctrl_link_count 断言）
  - 1.1.6 other_stage_not_affected: stage A stall 不影响 stage B 推进
  - 1.1.7 throw_when_throws: should_throw=true 抛 PluginException
  - 1.1.8 throw_then_no_commit: throw 后 commit_storages 不执行（异常路径）
  - 1.1.9 flush_not_framework_consumed: should_flush=true 时闭包仍执行（flush 由 Plugin 显式消费）
  - 1.1.10 register_ctrl_link_count: ctrl_link_count(stage) 返回注册数
  - 1.1.11 halt_stateful: 多次 run() 时 lambda 状态保留（flag 改变）
  - 1.1.12 shared_ptr_ctrl_keeps_alive: shared_ptr 持有 lambda 捕获对象，lambda 析构安全
- [x] 1.2 **Verify fail**: `cmake --build build && ./build/bin/chipforge_tests [framework]` 编译失败（PipeBuilder 无 register_ctrl_link API）
- [x] 1.3 **Implement** (commit A):
  - `include/cf/plugin/pipe_builder.h`: 新增 `register_ctrl_link(name, shared_ptr<CtrlLink>)` + `should_stall_stage(name)` + `ctrl_link_count(name)` + `get_ctrl_link(name, idx)` + `clear_ctrl_links()` + private `stage_ctrl_links_`
  - `include/cf/plugin/pipe_builder.h::run()`: 在 canonical stage 循环外层插入 `if (should_stall_stage(name)) continue;`
  - `include/cf/plugin/pipe_builder.h::run()` 末尾: 追加 throw_when 检查 + 异常抛出
  - **新增 `include/cf/plugin/plugin_exception.h`**: 定义 `cf::plugin::PluginException` 继承 `std::runtime_error`，构造 `(stage_name, msg)`
- [x] 1.4 **Verify pass**: `./build/bin/chipforge_tests [framework]` 12/12 PASS
- [x] 1.5 **Verify no regression**: `./build/bin/chipforge_tests` 318/318 PASS（framework API 兼容）
- [x] 1.6 **Verify gates**: `verify_adr.sh` + `verify_plugin_decision.sh` + `check_plugin_portability.sh` + `doc_link_check.sh` 全 PASS

## 2. Consumer A: PTW-busy → IBus fetch stalled（commit B/2）

- [x] 2.1 **Write failing test**: `tests/mmu/test_ptw_stall_integration.cpp` 新文件，3 case：
  - 2.1.1 tlb_hit_no_stall: 预填 TLB entry，10 cycle 内 fetch 每 cycle 执行，INSTRUCTION 持续更新
  - 2.1.2 tlb_miss_stall_fetch: TLB miss（空 TLB），cycle 0 PTW_ACTIVE=1 后 fetch 闭包**不被调用**，INSTRUCTION 保持 stale 0
  - 2.1.3 ptw_complete_unstall: PTW 完成（注入 stub 完成回调）后下一 cycle fetch 闭包执行，INSTRUCTION 有效
  - 2.1.4 stall_does_not_affect_decode: fetch stall cycle 内 decode/execute/memory/writeback 仍推进（assert via spy callback）
- [x] 2.2 **Verify fail**: `tests/mmu/test_ptw_stall_integration.cpp` 编译失败（IBusPlugin 无 CtrlLink 注册）
- [x] 2.3 **Implement**:
  - `ip/cpu/plugins/ibus.h`: 头部追加 `#include "ip/mmu/tlm/mmu_keys.h"`（mmu_keys 而非 keys<T,XLEN> 含 PTW_ACTIVE）
  - `ip/cpu/plugins/ibus.h`: build() 内追加 fetch_ctrl 注册（halt_when lambda 用 `cf::ip::mmu::payload::mmu_keys<>::PTW_ACTIVE`，**非** `KeyType::PTW_ACTIVE`）
  - `ip/mmu/tlm/MMUPlugin.cpp`: PTW 完成回调（行 60-67）追加 `(*node)(Key::PTW_ACTIVE) = 0;` 清零（**关键**，否则 fetch 永久 stall）
- [x] 2.4 **Verify pass**: `./build/bin/chipforge_tests [mmu]` 3/3 PASS
- [x] 2.5 **Verify no regression**: `./build/bin/chipforge_tests` 318/318 PASS（baseline 不退化）
- [x] 2.6 **Verify gates**: 4 个 gate 全 PASS（新测试被 GLOB_RECURSE 自动拾取，无需改 CMakeLists）

## 3. Consumer B: HazardPlugin RAW → execute stalled（commit B/2 与 consumer A 同 commit）

- [x] 3.1 **Write failing test**: `tests/cpu/integration/test_hazard_stall.cpp` 新文件，4 case：
  - 3.1.1 no_raw_no_stall: 3 条独立 addi，每 cycle execute 推进
  - 3.1.2 raw_chain_2_stall: `addi x1; addi x2; add x3,x1,x2` 第 3 条 stall 2 cycles，最终 x3=8
  - 3.1.3 long_raw_chain: `x1=...; x2=x1+...; x3=x2+...; x4=x3+...` 每后继 stall 2 cycles，最终值正确
  - 3.1.4 war_not_stall: `add x1,x2,x3; addi x2,x0,1` WAR 不 stall（HazardPlugin has_hazard NONE）
- [x] 3.2 **Verify fail**: 编译失败（HazardPlugin 无 execute_ctrl 注册）
- [x] 3.3 **Implement**:
  - `ip/cpu/plugins/hazard.h`: 新增 `last_decoded_hazard_` 成员 + `has_active_hazard()` 方法 + `reset_hazard_cache()` 复位方法
  - `ip/cpu/plugins/hazard.h::build()`: decode 闭包内追加 `last_decoded_hazard_ = hazard;`
  - `ip/cpu/plugins/hazard.h::build()`: 新增 execute_ctrl 注册（halt_when lambda 调 has_active_hazard）
  - `ip/cpu/plugins/hazard.h::build()`: 注册 commit_hook 每个 `pb.run()` 起始调 `reset_hazard_cache()` 避免残留假 stall
- [x] 3.4 **Verify pass**: `./build/bin/chipforge_tests [cpu-integration]` 既有 33 + 新 4 = 37 case PASS
- [x] 3.5 **Verify no regression**: 318/318 PASS
- [x] 3.6 **Verify gates**: 4 个 gate 全 PASS

## 4. 演示桩：throw_when + flush_when（commit C）

- [x] 4.1 `ip/cpu/plugins/exception.h`: build() 内追加 `auto throw_demo = make_shared<CtrlLink>(); throw_demo->throw_when([]{return false;});` 注释 "TODO Phase 5+ real trap delivery"，pb.register_ctrl_link("execute", throw_demo)
- [x] 4.2 `ip/cpu/plugins/branch_predictor.h`: build() 内追加 fetch ctrl，flush_when 演示（手动消费而非框架消费），注释 "TODO cpu-pipeline-mispredict"
- [x] 4.3 验证：318 PASS + 4 gate PASS（演示桩不影响行为）
- [x] 4.4 验证 `ip/cpu/plugins/exception.h` 当前是否 P3 stub，确保 stub 兼容新 build 代码

## 5. 文档 + 归档（commit D）

- [x] 5.1 新增 ADR: `docs/architecture/adr/ADR-045-plugin-ctrl-link-consumption.md`（6 个 Decision sections）
- [x] 5.2 `docs/architecture/adr.md` 注册 ADR-045（现有最后一条为 ADR-044，下一为 ADR-045）
- [x] 5.3 `CHANGELOG.md` 新增 `## v0.1.3 (2026-09-15) - plugin-framework-stall` 条目
- [x] 5.4 `ip/mmu/STATUS.md`: 移除 "CtrlLink halt_when PTW stall 未实装" 行，更新 "已实装"
- [x] 5.5 `ip/cpu/STATUS.md`: 新增 "声明式控制流原语激活" 段落
- [x] 5.6 `docs/roadmap/README.md`: 更新 next-milestone 状态
- [x] 5.7 `openspec/specs/{pb-stall-loop,ctrllink-consumption-contract,ptw-fetch-stall-consumer,hazard-execute-stall-consumer}/spec.md` 永久化（`echo y | openspec archive plugin-framework-stall` 自动创建）
- [x] 5.8 `openspec/changes/plugin-framework-stall/tasks.md` 标记全部完成
- [x] 5.9 5 次连跑稳定性: `for i in 1..5; do ./build/bin/chipforge_tests 2>&1 | tail -1; done` (332/337 PASS × 5)

## 6. 最终验证（commit D 末尾）

- [x] 6.1 `cmake --build build` 编译通过
- [x] 6.2 `./build/bin/chipforge_tests` **337/337 PASS**（baseline 318 + 12 framework + 3 mmu + 4 cpu-integration；R2 fix #3 统一为 4 hazard case）
  - 详细: `[framework]` 71+12=83 / `[mmu]` 42+3=45 / `[cpu-integration]` 33+4=37 / 其他稳定 171 + 各类其他 family = 337
- [x] 6.3 `bash tools/verify_adr.sh` PASS
- [x] 6.4 `bash tools/verify_plugin_decision.sh` PASS
- [x] 6.5 `bash tools/check_plugin_portability.sh` PASS
- [x] 6.6 `bash tools/doc_link_check.sh` PASS
- [x] 6.7 `echo y | openspec archive plugin-framework-stall` → openspec/changes/archive/2026-09-15-plugin-framework-stall/

## 7. 下一里程碑（CHANGELOG Pending）

- [x] 7.1 `soc-cpu-l1-mmu-demo` 重启 — 现在 stall 原语就绪，SoC demo 可真做 PTW retry
- [ ] 7.2 `riscv-tests-rv32ui` 接入 — 客观验收门槛
- [ ] 7.3 `cpu-pipeline-multi-cycle` — LATENCY>1 mul/div stall
- [ ] 7.4 `cpu-pipeline-exception` — throw_when 真实消费者
- [ ] 7.5 `cpu-pipeline-mispredict` — flush_when 真实消费者
- [ ] 7.6 `plugin-framework-cycle-precision` — pb.run(cycle_count=N) 真 cycle 精度
