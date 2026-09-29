# Tasks

> **TDD 5 步结构**（参考 OpenSpec 实践）：每条 task 标注 [RED → GREEN → REFACTOR] 阶段
> **总入口**：与 [`proposal.md`](proposal.md) + [`design.md`](design.md) + [`specs/vexii-riscv-parity-runner/spec.md`](specs/vexii-riscv-parity-runner/spec.md) 对齐

## Phase A — vexii_riscv_baseline.json + 骨架 + failing tests（v0.10.0 archive 前可启动）

> **重要前置决策**：
> 1. **路径**：`tests/cpu/parity/`（新建目录，与既有 `tests/cpu/riscv-tests-fixture/` 平级）
> 2. **依赖锁定**：v0.10.0 阶段只建骨架 + baseline.json + failing test，**不实现** 实测逻辑（依赖 `plugin-framework-cycle-precision` archive; 当前 0/25 NOT STARTED, **Phase F optional** per 2026-09-29 (b) 决策, 兜底路径 = `cpu_sim actual_cycles`）
> 3. **VexiiRiscv baseline SSOT**：所有数值在仓库文件而非外部文档，Git LFS 跟踪 `history/` 子目录
> 4. **CoreMark v1.01 锁定**：与 VexiiRiscv 官方使用同版本，跨版本基准不可比
> 5. **PoC-15 [v1.4-pending]**：v1.4+ 启动前 SKIP，不阻塞 PoC-14

### A.1 [RED] 写 `[vexii-parity][single]` failing test：PoC-14 单发射对拍（无实现）

- [ ] **新建测试文件**：`tests/cpu/parity/test_vexii_parity.cpp`（~150 LOC）
- [ ] **断言数**：5-7 assertions
- [ ] **覆盖范围**：
  - 加载 `vexii_riscv_baseline.json` 成功且数值正确
  - SHA256 校验通过（首次跑）
  - 模拟 chipforge_coremark_per_mhz = 2.5（in-range 模拟值），跑 vexii_riscv_runner PoC-14
  - 期望偏差 ≤ 10% → PASS
  - 模拟 chipforge_coremark_per_mhz = 1.8（out-of-range），跑 PoC-14
  - 期望偏差 > 10% → FAIL + 退出码 1
- [ ] **Family tag**：`[vexii-parity][single]`（Catch2 多 tag 派生）
- [ ] **运行命令**：`./build/bin/chipforge_tests "[vexii-parity][single]"`
- [ ] **当前状态**：测试文件存在但 vexii_riscv_runner 未实现 → FAIL（这就是 [RED] 的意义）

### A.2 [GREEN] 实现 `vexii_riscv_baseline.json` + 加载模块（最小可跑版本）

- [ ] **新建文件**：`tests/cpu/parity/vexii_riscv_baseline.json`（含 VexiiRiscv 官方数值 + SHA256 + 版本戳 + 阈值常量）
- [ ] **JSON schema 校验**：参考 [`specs/vexii-riscv-parity-runner/spec.md` §Requirement: VexiiRiscv 官方数值 SSOT 加载](../../specs/vexii-riscv-parity-runner/spec.md)
- [ ] **新建文件**：`tests/cpu/parity/baseline_loader.{h,cpp}`（~80 LOC）
  - `BaselineLoader::load(path)` 解析 JSON + SHA256 校验
  - `BaselineLoader::get_single_issue_coremark_range()` 返回 std::pair<double, double>
  - `BaselineLoader::get_dual_issue_coremark()` 返回 double
- [ ] **SHA256 校验函数**：使用 `std::sha256` 或第三方库（如未引入 → 引入 `<openssl/sha.h>` 作为可选项）
- [ ] **运行 A.1 测试**：期望 PASS

### A.3 [GREEN] 实现 `vexii_riscv_runner.cpp` 偏差计算核心（无实测，纯算式）

- [ ] **新建文件**：`tests/cpu/parity/vexii_riscv_runner.{h,cpp}`（~200 LOC）
- [ ] **核心函数**：
  ```cpp
  namespace cf::cpu::parity {
    enum class Status { PASS, FAIL };
    struct Result {
        std::string test_name;
        double chipforge_coremark_per_mhz;
        double vexii_riscv_official_min;
        double vexii_riscv_official_max;
        double deviation_pct;
        double threshold_pct;
        Status status;
        std::string source_url;
        std::string timestamp;
    };
    Result run_poc14_single_issue(double measured);
    Result run_poc15_dual_issue(double measured);
    void write_result_json(const Result& r, const std::filesystem::path& output_path);
  }
  ```
- [ ] **公式实现**：
  - PoC-14: `deviation_pct = abs((measured - (min+max)/2) / ((min+max)/2)) * 100`，PASS iff `deviation_pct <= 10.0`
  - PoC-15: `deviation_pct = abs((measured - official) / official) * 100`，PASS iff `deviation_pct <= 15.0`
- [ ] **JSON 输出**：用 `nlohmann::json`（项目已用），与 spec §Requirement: 偏差结果 JSON 输出 对齐
- [ ] **运行 A.1 测试**：期望 PASS（含 PASS + FAIL 两个 scenario）

### A.4 [REFACTOR] baseline_loader + runner 单元测试 + 文档

- [ ] **新建测试文件**：`tests/cpu/parity/test_baseline_loader.cpp`（~80 LOC，6-8 assertions）
  - 加载成功、SHA256 校验失败、字段缺失、版本过期（>180 天 WARN）
- [ ] **新建测试文件**：`tests/cpu/parity/test_vexii_runner_core.cpp`（~100 LOC，6-8 assertions）
  - 边界值测试（deviation = 10% / 15% 临界点）
  - NaN / Inf 输入处理
  - JSON 输出 schema 校验
- [ ] **文档**：`tests/cpu/parity/README.md`（~50 行）
  - vexii_riscv_runner 用法
  - baseline JSON 维护流程
  - 阈值常量调整流程
- [ ] **Family tag 验证**：`./build/bin/chipforge_tests --list-tags | grep vexii-parity` 必须出现 `[vexii-parity]` tag
- [ ] **运行全部 vexii-parity family**：`./build/bin/chipforge_tests "[vexii-parity]"` 必须 PASS（PoC-14 + 单元测试）

### A.5 [GREEN] 写 `[vexii-parity][dual][v1.4-pending]` PoC-15 failing test（占位 SKIP）

- [ ] **在 A.1 测试文件中追加** ~50 LOC（PoC-15 test case）
- [ ] **Family tag**：`[vexii-parity][dual][v1.4-pending]`
- [ ] **当前行为**：检测 `CHIPFORGE_VERSION` 环境变量，若 < v1.4 → SKIP（不是 FAIL）
- [ ] **Future 行为**：v1.4+ archive 时移除 `[v1.4-pending]` tag + 实现实测逻辑（Phase D）

## Phase B — CoreMark v1.01 ELF 加载 + FPGA cycle 采集（v1.0.0 archive 前）

> **依赖**：`plugin-framework-cycle-precision` Phase F optional 实装（提供 `pb.run(N)` API + cycle_counter Payload Key; 当前 0/25 NOT STARTED per 2026-09-29 (b) 决策; 兜底路径 = `cpu_sim actual_cycles`）

### B.1 [RED] 写 `[vexii-parity][single][fpga]` failing test：CoreMark ELF 加载 + cycle 采集

- [ ] **在 A.1 测试文件中追加** ~100 LOC（FPGA 实测 test case）
- [ ] **断言数**：5-7 assertions
- [ ] **覆盖范围**：
  - 加载 `coremark_v1.01.elf`（需 vendor，估 ~30 KB）
  - 加载 ChipForge v1.0.0 配置生成的 FPGA bitstream
  - 跑 CoreMark 完整 suite
  - 记录 total_cycles
  - 计算 CoreMark/MHz = (iterations / total_cycles) * clock_mhz
- [ ] **当前状态**：test_coremark_loader 未实现 → FAIL

### B.2 [GREEN] vendor CoreMark v1.01 ELF + 实现 ELF 加载器

- [ ] **Vendor CoreMark v1.01 ELF**：`tests/cpu/riscv_tests/elf/coremark_v1.01.elf`（从 EEMBC 官方下载，需 license 审核）
- [ ] **SHA256 校验文件**：`tests/cpu/riscv_tests/elf/coremark_v1.01.sha256`
- [ ] **新建模块**：`tests/cpu/parity/coremark_loader.{h,cpp}`（~150 LOC）
  - 解析 ELF header + program headers
  - 加载到内存（PicolibcHostMemory 实例）
  - 设置 stack pointer + entry point
  - 启动 CPU 仿真
- [ ] **CoreMark 输出协议**：监听 `tohost` 寄存器（Picolibc HTIF 协议）等待 `tohost != 0` 退出
- [ ] **运行 B.1 测试**：期望 PASS（TLM 仿真可跑，FPGA 跑可后续）

### B.3 [GREEN] 实现 `cycle_counter` 集成（依赖 P1#4 plugin-framework-cycle-precision, Phase F optional per 2026-09-29 (b) 决策; 兜底 = cpu_sim actual_cycles）

- [ ] **新建文件**：`tests/cpu/parity/cycle_collector.{h,cpp}`（~100 LOC）
- [ ] **核心逻辑**：
  - 利用 `plugin-framework-cycle-precision` 提供的 `cycle_counter()` API
  - 在 CoreMark 跑测期间订阅 `busy_cycles` Payload Key
  - 累计 total_cycles（排除 idle cycles）
- [ ] **TLM 模式验证**：单测跑 CoreMark，记录 cycle 数与预期 ±5%
- [ ] **CH_MEM 模式验证**：同 TLM，cycle 数偏差 < 5%（与 spec §TLM↔CH_MEM 双模式同语义 对齐）

### B.4 [REFACTOR] 集成 PoC-14 failing test 改成实际 FPGA 跑测

- [ ] **修改 A.1 test case**：移除"模拟 chipforge_coremark_per_mhz" stub，改为调 B.2/B.3 实测 API
- [ ] **新断言**：实测偏差 ≤ 10% → PASS（与 VexiiRiscv 2.4-2.6 对拍）
- [ ] **FPGA 实测约束**：若 CI 环境无 Vivado license，则用 TLM 仿真预验证，FPGA 实测作为 follow-up issue 跟踪
- [ ] **Family tag**：`[vexii-parity][single][fpga]` 或 `[vexii-parity][single][tlm-pre-silicon]`
- [ ] **运行全部**：`./build/bin/chipforge_tests "[vexii-parity][single]"` 必须 PASS（含 failing test + 实测）

## Phase C — TLM↔CH_MEM 双模式对拍（v1.3.0 archive 前）

### C.1 [RED] 写 `[vexii-parity][single][dual-mode]` failing test：TLM vs CH_MEM 偏差

- [ ] **在 A.1 测试文件中追加** ~80 LOC（双模式 test case）
- [ ] **断言数**：4-5 assertions
- [ ] **覆盖范围**：
  - 用 TLM 模式跑 CoreMark，记录 cycle_count_tlm
  - 用 CH_MEM 模式跑 CoreMark，记录 cycle_count_chmem
  - 计算 deviation_pct = abs((chmem - tlm) / tlm) * 100
  - 偏差 ≤ 5% → PASS
- [ ] **当前状态**：CH_MEM 模式无 CoreMark 跑测 → FAIL

### C.2 [GREEN] 实现 CH_MEM 模式 CoreMark 跑测

- [ ] **复用 B.3 cycle_collector**，加 CH_MEM 模式分支
- [ ] **CH_MEM 跑测入口**：`tests/cpu/parity/coremark_chmem_runner.cpp`（~100 LOC）
- [ ] **与 ADR-040 v2.0 §3 实现样本对齐**：复用 `array_store` 双缓冲，TLM↔CH_MEM 同源代码
- [ ] **运行 C.1 测试**：期望 PASS（若 CH_MEM 模式已实装 Cycle precision）

### C.3 [REFACTOR] 双模式结果 JSON 合并 + 历史归档

- [ ] **修改 A.3 Result struct**：加 `tlm_coremark_per_mhz` + `chmem_coremark_per_mhz` + `dual_mode_deviation_pct` 字段
- [ ] **JSON 输出 schema**：参考 spec §Requirement: 偏差结果 JSON 输出 + §TLM↔CH_MEM 双模式同语义
- [ ] **历史归档**：C.2 runner 输出到 `tests/cpu/parity/history/vexii_parity_YYYY-MM-DD_HHMMSS.json`
- [ ] **Git LFS 配置**：`.gitattributes` 加 `tests/cpu/parity/history/*.json filter=lfs diff=lfs merge=lfs -text`
- [ ] **运行 C.1 测试**：期望 PASS

## Phase D — PoC-15 激活（v1.4+ 启动时）

> **触发**：`mfc-cpu-pipeline-multi-cycle-fsm` v0.10.0 archive + `cache-phase1.5-4way` v1.2.0 archive + ADR-083 write-back FSM 实装 + dual-issue 实现

### D.1 [REFACTOR] 移除 PoC-15 `[v1.4-pending]` tag

- [ ] **修改 A.5 test case**：删除 `[v1.4-pending]` tag，移除 SKIP 逻辑
- [ ] **触发条件**：`CHIPFORGE_VERSION >= v1.4` 且 dual 全家桶实装完成

### D.2 [GREEN] 实现 dual 全家桶 CoreMark 跑测（复用 B.2/B.3 + dual 配置）

- [ ] **dual 配置加载**：`tests/cpu/parity/configs/dual_full_fireplace.v1.yml`（含 dual-issue + HW prefetch + write-back + store buffer 全配置）
- [ ] **dual 配置生成脚本**：`tools/parity/generate_dual_config.sh`（基于 chip-selector CLI v1.3.0 生成）
- [ ] **dual 跑测入口**：`tests/cpu/parity/coremark_dual_runner.cpp`（~120 LOC）
- [ ] **与 A.3 run_poc15_dual_issue() 串联**

### D.3 [GREEN] PoC-15 跑测 + 偏差验证

- [ ] **运行 PoC-15**：v1.4+ archive 前必须跑，期望 CoreMark/MHz ≥ 4.5 + 偏差 ≤ 15%
- [ ] **Family tag**：`[vexii-parity][dual]`
- [ ] **CI gate**：v1.4+ archive 前 `./build/bin/chipforge_tests "[vexii-parity][dual]"` 必须 PASS

### D.4 [REFACTOR] PoC-15 历史归档与基线更新

- [ ] **若 VexiiRiscv 官方数字更新**（如 dual CoreMark 涨到 6.0）：更新 `vexii_riscv_baseline.json` + 提交 git commit + 更新 ADR-090
- [ ] **季度人工审核**：每 90 天 review baseline 是否过期

## Phase E — ADR-090 注册 + 文档同步（v1.0.0 archive 前 1 个月）

### E.1 [GREEN] 注册 ADR-090 vexii-riscv-parity-runner-credential

- [ ] **新建 ADR**：`docs/architecture/adr/ADR-090-vexii-riscv-parity-runner-credential.md`（~150 LOC）
- [ ] **ADR 内容**：
  - Context（VexiiRiscv baseline 必要性）
  - Decision（baseline SSOT 化 + 偏差公式 + CI gate）
  - Consequences（v1.0.0 archive 阻塞条件显式化）
  - Alternatives（Dhrystone 降级 vs CoreMark 升级 vs 自定义 benchmark）
- [ ] **更新 `docs/architecture/adr.md` 注册表**：ADR-090 加入索引
- [ ] **ADR-090 锚点引用**：`proposal.md` + `design.md` + `tasks.md` 中加 `ADR-090` 链接

### E.2 [REFACTOR] 更新 `references/multi-core-comparison.md` 与 PoC 表

- [ ] **更新 PoC 表**：§1 47 行对比矩阵 + §2 8 个独占维度中 PoC-14 / PoC-15 加入引用
- [ ] **更新 §3.3 PoC 速查表**：在 `soc/cpu/docs/roadmap/execution-roadmap.md §3.3` 把 PoC-14/PoC-15 与 ADR-090 关联
- [ ] **更新 `references/poics-and-risks.md §1`**：PoC↔change 映射表加 vexii-riscv-parity-poc 条目（Action 3）

## 实施顺序（汇总）

```
Phase A (2 周, v0.10.0 archive 前)
    ↓
Phase B (3 周, v1.0.0 archive 前)
    ↓
Phase C (1 周, v1.3.0 archive 前)
    ↓
Phase D (2 周, v1.4+ 启动时)
    ↓
Phase E (1 周, v1.0.0 archive 前 1 个月)
```

**关键路径**：Phase A → Phase B → v1.0.0 archive gate → Phase C → v1.3.0 archive gate → Phase D → v1.4+ archive gate

## 失败 → 砍分叉动作（汇总）

- **R-A**: PoC-14 偏差 > 10% → 4 周 RCA + 复测，仍不达标 → 推迟 v1.0.0 archive
- **R-B**: PoC-15 偏差 > 15% → 8 周 RCA + 复测，仍不达标 → 砍 dual-issue 分叉，转 single+late-alu
- **R-C**: FPGA 综合不可行 → 走 sim-only，FPGA 综合推迟 v1.4+
- **R-D**: CoreMark ELF 不可获取（license 问题）→ 降级 Dhrystone 对拍，阈值放宽 15%
- **R-E**: SHA256 校验失败（baseline 文件被未授权修改）→ 拒绝执行，CI 阻塞

## 总任务数（汇总）

| Phase | Task 数 | 估时 |
|-------|---------|------|
| Phase A | 5 | 2 周 |
| Phase B | 4 | 3 周 |
| Phase C | 3 | 1 周 |
| Phase D | 4 | 2 周 |
| Phase E | 2 | 1 周 |
| **合计** | **18** | **~9 周（2027 Q4 → 2029 Q3 跨年度）** |