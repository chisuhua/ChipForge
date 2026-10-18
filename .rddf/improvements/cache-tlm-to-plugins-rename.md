---
id: cache-tlm-to-plugins-rename
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
manual_deps: []
review_by: 2026-12-31
---

# cache-tlm-to-plugins-rename

> 把 `ip/cache/tlm/` 目录重命名为 `ip/cache/plugins/`，与 `ip/cpu/plugins/`（ADR-083 v2.0 已落地）命名统一。
>
> **本文件是 rdd-planner 起的 5 段草稿**（per add-improve HARD-GATE），下一步由 rdd-builder P0 落地 openspec `proposal.md` + `tasks.md`。

## 1. Why

### 项目上下文（v0.11.0 Wave 5 收官窗口 + 后 Wave 6 治理窗口）

本次梳理产出（`/tmp/opencode/code-test-ownership-report.md §F.1`）发现：**ChipForge 8 个 IP 中只有 `ip/cache/` 仍在用旧 `tlm/` 命名**，与 ADR-083 v2.0 的统一规范不一致：

| IP | 当前业务代码目录 | 是否符合 ADR-083 v2.0 (单一 source of truth) |
|----|---------|---------|
| `ip/cpu/` | `ip/cpu/plugins/` (28 个 .h) | ✅ 已落地 |
| `ip/mmu/` | `ip/mmu/tlm/` + `ip/mmu/lib/` 拆分 | ✅ lib 算法 + tlm 业务，命名统一 |
| `ip/cache/` | `ip/cache/tlm/` (L1CachePlugin.{h,cpp} + cache_keys.h + l1_cache_refill_fsm_chmem.h) | ⚠️ **未统一** — 旧 tlm/ 命名 + chmem 混在 tlm/ 下 |
| `ip/{interconnect,memory,peripheral,tilecopy,tilecore}/` | 仅 README+STATUS 占位 | N/A (未实施) |

**两条具体 drift**：

1. **目录命名不一致** (`§F.1`)：`ip/cache/tlm/` vs `ip/cpu/plugins/` —— 同一仓库两个 IP 用不同目录名表达 "业务 Plugin 代码"，对新读者是认知开销（`cache/` 是 Phase 1.2 最早 IP，时序早于 ADR-083）。
2. **chmem 文件混在 tlm/ 下** (`§F.2`)：`ip/cache/tlm/l1_cache_refill_fsm_chmem.h` 与 TLM 文件混在同一目录——违反 ADR-040 v2.0 v2 决策 "CH_MEM 是新正道" 隐含的双文件分离组织惯例。

### 本 change 要解决的架构债

让 `ip/cache/` 与 `ip/cpu/` / `ip/mmu/` 在目录命名上同构，对齐 ADR-083 v2.0 (Plugin-style 单一 source of truth)。同时为后续 cache-phase1.5-4way (`openspec/changes/2026-10-08-cache-phase1.5-4way/`) 与 cache-dse-sweep (Wave 3) 落地铺路——这两条都预期在 `ip/cache/plugins/` 下加新代码，先规范名称比先写新代码再改名廉价得多。

### 非问题（非本 change 范围）

- `ip/mmu/tlm/` 也带 `tlm/` 命名，但 mmu 已有 `lib/` + `tlm/` 双目录切分（`lib/` 装算法纯 C++，`tlm/` 装 Plugin 集成），命名是合理的 ADR-083 v2.0 范例。**mmu 不动**。
- `ip/{interconnect,memory,peripheral}/` 是 5 个空壳 IP，仅 README 占位，本 change 不动。
- 本 change 是纯命名重整，不改任何 .h/.cpp 语义、不改 README/STATUS 内容（除路径字符串外）。

## 2. What Changes

### 变更范围

- **目录改名**：`ip/cache/tlm/` → `ip/cache/plugins/`（4 个文件原地迁移，git mv 保持 history）。
- **路径引用同步**：扫描全仓引用 `ip/cache/tlm/` 的位置，按类型分四类处理。
- **README/STATUS 同步**：`ip/cache/README.md` + `ip/cache/STATUS.md` 内的 `tlm/` 路径字符串改为 `plugins/`。

### 引用分类与处理（精确扫描结果）

| # | 引用类型 | 数量 | 处理动作 |
|---|---------|------|---------|
| 1 | `#include "ip/cache/tlm/L1CachePlugin.h"` 真编译依赖 | 4 处 (src/cf_plugin/bridge/l1_cache_bridge.cpp + .h + tests/cache/test_l1_cache_bridge.cpp + test_l1_cache_plugin_unit.cpp) | 改 include 路径 |
| 2 | `tests/CMakeLists.txt` 第 20 行 CMake 路径 | 1 处 | 改 CMake 引用 |
| 3 | `openspec/changes/<active>/` (ACTIVE) | 4 处（mmu-chmem-pipeline-integration + mmu-chmem-phase-b-c-launch 等活跃 change） | 批量改 |
| 4 | `openspec/changes/archive/` (HISTORICAL) | 23 处（archived change 不可改，会破坏 git 镜像） | **保留 + 在 openspec archive 索引或 doc_link_check 例外清单中标注 `legacy: ip/cache/tlm/`** |
| 5 | `openspec/specs/` (跨版本 SSOT spec) | 6 处 (mmu-vipt-data-flow / mmu-cpptlm-bridge / mmu-soc-minimal-topology / soc-cpu-mmu-demo-topology / mmu-cache-idx-vipt / cpu-l1-mmu-demo-regression / mmufault-verilator-sv32-e2e-flip) | 改 spec 路径 |
| 6 | `docs/architecture/*.md` / `docs/lessons/*.md` / `docs/methodology/*.md` / `docs/superpowers/*.md` | ~25 处 | 改文档路径 |
| 7 | `AGENTS.md` / `CHANGELOG.md` / `.rddf/roadmap/*.md` / `.rddf/plans/*.md` / `.rddf/roadmap/archive/*.md` | ~15 处 | 改元文档路径 |
| 8 | `tools/verify_adr.sh` | 1 处（grep 检查 ADR 内容） | 改工具脚本 |
| 9 | `build/*` (CMake 缓存) | n | 重新 `cmake --build` 自动消解 |
| 10 | `ip/cache/tlm/*.chmem.h` 内 `l1_cache_refill_fsm_chmem.h` 的子目录归属 | 1 处 | 跟随 tlm/ → plugins/ 改；该文件名保持不变（已带 `_chmem.h` 后缀） |

### 非目标（Out of Scope）

- **不改任何 .h/.cpp 业务代码语义**（除 #include 路径字符串外）。
- **不改 ADR-040 v2.0 自身**（ADR 内容已正确，无需修订）。
- **不改 docs/architecture/adr.md 的 ADR 列表**（ADR 都是 Plugin 框架级，不是 cache-specific）。
- **不动 ip/mmu/tlm/**（mmu 命名合规）。
- **不动 placeholder IP** (`ip/{interconnect,memory,peripheral,tilecopy,tilecore}/`)。
- **不动 tests/soc/**（这是另一次 change `tests-soc-topology-restructure` 的范围）。
- **不动 `mfc-cpu-pipeline-multi-cycle-fsm` archive 路径**（archived 永远保留）。

## 3. Acceptance

### 必达（Hard）

- [ ] `git mv ip/cache/tlm ip/cache/plugins` 完成，git log 保留文件历史。
- [ ] 全仓 `grep -rn "ip/cache/tlm"` 返回 **仅 openspec/changes/archive/ 命中**（旧 archive 不可改）+ `0` 其他命中（业务代码、CMake、活 openspec、docs、AGENTS、CHANGELOG、tools、.rddf 全部已改）。
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS（与本 change 无关项不退化）。
- [ ] `bash tools/verify_plugin_decision.sh` 0 fail（与本 change 无关项不退化）。
- [ ] `bash tools/verify_adr.sh` 0 fail（与本 change 无关项不退化）。
- [ ] `bash tools/doc_link_check.sh` 0 fail（除 `openspec/changes/archive/` 内 legacy 路径不计入 fail）。
- [ ] `bash tools/run_chipforge_tests.sh` 0 regression（`[framework] 89/89` + `[cache] 7/7` + `[mmu] 59/59` + `[cpu-l1-mmu-demo] 6/6` + `[mmu-verilator] 3/3` 维持）。
- [ ] cmake 重新 configure 无 hard error：`cmake -B build` + `cmake --build build`。

### 应达（Soft）

- [ ] `ip/cache/README.md` 目录结构表更新（`tlm/` → `plugins/`）。
- [ ] `ip/cache/STATUS.md` 第 2 行文件路径更新。
- [ ] 添加 ADR-083 (Plugin-style 单一 source of truth) 在文档链接中的引用，让 cache README 与 cpu README 对齐。
- [ ] AGENTS.md 内 `ip/cache/README.md` 引用保持一致。

### 治理（Meta）

- [ ] openspec change 提案 `proposal.md` 含 frontmatter `initiative: wave5-isa-coverage-and-bp`（per `.rddf/sync_strategy_status.sh` 派生需求）。
- [ ] `openspec archive cache-tlm-to-plugins-rename` 成功（仅 1 个 change 而非分拆）。
- [ ] 本 improvement 草稿状态从 `draft` → `archived`（移动到 `archive/` 或在 `.rddf/improvements/` 保留已 approved 状态供审计）。

## 4. Capabilities

新增能力（对齐 code-test-ownership-report.md §E 归属划分原则）：

- **目录命名同构**：8 个 IP 中的 3 个实质业务 IP（cpu/cache/mmu）现在都用 `plugins/`（mmu 用 `lib+tlm`，cache 升级后用 `plugins/`），新读者看到 `ip/<name>/plugins/` 即知 "这里是双文件分离的双模式 Plugin 业务代码"。
- **chmem 文件命名清晰**：`ip/cache/plugins/*.chmem.h` 与 TLM 文件同目录，符合 ADR-040 v2.0 "CH_MEM 是新正道" 隐含的文件组织规范。
- **为 cache-phase1.5-4way 铺路**：Wave 3 `cache-phase1.5-4way` (`openspec/changes/2026-10-08-cache-phase1.5-4way/`) 落地时直接在 `plugins/` 下加新 plugin 文件，避免先写代码再改名。
- **未来新 IP 模板**：作为参照，后续 `ip/peripheral/`、`ip/interconnect/` 等实施时直接用 `plugins/` 命名，无需纠结历史包袱。
- **doc_link_check 例外列表清晰化**：本次 archive legacy 路径的 doc_link_check 例外化做法形成模板，未来 Wave 6 治理（如 mmu rename）可复用。

## 5. Impact

### 受影响文件

| 文件 / 目录 | 动作 | 影响 |
|------|------|------|
| `ip/cache/tlm/` 4 文件 | git mv | L1CachePlugin.h + .cpp + cache_keys.h + l1_cache_refill_fsm_chmem.h 移入 plugins/ |
| `ip/cache/README.md` | 改路径字符串 | 1-2 行 |
| `ip/cache/STATUS.md` | 改路径字符串 | 1-2 行 |
| `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` | 改 `#include` | 2 行 |
| `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` | 改 `#include` | 2 行（已知 c2 接口用） |
| `tests/CMakeLists.txt` | 改 CMake 路径 (L20 + L25 附近) | 1 行 |
| `tests/cache/test_l1_cache_bridge.cpp` | 改 `#include` | 1 行 |
| `tests/cache/test_l1_cache_plugin_unit.cpp` | 改 `#include` | 1 行 |
| `tools/verify_adr.sh` | 改 grep 模式 | 1 行 |
| 活 openspec changes/ 4 文件 | 改路径字符串 | ~6 行 |
| openspec/specs/ 6 文件 | 改路径字符串 | ~12 行 |
| docs/ 顶层 ~10 文件 | 改路径字符串 | ~25 行 |
| AGENTS.md + CHANGELOG.md | 改路径字符串 | ~10 行 |
| .rddf/roadmap/archive/ + plans/ | 改路径字符串 | ~10 行 |

总估时：**40+ 文件路径改动**，**0 行业务语义改动**。

### 风险（per `.rddf/roadmap/strategy.md` §5）

- **R-DOC-1（doc_link_check 例外）**：openspec archive 23 处无法改 → 必须把 archive 内的 `ip/cache/tlm/` 路径纳入 `doc_link_check.sh` 的 KNOWN_LEGACY 列表。**缓解**：本次 change tasks §D 加 KNOWN_LEGACY 行；ADRs repo 例外化做法对齐。
- **R-COMPILE-1（include 改错）**：4 处真 `#include` 路径改错会导致编译失败 → **缓解**：CMake 重新 configure + 一次性 build，0 警告 0 错。
- **R-CMAKE-1（CMake 路径 cache 残留）**：`build/CMakeCache.txt` 残留旧绝对路径 → **缓解**：本 change 文档第 §3 必达项加 `rm -rf build/CMakeCache.txt && cmake -B build`。
- **R-TEST-1（[cache] 测试回归）**：原有 7/7 PASS 在 include 路径改后可能因符号解析回归 → **缓解**：先单跑 `[cache]` family，发现 1 fail 即查 diff。
- **R-DOC-LINK-1（active openspec 没改）**：活 openspec changes/ 4 处遗漏 1 处 → **缓解**：本 change tasks §C.3 用自动化 grep 验证（`grep -rn 'ip/cache/tlm' openspec/changes/ | grep -v archive` 必须返回 0 hits）。
- **回退**：单目录重命名 + 路径字符串同步，atomic commit 即可 `git revert`，无 trail effect。

### 下游依赖

- **上游**：无（无 manual_deps，本 change 是独立 renaming refactor）。
- **下游**：
  - `cache-phase1.5-4way` (openspec/changes/2026-10-08-cache-phase1.5-4way/) — 直接收益，新 plugin 文件在 `plugins/` 下创建，无需改 include 路径。
  - `cache-dse-sweep` (Wave 3, deferred) — 同上。
  - `verilator-l1cache-e2e-coverage` (`openspec/changes/2026-10-08-verilator-l1cache-e2e-coverage/`) — 该 change 的引用路径同步需要本 change 先 archive。

### 不解决

- `tests/soc/` 拆细 → 单独的 `tests-soc-topology-restructure` change。
- `ip/cache/docs/adr/ADR-044-l1-cache-vipt-coherence.md` → 独立 ADR 治理 change（如需）。
- `tests/bundles/` 补 csr/tlb bundle 测试 → 独立测试补全 change。
- 占位 IP (interconnect/memory/peripheral/tilecopy/tilecore) 治理 → 独立 cleanup change。
- `tests/cpu/schema/` 命名 → 独立 rename change。
