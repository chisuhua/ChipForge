---
initiative: wave5-isa-coverage-and-bp
priority: P1
version_target: v0.11.0
status: ready
depends_on:
  - chmem-dmem-read-latency  # ✅ resolved (archived 2026-10-20 → openspec/changes/archive/2026-10-20-chmem-dmem-read-latency/)
---

# chmem-dmem-read-latency-aligned — 上游 read-port 契约落地后的 CH_MEM dmem 对齐闭环

> **本 change 是 `2026-10-20-chmem-dmem-read-latency`（归位 change）的 follow-up**。
> 前者只做归位与固化（**明确不实施修复**），把"10 个 rv32ui load/store ELF FAIL"登记为
> 有 owner、有根因、有方案的已知项，并写下 A/B/C 三个方案。
> 本 change 承接方案 B 的落地结果，把该已知项**闭环**。

## Why

上游 CppHDL 已提交并归档该修复：

| 上游 commit | 内容 |
|---|---|
| `c27543d` | `fix(sim): align ch_mem::aread with generated Verilog 0-cycle assign` |
| `5b37b7d` | `docs(openspec): archive sim-mem-read-timing-align-2026-10` |
| `eccf58f` | `test(dual): add cross-backend parity for mem read port timing` |
| `cba0571` | `docs(openspec): archive dual-backend-mem-read-timing-parity-2026-10` |

即上游把 `async_read` 读端口归入组合求值并建立 `read_port → data_proxy` 数据流边，
**Simulator 的 `ch_mem::aread` 现在与生成的 Verilog `assign` 同为 0-cycle**，
`mem-read-timing` capability 的 R3/R4 已随之改写。

**ChipForge 侧实测结果（2026-10-21，升级上游 deps 后）**：

| 指标 | 修复前 | 修复后 |
|---|---|---|
| `[chmem]` family | 73 case / 72 PASS / **1 failed**（10 个 ELF） | 73 case / **73 PASS / 0 failed** |
| `chmem_vendored_elf_tohost1` | 47 ELF 中 10 个 load/store FAIL | **47 ELF 全 PASS**（16410 assertions） |
| 失败断言数 | 10 | **0**（唯一失败是本项目自己的 drift pin，见下） |
| `[verilator]` | 4 case / 122 assertions PASS | 不变 |
| TLM | 480/481 PASS | 不变（TLM 路径不涉及） |
| **ChipForge 业务代码改动** | — | **零**（`ip/cpu/plugins/dmem_chmem.h` 无需改） |

⇒ **方案 B（上游改）被正式验证为"零 ChipForge 数据通路改动"的唯一解法**；
方案 A（dmem 改 `sread`）此前实测反而恶化（10 → 11 FAIL），已放弃。

## What Changes

1. **drift pin 反转**（`tests/cpu/test_cpu_chmem_mem_read_drift.cpp`）
   该测试原本断言"**存在** 1 拍滞后"（`mismatches == 6` 且 `off_by_one == 6`），
   是"钉死已知缺陷"的回归锚。上游修复后它**必然 FAIL**（实测 `0 == 6`）。
   ⇒ 改为断言"**skew 已消除**"：每拍观测值 == 当前地址内容，且 != 上一拍地址内容。
   断言**幅度**的强度保留（`off_by_one == 0` 排除"恰好等于上一拍"的巧合），
   并新增"上游若退回 posedge 求值即 FAIL"的方向敏感性 —— 即把回归锚从
   "钉死缺陷"改成"**锁住修复**"。

2. **capability spec 的 known-deviation 撤销**（`openspec/specs/chmem-dmem-read-latency/spec.md`）
   - Requirement 1 的 Scenario「The known read-port skew SHALL remain pinned」
     → 改为「The read-port skew SHALL remain absent」（锁住 0-cycle 对齐）
   - Requirement 2「The upstream read-port contract SHALL NOT be silently violated」
     → 改为陈述"上游契约已由 `c27543d` 改写，CH_MEM 侧现与其一致"
   - Purpose 段的 **Known deviation** 警示块删除（该 deviation 已不存在）
   - Requirement 1 的正文里"这是今天被违反的"改为"修复前的成因"

3. **台账更新**
   - `AGENTS.md` known-issues：`⚠️ CH_MEM dmem 读路径 1-cycle 滞后` 条目
     → 移入「已解决」并写明上游 commit 与实测数据
   - `AGENTS.md` `[chmem]` family 计数：`72/73 PASS` → `73/73 PASS`
   - `.rddf/roadmap/features/feat-phase-6d-rtl-verification.md`：
     「已知问题（owned）」段关闭；**E13 阻塞解除**（`chipforge_tests_chmem` 完整
     5-stage + riscv-tests + Verilator 全 PASS 的前置条件已满足）

## Acceptance

- [ ] `tests/cpu/test_cpu_chmem_mem_read_drift.cpp` 反转断言后 PASS，且对
      "上游退回 posedge 求值"敏感（变异对照，见下）
- [ ] `./build/bin/chipforge_tests_chmem "[chmem]"` → **73 case / 73 PASS / 0 failed**
- [ ] `chmem_vendored_elf_tohost1` → 47 ELF 全 PASS（`tohost == 1`，严格判定）
- [ ] `./build/bin/chipforge_tests`（TLM）→ 与修复前一致（480/481，唯一失败为
      已知 `test_7stage_riscv.cpp:124` infra drift）
- [ ] `./build/bin/chipforge_tests_chmem "[verilator],[mmu-verilator]"` → 不变（PASS）
- [ ] `openspec validate --specs --strict` 中本 capability PASS
- [ ] 5 个架构 gate 全 exit 0

### 变异对照（Task 3 的实测证据）

上游 `c27543d` 含**两个独立成分**，二者作用不同 —— 用两次变异把责任分清：

| 变异 | 改了什么 | `[mem-drift]` pin | vendored ELF |
|---|---|---|---|
| A | 只把 async 读端口改回 `sequential_instr_list_`（`simulator_init.cpp`） | **FAIL**（`6 == 0`） | **全 PASS**（0 failed） |
| B | A + 把 `mem_port_impl.h/.cpp` 回退到 `c27543d^`（无 `src` 边） | **FAIL** | **FAIL，精确 10 failed**（= 原有 10 个 load/store ELF） |
| 还原 | `git checkout HEAD --` 三个文件 | PASS | 47/47 PASS |

⇒ **承重件是 ① `read_port → data_proxy` 的 src 边**（提供拓扑序 ⇒ 修好 ELF 失败面）；
**② 分类改动**决定与 Verilog 的 0-cycle 对齐 ⇒ 正是 `[mem-drift]` pin 的检测对象。
⇒ 该 pin 对"上游退回 posedge 求值"**方向敏感**，作为回归锚有效。

> 注：变异 B 同时精确重现了 10 个 ELF FAIL ⇒ 原始故障面可按需复现，
> 排查上游读端口回归时不必"相信历史记录"。

## Capabilities

- **MODIFIED** `chmem-dmem-read-latency` ×2（Requirement 1 / Requirement 2）

## Impact

- **受益**：10 个 rv32ui load/store ELF 从长期红点变为全绿；CH_MEM 与 Verilator
  后端在 dmem 读路径上**真正同构**；`feat-phase-6d-rtl-verification` 的 E13 阻塞解除
- **回归面**：改动仅 1 个测试 + 3 处文档 + spec delta，**零业务代码改动**
- **风险**：drift pin 的语义从"钉死缺陷"变为"锁住修复"，若上游未来再把 async
  读端口改回 sequential，本测试会 FAIL —— 这正是期望的方向敏感性
- **上游依赖**：需要 CppHDL ≥ `c27543d`（含 `5b37b7d`/`eccf58f`/`cba0571`）；
  本仓库的 `build/_deps/install` 需重新 install 上游 lib

### Non-Goals

- 不改 `ip/cpu/plugins/dmem_chmem.h`（实测无需改）
- 不改 TLM 路径
- 不动上游 CppHDL 仓库
- 不处理 `test_7stage_riscv.cpp:124` 的 node_count infra drift（独立项）
