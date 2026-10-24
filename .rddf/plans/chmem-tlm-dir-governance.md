# chmem-tlm-dir-governance — 执行文件

> **change**: `chmem-tlm-dir-governance`
> **来源**: B0 剩余项 86（`docs/architecture/tlm-retirement-b1-capacity-plan.md` §5.1 末行）· ADR-088 §5.1 双模共享文件清单（9 个，含 `cpu_keys.h` / `mmu_keys.h`）
> **规模**: 小（**纯改名 + 文档，不删任何业务代码**）· **前置**: 无
> **性质**: **目录名去误导**（**不是**删除）—— 把「目录名带 `tlm/` 但实际双模共享」的文件迁到中性命名空间；把**真正 TLM-only** 的文件**原地保留**（删除属 B3）
> **路线图**: `feat-tlm-backend-retirement` B0 剩余项 86 · `objective-ip-cpu-tlm-backend-retirement` B0 段
> **上游裁决**: ADR-088 §5.1（双模共享文件清单 —— **include 闭包实测**，禁止按目录名/文件名推断）· ADR-088 §2 冻结条款（迁文件不改函数 ⇒ 不触发冻结门禁）

---

## Goal

消除 `ip/*/tlm/` 目录名带来的**「目录名 = 后端绑定」误导**，同时**零风险**（不改任何行为、不删任何业务代码）。

### 为什么必须拆成两件事（实施前的实测结论）

原 B0 剩余项 86 的表述是「`ip/{mmu,cpu}/tlm/` 双模共享文件迁 `core/`」—— **实测证明该表述不准确**（2026-10-24 planner 核验）：

| 目录 | 文件 | 行数 | 实测归属 | 正确处置 |
|---|---|---|---|---|
| `ip/cpu/tlm/` | `README.md` | 41 | **纯文档**（描述 CppTLM ISS） | **迁 `ip/cpu/docs/`** 或改写为「历史说明」 |
| `ip/cpu/tlm/` | `cpu_keys.h` | 73 | **真双模共享**（`ip/cpu/plugins/mmu.cpp:27` · `mmu_exception_handler.h:18` · `tests/cpu/test_cpu_chmem_keys_modality.cpp:62` ⇒ **CH_MEM binary 真编译**） | **迁 `ip/cpu/core/`** ✅ |
| `ip/mmu/tlm/` | `mmu_keys.h` | 96 | **真双模共享**（`ip/mmu/plugins/MMUPlugin_chmem.h:70` · `ip/cpu/plugins/dmem_chmem.h:24` · `ibus_chmem.h:26` ⇒ **CH_MEM binary 真编译**） | **迁 `ip/mmu/core/`** ✅ |
| `ip/mmu/tlm/` | `MMUPlugin.h` | 151 | **TLM-only**（CH_MEM 侧 **0 处真 include** —— `hazard_chmem.h:167` / `mmu_exception_handler_chmem.h:21,27` 只是**注释提及** `RiscvMMUPluginChmem`，非 include） | **原地保留**（删除属 B3）⇒ 仅需**文档标注**「TLM-only」 |
| `ip/mmu/tlm/` | `MMUPlugin.cpp` | 178 | **TLM-only**（`src/cf_plugin/CMakeLists.txt:97` 编译 · `src/cf_plugin/bridge/mmu_bridge*.{h,cpp}` 消费 ⇒ **bridge 适配层**，ADR-041 D4 例外） | **原地保留**（同上） |
| `ip/cache/tlm/` | `.gitkeep` | 0 | **空目录占位** | **直接删除空目录**（无消费点） |

⇒ **两件事**：
- **事情 A（去误导）** —— 迁 2 个真双模共享文件 + 1 个纯文档 README ⇒ **「目录名带 tlm/ 但内容与后端无关」这个误导源被消除**
- **事情 B（标注而非删除）** —— `MMUPlugin.{h,cpp}` 是**真 TLM-only**，但**删除属 B3**（`ip/cpu/plugins/mmu.h` + bridge 层 + 3 个测试文件消费）⇒ 本 change **只做文档标注 + 冻结门禁登记**，**不删**

> ★**归属纪律**（ADR-088 §5.1）：**禁止按目录名/文件名/「看起来像 TLM」推断**。本 plan 的每一行归属都由 **include 闭包实测**（`grep -rn '^#include.*<file>'`）确定，**注释提及不算 include**。

---

## Architecture

```
ip/cpu/tlm/cpu_keys.h        →  ip/cpu/core/cpu_keys.h          (73 行, 双模共享)
ip/mmu/tlm/mmu_keys.h        →  ip/mmu/core/mmu_keys.h          (96 行, 双模共享)
ip/cpu/tlm/README.md         →  ip/cpu/docs/tlm-legacy.md       (41 行, 纯文档改写)

ip/mmu/tlm/MMUPlugin.{h,cpp}  →  原地保留 + 文件头加 TLM-only 标注 (329 行)
ip/cache/tlm/                →  删除空目录 (0 行)
```

**不动**：`ip/cpu/core/payload_common.h` 已在 `core/`（ADR-088 §5.1 第 6 项）⇒ **命名先例已存在** ⇒ 本次迁移与既有布局一致，不是新发明。

### 为什么 `ip/cpu/core/` 与 `ip/mmu/core/` 是**两个**目录而非一个共享 `core/`

`ip/cpu/core/payload_common.h` 已存在（CPU 侧命名空间）⇒ CPU 的 keys 落 `ip/cpu/core/`；MMU 的 keys 落 `ip/mmu/core/` ⇒ **保持每个 IP 自包含**（与 ADR-083 的 IP 自治一致），不引入跨 IP 共享目录。

---

## Tech Stack

C++23 · CMake（`src/cf_plugin/CMakeLists.txt` 的 include 路径）· 无新依赖 · **零行为改动**

---

## Tasks

### Task 1: 事实复核（不写代码）

- [ ] **Step 1: include 闭包实测** —— 对 6 个文件各跑 `grep -rn '^#include.*<basename>' ip/ src/ tests/ bundles/`，**逐条记录真 include vs 注释提及** ⇒ 复核本 plan「Architecture」表的归属列（**若与实测不符 ⇒ 以实测为准，先更新本 plan 再动手**）
- [ ] **Step 2: 冻结门禁候选面确认** —— 跑 `bash tools/check_tlm_freeze.sh` ⇒ 确认 `cpu_keys.h` / `mmu_keys.h` **不在候选面**（它们在 `.rddf/state/tlm-shared-files.txt` 白名单里）⇒ 迁移后**白名单路径须同步更新**（否则门禁会把新路径当 TLM-only 候选 ⇒ 误报）
- [ ] **Step 3: include 路径影响面** —— 确认无 `#include <ip/cpu/tlm/...>` 形式的**绝对路径**依赖（CMake `include_directories` 层面）⇒ 若有，须同步改 CMake

### Task 2: 事情 A —— 双模共享文件迁 `core/`

- [ ] **Step 1: `git mv` 两个 keys 头** —— `ip/cpu/tlm/cpu_keys.h → ip/cpu/core/cpu_keys.h`；`ip/mmu/tlm/mmu_keys.h → ip/mmu/core/mmu_keys.h`（**用 `git mv` 保留历史**）
- [ ] **Step 2: 更新全部真 include 路径** —— 逐个改（实测清单）：
  - `cpu_keys.h`：`ip/cpu/plugins/mmu.cpp:27` · `ip/cpu/plugins/mmu_exception_handler.h:18` · `tests/cpu/test_cpu_chmem_keys_modality.cpp:62` · `tests/cpu/test_cpu_riscv_mmu_hooks.cpp:21` · `tests/cpu/test_mmu_exception_handler.cpp:22`
  - `mmu_keys.h`：`ip/mmu/plugins/MMUPlugin_chmem.h:70` · `ip/cpu/plugins/dmem_chmem.h:24` · `ip/cpu/plugins/ibus.h:31` · `ip/cpu/plugins/ibus_chmem.h:26` · `ip/cpu/plugins/dbus.h:29` · `ip/cache/plugins/L1CachePlugin.cpp:35` · `ip/mmu/tlm/MMUPlugin.cpp:15` · `ip/mmu/tlm/MMUPlugin.h:27` · + 6 个测试文件（`tests/cache/test_mmu_cache_integration.cpp` · `tests/cpu/test_cpu_riscv_mmu_hooks.cpp` · `tests/cpu/test_mmu_paddr_propagation.cpp` · `tests/mmu/test_mmu_keys_modality.cpp` · `tests/mmu/test_mmu_plugin.cpp` · `tests/mmu/test_ptw_stall_integration.cpp` · `tests/mmu/test_ptw_tlb_refill_integration.cpp`）
  - ⚠️ **注释里的路径引用也要更新**（`mmu_exception_handler_chmem.h:17,85,105` 提到 `ip/cpu/tlm/cpu_keys.h`）⇒ 否则留下**指向已不存在路径的注释** = 误导源未消除
- [ ] **Step 3: 同步冻结门禁白名单** —— `.rddf/state/tlm-shared-files.txt` 的 2 条路径改新位置（**必须** —— 否则门禁把新路径当 TLM-only 候选 ⇒ 误报）
- [ ] **Step 4: 编译验证** —— 双 binary（TLM + CH_MEM）clean build 通过（**CH_MEM binary 必须过** ⇒ 它真编译 `mmu_keys.h`；`cpu_keys.h` 由 `test_cpu_chmem_keys_modality.cpp` 真编译）

### Task 3: 事情 A —— README 改写

- [ ] **Step 1: `git mv`** —— `ip/cpu/tlm/README.md → ip/cpu/docs/tlm-legacy.md`
- [ ] **Step 2: 内容改写** —— 现内容是「CppTLM ISS 规划」且标注 `⚠️ Status: Phase 5 Planning — RTL not yet implemented` ⇒ 已是**过时文档** ⇒ 改写为**历史说明**（说明该目录曾承载 TLM ISS 规划、现已退役、CH_MEM 为唯一后端），并指向 ADR-088
- [ ] **Step 3: 修引用** —— `ip/README.md` · `ip/cpu/README.md` · `ip/cpu/STATUS.md` · `ip/cpu/docs/README.md` · `ip/cache/STATUS.md` · `ip/cache/docs/architecture.md` 中指向 `ip/cpu/tlm/README.md` 的链接改指新路径

### Task 4: 事情 B —— TLM-only 文件标注（**不删**）

- [ ] **Step 1: `MMUPlugin.h` / `MMUPlugin.cpp` 文件头加标注** —— 明确「**TLM-only**（CH_MEM 侧无对应实现；CH_MEM 版在 `ip/mmu/plugins/MMUPlugin_chmem.h`）· 删除属 B3 · ADR-088 §5.2」
- [ ] **Step 2: 目录级说明** —— 在 `ip/mmu/tlm/` 放 `README.md` 说明该目录现存文件的性质（**TLM-only ⇒ B3 删除**）⇒ 消除「目录名 = 后端绑定」的**反向**误导（读者可能以为 `tlm/` 下的东西都是双模共享）
- [ ] **Step 3: `ip/cache/tlm/` 空目录删除** —— `git rm` 掉 `.gitkeep` + 删目录（0 行 · 0 消费点 ⇒ **零风险**）

### Task 5: 收口

- [ ] **Step 1: 8 架构门禁全 PASS** —— 特别验 `check_tlm_freeze.sh`（白名单路径同步后不得误报）与 `check_coverage_parity_ledger.sh`（台账**条目名**含路径 ⇒ 迁文件后若台账引用旧路径须同步）
- [ ] **Step 2: 全量回归 0 退化** —— 失败集合**恰为 1 个 pre-existing**（`tests/cpu/integration/test_7stage_riscv.cpp:124`）· TLM 493=492+1 · `[cpu]` 137 · chmem 130
- [ ] **Step 3: 文档同步** —— `ip/cpu/README.md:87`（提到 `ip/cpu/tlm/cpu_keys.h`）· `ip/mmu/docs/integration.md` · `ip/cache/docs/integration.md` 等处路径引用同步
- [ ] **Step 4: 台账复核** —— `docs/architecture/tlm-coverage-parity-ledger.md` 中若有引用 `ip/cpu/tlm/` 或 `ip/mmu/tlm/` 的路径 ⇒ 同步（**台账红线：条目态继承族级 §2 ⇒ 改条目必须同改族级**）

---

## Acceptance

- [ ] **AC-1** 6 个文件的归属与本 plan 表**一致**（include 闭包实测复核通过；**注释提及不算 include**）
- [ ] **AC-2** `ip/cpu/tlm/` 与 `ip/mmu/tlm/` **不再含任何双模共享文件**（只剩 TLM-only 的 `MMUPlugin.{h,cpp}`）⇒ 「目录名 = 后端绑定」的误导源消除
- [ ] **AC-3** `ip/cpu/core/cpu_keys.h` + `ip/mmu/core/mmu_keys.h` 存在且被**全部真 include 点**正确引用（**0 处残留旧路径**，含注释）
- [ ] **AC-4** 双 binary clean build 通过（CH_MEM binary 真编译 `mmu_keys.h` ⇒ **必过**）
- [ ] **AC-5** `.rddf/state/tlm-shared-files.txt` 的 2 条路径已同步 ⇒ `check_tlm_freeze.sh` **violations=0** 且候选面**不含**新路径
- [ ] **AC-6** `ip/mmu/tlm/MMUPlugin.{h,cpp}` **保留**（329 行未删）且有 TLM-only 标注；`ip/cache/tlm/` 空目录已删
- [ ] **AC-7** 8 架构门禁全 PASS + 全量回归 0 退化（失败集合恰 1 个 pre-existing）
- [ ] **AC-8** **零行为改动**（纯改名 + 注释/文档）⇒ 无需新增测试；**无 `ip/` 业务逻辑变更**

---

## 风险与未知

| # | 风险 | 处置 |
|---|------|------|
| R1 | **误把注释提及当 include** ⇒ 把 TLM-only 文件误判为双模共享 ⇒ 迁到 `core/` 后 CH_MEM 侧编译断裂 | Task 1 Step 1 强制用 `^#include` 锚定；Task 2 Step 4 用 CH_MEM binary 编译做硬验证 |
| R2 | **冻结门禁白名单未同步** ⇒ 迁后新路径被当 TLM-only 候选 ⇒ 门禁误报 | Task 2 Step 3 显式列为 task；AC-5 机器可验 |
| R3 | **台账条目引用旧路径** ⇒ 台账与仓库不一致（台账是机检对象） | Task 5 Step 4 + AC-7 的 `check_coverage_parity_ledger.sh` |
| R4 | 有人把本 change 当成「删 TLM」 ⇒ 提前删 `MMUPlugin.{h,cpp}` ⇒ bridge 层与 3 个测试断裂 | **本 plan 不删任何业务代码**（AC-6 明确）；删除属 B3（ADR-088 §5.2 判据 1-5） |
| R5 | 迁移后 `ip/cpu/core/` 同时有 `payload_common.h` + `cpu_keys.h` ⇒ 有人误以为 `core/` 是「共享层」而把 MMU 的也塞进来 | Task 2 明确「每 IP 自包含，不引入跨 IP 共享目录」⇒ `mmu_keys.h` 落 `ip/mmu/core/` |
| R6 | **`README.md` 内容本身已过时**（`Phase 5 Planning — RTL not yet implemented`）⇒ 只改路径不改内容 ⇒ 过时文档继续误导 | Task 3 Step 2 强制**内容改写**（历史说明 + 指向 ADR-088） |

---

## 迁移说明

- **执行顺序**：Task 1（复核）→ Task 2（迁 keys）→ Task 3（README）→ Task 4（TLM-only 标注）→ Task 5（收口）
- **与 ②/③/④/⑤ 的关系**：**完全独立**（零 `ip/` 业务逻辑改动）⇒ 可与任意一项并行
- **与 B3 的关系**：本 change 是 B3 的**前置之一**（消除目录名误导，让 B3 的「TLM-only 文件归零」判定有清晰起点）；但 **B3 本身不依赖本 change 完成**
- **不在本 change 范围**：删除任何 TLM-only 文件（属 B3）· 改 `ip/{cpu,mmu}/tlm/` 之外的目录布局 · 冻结门禁逻辑本身的改造
