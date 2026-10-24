# ④ chmem-dbus-lsu — 执行文件（v2，oracle + metis 审阅后重写）

> **change**: `chmem-dbus-lsu`
> **规模**: 中 · **前置**: 阶段 A（抽取重构）**无前置**；★**阶段 B（接 Cache）需 P-2 `chmem-load-use-bypass`**（同 ③）；★**与 P-2 的边界（planner 裁决）**：本 change 拥有**总线层组织**（仲裁 + 宽度语义抽取），P-2 拥有**依赖判定**（RAW 精确化 / 旁路）⇒ **若 P-2 选型结论为「真旁路」且需数据通路重组 ⇒ 该重组归本 change 阶段 A ⇒ P-2b 必须排在本 change 阶段 A 之后**（避免双写）
> **路线图**: `feat-tlm-backend-retirement` B1 第 4 项
> **上游**: ADR-040 v3.0（payload key 模态 §10.5 / 顺序契约 §10.6）· ADR-083（双文件分离）
> **v2 重写原因**: oracle 审阅发现 **v1 的因果链是错的** —— ①「宽度语义」**已经存在**于 `ip/cpu/plugins/dmem_chmem.h:104-160`（funct3 全宽度译码 + sub-word RMW + 符号扩展；vendored 47 ELF 全 PASS 为证）；② TLM 侧**不存在**宽度语义 case（`test_lsu.cpp` 4 条 / `test_dbus.cpp` 5 条全是 instantiate/build/destruction）⇒ v1 的「迁移 TLM 侧 LSU 宽度判据」是**虚构的**；③ v1 的「取消阶段 1 / 与③并行」会造成**双 owner**（阶段 2「接 Cache」与 ③ 的「Cache 确实生效」是同一路径）。

---

## Goal

把 **CH_MEM 侧已实现但寄居在 `dmem_chmem.h` 里的访存宽度语义，抽取为独立的 LSU / DBus 层**，并新增**总线仲裁层**（为 ③ 的多主端口接入准备）。

**这不是「补宽度语义」** —— 宽度语义（lb/lh/lw/lbu/lhu/sb/sh/sw + 符号扩展 + sub-word RMW）**已在 `dmem_chmem.h` 里工作**（vendored 47 个 rv32ui/rv32um ELF 全 PASS 即为行为证据）。本 change 的实质是**两件事**：

1. **抽取重构**（存量）：把译码/宽度/掩码逻辑从 `dmem_chmem.h` 抽出为 `ip/cpu/arch/riscv/lsu_chmem.h` + `ip/cpu/plugins/dbus_chmem.h` ⇒ **行为必须逐位不变**（判据 = vendored 47 ELF 仍全 PASS + **byte-identical**）
2. **仲裁层**（增量）：DBus 作为总线层存在（多主端口仲裁 + 握手）⇒ **只有在接入 Cache（③，多主端口）时才有实际价值** ⇒ 这是本 change 依赖 ③ 的**真正原因**（v1 写成「命中判定需要 Cache」是错的）

## Architecture

```
ip/cpu/arch/riscv/lsu_chmem.h     (新: 从 dmem_chmem.h 抽出的 load/store 译码+宽度+掩码)
ip/cpu/plugins/dbus_chmem.h       (新: 总线层 —— 仲裁/握手/byte-enable; 多主端口时仲裁才生效)
        │
        ├─ 阶段 A (可独立, 无需 ③): 抽取重构 + 仲裁层骨架(单主端口) ⇒ 行为 byte-identical
        └─ 阶段 B (需 ③): 多主端口仲裁接入 Cache ⇒ **本 change 独占 Cache 接线**（③ 只交付 Cache 本体）
```

**owner 分配（v2 明确，避免双 owner）**：

| 事项 | owner |
|------|-------|
| L1CachePlugin **本体**（存储/替换策略/命中判定/接口） | **③** |
| Cache **接入 CPU 数据通路**（DBus↔Cache 接线、命中/未命中分流、回填） | **④**（本 change，阶段 B） |
| Cache **注册进工厂** + `enable_cache` 解禁 | **③** |

## Tech Stack

C++23 · CppHDL · Catch2 · ADR-040 v3.0 约束

---

## Tasks

### Task 1: 事实复核（不写代码）

- [ ] **Step 1: 读 CH_MEM 侧现状** —— `ip/cpu/plugins/dmem_chmem.h:104-160` ⇒ 提取已有的宽度译码/掩码/符号扩展逻辑（**这就是要抽取的代码本体**）；`dmem_chmem.h:138-148` 的 PADDR select 接线口径也要对齐（③ 预埋的接口）
- [ ] **Step 2: 读 TLM 侧** —— `ip/cpu/plugins/dbus.h`(150) + `.cpp`(20) + `ip/cpu/arch/riscv/lsu.h`(82) ⇒ 提取**总线层语义**（请求/响应握手、仲裁、byte-enable）；⚠️ **TLM 侧无宽度语义 case**（`test_lsu.cpp` 4 条 / `test_dbus.cpp` 5 条全是 instantiate/build/destruction）⇒ **没有可「迁移」的宽度判据**，宽度行为锚 = vendored rv32ui/rv32um ELF（`[riscv-tests]` 族已 covered）
- [ ] **Step 3: 抽取边界确认** —— 判定哪些逻辑搬进 `lsu_chmem.h`、哪些留在 `dmem_chmem.h`（存储 vs 译码）⇒ 写成一页「搬/留」清单
- [ ] **Step 4: 确认无 owner 冲突** —— `grep -rn "dbus\|lsu" openspec/changes/ .rddf/roadmap/`（排除 archive）

### Task 2: 阶段划分与依赖（v2 修正）

- [ ] **Step 1: 阶段 A 的交付边界** —— 抽取重构 + 单主端口仲裁骨架 ⇒ **行为必须 byte-identical**（不改任何 ISA 可见行为）
- [ ] **Step 2: 阶段 B 的前置（★v3）** —— ③ 的 Cache 本体交付且注册生效 **且 P-2 `chmem-load-use-bypass` 已落地**（Cache 命中路径的 load-use 解析依赖它）⇒ 阶段 B 才能接线
- [ ] **Step 3: 非对齐访问语义基准（★v2 必须先定）** —— RISC-V 对非对齐 load/store **要求 trap**（除非实现显式支持）⇒ 必须先决定 CH_MEM 侧是「支持」还是「trap」：
  - **建议：保持现状 = 不支持非对齐**（沿用当前 `dmem_chmem.h` 的行为，vendored 47 ELF 全是对齐访问 ⇒ 无行为变化）⇒ **不得**在重构中「顺手支持」非对齐（那是新功能，超出本 change scope，且会与 ③ 的 Cache 行边界语义纠缠）
  - ⇒ 若决定支持 ⇒ 须先出 ADR（架构后果）并单独立项

### Task 3: 抽取 LSU（`lsu_chmem.h`）

- [ ] **Step 1: 搬移** —— 从 `dmem_chmem.h` 抽出 funct3 译码 + 宽度掩码 + 符号/零扩展 + sub-word RMW 掩码生成 ⇒ `lsu_chmem.h`；`dmem_chmem.h` 改为调用
- [ ] **Step 2: 不改语义** —— 逐条核对搬移前后的表达式**逐字等价**（含 `bits<14,12>` 等位选写法）
- [ ] **Step 3: 双文件分离合规** —— ADR-083：`lsu_chmem.h` 在 `ip/cpu/arch/riscv/`（与 `lsu.h` 同域，CH_MEM 变体）· 不得在 TLM 文件里混 `ch_*` ⇒ `tools/check_plugin_portability.sh` 13/13 须绿
- [ ] **Step 4: 回归** —— vendored **47 ELF 全 PASS**（`[chmem] chmem_vendored_elf_tohost1`）⇒ **行为零变化**的强证据

### Task 4: 新增 DBus 仲裁层（`dbus_chmem.h`）

- [ ] **Step 1: 单主端口骨架** —— 请求/响应握手（valid/ready）+ byte-enable 通路；单主端口下仲裁退化为直通
- [ ] **Step 2: payload key 模态** —— 跨 stage 信号用 `uint_t<N>`；方向信号必须是**活节点**（禁 `static_cast<bool>`，ADR-040 §10.5）
- [ ] **Step 3: 顺序契约** —— ADR-040 §10.6：生产者 EARLY ⇒ 消费者 NORMAL，同 stage 名；**不得**依赖 plugin 注册顺序
- [ ] **Step 4: 阶段 A 单测** —— 握手时序（valid/ready 组合）+ byte-enable 掩码的期望字节序列

### Task 5: 阶段 B —— 接 Cache（需 ③）

- [ ] **Step 1: 接线** —— DBus 请求先查 ③ 的 Cache ⇒ 命中走 Cache，未命中走 DMem 并回填
- [ ] **Step 2: owner 确认** —— 本 Task 归 **④**（独占）；③ 只交付 Cache 本体 + 注册 ⇒ **无双 owner**
- [ ] **Step 3: 集成判据** —— 命中/未命中**两条分支各 ≥1 次**（可机检：命中计数 > 0）+ 命中时数据来自 Cache、未命中时来自 DMem 且**回填正确**

### Task 6: 收口

- [ ] **Step 1: 8 门禁** —— 全绿（`check_plugin_portability.sh` 是本 change 主要抓手）
- [ ] **Step 2: 全量回归** —— `run_chipforge_tests.sh --all` ⇒ **0 退化**（TLM 493 = 492 + 1 pre-existing；chmem 130）
- [ ] **Step 3: 台账更新（★v2：写死条数与口径）** —— 台账 `[cpu]` 族内 LSU/DBus 相关条目（`dbus_multiple_build_calls` 等）逐条重判：**「行为不变的重构」不改变覆盖状态** ⇒ 这批条目应维持原态（`partial`/`covered` 不变），仅在差异说明里补「CH_MEM 侧访存宽度语义由 `lsu_chmem.h`/`dbus_chmem.h` 承载（重构自 `dmem_chmem.h`）」
  - ⚠️ **同时同步台账 §2 族级判定行**（条目态从族级继承）
- [ ] **Step 4: commit**

---

## Acceptance

- [ ] AC-1: `ip/cpu/arch/riscv/lsu_chmem.h` 与 `ip/cpu/plugins/dbus_chmem.h` 存在；宽度译码/掩码/符号扩展**已从 `dmem_chmem.h` 抽出**（`dmem_chmem.h` 不再承担 funct3 译码）
- [ ] AC-2: ★**行为 byte-identical** —— 抽取后 vendored **47 ELF 全 PASS**（`[chmem] chmem_vendored_elf_tohost1`），且**宽度语义与抽取前逐字等价**（搬移清单逐条核对）
- [ ] AC-3: ★**非对齐访问语义不变** —— 保持「不支持非对齐（沿用现状）」，**不得**在重构中新增支持；语义若要变更 ⇒ 须先出 ADR 并单独立项（AC 的这条是**否定式**判据，防 scope 蔓延）
- [ ] AC-4: DBus 仲裁层存在（valid/ready + byte-enable）；单主端口下退化为直通
- [ ] AC-5: 跨 stage 信号全部 mode-aware（`uint_t<N>`）+ 方向信号为**活节点**（ADR-040 §10.5）
- [ ] AC-6: 阶段 B（接 Cache）**由本 change 独占**；③ 只交付 Cache 本体 + 注册 ⇒ **无双 owner**（两个 change 的文档互相登记）
- [ ] AC-7: 阶段 B 的命中/未命中两条分支各 ≥1 次（命中计数 > 0）+ 回填正确
- [ ] AC-8: 8 门禁全 PASS + 全量回归 0 退化
- [ ] AC-9: 台账 LSU/DBus 相关条目**维持原覆盖态**（重构不改覆盖）+ 差异说明补锚点 + §2 族级行同步

## 风险与未知

| # | 风险 | 缓解 |
|---|------|------|
| R1 | **抽取重构的回归面**：动 `dmem_chmem.h` ⇒ 47 ELF 回归面 | AC-2（47 ELF byte-identical）+ 「搬/留」清单（Task 1 Step 3）+ 逐字等价核对 |
| R2 | **「顺手支持非对齐」**（重构时最容易 scope 蔓延） | AC-3 是**否定式**判据 + Task 2 Step 3 先定基准 |
| R3 | **双 owner**（阶段 B 接 Cache vs ③ 的 Cache 生效） | ★v2 已明确分工：本体+注册归 ③，**接线归 ④**（本 change） |
| R4 | 阶段 A 单独交付后仍无多端口价值 ⇒ 「为何存在」 | 阶段 A 的价值 = **解耦**（译码不再寄生在存储插件里）+ 为阶段 B 铺路；阶段 A/B 可分属两个 change（若 ③ 延期） |
| R5 | 无符号扩展原语（CppHDL 无 `ch_int`） | 现有 `dmem_chmem.h` 已有可用实现 ⇒ **搬运即可**，不必新造 |
| R6 | ③ 延期 ⇒ 阶段 B 无限期挂起 | Task 2 已把 A/B 拆开；阶段 A 可独立交付并归档（**不得**用「阶段 B 未完成」阻塞阶段 A 的归档） |

## 迁移说明

落地为 `.rddf/plans/chmem-dbus-lsu.md`；**阶段 A 可 rdd-quick**（存量重构 + byte-identical 判据机械可验）；**阶段 B 需完整 rdd-builder**（依赖 ③ + 多主端口语义）。
