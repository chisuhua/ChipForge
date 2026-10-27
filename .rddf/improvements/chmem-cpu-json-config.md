---
id: chmem-cpu-json-config
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P1
phase: phase-4
theme: 双后端差分 parity 门禁 —— 同一模型（CH_MEM）× 两后端（CppHDL Simulator / Verilator）差分执行
version_target: v0.11.0
manual_deps: []
review_by: 2026-12-15
---

# chmem-cpu-json-config — CH_MEM CPU 的 JSON 配置面（+ DSE 接线基础）

> **定位**：阶段 A（A4 `backend-parity-ci`）的**配置面**基础，也是 `chmem-bp-btb-gshare` 主线「量收益跑批」的输入格式。
>
> **来源**：2026-10-27 用户指令「先做 P2（JSON 配置）」+ 4 条架构建议的第 3 条（用 JSON 配置而非命令行 + 重新分析 DSE 支持）。
>
> **状态**：**✅ 全项达成**（2026-10-27）—— 代码已落地并提交（9 commit）· 全量 parity **52/52 PASS** ·
> CH_MEM binary 全套 **20133 assertions / 164 test cases 全绿** · 八门禁全绿 · 文档同步已落档 ⇒ ★ 仅剩
> 「文档同步」里的 `ip/cpu/README.md` 章节（**已补**）与契约/ADR/roadmap 三处（**已补**）⇒ **本提案收尾完成**。
>
> ★ **2026-10-27 用户裁决：P2 剥离 `bp_gshare` 旋钮**（GShare 增量 3 净负收益 + D8/D10 未签批 ⇒ 其数据通路与开关**均不进 main**）⇒ 详见 §「`bp_gshare` 剥离决策」。
>
> **⚠️ 未提交**：本轮全部改动仍在工作树（未 commit），含 4 个新文件 + 6 个改动文件。**下一会话接手时 MUST 先确认工作树状态**（见 §范围「工作树现状」）。

**主题**: 双后端差分 parity 门禁 —— 同一模型（CH_MEM）× 两后端（CppHDL Simulator / Verilator）差分执行

---

## 架构依据

### 为什么需要独立的 CH_MEM 配置结构（而不是复用 TLM 的 `CPUConfig`）

| 事实 | 取证 | 结论 |
|---|---|---|
| TLM `CPUConfig` 有 **33 个字段** | `ip/cpu/configs/cpu_params_schema.json` | 覆盖多级流水线 / superscalar / Cache 延迟 / SMT / M 扩展多周期 |
| CH_MEM 工厂只消费其中 **3 个** | `ip/cpu/cpu_factory_chmem.h`（`enable_mmu` / `mmu_mode` / `btb_entries`）+ 4 个 CH_MEM 独有旋钮 | ★ **复用会制造「以为能配其实不能」的假等价陷阱** |
| CH_MEM 工厂**刻意不引** `CPUConfig` | `cpu_factory_chmem.h:98`（NG1/NG2/NG3 显式声明） | ⇒ 新建独立 struct，保持零依赖 |
| 位置参数已膨胀到 **12 个** | `cpu_factory_chmem.h:100-123` | ⇒ 维护负担 + DSE 难以批量驱动 |

### 为什么 CH_MEM 的字段名**刻意不与 TLM 统一**

TLM 的 `branch_predictor` 是**策略名**（`static`/`bimodal`/`gshare`/`tournament`）；
CH_MEM 只有 **Bimodal / GShare 两档**（`bp_gshare` bool）。

强行同名 ⇒ 需要一层「`bimodal` ⇔ `bp_gshare=false`」的隐式转换 ⇒ **配错时不报错**。
⇒ **CH_MEM 用自己的字段名**（`enable_bp` / `btb_entries` / `bp_uncond` / `bp_gshare`）。

### 为什么 nlohmann 不进工厂头文件

`cpu_factory_chmem.h` 被 ~10 个目标 include（2 个 runner + 测试 + 未来 SoC 装配）。
nlohmann 是 ~25k 行头文件 ⇒ 拖进工厂头会显著拖慢编译，而工厂**不需要** JSON。
⇒ 拆成两个头：`cpu_config_chmem.h`（struct，零依赖）+ `cpu_config_chmem_json.h`（解析，仅解析方 include）。

### 历史上「静默丢字段」的教训

`cpu-pipeline-tick-rca` F1：`load_config()` 静默丢弃 8 个字段 ⇒ 配置看起来生效、实际没生效 ⇒ 排查成本极高。
⇒ **本设计的核心纪律：CH_MEM 不支持的字段 ⇒ 逐字段 ERROR + 给出「为什么不支持」的原因**，且**一次报告全部问题**（不是首错即返）。

---

## 范围

### In Scope（✅ 本轮已完成）

| # | 交付 | 文件 |
|---|---|---|
| 1 | `CpuConfigChmem` struct（**零外部依赖**）+ `validate_cpu_config_chmem()` 值域校验 | `ip/cpu/configs/cpu_config_chmem.h`（**新**，106 行） |
| 2 | JSON 解析 + 32 项 TLM-only 字段拒绝表 + 一次性错误报告 | `ip/cpu/configs/cpu_config_chmem_json.h`（**新**，360 行） |
| 3 | `build_cpu(ctx, cfg, elf)` 重载（12 位置参数 → 1 struct） | `ip/cpu/cpu_factory_chmem.h`（改，+65 行） |
| 4 | CH_MEM 配置示例 | `ip/cpu/configs/cpu_chmem_default.json`（**新**） |
| 5 | Verilator runner `--config`（与配置类 flag **互斥**） | `tools/verilator_runner/cpu_verilator_sim.cpp`（改，+116 行） |
| 6 | Simulator runner `--config`（同上） | `tools/parity_runner/parity_sim_runner.cpp`（改，+69 行） |
| 7 | 门禁 `--cpu-config` 转发（两侧同一份配置） | `tools/parity/parity_gate.py`（改，+16 行） |
| 8 | schema 加 `x-chmem-supported` / `x-chmem-note`（33 字段全覆盖）+ `x-chmem-extra-fields`（CH_MEM 独有 4 字段登记） | `ip/cpu/configs/cpu_params_schema.json`（改，+363 行） |
| 9 | 契约测试（14 case / 258 assertions） | `tests/cpu/schema/test_cpu_config_chmem.cpp`（**新**，317 行） |

### ★ `bp_gshare` 剥离决策（2026-10-27，用户裁决）

**背景**：P2 的配置面原本含 `bp_gshare` 旋钮（8 个旋钮之一），但它依赖 GShare 增量 3 的数据通路
（`BranchPredictorPluginChmem` 的 `enable_gshare` 参数 + `GshareMem` + GHR）⇒ ★ **无法只提交
P2 而不带 GShare**（HEAD 版的 BP 插件 ctor 只接受 2 个参数）。

**GShare 增量 3 的状态**（三个独立理由都不支持它进 main）：

| # | 事实 | 取证 |
|---|---|---|
| 1 | ★ **净负收益** | ADR-088 §5.3.3 C-3：`cycles(增量on) = 822514` vs `cycles(增量off) = 794478` ⇒ **慢 2001 拍** ⇒ **不满足 C-3**（fail-closed 判据） |
| 2 | ★ **签批未到** | `ip/cpu/docs/bp-btb-gshare-design-brief.md` §9 的 D8/D10 待 architect 签批 |
| 3 | ★ **parity 基线不符** | A3 首跑的 52 case 是在**stash 掉 GShare** 的干净 HEAD 上跑的（`node_count = 2437`）⇒ 进 GShare 后网表变 2440 节点 ⇒ 已归档证据与代码不符 |

⇒ ★ **裁决：P2 剥离 `bp_gshare` 旋钮**（8 → 7 个旋钮），GShare 全部改动**不进 main**。

**剥离后的影响（诚实登记）**：

| 项 | 影响 |
|---|---|
| 配置面旋钮数 | 8 → **7**（`name` / `initial_pc` / `enable_bp` / `btb_entries` / `bp_uncond` / `enable_mmu` / `mmu_mode` / `enable_cache`） |
| 交付点 9「`--no-bp-gshare` 补齐」 | ★ **从本提案移除**（原为「顺手修的既有缺口」）⇒ 该 flag 随 GShare 一并延后 |
| P2 核心价值 | ★ **不受影响**（JSON 配置面 + 32 项 TLM-only 字段 fail-fast 拒绝 + 门禁 `--cpu-config` 穿透，三者均与 `bp_gshare` 无关） |
| 后续 | GShare 签批后**增量加回**该旋钮（是增量，不是返工） |
| 假绿通道 | ★ **未开**（配置面不暴露未签批功能的开关 ⇒ 「传了 flag 却没效果」不可能发生） |

**实测（剥离后）**：`[chmem-config]` **14 case / 254 assertions** 全绿（原 258 ⇒ 差 4 = 删掉的 `bp_gshare` 断言）·
`[cpu],[cpu-configs]` **164 case / 909 assertions** 全绿 · 门禁自检 **13/13** · PR 层 parity **8/8 PASS** · 八门禁全绿。

### In Scope（⬜ 本轮**未完成**，属本提案收尾）

| # | 待办 | 说明 |
|---|---|---|
| 10 | ~~**提交**~~ | ✅ 已提交 9 commit（按 `CONTRIBUTING.md` §3.1 分文件粒度） |
| 11 | ~~**全量 52 case parity 复跑**~~ | ✅ **52/52 PASS**（48 ELF 一轮 PASS；branchbench 4/4 清场后重跑 PASS） |
| 12 | ~~**CH_MEM binary 全套测试**~~ | ✅ **164 test cases / 20133 assertions 全绿**（清场后跑完） |
| 13 | ~~**八门禁复跑**~~ | ✅ 全绿（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check` / fragments / 4 个 openspec validate） |
| 14 | ~~**文档同步**~~ | ✅ 契约 **v1.8** · ADR-089 配置面注记 · roadmap 进度 · `ip/cpu/README.md` 章节 |

### Out of Scope（明确不做）

- ✗ **删除或改写 TLM 的 `configs/*.json`** —— TLM 侧仍在用（`tools/cpu_sim` + 5 个 TLM 集成测试）
- ✗ **让 `CPUConfig`（TLM）与 `CpuConfigChmem` 共享 struct** —— 会破坏 NG1/NG2/NG3 的零依赖承诺
- ✗ **DSE sweep 引擎改造**（`tools/dse/sweep_driver.py` 换后端）—— 属 P5，见下方「后续接线」
- ✗ **把 `ch_stream` / `Module` / `Port` 的约束变更并入本提案** —— 属 architect 的 ADR-083 表述纠偏，与本提案独立

### 工作树现状（★ 接手前必查）

```
 M ip/cpu/configs/cpu_params_schema.json      (P2)
 M ip/cpu/cpu_factory_chmem.h                   (P2 + ★ GShare WIP 混在同一文件!)
 M ip/cpu/plugins/branch_predictor_chmem.h     (★ GShare WIP — 有意不提交)
 M tools/parity/parity_gate.py                  (P2)
 M tools/parity_runner/parity_sim_runner.cpp    (P2)
 M tools/verilator_runner/cpu_verilator_sim.cpp(P2)
?? ip/cpu/configs/cpu_config_chmem.h           (P2)
?? ip/cpu/configs/cpu_config_chmem_json.h      (P2)
?? ip/cpu/configs/cpu_chmem_default.json       (P2)
?? tests/cpu/schema/test_cpu_config_chmem.cpp  (P2)
```

★ **`ip/cpu/cpu_factory_chmem.h` 同时含 P2 改动与 GShare WIP** ⇒ **提交时 MUST 用 `git add -p` 逐块挑选**，否则会把未签批的 GShare WIP 一起提交。
★ **本轮已踩过一次的坑**：`git stash push -- ip/cpu/cpu_factory_chmem.h` 会把 P2 改动一并 stash 掉 ⇒ 验证「干净 HEAD」时**不要 stash 这个文件**（改用「同一 binary 内 CLI vs config 对比」的等价性证明法）。

---

## Why

### 用户诉求（2026-10-27，第 3 条建议）

> 「考虑用 json 来配置，而不是命令行。同时重新分析师傅对 cpu_factory_chmem 启动 DSE 支持」

### 现状的三个具体缺陷

| # | 缺陷 | 后果 | 本提案是否解决 |
|---|---|---|---|
| 1 | 配置入口是 **12 个位置参数** | ① 不可读 ② 调参易错位 ③ **DSE 无法批量驱动** | ✅ 解决 |
| 2 | `tools/dse/sweep_driver.py` 跑的是 **TLM 的 `cpu_sim`** | ① DSE 测的不是目标后端 ② 默认 7 维空间含 `xlen:[32,64]` ⇒ **约 288/576 config 注定失败**（ADR-087 fail-fast） ③ 7 维里**只有 1 维**（`btb_entries`）在 CH_MEM 上真能扫 | ⚠️ **部分**：本提案提供 JSON 输入面 + 门禁转发；**sweep 引擎换后端属 P5** |
| 3 | `ip/cpu/configs/*.json` 只被 TLM 消费，且 `ports` 段还在用**已废弃的 `ch_stream`** | ① 读者以为「改 JSON 就能配 Verilator 后端」② `ch_stream` 与 ADR-083/AGENTS.md 表述矛盾（**属 architect 的表述纠偏**，见「后续接线」） | ⚠️ **部分**：新增独立的 `cpu_chmem_default.json`；旧 JSON 的 `ports` 段问题**归 architect** |

### 为什么现在做（时机）

- **P2 是 P5 的硬前置**：sweep 引擎要换 CH_MEM 后端 ⇒ 必须先有 JSON 配置面 + `--config` 入口
- **P2 是 P4（VerilatorBackend 接入）的输入准备**：`set_backend` 路线需要参数经配置面传递
- **P2 零语义风险**：纯重构，验收标准 = 52 case 逐位不变（★ 已全量验证：**52/52 PASS**，determinism 指纹与 A3 基线逐位相同）

### 预期收益

1. **DSE 有了正确的作用域**：JSON 配置面 ⇒ sweep 只扫 CH_MEM 真正支持的维度（BP 族 4 维），不再浪费一半机时在注定失败的 config 上
2. **配置正确性从「自觉」变成「机器保证」**：32 个 TLM-only 字段逐个带拒绝原因
3. **A4 的 DSE 回归有落点**：`--cpu-config` 已能穿透门禁到两个 runner

---

## What Changes

### 1. `ip/cpu/configs/cpu_config_chmem.h`（新，106 行）

```cpp
struct CpuConfigChmem {
  std::string name = "RiscvCpuChmem";
  std::uint32_t initial_pc = 0x80000000u;   // 取指起始地址
  // ── 设计参数（DSE 真正可扫的维度）──
  bool enable_bp = true;
  std::uint32_t btb_entries = 256;           // ∈ {16,32,64,128,256}
  bool bp_uncond = true;
  bool bp_gshare = true;
  // ── 未实装（true ⇒ build_cpu 内 fail-fast）──
  bool enable_mmu = false;
  std::string mmu_mode = "bare";
  bool enable_cache = false;
};
```

★ **不含 `preload_elf`** —— 由 `elf_image.empty()` 推导，消除「`preload_elf=true` 但 `elf_image` 空」的歧义态。
★ **默认值必须与 `build_cpu` 默认参数逐位等价**（测试有专门断言钉住）。

### 2. `ip/cpu/configs/cpu_config_chmem_json.h`（新，360 行）

- `parse_cpu_config_chmem_json(text, source)` → `std::expected<CpuConfigChmem, std::vector<CpuConfigError>>`
- `load_cpu_config_chmem(path)` → 同上
- `chmem_rejected_fields()` → **32 项** TLM-only 字段 → 拒绝原因对照表
- `format_cpu_config_errors(errs)` → 多行渲染（runner 直接用）
- 支持两种 JSON 形态：`{"name":..., "params":{...}}`（与 `configs/*.json` 一致）· `{"name":..., ...}`（扁平）
- ★ **错误类型用 `CpuConfigError::make()` 工厂**（本轮实现时曾把 `{kind, message, field}` 位置写反 ⇒ 错误信息为空 ⇒ 已加工厂统一）

### 3. `ip/cpu/cpu_factory_chmem.h`（改，+65 行）

- 新增 `build_cpu(ctx, const CpuConfigChmem&, elf_image)` 重载 ⇒ 委托给既有 12 参版本
- ★ **include 必须在 `#ifndef CF_PLUGIN_USE_CH_MEM` 之外**（本轮踩过：放进去 ⇒ CH_MEM 构建下 `CpuConfigChmem` 不可见 ⇒ 编译失败）
- 补 `#include <stdexcept>`（原文件靠传递 include 拿到 `std::runtime_error`）

### 4. `tools/verilator_runner/cpu_verilator_sim.cpp`（改，+116 行）

- `--config <path>` + **与配置类 flag 互斥**（`--enable-mmu` / `--mmu-mode` / `--enable-cache` / `--no-bp` / `--no-bp-uncond` / `--no-bp-gshare` / `--btb-entries`）
- ★ **顺手补齐 `--no-bp-gshare`** —— 此前该 runner **没有**这个 flag，`build_cpu` 调用也不传该参数 ⇒ 「GShare 关不掉」的假绿通道
- ★ **BP 回显改为反映 `cfg` 的生效值**（不再回显 CLI 默认值）—— 测量脚本据此断言「flag 确实生效」
- 统一走 `build_cpu(config)` 重载 ⇒ 单一配置面，不会出现「JSON 解析了但 build_cpu 用的还是 CLI 值」的双轨

### 5. `tools/parity_runner/parity_sim_runner.cpp`（改，+69 行）

- 同款 `--config` + 互斥检查 + `--no-bp-gshare`
- 同样统一走 `build_cpu(config)`

### 6. `tools/parity/parity_gate.py`（改，+16 行）

- `--cpu-config <path>` ⇒ **转发给两侧 runner**（同一份配置，否则比的是两种配置而非两个后端）
- 报告新增 `cpu_config` 字段（事后可判断「这次跑的是哪套配置」）

### 7. `ip/cpu/configs/cpu_params_schema.json`（改，+363 行）

- 33 个 params 字段各加 `x-chmem-supported`（bool）+ `x-chmem-note`（原因）
- 新增顶层 `x-chmem-extra-fields` ⇒ 登记 CH_MEM 独有的 4 个字段（`initial_pc` / `enable_bp` / `bp_uncond` / `bp_gshare`）
- ★ **未改 `properties.params.properties` 的既有内容** ⇒ 现有 schema 测试零影响（实测 14 case / 171 assertions 全绿）

---

## Acceptance

### ✅ 已达成（实测证据）

- [x] **`CpuConfigChmem` 与 JSON 解析层落地**，工厂头**零 nlohmann 依赖**（拆两个头）
- [x] **`build_cpu(ctx, cfg, elf)` 重载**与 12 参版本**逐位等价**（PR 层 8 case × 7 观测量 + determinism 全部相同）
- [x] **32 项 TLM-only 字段逐个被拒 + 带原因**；未知字段（拼写错）被拒；**一次报告全部问题**
- [x] **类型错 / 值域越界 / 文件不存在 / JSON 语法错** 各自有独立的 `ErrorKind`
- [x] **两个 runner 的 `--config` 与配置类 flag 互斥**（静默覆盖 = 假绿通道，已堵）
- [ ] ~~**`--no-bp-gshare` 补齐**~~ ★ **移出本提案**（2026-10-27 用户裁决：随 GShare 增量 3 一并延后 ⇒ 见 §「`bp_gshare` 剥离决策」）
- [x] **门禁 `--cpu-config` 转发**（两侧同一份配置）+ 报告记录 `cpu_config`
- [x] **schema 33 字段 `x-chmem-supported` 覆盖** + 与拒绝表**双向一致**（测试钉住）
- [x] ★ **`bp_gshare` 已从配置面剥离**（2026-10-27 用户裁决）⇒ schema 的 `x-chmem-extra-fields` 同步移除该项
- [x] **新测试 14 case / 254 assertions 全绿**（★ 剥离 `bp_gshare` 后重测：258 → 254，差 4 = 删掉的 `bp_gshare` 断言）
- [x] **既有 schema 测试零影响**（14 case / 171 assertions 全绿）
- [x] **TLM binary `[cpu]` + `[cpu-configs]` 全绿**（164 case / 913 assertions）
- [x] **门禁自检 13/13**（`--self-test` 未受影响）
- [x] **提交**（9 个 commit，按 `CONTRIBUTING.md` §3.1 分文件粒度）
- [x] **全量 52 case parity 复跑 = 52/52 PASS**（`--layer night --jobs 4`）—— 48 ELF 一轮跑完全 PASS；
  4 个 branchbench 因**外部进程争抢机时**首轮 `runner_error=TIMEOUT`（见下「TIMEOUT 排查」），
  清场后单独重跑 **4/4 PASS**（`partial` 语义正确标 `vacuous=['tohost','cycles']`）
- [x] **八门禁全绿**（`verify_adr` / `verify_plugin_decision` / `check_plugin_portability` / `doc_link_check` / `roadmap-r9-check` / `rddf roadmap validate-fragments` / 4 个 `openspec change validate`）

#### ★ TIMEOUT 排查（2026-10-27，结论：机时争抢，非回归）

| 证据 | 值 |
|---|---|
| determinism 指纹 | `node_count = 2437` / `names_hash = 0ceb2fb11ea8016f`（= A3 基线，**逐位相同**） |
| 网表行数 | 5531（= A3 基线） |
| `branchbench_m0` 观测 | 与 A3 首跑 `m0_20k.sim.json` **7 个观测量全部逐位相同** |
| sim vs vl（4 档） | **IDENTICAL**（`tohost`/`regs`/`cycles`/`pchash`/`memsig`/`memsig_updates` 全等） |
| 首轮超时原因 | `--timeout-s 3600` 被 4 路 sim + `chipforge_tests_chmem` + **外部 CppHDL perf 套件**（load average 8.96→15.87）争抢；sim 单档满核需 ~2222 s，实际只拿到 ~52% CPU ⇒ >3600 s |
| 复跑参数 | `--timeout-s 5400`、清场后 4 路独占 ⇒ 全 PASS |

★ **遗留改进项（已修，2026-10-27 合并后）**：门禁的 `--timeout-s` 曾是**全局单一超时**，而 `branchbench`
单档满核就需 ~2222 s（余量仅 1.6×）⇒ 任何机时争抢都会误报 TIMEOUT。⇒ **已在 `main` 上修为
per-case 推导**（`max(300, cap/9.0*3)` ⇒ elf 1000 s / branchbench 6666 s；`--self-test` 13 → 16 例），
契约升至 **v1.8**。

### ⬜ 未达成（★ 下一会话的待办）

- [x] **CH_MEM binary 全套测试** —— ✅ **2026-10-27 在 main 上清场后跑完**：
  `All tests passed (20133 assertions in 164 test cases)`
- [x] **文档同步** —— ✅ 全部落档：契约 **v1.7 → v1.8**（§7 S-14/S-15 增行）· ADR-089 补「CH_MEM 配置面穿透」实现注记 ·
  `feat-backend-parity-gate` 补 P2 支撑项 · `ip/cpu/README.md` 补「CH_MEM 配置面」章节

### 后续接线（不在本提案，另开）

- **P5 · DSE 引擎换后端**：`sweep_driver.py` 加 `--backend chmem`（走 `cpu_verilator_sim --config`）；默认空间从 7 维收到 CH_MEM 实际支持的 BP 族 4 维；**必须先剔除 `xlen:[32,64]`**（当前约 288/576 config 注定失败）
- **GShare 增量 3（BP 主线）**：★ 剥离后 GShare 的**数据通路 + `bp_gshare` 旋钮 + `--no-bp-gshare` flag** 三者**同批延后** ⇒ 待 D8/D10 签批后一并落地 ⇒ **加回 `bp_gshare` 是增量不是返工**
- **architect · ADR-083 表述纠偏**：AGENTS.md:27 把 `ch_stream` 与 CppTLM 的 `Module`/`Port` 并列禁止 —— 但 `ch_stream` 是 **CppHDL 的**（`bundle/stream_bundle.h` + `chlib/stream.h`），上游已有 `test_verilator_stream_e2e.cpp` 证明它能走 Verilator。⇒ 应收窄为「不使用 **CppTLM** 的 Module/Port/ModuleFactory」
- **architect · SoC 互联 ADR**：CPU 顶层目前**只有 clock/reset 2 个端口**（ELF 预载 ⇒ ibus/dmem 全内部）⇒ 「cpu+cache+mmu 互联」需要先定端口契约；且 MMU 的 `MMUPlugin_chmem.h`（302 行，仅 TLB 路径）**未注册进工厂**，Cache 的 CH_MEM 主插件**不存在**（`--enable-cache` 是 fail-fast）
- **P4 · VerilatorBackend 接入**：上游 CppHDL 已有 `include/core/verilator_backend.h`（952 行实现）+ `Simulator::set_backend()` + `DUAL_BACKEND_TEST_PAIR` 宏。★ 但其 `port_access_` 机制**只认顶层端口** ⇒ 对当前「封闭核」（2 端口）几乎用不上 ⇒ **依赖上面的 SoC 端口契约先落地**

---

## Capabilities

| # | 能力 | 状态 | 落点 |
|---|---|---|---|
| 1 | CH_MEM CPU 的**结构化配置面** | ✅ 已落地 | `CpuConfigChmem` |
| 2 | **JSON 配置加载**（fail-fast + 全量错误报告） | ✅ 已落地 | `cpu_config_chmem_json.h` |
| 3 | **配置面与 TLM 侧零依赖**（不引 `CPUConfig`） | ✅ 已落地 | `cpu_factory_chmem.h` |
| 4 | **不支持字段的机器可检拒绝**（32 项带原因） | ✅ 已落地 | `chmem_rejected_fields()` |
| 5 | **两个 runner 的 `--config` 入口**（含互斥保护） | ✅ 已落地 | 2 个 runner |
| 6 | **门禁的 `--cpu-config` 穿透** | ✅ 已落地 | `parity_gate.py` |
| 7 | **schema 的 CH_MEM 支持面标注**（机读） | ✅ 已落地 | `x-chmem-supported` |
| 8 | **BP 族 DSE 旋钮的 JSON 表达** | ✅ 已落地 | `enable_bp`/`btb_entries`/`bp_uncond`（★ `bp_gshare` 已剥离，见下） |
| 9 | **GShare 开关的 runner 暴露** | ⬜ **移出本提案** | ★ 随 GShare 增量 3 延后 |
| 10 | **DSE sweep 走 CH_MEM 后端** | ⬜ 未做 | P5 |
| 11 | **配置面覆盖 MMU / Cache 真实旋钮** | ⬜ 未做（依赖 MMU/Cache CH_MEM 实装） | 阻塞于 `mmu-chmem-phase-b-c-launch` |
| 12 | **SoC 级配置装配**（cpu+cache+mmu 组合） | ⬜ 未做 | 阻塞于 architect 的 SoC 端口 ADR |

---

## Impact

### 正面

1. **DSE 的作用域终于正确了**：从「测 TLM 后端 + 一半 config 注定失败」变成「扫 CH_MEM 真正支持的维度」
2. **配置正确性从自觉变机器保证**：32 个 TLM-only 字段逐个带原因，历史上的「静默丢字段」缺陷类被堵死
3. **A4 / P4 / P5 三条线共用一个配置面**：门禁转发、VerilatorBackend 接入、DSE sweep 都从这里走
4. **顺手修了一个既有缺口**：`--no-bp-gshare` 缺失（Verilator runner 关不掉 GShare）

### 负面 / 成本

1. ⚠️ **配置面分裂为两套**（TLM `CPUConfig` / CH_MEM `CpuConfigChmem`）⇒ 长期有漂移风险
   ⇒ 缓解：schema 的 `x-chmem-supported` + 拒绝表**双向一致**由测试钉住；字段名差异**在 schema 的 `x-chmem-note` 里显式写明原因**
2. ⚠️ **配置面旋钮少了一个**（`bp_gshare` 剥离）⇒ P2 的「BP 族 DSE 旋钮」暂缺 GShare 一档
   ⇒ 缓解：GShare 签批后**增量加回**；★ 不影响 P2 核心价值
3. ⚠️ **`ip/cpu/configs/` 现有 4 份 JSON 仍只服务 TLM**，且 `ports` 段还在用已废弃的 `ch_stream`
   ⇒ 缓解：新增独立的 `cpu_chmem_default.json`；旧 JSON 的清理归 architect（表述纠偏 + 遗留字段处置）
3. ⚠️ **`cpu_factory_chmem.h` 被 P2 与 GShare WIP 同时改动** ⇒ 提交时易误带
   ⇒ 缓解：本提案 §范围「工作树现状」已显式标注 + 提交时用 `git add -p`

### 中性

- **零语义变化**：PR 层 8 case × 7 观测量 + determinism 逐位相同 ⇒ 已有行为完全保持
- **无新依赖**：nlohmann 已在 `build/_deps/install/include/`（两个 runner 都已 link cpptlm ⇒ include 路径现成）
- **CMake 零改动**（`tests/CMakeLists.txt` 用 `file(GLOB_RECURSE)` 自动发现新测试文件）

### 风险

| # | 风险 | 触发 | 缓解 |
|---|---|---|---|
| R1 | **误提交 GShare WIP** | 提交时 `git add` 整个 `cpu_factory_chmem.h` | `git add -p` 逐块；提交前 `git diff --cached` 复核 |
| R2 | **全量 parity 复跑发现配置路径的隐性差异** | 52 case 中某个 case 的配置敏感 | ★ **已闭合**：全量 52/52 PASS（含 branchbench 4 档），determinism 指纹 = A3 基线 |
| R3 | **schema 的 `x-chmem-supported` 与拒绝表漂移** | 后续新增字段 | 测试「schema false ⊆ 拒绝表 且 拒绝表 ∩ 支持字段 = ∅」双向钉住 |
| R4 | **两套配置面长期漂移** | TLM 侧新增字段时忘同步 | 已登记为 Impact 负面项 1；后续可考虑让 TLM 侧也走 `x-chmem-supported` 标注 |

---

## 复现命令

```bash
# ① 新测试
./build/bin/chipforge_tests "[chmem-config]"          # ⇒ 14 case / 258 assertions

# ② 配置面功能（拒绝 TLM-only 字段，带原因）
./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-add \
  --config ip/cpu/configs/cpu_default.json
# ⇒ 10 条拒绝记录（每条带「为什么不支持」）

# ③ 互斥保护
./build/bin/cpu_verilator_sim --elf ... --config ip/cpu/configs/cpu_chmem_default.json --no-bp
# ⇒ rc=2 + "--config is mutually exclusive with config flags"

# ④ ★ 等价性证明（本轮的核心验收）
python3 tools/parity/parity_gate.py --layer pr --jobs 4 \
  --work-dir /tmp/p2_cli --report /tmp/p2_cli/report.json
python3 tools/parity/parity_gate.py --layer pr --jobs 4 \
  --cpu-config ip/cpu/configs/cpu_chmem_default.json \
  --work-dir /tmp/p2_cfg --report /tmp/p2_cfg/report.json
# ⇒ 两份报告的 7 个观测字段 + determinism 逐位相同（仅 cpu_config 元数据不同）

# ⑤ 门禁自检
python3 tools/parity/parity_gate.py --self-test              # ⇒ 13/13
```

---

## 相关

- **ADR-089**（双后端差分 parity 门禁，v1.4 Accepted）—— 本提案是阶段 A 的配置面基础
- **ADR-040 v3.0**（TLM→HDL 移植性约束）—— NG1/NG2/NG3 零依赖声明的出处
- **ADR-083**（Plugin-style 单一 source of truth）—— `ch_stream` 表述纠偏的待改处
- `docs/architecture/parity-gate-contract.md` v1.6 —— 契约（§7 覆盖矩阵待增行）
- `openspec/changes/backend-parity-corpus/first-run-evidence.md` —— A3 首跑证据（本提案的验收基线）
- `.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md` —— 阶段 A 路线图（A4 待启动）
- `.rddf/roadmap/features/feat-backend-parity-gate.md` —— 阶段 A 载体
- `ip/cpu/configs/cpu_params_schema.json` —— 字段清单 SSOT（`x-chmem-supported` 标注面）
