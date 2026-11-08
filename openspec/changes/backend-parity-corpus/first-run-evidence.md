# A3 首跑证据 —— 既存后端分歧清单（2026-10-27）

> 本文件是 **G-A3 的判定物**（`objective-backend-parity-and-single-model.md` §G-A3：
> 「两侧交集 48 + branchbench 4 档差分跑通 + 既存分歧清单归零或全部显式登记」）。
>
> ★ **结论：既存后端分歧清单 = 空**（52/52 PASS，零分歧，真实例外数 0）。

---

## 1. 跑批参数

| 项 | 值 |
|---|---|
| 命令 | `python3 tools/parity/parity_gate.py --layer night --jobs 4 --report build/parity/report-night.json` |
| 语料 | `tests/cpu/parity/corpus.txt` ⇒ **52 case = 48 ELF + 4 branchbench** |
| commit | `95be8d122154d83d6d70f2fd14a49dc2b0eaf7fd` |
| 树状态 | ★ **干净 HEAD**（GShare 增量 3 的 WIP 已 `git stash` ⇒ build 无未提交代码） |
| determinism | `node_count = 2437` · `names_hash = 0ceb2fb11ea8016f`（★ **52 case 全部相同**，两侧也相同） |
| 耗时 | ≈ **50 min**（4 核；Simulator 侧 9~10 拍/s 是瓶颈） |
| verdict | **PASS** · `error_kind = None` · `warnings = []` |

## 2. 结果汇总

| 项 | 数 |
|---|---|
| case 总数 | **52** |
| PASS | **52** |
| FAIL（后端分歧） | **0** |
| ERROR（门禁/环境故障） | **0** |
| partial（cap 截断） | **4**（branchbench 4 档） |
| 白名单生效条目 | **0**（真实例外数保持 0） |

### 2.1 按观测量的覆盖

| 观测量 | 48 ELF | 4 branchbench | 判定强度 |
|---|---|---|---|
| `tohost` | ✅ 全部 = 1 | ★ **vacuous**（未跑到 done ⇒ 两侧恒 0） | 弱（branchbench 上无判别力） |
| `regs`（x1..x31 原始值） | ✅ 逐位相等 | ✅ 逐位相等 | 强 |
| `memsig` + `memsig_updates` | ✅ 逐位相等 | ✅ 逐位相等（9 次写） | 强 |
| `cycles` | ✅ 相等 | ★ **vacuous**（两侧 = cap 20000） | 中（branchbench 上无判别力） |
| ★ `pchash`（逐拍 PC 流 FNV-1a） | ✅ 逐位相等 | ✅ **逐位相等（20000 拍 PC 流）** | ★ **最强** |

⇒ ★ **分支密集负载（branchbench）的前 20000 拍，两后端的逐拍 PC 流 hash 完全相同** ——
这是本次首跑最有价值的单条证据（分支/flush 路径最容易出现后端分歧）。

## 3. ★ 首跑暴露的 4 个门禁自身缺陷（全部已修）

| # | 缺陷 | 后果（若未修） | 修复 |
|---|---|---|---|
| 1 | ★ **partial 假绿**：`judge()` 不知道 case 是否 `done` | branchbench 两侧 `tohost` 恒 0 ⇒ 「相等」被当**通过证据** ⇒ **假绿** | 新增契约 **S-9a**：`tohost`/`cycles` 标 **vacuous**，承重判据 = `regs`/`memsig`/`pchash` |
| 2 | **Verilator 产物串扰**：`--work-dir`/`--verilog` 全局共享 | 并行跑批时后一个 case **覆盖**前一个的 `top.v` + `obj_dir/` ⇒ **静默错测** | per-case 隔离 + **绝对路径**（内部 `cd <work-dir>` 使相对路径失效） |
| 3 | **per-case cap 缺失** | ELF 需 ~1k 拍 vs branchbench 需 ~86k 拍 ⇒ 单一全局 cap 必二选一失败 | 清单第 5 列 `max_cycles` + 按 `kind` 默认（elf=3000 / branchbench=20000） |
| 4 | ★ **分层语义反了**：`--layer night` 只选 `layer=night` | 夜间层选集 **44 < PR 层 8**（漏 PR 层 8 条）⇒ 首跑报告只有 44 而非契约要求的 52 | `night` = **全量**（52）· `pr` = 快子集（8）|

## 4. ★ 两项**未覆盖**（诚实标注 —— MUST NOT 假装已覆盖）

| # | 未覆盖 | 原因 | 影响 | 归属 |
|---|---|---|---|---|
| **U-1** | branchbench 4 档**只跑到 20k 拍**（未跑到 done） | 完整需 ~86k 拍 ⇒ Simulator 侧 9~10 拍/s ⇒ **61 h/档** | 该 4 档的 `tohost`（完整校验和）**未验证** | 开放项 **O-5**（ELF 移出 elaboration 期 ⇒ 才能跑完） |
| **U-2** | **BP 相关路径未覆盖** | GShare（增量 3）WIP 已 stash（净负收益 + D8/D10 未签批）⇒ 本次跑的是 **Bimodal + uncond BTB** 状态 | 增量 3 落地后 MUST **重跑** | BP 增量 3 定稿后 |

## 5. ★ 一条需要留痕的历史事实

`determinism.node_count` 从 **2511**（A2a 时期的实测）变为 **2437**（本首跑）：

- 差额 **74 节点** = GShare WIP 新增的结构（`ch_mem<ch_uint<2>, 16>` 表 + `ch_reg<ch_uint<8>>` GHR
  + 2 个 read_port + 1 个 write_port）
- ⇒ ★ **A2a 时期的 5/5 corpus parity 实测，是在含未提交 GShare WIP 的 build 上跑的**
- ⇒ 该结论**仍然有效**（两侧用同一 build ⇒ 差分语义不变），但 **A3 用干净 HEAD 重跑 52 case 全 PASS
  覆盖了它** ⇒ 以 A3 的结果为准。

## 6. 复现

```bash
cmake --build build -j4                # branchbench 4 个 ELF 是构建产物（target branchbench_elfs）
python3 tools/parity/parity_gate.py --self-test          # 13/13
python3 tools/parity/parity_gate.py --layer pr  --jobs 4  # ≈6.5 min，8 case
python3 tools/parity/parity_gate.py --layer night --jobs 4 --report build/parity/report-night.json  # ≈50 min，52 case
python3 tools/parity/parity_report.py --report build/parity/report-night.json   # 分歧清单
```

★ **重放**（门禁代码改动后不必重跑 50 min；★ 但重放不是「跑过」）：

```bash
python3 tools/parity/parity_replay.py --work-dir build/parity --out build/parity/report-replay.json
```

---

## 5. ★ 当前 HEAD 复跑证据（2026-11-08 · G-A3 签认的锚）

> ★ **为什么需要这一节**：★ §1~§4 的 A3 首跑绑定 commit **`95be8d1`**；
> ★ 但首跑之后**门禁改了 6 次**（`556af67` 分层语义 / `3a97443` 超时 / `75cce61` `--cpu-config`
> / `d002473` timings / `9b36f5c` read_back / `c10041a` 语料校验）⇒ ★ **用旧跑批签 G-A3 不成立**
> ⇒ ★ 本节是**当前 HEAD** 的复跑证据。

### 5.1 复跑命令与结果

```bash
python3 tools/parity/parity_gate.py --layer night --jobs 4 --report build/parity/report-night-head.json
```

| 字段 | 值 |
|---|---|
| `verdict` | **PASS** |
| `error_kind` | `null` |
| `layer` | `night` |
| `commit` | **`8c94f5a2b7ee`**（★ 当前 HEAD） |
| `per_case` | **52 / 52 PASS** |
| ★ `partial_cases` | **4** —— `branchbench_m0/m1/m2/m3`（★ cap 20000 截断 ⇒ `tohost` / `cycles` **vacuous**） |
| ★ `exceptions_applied` | **`[]`** ⇒ ★ **真实例外 0 条** ⇒ ★ **既存后端分歧清单 = 空** |
| `warnings` | `[]` |
| `determinism` | `node_count = 2437` · `names_hash = 0ceb2fb11ea8016f` |
| `timings` | `count = 52` · `p50 = 66.99` · `p95 = 1764.745` · `total = 10295.644` |
| `reset_mode` | `sim:ctor-init / vl:reset-pulse-1`（★ C-2 裁决落地） |

### 5.2 ★ 关键结论：**门禁语义零漂移**

| 项 | A3 首跑（`95be8d1`） | 当前 HEAD（`8c94f5a`） | 结论 |
|---|---|---|---|
| `determinism.node_count` | 2437 | **2437** | ★ 逐位相同 |
| `determinism.names_hash` | `0ceb2fb11ea8016f` | **`0ceb2fb11ea8016f`** | ★ 逐位相同 |
| 结论 | — | — | ★ ★ 中间 6 次门禁改动（分层语义 / 超时 / 配置面 / 时长采集 / 回读 / 语料校验）**未改变判定语义** |

### 5.3 ★ 判别力证据（★ G-A3 的 fail-closed 与变异自检两条）

| 判据 | 实测 | 证据落点 |
|---|---|---|
| **fail-closed ≥5 例** | **6/6** | `openspec/changes/backend-parity-harness/tasks.md` §2b |
| **变异自检 ≥2 例** | ≥2 | 契约 §7 **S-6**（`memsig` 数据取反 `a55f52c5`→`8fcc53ba`）· **S-9a**（实跑 `branchbench_m0` 20k）· **S-10**（负向 N1~N5 均 `rc=2`） |
| 判定层鉴别力 | `--self-test` **16/16** | `python3 tools/parity/parity_gate.py --self-test` |
| ★ 白名单校验 | exit 0（★ 真实例外 **0** 条 + 常驻合成例外 `synthetic-self-test`） | `load_exceptions()` 内嵌校验 |

### 5.4 ★ 措辞纪律（★ 防止「弱判据」被当「强判据」传递）

★ ★ **MUST NOT** 把本节表述为「**52/52 全绿**」⇒ ★ 正确的表述是
「**48/48 强判据 + 4/4 partial（20k 前缀，`tohost`/`cycles` vacuous）**」
⇒ ★ 理由：branchbench 的 `tohost` / `cycles` 在 cap 截断下**两侧恒等** ⇒ **无判别力**
⇒ ★ 若写成「52/52 全绿」⇒ 会**误传**给 ADR-088 §5.2 判据 6（删 TLM 的硬判据）⇒ ★ 那是**公开错账**。

★ **G-A3 签认落点**（★ architect 2026-11-08）：`.rddf/roadmap/objectives/objective-backend-parity-and-single-model.md` 的 G-A3 行
（★ 单点签认）· ★ `strategy.md` 的阶段 A 门禁行（★ 47 → 48 修正 + 证据指针）· ★ `feat-backend-parity-gate.md` 的 G-A3 checkbox（★ builder 勾）。
