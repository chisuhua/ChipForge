---
initiative: wave5-isa-coverage-and-bp
priority: P2
version_target: v0.11.0+post-launch
revision_history:
  - date: 2026-10-18
    author: rdd-builder (Sisyphus bootstrap, cache-tlm/tests-soc predecessor)
    summary: |
      Initial draft. Simpler than cache-tlm/tests-soc counterparts:
      1 git mv + 4 path-string updates + 0 business code change.
      Per critic lessons learned from cache-tlm 2-round BLOCKER fix,
      pre-emptive frontmatter added.
---

# tests-cpu-configs-to-schema-rename — `tests/cpu/configs/` → `tests/cpu/schema/`

## Why

code-test-ownership-report.md §F.5 命名漂移指认：`tests/cpu/configs/` 目录语义是 "**测试** for cpu configs"，但 `ip/cpu/configs/` 是真 JSON configs (IP 配置)。

| 路径 | 实际内容 | 类型 |
|------|---------|------|
| `ip/cpu/configs/cpu_default.json` | CPU JSON config | **真 configs** |
| `tests/cpu/configs/test_schema_m5_19.cpp` | Catch2 test for CPU config JSON schema validation | **tests for configs** |

两条目录独立但名字相同 (configs) 让新人 + doc_link_check 容易混淆。本次 change 改 `tests/cpu/configs/` → `tests/cpu/schema/`，与 `ip/cpu/configs/` 物理 + 语义分离。

详见 `.rddf/improvements/tests-cpu-configs-to-schema-rename.md §1`。

## What Changes

### In Scope

- **目录重命名**：`tests/cpu/configs/` → `tests/cpu/schema/`（1 文件 git mv，history preserved）。
- **4 个 .md 路径字符串同步**：

| # | 引用类型 | 文件 | 处理 |
|---|---------|------|------|
| 1 | docs 例子 | `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` | 改 1 处 |
| 2 | plan doc | `.rddf/plans/cpu-pipeline-tick-rca.md` | 改 1 处 |
| 3 | improvement 同伴 | `.rddf/improvements/cache-tlm-to-plugins-rename.md` | 改 1 处 |
| 4 | improvement 同伴 | `.rddf/improvements/tests-soc-topology-restructure.md` | 改 1 处 |

- **新增 openspec spec-delta**：`openspec/specs/tests-cpu-configs-to-schema-rename/spec.md` (D3 mapping)。

### Out of Scope

- **不动 `ip/cpu/configs/`**（真 CPU configs）。
- **不动** `tests/cpu/{integration,manual_elf,riscv_tests}/`（fixture/集成，与 configs 漂移无关）。
- **不动 Catch2 family tag** (保留 `[cpu-configs]`，review 评估 rename 为 follow-up)。
- **不动 `ip/cpu/plugins/`、`ip/cpu/docs/`、`ip/cpu/{arch,core}/`**（cpu IP 内部目录命名合规）。
- **不动 openspec/changes/archive/2026-10-15-cpu-pipeline-tick-rca/** (3 处历史 archive 不可改)。

### 关联 ADR / Spec / Issue

- **code-test-ownership-report.md §F.5** — 本次 drift 来源
- **回退指引**：cache-tlm-to-plugins-rename + tests-soc-topology-restructure 经验（2 precedent rdd-workflow change），同 generator pattern

## Capabilities

### MUST

- `tests/cpu/schema/test_schema_m5_19.cpp` 文件存在（git mv from `tests/cpu/configs/`）。
- 4 个 .md 路径字符串同步。
- `[cpu-configs]` family test 仍 PASS（family tag 不变）。

### MUST NOT

- 不动 `ip/cpu/configs/`。
- 不动 `tests/cpu/` 下其他子目录。
- 不动 Catch2 family tag。
- 0 business code 改动。

详见 `.rddf/improvements/tests-cpu-configs-to-schema-rename.md §4`。

## Impact

### 受影响文件（精确清单）

| 文件 | 动作 |
|------|------|
| `tests/cpu/configs/test_schema_m5_19.cpp` | git mv → `tests/cpu/schema/test_schema_m5_19.cpp` |
| `docs/superpowers/plans/2026-06-30-catch2-test-framework.md` | 改 1 处路径字符串 |
| `.rddf/plans/cpu-pipeline-tick-rca.md` | 改 1 处路径字符串 |
| `.rddf/improvements/cache-tlm-to-plugins-rename.md` | 改 1 处路径字符串 |
| `.rddf/improvements/tests-soc-topology-restructure.md` | 改 1 处路径字符串 |

**总计: 1 git mv + 4 个 .md 字符串同步 + 0 业务代码改动.**

### 风险

- **R-TAG-1**：`[cpu-configs]` family tag 仍能找到 test — 缓解：本 change 不改 tag；follow-up 评估 `[cpu-configs]→[cpu-schema]` rename。
- **R-CARVE-1**：`GLOB_RECURSE` 自动发现新位置 — 缓解：cmake rebuild 验证。
- **R-DOC-1**：openspec/changes/archive/3 处不可改 — 缓解：`doc_checker.py:76` 默认豁免；与 cache-tlm/tests-soc 一致策略。
- **回退**：单目录 rename + 路径字符串同步，atomic commit `git revert` 可恢复。

详见 `.rddf/improvements/tests-cpu-configs-to-schema-rename.md §5`。

## Acceptance

### 必达（Hard）

- [ ] `git mv tests/cpu/configs tests/cpu/schema` 完成（history preserved）。
- [ ] `gcc-14 cmake --build build -j$(nproc)` 0 error。
- [ ] `[cpu-configs]` family 全 PASS。
- [ ] `[cpu]`, `[cpu-integration]`, `[cpu-l1-mmu-demo]`, `[soc]`, `[mmu]`, `[cache]`, `[riscv-tests]` 各 family **无回归**。
- [ ] 全仓 `grep -rn 'tests/cpu/configs' | grep -v 'build/'` 排除 archive + improvement drafts + READMEs historical 后，**0 hits**。
- [ ] `bash tools/check_plugin_portability.sh` 12/12 PASS。
- [ ] `bash tools/verify_plugin_decision.sh` 8/8 PASS。
- [ ] `bash tools/verify_adr.sh` 0 fail。
- [ ] `python tools/doc_checker.py --format text` 0 新增 fail。
- [ ] openspec archive `tests-cpu-configs-to-schema-rename` 成功。
