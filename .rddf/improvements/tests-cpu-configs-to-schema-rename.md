---
id: tests-cpu-configs-to-schema-rename
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P2
manual_deps: []
review_by: 2026-12-31
---

# tests-cpu-configs-to-schema-rename

> 把 `tests/cpu/configs/test_schema_m5_19.cpp` (231 行) 改名为 `tests/cpu/schema/` 下，与 docs §F.5 命名漂移治理对齐。
>
> **本文件是 rdd-planner 起的 5 段草稿**，下一步由 rdd-builder P0 落地 openspec `proposal.md` + `tasks.md`。

## 1. Why

### 项目上下文

code-test-ownership-report.md §F.5 命名漂移指认：`tests/cpu/configs/test_schema_m5_19.cpp` 目录语义是 "**测试** for configs"（不是 configs 自身），但 `ip/cpu/configs/` 是 cpu IP 的 JSON configs。

| 路径 | 实际内容 | 命名歧义 |
|------|---------|---------|
| `ip/cpu/configs/cpu_default.json` | CPU JSON config | **真 configs** (IP 配置) |
| `tests/cpu/configs/test_schema_m5_19.cpp` | Catch2 test for CPU config JSON schema validation | **tests for configs** (与 `ip/cpu/configs/` 混淆) |

两条目录独立但名字相同 (configs) 让新人 + doc_link_check 容易混淆。本次 change 改 `tests/cpu/configs/` → `tests/cpu/schema/`，与 `ip/cpu/configs/` 物理 + 语义分离。

### 本 change 要解决的架构债

1. **`tests/` 下的 `configs/` 命名与 `ip/<name>/configs/` 是不同语义** — 这一 drift 在 `code-test-ownership-report.md §F.5` 中标记。
2. 与 `tests/soc/{soc-cpu-l1-mmu-demo,mmu-minimal,l1-cache-minimal}/` 命名对称（tests 目录按其测试对象的 type/dimension 命名，而**不是**按 `ip/<name>/<dir>` 1:1 镜像）。
3. **cache-tlm 已完成**: 之前的 `ip/cache/tlm/` + `tests/cache/` 漂移已通过 cache-tlm-to-plugins-rename change 治理。

### 非问题（非本 change 范围）

- **不动 `ip/cpu/configs/`**（真 configs）。
- **不动 `ip/cpu/docs/`**（CPU IP-level docs）。
- **不动 `ip/cpu/plugins/`**（CPU plugins）。
- **不动 `tests/{cache,mmu,framework,bundles}/`**。
- **不动 `tests/cpu/integration/`、`tests/cpu/{riscv_tests,manual_elf}/`**（这些不是 `configs/` 命名问题）。

## 2. What Changes

### 变更范围

- **目录重命名**：`tests/cpu/configs/` → `tests/cpu/schema/`（1 个文件，git mv）。
- **路径字符串同步**：

| # | 引用类型 | 数量 | 处理动作 |
|---|---------|------|---------|
| 1 | 真编译依赖 | 0 处（`GLOB_RECURSE` + 命名无关） | ✅ 0 动作 |
| 2 | `tests/cpu/configs/test_schema_m5_19.cpp` 真 include | 0 处 | ✅ 0 动作 |
| 3 | docs/superpowers/plans/2026-06-30-catch2-test-framework.md | 1 处 | 改路径字符串 |
| 4 | openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/ (3 文件) | 3 处 | **保留 + doc_checker.py:76 豁免**（archive 不可改） |
| 5 | .rddf/plans/cpu-pipeline-tick-rca.md | 1 处 | 改路径字符串（plan 文档） |
| 6 | .rddf/improvements/cache-tlm-to-plugins-rename.md | 1 处 | 改路径字符串（dependency drift 同步） |
| 7 | .rddf/improvements/tests-soc-topology-restructure.md | 1 处 | 改路径字符串（dependency drift 同步） |
| 8 | `ip/cpu/configs/` 真 configs (5 JSON) | 0 命中（不同语义） | ✅ 0 动作 |

### 非目标（Out of Scope）

- **不改 test_schema_m5_19.cpp 业务代码**（仅 git mv 保留 history）。
- **不动 `ip/cpu/configs/`**（真 configs）。
- **不动 Catch2 family tag**（`[cpu-configs]` 当前 `[cpu]` 子集 — 改不改由 reviewer 决定，本提案**保留** `[cpu-configs]` tag）。
- **不动测试基础设施**（CMake GLOB_RECURSE 自动）。

## 3. Acceptance

### 必达（Hard）

- [ ] `git mv tests/cpu/configs tests/cpu/schema` 完成（history preserved）。
- [ ] `gcc-14 cmake --build build -j$(nproc)` 0 error。
- [ ] `[cpu-configs]` family 全 PASS (实测 1 TEST_CASE 内 1+ assertions)。
- [ ] `[cpu]`, `[cpu-integration]`, `[cpu-l1-mmu-demo]`, `[soc]`, `[mmu]`, `[cache]`, `[riscv-tests]` 各 family **无回归**。
- [ ] 全仓 `grep -rn 'tests/cpu/configs' | grep -v 'build/'` 排除 archive + docs/notes/archive + improvement drafts + READMEs historical 后，**0 hits**。
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS。
- [ ] `bash tools/verify_plugin_decision.sh` 8/8 PASS。
- [ ] `bash tools/verify_adr.sh` 0 fail。
- [ ] `python tools/doc_checker.py --format text` 0 新增 fail (默认已豁免 archive)。
- [ ] openspec archive `tests-cpu-configs-to-schema-rename` 成功。

### 应达（Soft）

- [ ] `tests/cpu/schema/README.md` 新建（如需要）说明该目录是 "CPU config schema validation tests"。
- [ ] `tests/cpu/README.md` 顶层索引（如有）更新指向 `schema/`。

### 治理（Meta）

- [ ] openspec/specs/tests-cpu-configs-to-schema-rename/spec.md 6-8 Requirement 落地。
- [ ] proposal.md frontmatter 含 `initiative: wave5-isa-coverage-and-bp`。
- [ ] `bash .rddf/sync_strategy_status.sh` 派生 strategy.md §7。

## 4. Capabilities

新增能力（对齐 code-test-ownership-report.md §E 归属划分原则）：

### MUST

- `tests/cpu/schema/test_schema_m5_19.cpp` 文件物理位置对应该测试对象的 type (CPU config schema validation)，而非 ip 镜像。

### MUST NOT

- 不动 `ip/cpu/configs/`（真 configs，是 IP 配置，不是测试）。
- 不改 Catch2 family tag (但留为 follow-up 给 reviewer 评估 `cpu-configs` → `cpu-schema` tag rename 决策)。
- 不破坏 `[cpu]` family 过滤器（schema test 仍属 cpu 子集）。

详见 code-test-ownership-report.md §E（归属划分原则）。

## 5. Impact

### 受影响文件（精确清单 per critic BLOCKER）

| 文件 | 动作 |
|------|------|
| `tests/cpu/configs/test_schema_m5_19.cpp` | git mv → `tests/cpu/schema/test_schema_m5_19.cpp` |
| `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` | 改 1 处路径字符串 |
| `.rddf/plans/cpu-pipeline-tick-rca.md` | 改 1 处路径字符串 |
| `.rddf/improvements/cache-tlm-to-plugins-rename.md` | 改 1 处路径字符串 |
| `.rddf/improvements/tests-soc-topology-restructure.md` | 改 1 处路径字符串 |

**总计: 1 git mv + 4 个 .md 字符串同步 + 0 业务代码改动.**

### 风险

- **R-TAG-1**：Catch2 family tag `[cpu-configs]` 仍能找到 test — 缓解：本 change 不改 tag；review 评估 `[cpu-configs]→[cpu-schema]` 为 follow-up。
- **R-CARVE-1**：`GLOB_RECURSE` 自动发现新位置 — 缓解：cmake rebuild 验证。
- **R-DOC-1**：openspec/changes/archive/3 处不可改 — 缓解：`doc_checker.py:76` 默认豁免；与 cache-tlm/tests-soc 一致策略。
- **回退**：单目录 rename + 路径字符串同步，atomic commit `git revert` 可恢复。

### 下游依赖

- 上游：无
- 下游：未来 cache-dse-sweep / wave6 governance 可能引入新的"tests for configs vs 真 configs"漂移，本 change 留下命名纪律。

### 不解决

- `tests/cpu/integration/` 命名（与 tests/soc/{,...}/ 命名语义不同，但与 ip/cpu 不冲突，不动）。
- `tests/cpu/manual_elf/` 命名（fixture 数据，不是 tests/configs 漂移）。
- 5 个空壳 placeholder IP (`ip/{interconnect,memory,peripheral,tilecopy,tilecore}`) — 独立 cleanup change。
- `tests/bundles/` 补 csr/tlb bundle 测试 — 独立测试补全 change。
