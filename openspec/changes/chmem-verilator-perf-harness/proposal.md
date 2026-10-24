---
initiative: wave6-linux-and-productization
priority: P1
version_target: v0.11.0-prep
status: ready
created_from: .rddf/plans/chmem-verilator-perf-harness.md
adr:
  - ADR-088
  - ADR-040
  - ADR-047
---

# chmem-verilator-perf-harness

## Why

**CH_MEM/Verilator 后端目前没有任何可复现的性能测量链路**，而 TLM 侧的两个硬门禁载体 100% 在 TLM：

| 载体 | 现状 | 后果 |
|------|------|------|
| Dhrystone DMIPS/MHz ≥1.4 | `tests/cpu/integration/test_dhrystone.cpp` 硬绑 `CpuFactory<uint32_t>`（TLM） | TLM 删除后**判据无载体**（ADR-088 §5.2 判据 4） |
| CoreMark/MHz ≥1.9（PoC-6） | 被测对象 `ip/cpu/plugins/branch_predictor.h` 在 CH_MEM 侧**完全缺失** | B1 的 ② 无法验收 |

**实测根因（本 change 交付前的现状）**：`dhrystone.elf` 在 Verilator 侧**跑不起来**：

```
TOHOST=1498564676 CYCLES=100000 ELF=dhrystone.elf TIMEOUT
```

`1498564676` = `0x59524F48` = ASCII **"HORY"** ⇒ 读到的是 `.rodata` 文本，不是 tohost。两个叠加缺陷：

1. **单 RWE LOAD 段** ⇒ CH_MEM 的 ELF 路由（`cpu_factory_chmem.h:304-310`：`is_exec && !is_writable → IBus`）落空 ⇒ **整段进 DMem、IBus 为空 ⇒ 取指失败**（且**纯 R 段会被整段丢弃**，无 else 分支）
2. **tohost 与 `.rodata[0]` 重叠**（ld 脚本 `tohost = .` 紧接 `.rodata` ⇒ 同址 0x80001000）

⇒ 只修 2（overlap）**永远跑不起来**；必须 **PHDRS 拆 RX+RW 两段**（vendored rv32ui ELF 正是两段，故可跑）。

## What Changes

### In Scope（交付点 A —— 本 change）

1. **修 Dhrystone ELF 布局** —— `link_dhrystone.ld` 引入 `PHDRS`：`PT_LOAD#0 = RX(.text)` + `PT_LOAD#1 = RW(tohost 独占 1 word + .rodata/.data/.bss)`；**禁止**出现纯 R 段
2. **可复现构建** —— `build_manual_elf.sh` 新增 `dhrystone` 目标（`g++` 驱动；`dhrystone.c` 是 C++，`crt0` `jal _cstart`）+ 字节一致性 + `nm -u` 复检（memcpy/memset）
3. **测量脚本** —— `tools/measure_perf.sh`：解析 `cpu_verilator_sim` 的 `TOHOST=%u CYCLES=%u` ⇒ DMIPS/MHz（`N*1e6/(1757*cycles)`，N=2000）+ **程序结果正确性校验**（`Number_Of_Runs` + checksum）
4. **新基线 CSV** —— `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv`（**只含 CH_MEM/Verilator 行**；既有 `dhrystone-baseline-matrix.csv` 的 TLM legacy/fsm 行**不动**）
5. **CI 门禁 step** —— `architecture-gates.yml` 增一步（warn 级）调用测量脚本做「基线未漂移」检查（**精确相等，无容差**）
6. **ctest entry** —— `verilator_perf_baseline`（TIMEOUT 先量后定）

### Out of Scope（5 项）

- ❌ **CoreMark 移植**（计划 Task 5）—— **交付点 B**，独立 change `chmem-verilator-coremark-harness`（含放弃判据与降级路径，见 plan Task 5 Step 6）
- ❌ **不改任何门禁判据值**（DMIPS 1.4 / CoreMark 1.9 / 2.3 原样保留）—— 判据重校准由 **architect 的显式 ADR** 裁决（owner + 窗口见 plan 索引注 3）
- ❌ **不改 `ip/cpu/**`** —— 含不动 `cpu_factory_chmem.h:313` 的 `set_tohost_probe_word(0x400)` 与 `sim_main.cpp:35` 的 `kToHostWord`（二者与 riscv-tests 约定一致且正确；PHDRS 双段后 tohost 仍落 0x80001000 ⇒ **不需要** `--tohost-addr`）
- ❌ **不改 TLM 侧 Dhrystone 测试**（tohost 符号地址不变 ⇒ TLM 侧无需改）
- ❌ **不新建仿真框架**（复用现有 Verilator 后端）

### 变更清单（6 个文件）

| 文件 | 动作 | 具体 |
|------|------|------|
| `tests/cpu/manual_elf/link_dhrystone.ld` | 改 | PHDRS 双段 + tohost 独占 word |
| `tests/cpu/manual_elf/build_manual_elf.sh` | 改 | 新增 `dhrystone` 目标 |
| `tests/cpu/manual_elf/dhrystone/dhrystone.elf` | 改（重生成） | 二进制产物 |
| `tools/measure_perf.sh` | **新增** | 测量 + 结果正确性校验 + 基线比对 |
| `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv` | **新增** | 新基线（只含 verilator 行） |
| `.github/workflows/architecture-gates.yml` | 改 | CI 门禁 step（warn 级） |
| `tests/CMakeLists.txt` | 改 | ctest entry |

## Capabilities

- **CH_MEM/Verilator 后端有可复现的性能测量链路**：Dhrystone 跑通 + DMIPS/MHz 实测 + 基线入库
- **ADR-088 §5.2 判据 4 获得「CI 有可回归的自动测试」的必要输入**（CI 本身不跑 ctest ⇒ 必须有 CI 门禁 step）
- **B1 的 ②（BP）有判据载体**：CoreMark 测量链路（交付点 B）的前置

## Impact

### 风险

1. **R1 ELF 段路由**：纯 R 段被 loader 丢弃 ⇒ 只读常量读成 0 ⇒ **分数静默错**（不崩不报错）⇒ 双约束（readelf 段布局 + checksum 判据）
2. **R2 裁剪版 Dhrystone**：实测 `.text` 仅 0x494 字节 ⇒ 其 DMIPS **不代表**完整工作负载 ⇒ 基线 notes 必须标注
3. **R3 旧 elf 失效**：旧 elf 是「单 RWE 段 + overlap」产物 ⇒ **其 cycle 值不可与新值比较** ⇒ notes 标注
4. **R4 `nm -u`**：`g++ -O2` 结构体赋值可能生成 `memcpy` 外部调用 ⇒ `-nostdlib` 链接失败 ⇒ 重建后必须复检
5. **R5 超时也 exit 0**：实测 `cpu_verilator_sim` 在 TIMEOUT 时仍 exit 0 ⇒ 测量脚本**必须判 `TOHOST==1`**，不可只看退出码
6. **R6 判据孤儿化**：判据重校准 ADR 若无 owner ⇒ 复制「BP 门禁孤儿化」错误 ⇒ owner = architect，窗口 = 本 change 合入后、② 开工前

## Acceptance

- [ ] AC-1 `cpu_verilator_sim --elf tests/cpu/manual_elf/dhrystone/dhrystone.elf` 退出码 0 **且 `TOHOST=1`**（当前为 `TOHOST=1498564676 ... TIMEOUT`）
- [ ] AC-2 新 `dhrystone.elf` 有**恰好两个** PT_LOAD（`R E` + `RW`），**无纯 R 段**（`readelf -l` 可验）
- [ ] AC-3 `readelf -s` 显示 `tohost` = **0x80001000**；`.rodata` 起始 **≥** `tohost + 4`
- [ ] AC-4 `build_manual_elf.sh` 有 `dhrystone` 目标，**连跑两次字节一致**（`cmp`）+ `nm -u` 无未定义符号
- [ ] AC-5 **程序结果正确性**：DH `Number_Of_Runs` = 2000 **且** checksum 与参考一致；测量脚本在结果错误时**必须失败**
- [ ] AC-6 **cycles 口径**：来自 `sim_main.cpp` 计数器（`CYCLES=%u`），不含 wall-clock / elaboration / 编译时间
- [ ] AC-7 **基线未漂移 = 精确相等**（无容差）；CI 门禁 step（warn 级）已接入 `architecture-gates.yml`
- [ ] AC-8 新基线 CSV 存在且**只含** verilator 行；既有 `dhrystone-baseline-matrix.csv` **未被修改**
- [ ] AC-9 CSV 数值由脚本实跑产出；notes 标注「裁剪版 workload」+「cycle 与 TLM 不可比（ADR-088 §3）」+「旧 elf 已替换不可比」
- [ ] AC-10 8 架构门禁全 PASS + 全量回归 0 退化 + **`ip/**` 零改动**
- [ ] AC-11 **不改**门禁判据值（1.4 / 1.9 / 2.3）；判据重校准 ADR 的 owner + 窗口已登记
- [ ] AC-12 交付点 A **可独立归档**（不含 CoreMark ⇒ 不阻塞 ②）

## References

- ADR-088 v1.1 §3（M 扩展 cycle 判据退役 ⇒ 不可比）· §5.2 判据 4 · ADR-047（配置错误要静态可发现 ⇒ 基线可复现）
- plan: `.rddf/plans/chmem-verilator-perf-harness.md`（v2，oracle + metis 双审阅修订）
- 姊妹交付点 B: `chmem-verilator-coremark-harness`（未立项）
