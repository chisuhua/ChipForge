---
change: chmem-verilator-perf-harness
priority: P1
initiative: wave6-linux-and-productization
adr:
  - ADR-088
  - ADR-040
  - ADR-047
plan: .rddf/plans/chmem-verilator-perf-harness.md
status: in-progress
---

# Tasks — chmem-verilator-perf-harness（交付点 A）

> **交付范围**: **交付点 A**（Dhrystone 全链路 + 测量脚本 + 基线 CSV + CI 门禁）。
> **交付点 B**（CoreMark 移植）**不在本 change** ⇒ 独立 change `chmem-verilator-coremark-harness`（未立项，含放弃判据与降级路径，见 plan Task 5 Step 6）。
> **plan**: `.rddf/plans/chmem-verilator-perf-harness.md`（v2，oracle + metis 双审阅 + 实施期修正 6 处）。

## Phase 0 备忘

- **P0 决策**: case-1 approve（auto-decision ADR-0049：AC 12 + 多文件 + CI 门禁 ⇒ 完整 P1–P3）
- **上游**: ADR-088 v1.1 §3（M 扩展 cycle 判据退役 ⇒ **cycle 与 TLM 不可比**）· §5.2 判据 4（CI 有可回归的自动测试）· ADR-047（基线须可复现构建）
- **★根因（实测）**: Dhrystone 在 CH_MEM/Verilator 跑不起来的真因是**单 RWE LOAD 段** ⇒ CH_MEM 段路由（`cpu_factory_chmem.h:304-310`，`is_exec && !is_writable → IBus`）落空 ⇒ **整段进 DMem、IBus 为空**；且**纯 R 段被整段丢弃**（无 else 分支）⇒ 修法 = **PHDRS 拆 RX+RW 两段**
- **★铁律**: 不改任何门禁判据值（DMIPS 1.4 / CoreMark 1.9 / 2.3）⇒ 判据重校准由 architect 出显式 ADR

## Implementation

- [x] Task 1 事实复核 + 根因确认（ELF 路由 / 段布局 / before 基线 / owner / 工具链）
- [x] Task 2 修 Dhrystone ELF 段布局（**PHDRS 双段**：RX `.text` + RW `tohost`+data；tohost 独占 1 word）
- [x] Task 3 可复现构建（**实施期修正**：构建路径 = CMake `dhrystone_elf` 目标，非 shell 脚本；`nm -u` 空；工具链前缀敏感）
- [x] Task 4 ~~tohost 可配置（`--tohost-addr`）~~ → **关闭**（前提实测成立：tohost = 0x80001000 = `kToHostWord` 0x400，三处一致）
- [x] Task 6 测量脚本 + 基线 CSV + **CI 门禁 step** + ctest entry
- [x] Task 7 收口（8 门禁 + 全量回归 + 零 `ip/` 改动 + commit）

## Acceptance

- [x] AC-1 `cpu_verilator_sim --elf …/dhrystone.elf` 退出码 0 **且 `TOHOST=1`**（before：`TOHOST=1498564676 … TIMEOUT`；after：`TOHOST=1 CYCLES=914544 PASS`）
- [x] AC-2 新 ELF 有**恰好两个** PT_LOAD（`R E` @0x80000000 + `RW` @0x80001000），**无纯 R 段**（`readelf -l` 可验）
- [x] AC-3 `readelf -s` 显示 `tohost` = **0x80001000**；`.tohost` @0x80001000（4B）/ `.rodata` @0x80001004 ⇒ **≥ tohost+4，不重叠**
- [x] AC-4 可复现构建：CMake 目标 `dhrystone_elf`（`src/cf_plugin/CMakeLists.txt:136-146`，`-O2 -fno-inline -march=rv32im_zicsr`）产出 `build/dhrystone.elf`；`nm -u` **空**；同工具链手工构建**字节一致**
- [x] AC-5 **程序结果正确性**：判据 = **TOHOST==1 且非 TIMEOUT**（`dhrystone.c:283 tohost_pass()` 写 1 后死循环 ⇒ 等价于 2000 迭代全部完成）；测量脚本在结果错误/超时时**失败且不产出数值**（负例实测：缺 elf ⇒ exit 1；基线漂移 ⇒ exit 1）
- [x] AC-6 **cycles 口径**：来自 `sim_main.cpp` 计数器（`CYCLES=%u`），**不含** wall-clock / elaboration / 编译时间
- [x] AC-7 **基线未漂移 = 精确相等**（无容差）；CI 门禁 step（warn 级）已接入 `architecture-gates.yml`（steps 11 → **12**）
      - ⚠️ **诚实更正（2026-10-24 实施后自查）**：CI 的 `architecture-gates.yml` **不构建项目**（其唯一的 cmake 是从源码构建 **Verilator 本身**）⇒ 该 step 在 CI 中**恒走 SKIP 分支**（缺 `build/bin/cpu_verilator_sim`）⇒ **目前零实际保护** ⇒ 本 step 是**挂载点（hook）**，**不是**已生效的门禁
      - ⇒ **ADR-088 §5.2 判据 4「CI 有可回归的自动测试」仍【未满足】**（既非 ctest，也非本 step）⇒ 「CI 具备 build 能力」登记为 **B3 前置**
- [x] AC-8 新基线 CSV `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv` 存在且**只含** `mode=verilator` 行；既有 `dhrystone-baseline-matrix.csv` **零改动**（TLM 侧实测 794478/1.43277 逐位一致 ⇒ 旧基线仍有效）
- [x] AC-9 CSV 数值由脚本实跑产出；notes 标注「裁剪版 workload」+「cycle 与 TLM 不可比（ADR-088 §3）」+「旧单 RWE 段 elf 不可比」
- [x] AC-10 8 架构门禁全 PASS + 全量回归 0 退化 + **`ip/**` 零改动**
- [x] AC-11 **不改**门禁判据值；判据重校准 ADR 的 owner（architect）+ 窗口（本 change 合入后、② 开工前）已登记
- [x] AC-12 交付点 A **可独立归档**（不含 CoreMark ⇒ 不阻塞 ② `chmem-bp-btb-gshare`）

### 实测留证（2026-10-24，branch `feature/chmem-verilator-perf-harness`）

| 项 | 实测 |
|---|---|
| before 基线 | `TOHOST=1498564676 CYCLES=100000 TIMEOUT` ⇒ `1498564676` = `0x59524F48` = ASCII **"HORY"** ⇒ 读到的是 `.rodata` 文本（**实证 tohost 与 `.rodata[0]` 重叠**） |
| after（Verilator） | **`TOHOST=1 CYCLES=914544 PASS`** ⇒ Dhrystone 在 CH_MEM 后端**首次跑通** |
| DMIPS/MHz（CH_MEM） | **1.24467**（`2000*1e6/(1757*914544)`）⇒ **低于 1.4 硬门禁** |
| TLM 侧（同一权威 elf） | `cycles=794478 / dmips_per_mhz=1.43277` ⇒ 与既有基线**逐位一致** ⇒ TLM 门禁**未退化**、TLM CSV **无需改** |
| ELF 段布局 | `LOAD 0x80000000 R E` + `LOAD 0x80001000 RW`；`tohost`=0x80001000；`.rodata`@0x80001004 |
| `nm -u` | 空（无 memcpy/memset 外部调用） |
| 工具链敏感性 | `riscv32-`（CMake 用）`.text` 0x494 ≡ 旧 elf；`riscv64-` 0x3d4 ⇒ **必须用 CMake 的 riscv32-** |
| 旧 elf 身份 | `git show HEAD:…/dhrystone.elf` ⇒ **不在 git**（`.gitignore` `*.elf`）⇒ 权威产物 = `build/dhrystone.elf` |
| 单跑耗时 | 20.72s（ctest 实测）⇒ TIMEOUT 300 充足 |
| 变异自证 | ① 缺 elf ⇒ exit 1 ② 基线 cycles 篡改 ⇒ 报出差异 + exit 1 ③ 还原 ⇒ PASS |
| CI | `architecture-gates.yml` steps 11 → **12**（新增 `Check CH_MEM perf baseline`，warn 级；缺 binary/elf 时显式 SKIP 不误判通过） |
| ctest | `verilator_perf_baseline`（LABELS `verilator;perf;e2e`，TIMEOUT 300）PASS |

### 实施期修正（6 处，已回写 plan）

| # | plan v2 判断 | 实测 |
|---|-------------|------|
| 1 | 「`build_manual_elf.sh` 无 dhrystone 目标 ⇒ 须新增」 | **错** —— 已有 CMake 目标 `dhrystone_elf` ⇒ 不新增（避免第二条构建路径） |
| 2 | 「替换旧 elf」 | 旧 elf **不在 git** ⇒ 权威产物 = `build/dhrystone.elf`；TLM 基线 CSV **逐位一致 ⇒ 无需改** |
| 3 | 「结果正确性用 checksum」 | `dhrystone.c` **无 checksum** ⇒ 判据 = **TOHOST==1 且非 TIMEOUT** |
| 4 | 「CH_MEM cycle 远小于 TLM」 | **反了** —— CH_MEM **多 15%**（组合式无 load-use 转发）⇒ DMIPS 1.24467 **< 1.4** ⇒ **判据重校准 ADR 从「预防性」变「实测必需」** |
| 5 | 工具链前缀无关 | **有关**（`riscv32-` vs `riscv64-` ⇒ `.text` 0x494 vs 0x3d4） |
| 6 | 「需要 `--tohost-addr`」 | **不需要** ⇒ Task 4 关闭（前提实测成立） |
