# ChipForge 工具脚本

> **状态**: Phase 0+ 持续维护 (2026-06-12 起)
> **集成**: 3 个验证脚本接入 GitHub Actions (`.github/workflows/architecture-gates.yml` PR 阻塞 + `.github/workflows/doc_check.yml` smoke)
> **决策**: `docs/architecture/adr.md` ADR-043 "CI 强制架构门禁"

## 1. 验证脚本 (Architecture Gates)

| 脚本 | 用途 | 退出码 | CI 集成 |
|------|------|--------|---------|
| `tools/verify_adr.sh` | 验证 ADR 注册表与代码实现对齐 (39 条 ADR) | 0=PASS / 1=❌ Critical drift / 2=脚本错误 | ✅ `architecture-gates.yml` step 1 (PR 阻塞) + `doc_check.yml` smoke (`--only=ADR-024`) |
| `tools/verify_plugin_decision.sh` | D4 Plugin-style 业务代码静态检查 (无 tick/无状态机/uint_t<N>) | 0=PASS / 1=❌ D4 违规 | ✅ `architecture-gates.yml` step 2 (PR 阻塞) |
| `tools/check_plugin_portability.sh` | ADR-040 移植性约束 (Tier-1: 早返/ch_mem 渗透/pb.run/array_store) | 0=PASS / 1=❌ 移植性违规 | ✅ `architecture-gates.yml` step 3 (PR 阻塞) |
| `tools/doc_link_check.sh` | 文档相对路径交叉引用完整性 (防止 2026-06-13 发现的 42 个死链回归) | 0=全 PASS / 1=有断链 | (推荐) `architecture-gates.yml` step 4 |

## 2. 脚本详情

### 2.1 `verify_adr.sh`

- **类别**: A 框架架构 / B CppTLM / C CppHDL / D 注册 / E 端口 / F Bundle / G Plugin / H Pipe / I 验证 / J 目录
- **ADR 状态**: 30 ✅ + 1 ⚠️ (ADR-024) + 8 🚧 (Phase 1 提案), 39 总
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

## 3. 本地运行

```bash
# 1. ADR 全验证
bash tools/verify_adr.sh

# 2. ADR-024 单条 smoke
bash tools/verify_adr.sh --only=ADR-024

# 3. D4 业务检查
bash tools/verify_plugin_decision.sh

# 4. ADR-040 移植性
bash tools/check_plugin_portability.sh

# 5. 完整 ctest (附加, 不在 3 脚本内)
bash tools/run_chipforge_tests.sh

# 6. 文档链接交叉引用 (新增, 防 2026-06-13 发现的 42 死链回归)
bash tools/doc_link_check.sh
bash tools/doc_link_check.sh --quiet  # 仅断链, 无进度输出
```

## 4. CI 集成详情

| Workflow | 触发器 | 3 脚本行为 |
|----------|--------|------------|
| `.github/workflows/architecture-gates.yml` | `pull_request` to main/develop | 3 脚本全部 PR 阻塞 (任一失败 → exit 1) |
| `.github/workflows/doc_check.yml` | `pull_request` (paths 含 docs/ / ip/) | 仅 `verify_adr.sh --only=ADR-024` smoke (`continue-on-error: true` 不阻塞) |

## 5. 故障排查

- **verify_adr.sh 报 `❌ FAILED`**: 检查具体 ADR 行号, 修复代码或 ADR 状态 (二者必居其一一致)
- **verify_plugin_decision.sh 报 `❌ D4 违规`**: 业务代码不得有 `void tick()` / 状态机 / raw uint_t 字段
- **check_plugin_portability.sh 报 `❌`**: 早返 / ch_mem 渗透 / pb.run 误用 / array_store 漏用

## 6. 相关文档

- `docs/architecture/adr.md` ADR-043 (CI 集成决策, 2026-06-12)
- `docs/architecture/adr.md` ADR-024 (Bundle 三层分层, ⚠️ Mapper 推迟)
- `docs/architecture/adr.md` ADR-033 (CtrlLink 4-control-API)
- `docs/architecture/adr.md` ADR-040 (TLM→HDL 移植性约束)
- `.omo/drafts/fix-plan-2026-06-12-design.md` §3 Change 4 (CI 集成设计依据)

