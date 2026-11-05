# ChipForge 工具脚本

> **状态**: Phase 0+ 持续维护 (2026-06-12 起)
> **集成**: 3 个验证脚本接入 GitHub Actions (`.github/workflows/architecture-gates.yml` PR 阻塞 + `.github/workflows/doc_check.yml` smoke)
> **决策**: `docs/architecture/adr.md` ADR-043 "CI 强制架构门禁"

## 1. 验证脚本 (Architecture Gates)

| 脚本 | 用途 | 退出码 | CI 集成 |
|------|------|--------|---------|
| `tools/verify_adr.sh` | 验证 ADR 注册表与代码实现对齐 (条数见 `docs/architecture/adr.md`) | 0=PASS / 1=❌ Critical drift / 2=脚本错误 | ✅ `architecture-gates.yml` step 1 (PR 阻塞) + `doc_check.yml` smoke (`--only=ADR-024`) |
| `tools/verify_plugin_decision.sh` | D4 Plugin-style 业务代码静态检查 (无 tick/无状态机/uint_t<N>) | 0=PASS / 1=❌ D4 违规 | ✅ `architecture-gates.yml` step 2 (PR 阻塞) |
| `tools/check_plugin_portability.sh` | ADR-040 移植性约束 (Tier-1: 早返/ch_mem 渗透/pb.run/array_store) | 0=PASS / 1=❌ 移植性违规 | ✅ `architecture-gates.yml` step 3 (PR 阻塞) |
| `tools/doc_link_check.sh` | 文档路径引用完整性 —— 检查 1: markdown 相对链接 (docs/ ip/ src/cf_plugin/ + 根级文档); 检查 2: 反引号包裹的仓库路径 (仅根级文档) | 0=全 PASS / 1=有断链 | (推荐) `architecture-gates.yml` step 4 |
| `tools/check_xlen_hygiene.sh` | ADR-087 §D6 XLEN 假设 hygiene 软门禁 (新增 `kXlenBits` 定义点须登记 `ip/cpu/docs/rv64_readiness_gap.md` §5) | 0=PASS (含 warn) / 1=STRICT 模式下有未登记定义点 | ✅ `architecture-gates.yml` step 4 (**warn 级**, `continue-on-error: true`) |
| `tools/check_tlm_freeze.sh` | ADR-088 §2 TLM-only 代码冻结门禁 (相对 baseline 新增的函数定义须登记 `.rddf/state/tlm-freeze-allowlist.txt`；检测面 = `ip/` 下非 `_chmem.h` 的 `.h` 减去 9 个双模共享文件) | 0=PASS / 1=有未登记的新增函数 | ✅ `architecture-gates.yml` step 5 (**warn 级**, `continue-on-error: true`) |
| `tools/gen_coverage_parity_ledger.sh` | ADR-088 §4 覆盖平价台账生成器 (从两个 binary 的 `--list-tests` 差集生成条目骨架 + 重算闭合校验；数字不写死) | 0=闭合 OK / 1=口径不闭合 | (本地工具, CI 不调用) |
| `tools/check_coverage_parity_ledger.sh` | ADR-088 §4 覆盖平价台账漂移检测 (新增 TLM-only case 未登记 / 标注缺失 / 批量退役红线 / 族求和口径误用) | 0=PASS / 1=有违规 | ✅ `architecture-gates.yml` step 6 (**warn 级**, `continue-on-error: true`) |

## 2. 构建/测试入口脚本

| 脚本 | 用途 | 退出码 | CI 集成 |
|------|------|--------|---------|
| `tools/build.sh` | cmake configure + build 入口 (封装 AGENTS.md 5 种构建模式) | 0=成功 / 非0=失败 | (推荐) 可接入 CI 构建阶段 |
| `tools/run_chipforge_tests.sh` | 测试运行器 (可选先 build)，封装 ctest + CppHDL 排除 + label 过滤 | 0=PASS / 非0=FAIL | (推荐) 可接入 CI 测试阶段 |

## 2. 脚本详情

### 2.1 `verify_adr.sh`

- **类别**: A 框架架构 / B CppTLM / C CppHDL / D 注册 / E 端口 / F Bundle / G Plugin / H Pipe / I 验证 / J 目录
- **ADR 状态**: 条数与分布以 `docs/architecture/adr.md` 注册表为准 (脚本运行时实测, 本文件不写死快照)
- **关键防护**:
  - ADR-024 (`verify_adr.sh:538-543`): 主动拒绝 `bundles/bundle_mapper.h` 提前实现, 推迟到 Phase 5/6
  - ADR-033: CtrlLink 4-control-API 验证 (`include/cf/plugin/ctrl_link.h:34/40/46/52`)
  - ADR-040: 3 级约束模型 (DRIFT 防护在 `check_plugin_portability.sh`)

### 2.2 `verify_plugin_decision.sh`

- **D4 业务检查 3 项**:
  1. 业务代码无 `void tick()` 重写 (LSP `cf::plugin` 防护性 `delete` + Bridge 框架层例外)
  2. 业务代码无状态机 (无 `enum class State` / `switch (state_)`)
  3. Bundle 字段用 `cf::plugin::uint_t<N>` (非 raw `uint32_t/64_t`, 非 `ch_uint<>`)

### 2.3 `check_plugin_portability.sh`

- **ADR-040 Tier-1 4 项**:
  1. 业务代码无 `if (cond) return;` 早返 (强制 `at_stage()` 完整执行)
  2. `ip/*/tlm/` 无 `ch_mem` / `ch_reg` / `ch_uint` / `ch::core` 渗透 (TLM/RTL 隔离)
  3. Plugin `build()` 不调用 `pb.run()` (由 PipeBuilder 调度)
  4. 存储声明优先 `array_store` (非 `std::array` 直用)

### 2.4 `doc_link_check.sh`

两项检查 (2026-11-05 起):

| # | 检查 | 范围 | 覆盖的失效类型 |
|---|------|------|--------------|
| 1 | markdown 相对链接 | `docs/` `ip/` `src/cf_plugin/` + 根级文档 (`AGENTS.md` / `README.md` / `CONTRIBUTING.md`) | `](路径)` 形式的死链 |
| 2 | 反引号包裹的仓库路径 | 仅根级文档 (`AGENTS.md` / `README.md` / `CONTRIBUTING.md`) | `` `docs/xxx.md` `` 形式的裸路径死引用 |

- **检查 2 为何只限根级文档**：`docs/` 下架构文档含大量历史 / 规划态路径引用 (实测 92 条), 全量扫描噪声过高 ⇒ 限定在"用户/agent 第一入口"，那里死引用危害最大且内容是人工维护的
- **忽略清单**：`tools/doc_link_check_ignore.txt` —— 允许"**故意**不存在"的引用 (如陈述"某目录已删除")；**不得**用来掩盖真实死引用
- **退出码**：任一检查有断链 ⇒ `1`；全过 ⇒ `0`
- **输出**：`BROKEN MARKDOWN CROSS-REFERENCES` / `BROKEN BACKTICK PATH REFERENCES` 两段，带 `文件:行号`
- `--quiet` 只输出断链，不打进度日志

## 3. 本地运行

### 3.1 构建/测试快速命令

```bash
# 默认 Release 构建 (首次自动 ExternalProject build CppTLM/CppHDL)
bash tools/build.sh

# Debug 构建
bash tools/build.sh --type Debug

# ASan 构建 (ENABLE_ASAN=ON + Debug, 隐含覆盖 --type)
bash tools/build.sh --asan

# 重建 CppTLM/CppHDL 依赖 (源码修改后)
bash tools/build.sh --rebuild-deps

# 源码嵌入模式 (add_subdirectory, 改 API 即时生效)
bash tools/build.sh --source-deps

# 仅清理 build/_deps (强制重建依赖)
bash tools/build.sh --clean

# 完全重置 (rm -rf build)
bash tools/build.sh --reset

# 仅 configure 不 build
bash tools/build.sh --no-build

# 并行任务数 (默认 nproc)
bash tools/build.sh -j 8

# 仅运行测试 (假设 build/ 已存在)
bash tools/run_chipforge_tests.sh                    # TLM only (case 总数运行时实测打印)

# 一键: 构建 + 测试
bash tools/run_chipforge_tests.sh --build

# 一键: ASan 构建 + 测试
bash tools/run_chipforge_tests.sh --asan

# 按 Catch2 tag 过滤 (TLM only)
bash tools/run_chipforge_tests.sh --tag "[cache]"
bash tools/run_chipforge_tests.sh --exclude "[mmu]"

# 全量 (TLM + CH_MEM + Verilator smoke + gate)
bash tools/run_chipforge_tests.sh --all

# Verilator 后端完整测试 (Phase 6d.5 E8 + 6d P2)
./build/bin/chipforge_tests_chmem [verilator],[mmu-verilator]  # Verilator 两个 family
./build/bin/cpu_verilator_sim --elf tests/cpu/riscv_tests/elf/rv32ui-p-add  # 1 ELF smoke (开发者 debug 入口)

# 详细输出
bash tools/run_chipforge_tests.sh --verbose
```

### 3.2 架构门禁 + 文档检查

```bash
# 1. ADR 全验证
bash tools/verify_adr.sh

# 2. ADR-024 单条 smoke
bash tools/verify_adr.sh --only=ADR-024

# 3. D4 业务检查
bash tools/verify_plugin_decision.sh

# 4. ADR-040 移植性
bash tools/check_plugin_portability.sh

# 5. 文档链接交叉引用 (markdown 链接 + 反引号路径, 防文档债审计发现的死链回归)
bash tools/doc_link_check.sh
bash tools/doc_link_check.sh --quiet  # 仅断链, 无进度输出
```

## 4. CI 集成详情

| Workflow | 触发器 | 3 脚本行为 |
|----------|--------|------------|
| `.github/workflows/architecture-gates.yml` | `pull_request` to main/develop | 3 脚本全部 PR 阻塞 (任一失败 → exit 1) |
| `.github/workflows/doc_check.yml` | `pull_request` (paths 含 docs/ / ip/) | 仅 `verify_adr.sh --only=ADR-024` smoke (`continue-on-error: true` 不阻塞) |

## 5. 故障排查

### 5.1 架构门禁

- **verify_adr.sh 报 `❌ FAILED`**: 检查具体 ADR 行号, 修复代码或 ADR 状态 (二者必居其一一致)
- **verify_plugin_decision.sh 报 `❌ D4 违规`**: 业务代码不得有 `void tick()` / 状态机 / raw uint_t 字段
- **check_plugin_portability.sh 报 `❌`**: 早返 / ch_mem 渗透 / pb.run 误用 / array_store 漏用

### 5.2 构建/测试

- **`build.sh` 报 `Unknown arg`**: 检查参数拼写；用 `build.sh --help` 查看完整列表
- **`build.sh --type` 不生效**: 如果同时给了 `--asan`, `--type` 会被 ASan 隐含的 Debug 覆盖（设计：ASan 必须配 Debug）
- **`run_chipforge_tests.sh --build` 失败**: 先单独跑 `build.sh --type Debug` 排查是 build 问题还是 test 问题
- **`run_chipforge_tests.sh --all` 大量 Not Run**: 已知 PA-4b 限制（CppHDL 在父项目 C++17 模式下 add_executable 失败），不需修

## 6. 相关文档

- AGENTS.md §3 构建与测试 (构建模式矩阵) / §6 工作流 (验证命令 + CI 集成)
- `docs/architecture/adr.md` ADR-043 (CI 集成决策, 2026-06-12)
- `docs/architecture/adr.md` ADR-024 (Bundle 三层分层, ⚠️ Mapper 推迟)
- `docs/architecture/adr.md` ADR-033 (CtrlLink 4-control-API)
- `docs/architecture/adr.md` ADR-040 (TLM→HDL 移植性约束)
- `tools/baseline_known_failures.txt` (baseline-known 失败清单 = 「回归是否通过」的判定真源)
