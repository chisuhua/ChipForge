# Design — vexii-riscv-parity-poc

> **承接**：[`proposal.md`](proposal.md) 的"为什么"和"做什么"，本文档回答"怎么做"。

## Context

当前 `soc/cpu/docs/roadmap/execution-roadmap.md §3.2` 中 v1.0.0 / v1.3.0 / v1.4+ 的"关键指标"列只有内部绝对值（CoreMark/MHz ≥ N），缺少与 VexiiRiscv 官方公开值的客观对拍门禁。

- **VexiiRiscv**（2025-07-01 status）：in-order + Plugin 范式 + SpinalHDL elaboration + FPGA 优先 —— 与 ChipForge 完全同架构赛道
  - single-issue 官方 CoreMark/MHz：**2.4–2.6**
  - dual+prefetch 官方 CoreMark/MHz：**5.24**
  - dual-issue 官方 DMIPS/MHz：**2.50**
- **ChipForge v0.10.0 (2027 Q1) 起**：PoC-1 (MUL/DIV FSM) + PoC-2 (RV32C) + PoC-3 (ICache) + `plugin-framework-cycle-precision` cycle counter（Phase F optional per 2026-09-29 (b) 决策, 兜底 = `tools/cpu_sim/main.cpp:230-242` `actual_cycles`）
- **ChipForge v1.0.0 (2027 Q3)**：BTB+GShare+RAS + S/U + RV32A + Cache 4-way + FreeRTOS demo → 理论具备 single-issue 追平能力

**核心设计问题**：如何把 VexiiRiscv 官方公开数值变成 CI 可机械校验的 HARD 门禁？

## Goals / Non-Goals

**Goals：**

1. 提供 `vexii_riscv_runner` 可重复使用 runner，支持 single-issue（PoC-14）与 dual 全家桶（PoC-15）两种配置
2. CoreMark/MHz 偏差计算公式与 VexiiRiscv 官方数值 SSOT 化（在仓库文件而非外部文档）
3. CI 门禁集成：`./build/bin/chipforge_tests "[vexii-parity]"` 必须 PASS
4. TLM↔CH_MEM 双模式同时跑（验证 ADR-040 v2.0 双模同语义 + ADR-080 byte-equal 协议）
5. PoC-14 在 v1.0.0 archive 前激活，PoC-15 在 v1.4+ archive 前激活

**Non-Goals：**

- 不做 VexiiRiscv 源码 fork / 移植 / 二次开发 —— 仅做"外部对标基准"
- 不追平 XiangShan / SiFive P870 等 OoO 强核（§1 非目标已声明，架构根本不支持）
- 不实现 VexiiRiscv 编译工具链（SpinalHDL → Verilog），仅消费其公开性能数字
- 不替代 `[riscv-tests]` 100% ISA 合规性 PoC-4（v1.0.0 rv32ua+si），本 runner 仅做性能层面对拍

## Decisions

### D1：CoreMark 版本选择

**选择**：CoreMark v1.01（与 VexiiRiscv 官方使用同版本）

**理由**：
- VexiiRiscv 官方 Performance 页公开的 5.24 CoreMark/MHz 是 CoreMark v1.01 实测
- 跨版本基准不可比（CoreMark v1.01 vs v1.02 在某些 workload 上偏差 5-10%）

**替代方案**：
- ❌ Dhrystone（噪声大，与 VexiiRiscv DMIPS 对比不可靠）
- ❌ 自定义 benchmark（无法对标 VexiiRiscv 公开数字）

**SSOT**：在 `tests/cpu/parity/coremark_version.json` 锁定 CoreMark v1.01 + 校验和 + 来源 URL

### D2：FPGA 实测 vs TLM 仿真数据采集

**选择**：FPGA 实测为唯一权威，TLM 仿真作为预验证（不参与偏差计算）

**理由**：
- VexiiRiscv 官方数字是 FPGA 实测（不是 TLM 仿真），与 TLM 仿真比对无意义
- TLM 仿真在 chip-selector 阶段（v1.3.0）做配置 DSE 扫描，但 final 偏差计算必须用 FPGA
- 单一权威数据源避免"两种结果不一致"的解释成本

**替代方案**：
- ❌ TLM+FPGA 双源（引入 SSOT 不一致风险）
- ❌ 仅 TLM 仿真（不可对标 VexiiRiscv 公开数字）

**实施**：`vexii_riscv_runner` 内部两个子模块：
- `parity_tlm_sim.cpp`（pre-silicon DSE 扫描，标记 [pre-silicon]）
- `parity_fpga_meas.cpp`（authoritative，提交 FPGA 实测数据）

### D3：偏差公式与阈值

**选择**：

```
PoC-14 (single-issue, range 2.4–2.6):
  reference = (min + max) / 2 = 2.5   # SSOT: 中点法 (VexiiRiscv single-issue 官方 2.4–2.6 的几何中心)
  deviation_pct = abs(chipforge - reference) / reference * 100
  PASS iff deviation_pct <= 10.0

PoC-15 (dual 全家桶, point 5.24):
  reference = 5.24   # VexiiRiscv dual+prefetch 官方单点
  deviation_pct = abs(chipforge - reference) / reference * 100
  PASS iff deviation_pct <= 15.0
```

> **公式选择依据**：single-issue 是**区间**（2.4–2.6，宽 8%），dual+prefetch 是**单点**（5.24）。区间需定义参考点，中点法用 (min+max)/2 = 2.5 是几何中心，对称且无歧义（避开了 min / max 偏向）。10%/15% 阈值由 VexiiRiscv 区间宽度 + 配置不确定性推导（详见 spec.md / tasks.md A.3）。

**理由**：
- 单发射 10% 阈值是"基本持平"的标准（VexiiRiscv single-issue 2.4-2.6 区间宽 8%，加 ±5% 噪声 = 10%）
- 双发射 15% 阈值放宽因 dual+prefetch 配置组合空间大（不同 prefetch 策略、store buffer 容量都影响结果）
- 阈值由 VexiiRiscv 官方数字区间宽度推导，**非拍脑袋**

**SSOT**：在 `tests/cpu/parity/parity_thresholds.h` 集中定义所有偏差阈值常量

### D4：CI 门禁集成方式

**选择**：复用现有 `[riscv-tests]` family tag 机制，新增 `[vexii-parity]` family

**理由**：
- `tests/CMakeLists.txt` 已用 `file(GLOB_RECURSE)` 自动发现测试文件 + `family` tag 自动派生
- PoC 命名约定（PoC-1, PoC-2, ...）已稳定，新 PoC 编号 PoC-14/15 不冲突

**实施**：
```cpp
// tests/cpu/parity/test_vexii_parity.cpp
TEST_CASE("vexii-parity PoC-14 single-issue", "[vexii-parity][single]") { ... }
TEST_CASE("vexii-parity PoC-15 dual 全家桶", "[vexii-parity][dual][v1.4-pending]") { ... }
```

**CI 集成**：
- v1.0.0 archive 前：`./build/bin/chipforge_tests "[vexii-parity][single]"` 必须 PASS
- v1.4+ archive 前：`./build/bin/chipforge_tests "[vexii-parity][dual]"` 必须 PASS

### D5：VexiiRiscv 数值 SSOT 化

**选择**：在仓库内文件 `tests/cpu/parity/vexii_riscv_baseline.json` 集中存储 VexiiRiscv 官方公开数值

**理由**：
- 避免"依赖外部网页"的脆弱性（spinalhdl.github.io 可能下线或数字更新）
- 校验和验证（每次 `vexii_riscv_runner` 启动时 SHA256 校验）
- 数据来源 URL + 取数日期戳在 JSON metadata 中保留可追溯性

**JSON 结构**：
```json
{
  "vexii_riscv_official_values": {
    "version": "2025-07-01",
    "source_url": "https://spinalhdl.github.io/VexiiRiscv-RTD/master/VexiiRiscv/Performance/index.html",
    "fetched_at": "2026-09-29",
    "single_issue": {
      "coremark_per_mhz_min": 2.4,
      "coremark_per_mhz_max": 2.6,
      "dmips_per_mhz": 1.4
    },
    "dual_issue_prefetch": {
      "coremark_per_mhz": 5.24,
      "dmips_per_mhz": 2.50
    }
  },
  "coremark_version": "1.01",
  "coremark_sha256": "...",
  "threshold": {
    "single_deviation_pct": 10,
    "dual_deviation_pct": 15
  }
}
```

## Risks / Trade-offs

| 风险 | 影响 | 缓解 |
|------|------|------|
| **R1** VexiiRiscv 官方数字未来更新（如 dual CoreMark 涨到 6.0）| SSOT 漂移 → 偏差计算错 | `vexii_riscv_baseline.json` 加 Git LFS + 季度人工审核 + 数据版本化 |
| **R2** CoreMark v1.01 ELF 在 VexiiRiscv 与 ChipForge 不同编译选项下结果不一致 | 偏差引入噪声 | 锁定 `-O2 -march=rv32imac -mabi=ilp32`，与 VexiiRiscv 官方 spec 对齐 |
| **R3** FPGA bitstream 不可重复（每次 synthesis 结果略异）| 实测数据噪声 | 跑 3 次取中位数；强制同 Vivado/Yosys 版本 |
| **R4** v1.4+ dual 全家桶实装推迟（如 dual-issue 砍分叉）| PoC-15 永不被激活 | PoC-15 标 `[v1.4-pending]`，v1.4 不启动则 PoC-15 标 archived-skipped，**不阻塞 PoC-14** |
| **R5** Vivado 商业 license 限制 | CI 跑 FPGA 实测成本高 | 允许 v1.0.0 archive 时用 TLM 仿真预验证（[pre-silicon] tag），FPGA 实测作为 follow-up issue 跟踪 |

## Migration Plan

### 阶段 1：骨架 + 空 PASS 测试（v0.10.0 archive 前）
- 创建 `tests/cpu/parity/` 目录
- 创建 `vexii_riscv_baseline.json`（初始内容 + 校验和）
- 创建 `vexii_riscv_runner.cpp`（函数签名 + 文档占位）
- 写 PoC-14 / PoC-15 failing test（标 FAIL）
- **不阻塞 v0.10.0**：仅骨架，不要求 PASS

### 阶段 2：PoC-14 实装（v1.0.0 archive 前）
- `vexii_riscv_runner.cpp` 完整实现 CoreMark ELF 加载 + cycle 收集
- FPGA bitstream 生成脚本（复用 `tools/cpu_sim` 流程）
- PoC-14 failing test → PASS
- **HARD 门禁生效**：v1.0.0 archive 时 `[vexii-parity][single]` PASS

### 阶段 3：PoC-14 复测 + PoC-15 骨架（v1.3.0 archive 前）
- 复用 v1.0.0 runner，加 FPGA 综合脚本
- PoC-14 复测 PASS（FPGA 实测数据）
- PoC-15 仍标 `[v1.4-pending]`

### 阶段 4：PoC-15 激活（v1.4+ 启动时）
- dual 全家桶实装后（v1.3.0 完成 ADR-083 write-back FSM + dual-issue 实现）
- PoC-15 激活为 HARD 门禁
- **v1.4+ archive gate**

### 回退策略

- 阶段 1 失败（无法写骨架）→ 推迟 v0.10.0 archive gate（不阻塞）
- 阶段 2 失败（PoC-14 偏差 > 10%）→ 触发 R-A 砍分叉流程，4 周 RCA + 复测，仍不达标 → 推迟 v1.0.0
- 阶段 3 失败（PoC-14 复测偏差 > 10%）→ 触发 R-A 砍分叉流程，v1.3.0 archive gate 阻塞
- 阶段 4 永不激活（dual 全家桶砍分叉）→ PoC-15 标 archived-skipped，不影响 PoC-14

## Open Questions

1. **Q1**: CoreMark v1.01 ELF 是否在仓库内 vendor？需要从 EEMBC 官方下载并校验吗？→ 待 v1.0.0 实施时确认 EEMBC license
2. **Q2**: FPGA 实测数据如何与 CI 集成？是手工提交 CSV 到 `tests/cpu/parity/fpga_measurements/` 还是 CI 自动跑？→ 倾向 CI 自动（与 `tools/cpu_sim` 复用），但 Vivado license 是障碍
3. **Q3**: PoC-14 是否要 backport 到 v0.10.0 / v1.2.0 archive gate？还是只在 v1.0.0 archive gate 起效？→ 当前设计仅 v1.0.0 + v1.3.0，v0.10.0 archive 不阻塞
4. **Q4**: VexiiRiscv 未来引入 ARM/Intel-style benchmark（如 SPECint2006）时，本 runner 是否扩展？→ 当前设计不支持，Q4 待研究
5. **Q5**: ADR-090（vexii-riscv-parity-runner-credential）何时正式注册？→ v1.0.0 archive 前 1 个月作为预备 ADR 注册