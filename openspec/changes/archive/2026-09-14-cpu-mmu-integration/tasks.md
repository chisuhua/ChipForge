## 1. cpu_factory 条件注册 + substage 声明（commit 1/6）

- [x] 1.1 修改 `ip/cpu/cpu_factory.h::build_cpu()`：在 PluginOrder 数组增加 `if (config.enable_mmu) { ... }` 条件块，注册 `RiscvMMUPlugin`
- [x] 1.2 映射 `config.mmu_mode` ("sv32"/"sv39"/"sv48") 到 `cf::ip::mmu::SvMode` 枚举，传给 MMUPlugin 构造
- [x] 1.3 默认 TLB 几何：2-level 8/8 + LRU + ptw_max_inflight=2（与 mmu-cache-integration commit 5 SoC JSON 一致）
- [x] 1.4 修改 `ip/cpu/cpu_factory.h::build_cpu()`：在 MMU Plugin 注册后增加 3 个 `pb.declare_substage()` 调用（csr_write_satp/sfence_vma 挂 execute, mmu_exit 挂 memory）
- [x] 1.5 **Oracle B2 修复**：更新 `tests/cpu/integration/test_7stage_riscv.cpp` 中 `node_count()` / `stage_count()` assertions——`enable_mmu=true` 默认路径下数字会变（具体数字由 commit 1 实装后实测确定，写成 "若 enable_mmu=true 则 +5/+5" 模式）
- [x] 1.6 **Oracle B2 修复**：审计 `tests/cpu/integration/test_10stage_riscv.cpp` 中 `10stage_topology_from_config`（cpu_deep_pipeline.json 含 `enable_mmu:true`），必要时更新 assertion
- [x] 1.7 **Oracle B2 修复**：更新 `tests/cpu/test_cpu_factory.cpp` `build_cpu_registers_11_real_plugins` 的 `plugins.size()==11` → `plugins.size()==12`（enable_mmu=true 默认路径）
- [x] 1.8 验证：`cmake --build build` 编译通过；`./build/bin/chipforge_tests "[cpu-integration]"` 25/25 PASS（enable_mmu=false baseline bit-identical）

## 2. RiscVMMUPlugin at_stage 三个闭包实装（commit 2/6）

- [x] 2.1 新建 `ip/cpu/tlm/cpu_keys.h`：声明 4 个 Payload Key（`SAT` / `SFENCE_VADDR` / `SFENCE_ASID` / `CPU_EXCEPTION_CODE`），全局 `cf::cpu::tlm::payload::cpu_keys<T>` 模板
- [x] 2.2 修改 `ip/cpu/plugins/mmu.cpp`：增加 `at_stage("csr_write_satp")` 闭包，读取 `cpu_keys::SAT`，调` `csr_write_satp(satp_value)`
- [x] 2.3 修改 `ip/cpu/plugins/mmu.cpp`：增加 `at_stage("sfence_vma")` 闭包，读取 `cpu_keys::SFENCE_VADDR`/`SFENCE_ASID`，调` `sfence_vma(rs1, rs2)`（4-way RISC-V Spec §6.2 dispatch）
- [x] 2.4 修改 `ip/cpu/plugins/mmu.cpp`：增加 `at_stage("mmu_exit")` 闭包，读取 `mmu_keys::EXCEPTION_CODE`，写入 `cpu_keys::CPU_EXCEPTION_CODE`
- [x] 2.5 验证：`cmake --build build` 编译通过；`[cpu-integration]` 25/25 + `[mmu]` 40/40 + `[RiscV]` 4/4 全部 PASS

## 3. 集成测试（commit 3/6）

- [x] 3.1 扩展 `tests/cpu/integration/test_3stage_riscv.cpp`：+1 case `EnableMMU3StageBuilds`（3-stage + 3 MMU substage 全部注册）
- [x] 3.2 扩展 `tests/cpu/integration/test_5stage_riscv.cpp`：+2 case（`EnableMMU5StageBuilds` + `EnableMMUDisabledBitIdenticalBaseline`）
- [x] 3.3 扩展 `tests/cpu/integration/test_7stage_riscv.cpp`：+1 case `EnableMMU7StageCommitRetire`（commit/retire 阶段不强制 MMU substage）
- [x] 3.4 扩展 `tests/cpu/integration/test_10stage_riscv.cpp`：+1 case `EnableMMU10StageDeepPipeline`
- [x] 3.5 新建 `tests/cpu/test_cpu_riscv_mmu_hooks.cpp`：3 cases（`CSRWriteSatpRoutesToRiscVMMUPlugin` + `SFENCEVMARoutesToRiscVMMUPlugin` + `MMUExceptionPropagatesToCPU`）
- [x] 3.6 验证：`./build/bin/chipforge_tests "[cpu-integration]"` **33/33** PASS（25 baseline + 8 new）；`./build/bin/chipforge_tests` **314/314** PASS

## 4. 文档与 ADR 同步（commit 4/6）

- [x] 4.1 修改 `ip/cpu/README.md`：新增 "RiscV MMU Integration" 段，说明 `enable_mmu=true` 时 MMU substage 注册位置 + RiscV hook 路由
- [x] 4.2 修改 `ip/cpu/configs/cpu_params_schema.json`：`enable_mmu` description 加 "详见 ip/cpu/plugins/mmu.h RiscVMMUPlugin"
- [x] 4.3 修改 `ip/mmu/STATUS.md`：`INTEGRATED (mmu-cache-integration + L1Cache VIPT + SoC 全链)` → `INTEGRATED + CPU PIPELINE`
- [x] 4.4 修改 `CHANGELOG.md`：v0.2.0 条目 — "cpu-mmu-integration: RiscVMMUPlugin 注册到 cpu_factory + at_stage substages + 5/3/7/10-stage integration tests + 4 个 RiscV hook 集成测试"
- [x] 4.5 验证：`bash tools/doc_link_check.sh --quiet` exit=0

## 5. 最终验证（commit 5/6）

- [x] 5.1 验证 `bash tools/verify_adr.sh` PASS（ADR-040 + ADR-044 unchanged; cpu_factory 修改不动 ADR 注册表）
- [x] 5.2 验证 `bash tools/verify_plugin_decision.sh` PASS（3+4/3 D4 + ADR-040 全部满足）
- [x] 5.3 验证 `bash tools/check_plugin_portability.sh` PASS（4/4 at_stage 闭包 HDL 1:1 友好）
- [x] 5.4 验证 `bash tools/doc_link_check.sh --quiet` PASS（exit 0，新文件链接完整）
- [x] 5.5 验证 `./build/bin/chipforge_tests` **314/314** PASS（291 + 15 mmu-cache-integration + 8 cpu-mmu-integration）
- [x] 5.6 5 次连跑稳定性：`for i in 1..5; do ./build/bin/chipforge_tests 2>&1 | tail -1; done` 全部 **314/314** PASS

## 6. 归档（commit 6/6）

- [x] 6.1 `openspec archive cpu-mmu-integration` 移动 change 到 `openspec/changes/archive/2026-09-13-cpu-mmu-integration/`
- [x] 6.2 验证 archive 成功：`openspec list` 显示无 active cpu-mmu-integration；`openspec/specs/` 永久化 5 个新 capability specs + 1 个 modified spec（mmu-riscv-isa-adapter）
- [x] 6.3 标记 tasks.md 完成：所有 `- [ ]` 改为 `- [x]`
- [x] 6.4 PR 提交（用户决策）：push 5 commits 到 origin/main + 创建 PR；CI gates 全 PASS