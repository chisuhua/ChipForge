# ① chmem-verilator-perf-harness — 执行文件（v2，metis 审阅后修订）

> **change**: `chmem-verilator-perf-harness`
> **规模**: 中 · **前置**: 无 · **owner**: 本 change 自持
> **路线图**: `feat-tlm-backend-retirement` 的 B1 第 1 项（Dhrystone 在 `cpu_verilator_sim` 跑通 + 新基线 CSV 入库 = **B3 判据 4 的必要输入**）
> **被依赖**: ② `chmem-bp-btb-gshare`（其 AC「CoreMark/MHz ≥1.9」需要本 change 的测量链路）
> **上游**: ADR-088 v1.1 §3（M 扩展 cycle 判据退役 ⇒ **cycle 与 TLM 不可比**）· ADR-088 §5.2 判据 4 · ADR-047（配置错误要静态可发现 ⇒ **基线必须可复现构建**）
> **v2 修订（2026-10-24，metis 审阅）**: ① **根因纠正**（失败主因是单 RWE LOAD 段，不是 tohost/.rodata 重叠）② 删除 `--tohost-addr`（PHDRS 双段后 tohost 仍落 0x80001000）③ 补「程序结果正确」判据 ④ 补 CoreMark/MHz 口径 ⑤ 补 CI 门禁步骤 ⑥ AC-2 off-by-one 修正 ⑦ 构建命令改 `g++` ⑧ 删除 `--iterations`

---

## Goal

在 **CH_MEM/Verilator 后端**建立**可复现的性能测量链路**：Dhrystone（已有资产）+ CoreMark（需移植）能在 `cpu_verilator_sim` 上跑通，产出 `DMIPS/MHz` 与 `CoreMark/MHz` 实测值，并把**新基线 CSV 入库**（CH_MEM 后端专属 mode 行）。

> ⚠️ **本 change 不改判据值**：DMIPS ≥1.4 / CoreMark ≥1.9 的门禁值重校准需**显式 ADR**（owner = architect，见索引 §2 注 3）；本 change 只交付**测量能力 + 实测数据 + 基线 + CI 门禁**。

## Architecture

复用现有 Verilator 后端（**不新建仿真框架**）：

```
dhrystone.elf / coremark.elf   (必须: RX(.text) + RW(tohost+data) 两个 PT_LOAD 段)
   → cpu_verilator_sim --elf <p> [--cycles <n>]
        ├─ CpuFactoryChmem<ch_uint<32>>(preload_elf=true) → ch::toVerilog
        ├─ verilator --cc + g++ -shared  ⇒ libVtop.so（运行时编译）
        └─ cf_run_sim ⇒ TOHOST=%u CYCLES=%u
   → tools/measure_perf.sh 解析输出 ⇒ DMIPS/MHz · CoreMark/MHz
   → 新基线 CSV: soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv (mode=verilator)
   → CI 门禁 step (warn 级) 调用测量脚本 ⇒ 才满足 ADR-088 §5.2 判据 4 的「CI 有可回归的自动测试」
```

**关键口径（写死，禁止 agent 自选）**：

| 口径 | 定义 |
|------|------|
| **cycles** | **来自 `sim_main.cpp` 的周期计数器**（`CYCLES=%u` 字段）—— **不是** wall-clock、**不是** elaboration/编译时间。DH 分母只用它 |
| **MHz** | 仿真无真实频率 ⇒ **固定名义基准 1.0 MHz**（与 TLM 侧 `dhrystone-baseline-matrix.csv` 的口径一致：该 CSV 的 `dmips_per_mhz_measured` 实为「DMIPS 值」，不除以真实频率）。⇒ **CH_MEM 的 DMIPS 与 TLM 的 DMIPS 数值可比，但 cycle 不可比**（TLM 有流水线停顿、CH_MEM 全组合）⇒ 两者差异**必须**在 notes 注明 |
| **DMIPS/MHz** | `N*1e6/(1757*cycles)`，N = 2000（`dhrystone.c:230` 的 `Number_Of_Runs`，**编译期常量**） |
| **CoreMark/MHz** | 照抄 EEMBC 官方 `COREMARK_TIME` 口径；`score` 由 crt0 写到**固定地址**供 host 读回；`MHz` 基准同上（1.0 名义） |
| **程序结果正确性** | Dhrystone：`Number_Of_Runs` = 2000 **且** `Dhrystone_Result` 与参考 checksum 一致；CoreMark：自校验 checksum 必须 PASS（`COREMARK_ERROR` == 0）。**只看 cycles 不校验结果 ⇒ 数值可静默错**（见 Task 2 的段路由风险） |

## Tech Stack

Bash + Python3（脚本）· CMake（加构建目标）· RISC-V toolchain（`riscv64-unknown-elf-g++`）· Verilator（已有）· 零 `ip/` 业务代码改动

---

## 依赖与前置（Task 1 必须先做）

| 项 | 状态 |
|---|---|
| `cpu_verilator_sim --elf` | ✅ 已有（`tools/verilator_runner/cpu_verilator_sim.cpp:107-128`） |
| **ELF 段路由** | ⚠️ **实测路由**（`cpu_factory_chmem.h:304-310`）：`is_exec && !is_writable → IBus`；`is_writable → DMem`；**其余（纯 R 段）无分支 ⇒ 整段丢弃**。⇒ ELF 必须**恰好两个** PT_LOAD（RX + RW），**不能有纯 R 段** |
| **Dhrystone 现状** | ❌ 实测 `readelf -l`：**单个 `LOAD ... RWE`** ⇒ `is_exec && is_writable` ⇒ **整段进 DMem、IBus 为空 ⇒ 取指失败**。对比 vendored rv32ui（能跑）：`LOAD R E` + `LOAD RW` 两段 |
| tohost 地址 | ✅ **无需改动** —— riscv-tests 约定 tohost = RW 段首 word = 0x80001000 = dmem word `0x400`；`cpu_factory_chmem.h:313` 的 `set_tohost_probe_word(0x400)` 与 `sim_main.cpp:35` 的 `kToHostWord = 0x400` **一致且正确** ⇒ **v1 的 `--tohost-addr` 需求不成立，已删除**（前提：ld 脚本产出同款两段布局） |
| Dhrystone 源 | ⚠️ `dhrystone.c` 是 **C++**（crt0 `jal _cstart`）⇒ 重建必须用 **`g++`** 驱动（v1 误写为 `gcc`） |
| Dhrystone 构建目标 | ❌ `build_manual_elf.sh` **无 dhrystone 目标** ⇒ 预编译 elf 无可复现构建路径（见 Task 3） |
| CoreMark 资产 | ❌ **零资产** ⇒ 需 EEMBC 移植（见 Task 5） |
| CH_MEM dmem 容量 | ✅ `kMemWords = 16*1024` = 64KB（`ip/cpu/plugins/dmem_chmem.h:35`）⇒ 容得下 Dhrystone（memsz 0x3a68） |
| CI | ⚠️ **CI 不跑任何测试**（实测 `.github/workflows/*.yml` 无 `ctest`）⇒ ctest entry ≠ 满足 ADR-088 §5.2 判据 4 ⇒ 须加 **CI 门禁 step**（见 Task 6 Step 5） |

---

## Tasks

### Task 1: 事实复核 + 根因确认（不写代码）

- [ ] **Step 1: 复核 ELF 路由** —— 读 `ip/cpu/cpu_factory_chmem.h:296-313`，确认 `is_exec/is_writable` 三分支与「纯 R 段被丢弃」
- [ ] **Step 2: 确认段布局事实** —— `readelf -l tests/cpu/manual_elf/dhrystone/dhrystone.elf | grep LOAD` ⇒ 期望**单个 RWE**；`readelf -l tests/cpu/riscv_tests/elf/rv32ui-p-add | grep LOAD` ⇒ 期望 `R E` + `RW` 两段
- [ ] **Step 3: 记录 before 基线** —— `./build/bin/cpu_verilator_sim --elf tests/cpu/manual_elf/dhrystone/dhrystone.elf --cycles 2000000; echo "exit=$?"` ⇒ 如实记录 `TOHOST=? CYCLES=?`（**预期失败**：IBus 为空）
- [ ] **Step 4: 确认无 owner 冲突** —— `grep -rn "verilator-perf-harness\|perf-baseline" openspec/changes/ .rddf/roadmap/`（排除 archive）
- [ ] **Step 5: 确认工具链** —— `riscv64-unknown-elf-g++ --version` 可用；`verilator --version` ≥ 5.020

### Task 2: **修 Dhrystone ELF 段布局（真正根因）**

> **v2 变更**：v1 把失败归因于「tohost 与 `.rodata` 重叠」并只改 `. += 4` ⇒ **永远跑不起来**（单 RWE 段 ⇒ IBus 为空）。真正的修法是**产出 RX + RW 两个 PT_LOAD**。

- [ ] **Step 1: 改 ld 脚本 —— PHDRS 双段**

  在 `tests/cpu/manual_elf/link_dhrystone.ld` 中引入 `PHDRS`：
  - `PT_LOAD` #0 = **RX**：`.text`（含 `dhrystone_crt0.S` 的 `_start`）
  - `PT_LOAD` #1 = **RW**：**tohost 独占 1 word** + `.rodata` + `.data` + `.bss`
  - tohost 落在 RW 段**首 word** = `0x80001000`（与 riscv-tests 约定一致 ⇒ `kToHostWord=0x400` 保持有效）
  - **禁止**出现纯 R 段（否则被 loader 丢弃 ⇒ 只读常量读成 0 ⇒ 分数静默错）
- [ ] **Step 2: 重建并验证段布局**

  Run:
  ```bash
  riscv64-unknown-elf-g++ -march=rv32im -mabi=ilp32 -nostdlib -nostartfiles \
    -Wl,--build-id=none -T tests/cpu/manual_elf/link_dhrystone.ld \
    tests/cpu/manual_elf/dhrystone/dhrystone_crt0.S tests/cpu/manual_elf/dhrystone/dhrystone.c \
    -o /tmp/opencode/dhrystone_check.elf
  readelf -l /tmp/opencode/dhrystone_check.elf | grep -E "LOAD"       # 期望: R E + RW 两段
  readelf -s /tmp/opencode/dhrystone_check.elf | grep -w tohost      # 期望: 0x80001000（符号, 不是 section）
  ```
  Expected: 两个 PT_LOAD（`R E` + `RW`）；`tohost` 符号 = 0x80001000（**注意用 `readelf -s` 查符号，`-S` 查不到**）
- [ ] **Step 3: 验证无重叠** —— `.rodata` 起始 **≥** `tohost + 4`（`≥` 不是 `>`：tohost 独占 1 word ⇒ `.rodata` 恰在 `tohost+4`）
- [ ] **Step 4: TLM 回归** —— `./build/bin/chipforge_tests "[dhrystone]"` 仍 PASS（TLM 侧用 `PicolibcHostMemory` 逐段加载，不依赖 PT_LOAD 拆分 ⇒ 应无变化；若有变化 ⇒ 记录并更新 TLM 侧 baseline CSV，**不得静默改数**）

### Task 3: 可复现构建（★实施期修正：**构建路径是 CMake，不是 shell 脚本**）

- [x] **Step 1: 构建路径确认（实施期发现 v1 判断有误）** —— v1 说「`build_manual_elf.sh` 无 dhrystone 目标 ⇒ 须新增」⇒ **实测错误**：仓内**已有** CMake 目标 `dhrystone_elf`（`src/cf_plugin/CMakeLists.txt:136-146`，`g++ -O2 -fno-inline -march=rv32im_zicsr -mabi=ilp32 -nostdlib -static -T link_dhrystone.ld`）⇒ **无需新增 shell 目标**（且新增会造成第二条构建路径 ⇒ 字节不一致风险）
- [x] **Step 2: 幂等性 + `nm -u` 复检** —— `nm -u` **空** ✓（无 memcpy/memset 外部调用）；字节一致性：CMake 产物与手工构建**在同工具链下**字节一致（⚠️ **工具链前缀有影响**：`riscv32-unknown-elf-g++`（CMake 用）产出 `.text` 0x494，而 `riscv64-unknown-elf-g++` 产出 0x3d4 ⇒ **必须用 CMake 的 riscv32- 工具链**，否则字节不一致且 cycle 数不同）
- [x] **Step 3: 旧 elf 的真实身份（★v2 修正）** —— 实测 `git show HEAD:tests/cpu/manual_elf/dhrystone/dhrystone.elf` ⇒ **该文件不在 git 里**（`tests/cpu/manual_elf/.gitignore:3` 的 `*.elf` 规则 + 从未提交）⇒ 它是**未跟踪的手工产物** ⇒ ① 「替换旧 elf」不成立（无仓库产物可替换）；② **权威产物 = CMake 的 `build/dhrystone.elf`**（可复现、已入库构建逻辑）；③ 仓库内手工副本已同步为 CMake 产物字节（保持 TLM 测试可用），但**不作为基线来源**；④ **TLM 基线 CSV 无需改** —— 实测 TLM 侧用权威 elf 得 **cycles=794478 / DMIPS=1.43277**，与既有基线**逐位一致** ✓
- [ ] **Step 4: TLM 侧对齐** —— TLM 测试的 `tohost_addr = 0x80000000 + 0x1000`（`test_dhrystone.cpp:47`）与新布局**仍一致**（tohost 符号地址不变）⇒ TLM 侧无需改

### Task 4: ~~tohost 地址可配置（`--tohost-addr`）~~ → **删除（前提不成立）**

> **v2 变更**：v1 要求加 `--tohost-addr`，理由是「tohost word 硬编码 `kToHostWord=0x400`」。实测该硬编码**与 riscv-tests 约定一致且正确**；Task 2 的 PHDRS 双段方案让 Dhrystone/CoreMark 的 tohost **仍落 0x80001000** ⇒ **不需要可配置**。
>
> ⚠️ **连带结论**：不需改 `cpu_factory_chmem.h:313` 的 `set_tohost_probe_word(0x400)` ⇒ **不触碰 `ip/cpu/**`**（v1 的 AC-4「默认行为不变」与「不得改 ip/cpu/**」的自相矛盾随之消失）。
>
> **保留条件**：若 Task 2 Step 2 实测发现 tohost 无法落在 0x80001000（例如 PHDRS 布局冲突）⇒ **上报 architect**（这意味着需要一个真正的 tohost 可配置机制），**不得**擅自改 `ip/cpu/**` 的硬编码。

- [ ] **Step 1: 确认前提成立** —— Task 2 Step 2 的 `readelf -s` 显示 `tohost = 0x80001000` ⇒ 前提成立，本 Task 关闭
- [ ] **Step 2: 回归** —— `./build/bin/chipforge_tests_chmem "[verilator]"` 仍 PASS（48 ELF，96 assertions）

### Task 5: CoreMark 移植（EEMBC → RV32IM）

- [ ] **Step 1: 取源与许可** —— EEMBC CoreMark（BSD）`core_main.c` + `crc.c` + `portme.h` ⇒ vendored 进 `tests/cpu/manual_elf/coremark/`，附 LICENSE + README（来源与版本）
- [ ] **Step 2: 移植要点（写死，避免 agent 自由发挥）**
  - **计时**：CH_MEM **无 user 态 `rdcycle`（0xC00）**；`mcycle`（0xB00）**已实现**（`csr_chmem.h:161,170,179`，但走单独路径且 `read_valid=false` ⇒ user 态不可读）⇒ 迭代数与 score 由 crt0 写到**固定地址**（RW 段内、tohost 之后），**host 侧从 DMem 读回**；**禁止**用 wall-clock
  - **libc**：无 libc ⇒ `printf` 替代为「写固定地址」；栈初始化照抄 `dhrystone_crt0.S`
  - **段布局**：必须与 Task 2 同款 **RX + RW** 两段（**纯 R 段会被 loader 丢弃**）
  - **自校验**：`COREMARK_ERROR` 必须为 0（否则结果不可信 ⇒ 数值静默错）
- [ ] **Step 3: crt0 + ld 脚本** —— `coremark_crt0.S` + `link_coremark.ld`（同款 PHDRS 双段；tohost @ 0x80001000；score/iterations 落在 RW 段固定地址）
- [ ] **Step 4: 构建目标** —— `build_manual_elf.sh` 加 `coremark` 目标（`g++` 驱动）
- [ ] **Step 5: 验证** —— `cpu_verilator_sim --elf coremark.elf` ⇒ `TOHOST=1` + cycles 量级与 Dhrystone 同阶（**具体值以实测为准**）
- [ ] **Step 6: ★ 放弃判据（防 AI 硬扛/静默跳过）** —— 若 Step 2–5 在**本 change 的预算内**无法完成（判据：**任一步骤连续 2 次尝试仍失败，或引入新外部依赖**）⇒ **立即停止本 Task**，在 plan 与 change 文档中登记「CoreMark 移植**未完成** + 阻塞原因」，并按 **D1 的降级路径**执行：② 的 AC-8 改为「CoreMark 实测值待补」而**不宣告达标**，② 的 BP 验收降级为「功能存在 + Dhrystone 无分支敏感 ⇒ 性能收益不可判定」⇒ **不得**用「已实现但未测」冒充通过

### Task 6: 测量脚本 + 基线 CSV + **CI 门禁**

- [ ] **Step 1: 写测量脚本** —— `tools/measure_perf.sh`（或 `.py`）：跑 `cpu_verilator_sim` ⇒ 解析 `TOHOST=%u CYCLES=%u` ⇒ 按「关键口径」表算 DMIPS/MHz · CoreMark/MHz ⇒ **同时校验程序结果正确性**（Dhrystone checksum / CoreMark `COREMARK_ERROR`）
- [ ] **Step 2: ★「基线未漂移」判据写死（v1 歧义点）** —— 采用 **逐位相等**（`cycles` 与 `dmips_per_mhz_measured` 均须完全一致），**不使用容差**；理由：Verilator 版本 / toolchain / host 差异会体现在基线 CSV 里，任何数值漂移都应**显式重录基线**（附 commit）而非被容差吸收 ⇒ **杜绝「agent 自选 ±N%」**。若实测发现同机同版本仍不可复现 ⇒ **上报 architect**，由 architect 决定是否引入容差（写进基线 CSV 的 `notes`）
- [ ] **Step 3: ★CSV 归属二选一已定（v1 歧义点）** —— **新建** `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv`（**只放 CH_MEM/Verilator 行**）；**不动**既有 `dhrystone-baseline-matrix.csv`（它保留 TLM 的 legacy/fsm 两行）⇒ 脚本**只读新 CSV**（消除「取哪个 CSV」歧义）
- [ ] **Step 4: CSV 列（写死）** —— `elf,status,mode,kMaxCycles,dmips_per_mhz_measured,coremark_score,cycles,cycles_per_iter,checksum_ok,notes`（`coremark_score` 对 Dhrystone 行留空；`checksum_ok` 为结果正确性判据）
- [ ] **Step 5: ★CI 门禁 step（判据 4 的必要条件）** —— 在 `.github/workflows/architecture-gates.yml` 加一步（warn 级，`continue-on-error: true`）：`bash tools/measure_perf.sh --check-baseline` ⇒ 输出与基线 CSV 不一致则报出。**仅加 ctest entry 不满足 ADR-088 §5.2 判据 4**（CI 不跑 ctest）⇒ 此 step 是本 change 对判据 4 的**唯一**贡献
- [ ] **Step 6: ctest entry（辅助）** —— `tests/CMakeLists.txt` 加 `verilator_perf_baseline`；**TIMEOUT 先量后定**（Dhrystone 单跑 ≈ 800k cycles；CH_MEM 全套已 833s，CTest 默认上限 1500s ⇒ 单跑实测耗时 + 余量，**不照抄 120**）
- [ ] **Step 7: 填入实测值** —— **不得**先写目标值再倒推；必须由脚本实跑产出；notes 注明「CH_MEM/Verilator 全组合通路 ⇒ cycle 与 TLM **不可比**（ADR-088 §3）」+ 旧 elf 失效说明

### Task 6.5: ★两个交付点拆分（oracle 建议，v2）

| 交付点 | 内容 | 判据 |
|--------|------|------|
| **交付点 A** | Task 1 + 2 + 3 + 4 + 6（Dhrystone 全链路 + 测量脚本 + 基线 CSV + CI 门禁） | Dhrystone 跑通 + 47 ELF 回归 0 退化 + CI 门禁生效 ⇒ **可独立归档** |
| **交付点 B** | Task 5（CoreMark 移植） | CoreMark 跑通 + 自校验 PASS ⇒ **可独立归档**；若按 Task 5 Step 6 放弃 ⇒ 登记「未完成 + 阻塞原因」，**不阻塞交付点 A** |

⇒ **目的**：避免 ② 被 CoreMark 移植的不确定性阻塞（① 是 ② 的硬前置）。

### Task 7: 收口

- [ ] **Step 1: 8 门禁** —— 逐条跑并**记录期望输出关键行**（只看 exit 0 不足以识别「因缺 binary 被跳过」）：`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check` / `check_xlen_hygiene` / `check_tlm_freeze` / `check_coverage_parity_ledger`
- [ ] **Step 2: 全量回归** —— `bash tools/run_chipforge_tests.sh --all` ⇒ 4 entry **0 退化**（TLM 493 = 492 + 1 pre-existing；chmem 130）
- [ ] **Step 3: 零 `ip/` 改动核对** —— `git diff --stat -- 'ip/**'` ⇒ **空**（本 change 只碰 `tools/` + `tests/` + `tests/cpu/manual_elf/` + CSV + CI）
- [ ] **Step 4: commit** —— 分批：① ld + crt0 + elf + 构建目标 ② 测量脚本 + CSV ③ CI step + ctest ④ 文档

---

## Acceptance

- [ ] AC-1: `cpu_verilator_sim --elf tests/cpu/manual_elf/dhrystone/dhrystone.elf` 退出码 0 且 `TOHOST=1`（**当前不可用 ⇒ 本 change 的核心交付**）
- [ ] AC-2: 新 `dhrystone.elf` 有**恰好两个** PT_LOAD（`R E` + `RW`），**无纯 R 段**；`readelf -l` 可验（实测：`LOAD 0x80000000 R E` + `LOAD 0x80001000 RW`）
- [ ] AC-3: `readelf -s` 显示 `tohost` 符号 = **0x80001000**；`.rodata` 起始 **≥** `tohost + 4`（实测 `.tohost` @0x80001000（4B）/ `.rodata` @0x80001004 ⇒ 不重叠）
- [ ] AC-4: `build_manual_elf.sh` 有 dhrystone 目标，**连跑两次字节一致**（`cmp` 可验）
- [ ] AC-5: ★**程序结果正确性** —— Dhrystone `Number_Of_Runs` = 2000 **且** checksum 与参考一致；CoreMark `COREMARK_ERROR` == 0 ⇒ 测量脚本在结果错误时**必须失败**（不得只报 cycles）
- [ ] AC-6: ★**cycles 口径** —— 测量脚本用的 cycles **来自 `sim_main.cpp` 计数器**（`CYCLES=%u`），**不含** wall-clock / elaboration / 编译时间；脚本内有可 grep 的口径注释
- [ ] AC-7: ★**基线未漂移判据 = 精确相等**（Verilator 仿真确定性 ⇒ 精确相等合理；**无容差**，杜绝「agent 自选 ±N%」）；重跑不一致 ⇒ **更新基线 CSV + commit 说明原因**。CI 门禁 step（warn 级）已接入 `.github/workflows/architecture-gates.yml` ⇒ **ADR-088 §5.2 判据 4 得到部分满足**（注意：CI 本身**不跑 ctest** ⇒ 仅 ctest entry 不够）
- [ ] AC-8: 新基线 CSV `soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv` 存在且**只含** CH_MEM/Verilator 行；既有 `dhrystone-baseline-matrix.csv` **未被修改**（`git diff` 为空）
- [ ] AC-9: CSV 全部数值由脚本实跑产出（notes 注明「cycle 与 TLM 不可比」+「旧 elf 已替换且不可比」）
- [ ] AC-10: CoreMark 资产 vendored（含 LICENSE + 来源说明）+ 可构建 + `TOHOST=1` + 自校验 PASS；**若放弃** ⇒ 按 Task 5 Step 6 的降级路径登记，**不宣告达标**
- [ ] AC-11: 8 架构门禁全 PASS + 全量回归 0 退化 + **`ip/**` 零改动**
- [ ] AC-12: **不改**任何门禁判据值（1.4 / 1.9 / 2.3 原样保留）；判据重校准 ADR 的 owner（architect）与窗口（① 合入后、② 开工前）已登记
- [ ] AC-13: ★**两个交付点可独立归档** —— 交付点 A（Dhrystone 全链路）在 CoreMark **未完成**时也能归档（不阻塞 ② 的开工）；交付点 B 的完成/放弃状态已登记

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **ELF 段路由是最易踩的坑**：纯 R 段被 loader 丢弃 ⇒ 只读常量读成 0 ⇒ **分数静默错**（不崩、不报错） | Task 1 Step 2 + Task 2 Step 1 双重约束；Task 6 Step 5 的 checksum 判据兜底 |
| R2 | **CoreMark 移植工作量超预期**（无 libc / 无 user 态 `rdcycle`） | Task 5 Step 6 有**明确的放弃判据 + 降级路径**；★**中间验收改用分支 microbenchmark**（oracle 建议）：一组可控预测率的 pattern 循环，直接测「预测命中/未命中 ⇒ flush 与 cycle 差异」，比全移植 CoreMark 便宜一个量级 ⇒ **CoreMark 保留为最终载体但允许顺延**（且「≥1.9」在 ADR 出前本无判定力，见 D2） |
| R3 | ~~CH_MEM 全组合 ⇒ cycle 数远小于 TLM~~ → **实施期实测推翻**：CH_MEM/Verilator **914544** vs TLM **794478** ⇒ CH_MEM 反而 **多 15%**（组合式无流水线 load-use 转发 ⇒ 依赖指令要等）⇒ **原假设「组合式更快」是错的**；且 DMIPS 1.24467 **低于 1.4 门禁** ⇒ **判据重校准 ADR 确实需要**（D2 从「预防性」变为「实测必需」） |
| R4 | 「基线未漂移」逐位相等可能因 toolchain/Verilator 版本抖动而 flaky | 抖动**显式重录基线**（附 commit），不用容差吸收；若同机同版本仍不可复现 ⇒ 上报 architect |
| R5 | ctest TIMEOUT 拍脑袋 | Task 6 Step 6：先量单跑耗时再定 |
| R6 | 旧 `dhrystone.elf` 被替换 ⇒ 旧基线值失效 | Task 3 Step 3 + AC-9：notes 显式标注「旧值不可比」 |
| R7 | ② 的 AC-8 依赖本 change 的 CoreMark 实测 ⇒ 若 CoreMark 放弃 ⇒ ② 的性能 AC 降级 | D1 降级路径已在 ① Task 5 Step 6 + ② AC-8 双向登记 |

## 迁移说明

落地为 `.rddf/plans/chmem-verilator-perf-harness.md`，change 名 `chmem-verilator-perf-harness`；建议走 **rdd-quick**（单一工具层 + 脚本/资产，AC 可机械验证）——但若 Task 5 的 CoreMark 移植被评为 complex，则升级为完整 rdd-builder P1–P3。


---

## 实施期修正记录（2026-10-24，交付点 A 实施实测）

| # | plan v2 的判断 | 实测事实 | 处置 |
|---|---------------|---------|------|
| 1 | 「`build_manual_elf.sh` 无 dhrystone 目标 ⇒ 须新增」 | **错** —— 仓内已有 CMake 目标 `dhrystone_elf`（`src/cf_plugin/CMakeLists.txt:136-146`） | 不新增 shell 目标（避免第二条构建路径）；**AC-4 改判据**为「CMake 目标可复现 + `nm -u` 空 + 同工具链字节一致」 |
| 2 | 「替换旧 elf」 | 旧 elf **不在 git 里**（`.gitignore` `*.elf`）⇒ 无仓库产物可替换 | 权威产物 = `build/dhrystone.elf`；仓库手工副本同步为权威字节（TLM 测试仍可用）；**TLM 基线 CSV 保持不变**（实测 794478/1.43277 逐位一致） |
| 3 | 「DH 结果正确性用 checksum」 | `dhrystone.c` **无 checksum 比对** ⇒ 最强可得信号是 **TOHOST==1**（`tohost_pass()` 写 1 后死循环 ⇒ 等价于 2000 迭代全部完成） | AC-5 判据改为「TOHOST==1 且非 TIMEOUT」；checksum 比对登记为后续项（需改 `dhrystone.c` ⇒ 超 scope） |
| 4 | 「CH_MEM 全组合 ⇒ cycle 远小于 TLM」 | **反了** —— CH_MEM **914544** vs TLM **794478**（多 15%）⇒ DMIPS 1.24467 **< 1.4 门禁** | R3 改写；⇒ **判据重校准 ADR 从「预防性」变为「实测必需」**（architect） |
| 5 | 「工具链前缀无关」 | **有关** —— `riscv32-unknown-elf-g++`（CMake 用）`.text` 0x494 vs `riscv64-` 0x3d4 ⇒ 字节与 cycle 均不同 | 测量脚本**只用** `build/dhrystone.elf`（CMake 产物）；手工构建仅用于对照 |
| 6 | 「tohost 需 `--tohost-addr`」 | **不需要** —— 实测 `tohost` = 0x80001000 = `kToHostWord` 0x400 ⇒ 三处一致 | Task 4 保持关闭（已验证前提成立） |

**实测结果（交付点 A 完成）**：

| 项 | 值 |
|---|---|
| Verilator 后端 | `TOHOST=1 CYCLES=914544 PASS` ⇒ **Dhrystone 在 CH_MEM 后端首次跑通** |
| DMIPS/MHz（CH_MEM） | **1.24467**（`2000*1e6/(1757*914544)`）⇒ **低于 1.4 硬门禁** |
| TLM 侧（同一权威 elf） | `cycles=794478 / dmips=1.43277` ⇒ 与既有基线**逐位一致**，TLM 门禁**未受影响** |
| 单跑耗时 | 20.72s（ctest 实测）⇒ TIMEOUT 300 充足 |
