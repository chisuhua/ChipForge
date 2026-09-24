## Context

2026-06-17 架构对齐审查（详见 `code-framework-mapping.md §7.4` 与本 change proposal）发现 ChipForge 文档/代码存在 8 类典型漂移，其中 3 项 CRITICAL（误导用户）。本 change 的设计目标是：**在不破坏已工作代码的前提下，将"文档/代码一致性"从 50% 提升到 80%+**，并以 CI 脚本固化未来防漂移机制。

**当前状态**：
- 框架层（CppTLM/CppHDL 集成 + Phase 0 Plugin 脚手架）已成熟，1500+ LOC 业务代码 + 51 单元测试 100% PASS
- 应用层（IP 库 + SoC 装配）仅 5% 完成，7 个 IP 中 5 个零代码
- 文档层有业界领先的"自审文化"（`code-framework-mapping.md §7.4` 主动列出 8 项背离），但缺修复机制
- `soc/riscv_virt.json` 引用 7 个不存在类，全仓 grep 0 匹配
- `ip/cpu/cpu_factory.cpp` 是 12 行 stub，无可调用的工厂
- D4 决策（Plugin-style 强制）实施后未留下 Bridge 模式合法性的 ADR 记录

**约束**：
- 已落地的 L1CachePlugin 业务代码必须保持可工作（Phase 1.3 e2e 测试 5/5 PASS）
- 已注册的 CppTLM 标准模块（CacheTLM/MemoryTLM/CrossbarTLM/CPUTLM 等）名不能改（受 `chstream_register.hh` 约束）
- `bundles/mem_bundles.h` 的 POD + `uint_t<N>` 设计（D4 合规）必须保留
- OpenSpec 是项目既定流程，需在 worktree 中执行

**利益相关者**：
- 未来新人：消除最大误导源（`riscv_virt.json`）
- IP 开发者：获得明确"业务 Plugin vs Bridge 适配"边界（ADR-041）
- CI 维护者：获得 grep 检查脚本

## Goals / Non-Goals

**Goals:**
- 删除 2 个不可工作文件（`riscv_virt.json` 幽灵 SoC + `cpu_factory.cpp` 12 行 stub）
- 重写 3 个文档段落（overview §"SoC 层" + §"ch_stream ISA 无关层" + soc/README.md）以反映真实状态
- 跨 7 文档清理 8 个幽灵类名（`RiscvIssTlm`/`L1CacheTlm`/...），统一改用 Plugin 风格命名
- 新增 ADR-041 明确 Bridge 模式合法性，消除 A5 战略债务
- 新增 CI 脚本 `tools/verify_no_ghost_refs.sh` 固化防漂移机制
- 更新 `code-framework-mapping.md §7.4` 把 8 项漂移从"待修复"移到"已修复"

**Non-Goals:**
- **不**实现 `RiscvIssTlm` / `L1CacheTlm` / `BusMatrixTlm` / `DramTlm` / `UartTlm` / `ClintTlm` / `PlicTlm` / `RiscvCoreRtl` 等幽灵类（推迟到 Phase 2+）
- **不**实现 `ImplMode` 枚举（推迟到 Phase 6）
- **不**实现 `RiscvVirtSoC.h/cpp` 装配类（推迟到 Phase 2）
- **不**改 `bundles/mem_bundles.h` 的 POD 设计（D4 合规已定）
- **不**改 `L1CachePlugin` / `L1CacheTLMBridge` / `L1CacheTLMBridgeAdapter` 的现有实现
- **不**改 CppTLM/CppHDL 框架层

## Decisions

### Decision 1: 直接删除 vs 重写 `riscv_virt.json`

**选择**: **直接删除**

**理由**:
- 该 JSON 当前不可运行（引用 7 个不存在类），任何按 JSON 跑的尝试都是错误示范
- 已存在 2 个可工作的 L1Cache 配置（`l1_cache_minimal.json` + `l1_cache_adapter_e2e.json`）作为新模板
- "重写为 Plugin 风格示例" 路径会引入 ~100 行新 JSON 配置，与 CHANGE-002（ip-catalog 状态修正）的工作重复

**替代方案**:
- ❌ 重写为可工作 JSON：会扩大本 change 范围，与其他 change 重叠
- ❌ 标记为 DEPRECATED 保留：保留误导源，不符合"零幽灵引用"目标

### Decision 2: 8 个幽灵类名的处理粒度

**选择**: **统一替换为 Plugin 风格命名 + 删除虚构示例**

**规则**:
| 旧名 | 新表述 | 出现文档 |
|------|--------|----------|
| `RiscvIssTlm` | "RISC-V CPU IP (Phase 2+ 实施，当前仅 `cpu_factory.h` 声明)" | overview §"SoC 层", riscv_virt.json |
| `L1CacheTlm` | `L1CachePlugin` (Phase 1.3 已实现) + `L1CacheTLMBridgeAdapter` (cpptlm 适配) | overview, riscv_virt.json, ip-catalog |
| `BusMatrixTlm` | "CrossbarTLM (4 端口, CppTLM 标准)" | overview, riscv_virt.json |
| `DramTlm` | "MemoryTLM (通用内存模型，非 DRAM 控制器)" | overview, riscv_virt.json, declarative-hybrid |
| `UartTlm`/`ClintTlm`/`PlicTlm` | "Phase 3+ 实施"（见 ip-catalog） | overview, riscv_virt.json, ip-catalog |
| `RiscvCoreRtl` | "Phase 5 RTL 实施（待 CppHDL 集成 enabled 后）" | overview, riscv_virt.json |

**理由**:
- `L1CacheTlm` 的 L1CachePlugin 实际存在，命名应反映现实
- 8 个类名在 7 个文档中出现，需要 grep 全仓替换（不能漏）
- 部分类名（`UartTlm` 等）当前不存在，应明示"未实现"

**替代方案**:
- ❌ 在每个文档单独加 TODO 注释：散落难维护
- ❌ 创建重命名映射表 + 全仓 alias：增加复杂度，本阶段不需要

### Decision 3: ADR-041 位置与命名

**选择**: **新建独立 ADR 文件 `docs/architecture/adr/ADR-041-bridge-tick-pattern.md`**

**理由**:
- `adr.md` 主表已有 40 条 ADR，新增第 41 条自然延续命名
- `adr/ADR-040-tlm-hdl-portability-constraints.md` 是独立文件模式，遵循同一模式
- 内容聚焦"业务 Plugin vs Bridge 适配"边界，与 D4 决策互补而非冲突

**替代方案**:
- ❌ 合并到 ADR-037 (Plugin 作为范式)：会让 ADR-037 过长
- ❌ 不写 ADR，只在代码注释说明：失去 ADR 制度的可追溯性

### Decision 4: CI 验证脚本的范围

**选择**: **新增 `tools/verify_no_ghost_refs.sh` 单独脚本**

**理由**:
- 现有 `tools/verify_adr.sh` 验证 ADR-XXX 的代码路径，与"幽灵引用"是不同维度
- 单独脚本便于单独运行（不依赖完整 ADR 验证流程）
- 未来 ghost ref 列表扩展时，单独脚本更易维护

**替代方案**:
- ❌ 把 ghost ref 检查合并到 `verify_adr.sh`：会改变 `verify_adr.sh` 的职责
- ❌ 写在 pre-commit hook：依赖开发者本地配置，CI 没法强制

## Risks / Trade-offs

**[Risk 1]** 删除 `riscv_virt.json` 后，未来 RISC-V virt SoC 配置从哪里开始？
→ **Mitigation**: 在 `soc/README.md` 顶部加链接指向 `l1_cache_minimal.json` 作为"当前工作模板"，并说明 RISC-V virt 推迟到 Phase 2+ 实施。

**[Risk 2]** 删除 `cpu_factory.cpp` 后，CPU IP 测试还能跑吗？
→ **Mitigation**: 检查 `tests/cpu/` 全部 .cpp 文件，确认无 #include 引用 `cpu_factory.cpp`（只引用 `cpu_factory.h`）；如果有任何 .cpp 引用，迁移到新位置。

**[Risk 3]** 跨 7 文档 grep 替换幽灵类名时，可能误改有效引用。
→ **Mitigation**: 在每次替换前先 `grep -l` 列出所有出现位置 + 人工 review 每个 hit，确保替换安全；保留原行加 "(未实现，Phase X+ 实施)" 标记而非整段删除。

**[Risk 4]** CI 脚本 `verify_no_ghost_refs.sh` 误报（如 `L1Cache` 子串匹配 `L1CacheTlm`）。
→ **Mitigation**: 使用 `\b` 词边界正则 + 显式完整类名列表（8 个）而非前缀匹配。

**[Risk 5]** ADR-041 与 D4 决策看起来矛盾（"禁止 tick"vs"允许 Bridge tick"）。
→ **Mitigation**: 在 ADR-041 显式说明"D4 适用于业务 IP（Plugin-style），Bridge 适配层是不同抽象层级"，并引用 ADR-025 (Plugin 基类无 tick) 作为对照。

## Migration Plan

无运行时迁移——本 change 仅修改文档和删除不可运行文件。

**部署步骤**:
1. 在 git worktree `change/doc-code-realignment` 中执行
2. 提交顺序：
   - Commit 1: 删除 `riscv_virt.json` + `cpu_factory.cpp`（先消除最大误导源）
   - Commit 2: 重写 3 个文档段落（overview §"SoC 层" + §"ch_stream ISA 无关层" + soc/README.md）
   - Commit 3: 跨 7 文档 grep 替换幽灵类名
   - Commit 4: 新增 ADR-041 + 更新 adr.md
   - Commit 5: 新增 CI 脚本 `tools/verify_no_ghost_refs.sh`
   - Commit 6: 更新 `code-framework-mapping.md §7.4` + `CHANGELOG.md`
3. PR 评审：用户 + 1 reviewer
4. 合并到主分支
5. CI 验证：grep 脚本应 PASS（0 幽灵引用）

**回滚策略**:
- 6 个 commit 可逐个 `git revert`，无运行时依赖
- 删除的 `riscv_virt.json` 和 `cpu_factory.cpp` 之前不可工作，回滚后再次产生漂移 → 不可行（仅可作为"留底"）

## Open Questions

1. `tools/verify_no_ghost_refs.sh` 是否加入 `.github/workflows/` 作为 PR check？ → 建议是，但留到本 change 完成后由 CI owner 决定
2. `RiscvVirtSoC.h/cpp` 描述是否在 `overview.md` 整段删除，还是改为"Phase 2 规划"占位？ → **本 change 决定**：改为"占位段 + 指向 CHANGE-002 的 IP 状态表"
3. 7 文档 grep 替换时，是否保留 `L1CacheTlm` 字符串作为"历史命名"附注？ → **本 change 决定**：**不保留**（避免命名分歧），直接用 `L1CachePlugin`（实际类名）
