# ChipForge 文档迁移日志（MIGRATION_LOG）

> **目的**: 记录项目**文档结构 / 职责 / 衔接关系**的变更, 而非代码变更（代码变更走 git log + CHANGELOG.md）。
>
> **何时更新**: 任何"读哪个文件"的导航规则发生变化时（如拆出/合并文档、新增/删除 phase doc、文档职责重定义）。
>
> **谁负责**: 新会话发现导航困惑时, 优先看本文件 → 找到最近的迁移原因 → 知道为什么当前结构是这样的。

---

## 2026-09-28: 架构演进内容统一化 + 文档结构分叉明确化

### 背景

会话开始时新会话 agent 面临两个困惑:
1. **两个 `execution-roadmap.md`** 各自讲什么? 哪个是主控?
2. **后续阶段（Phase 6d → v1.3.0）的架构演进**在哪里? 没有可视化 SSOT

### 变更内容（4 项）

#### 1. `soc/cpu/docs/roadmap/execution-roadmap.md` 新增 §3.5 架构演进章节

- **位置**: §3.4 后 / §4 前
- **范围**: 覆盖 v0.10.0 / v1.0.0 / v1.2.0 / v1.3.0 四版本节点
- **内容**: 7 个子章节（演进全景 + 4 个版本节点架构图 + 对比表 + 文档衔接）
- **影响**: SoC 级 execution-roadmap 从"文本驱动"升级为"文本+视觉双驱动"

| 子章节 | 内容 |
|--------|------|
| §3.5.1 | 4 版本节点时间线 + 关键架构能力栈 |
| §3.5.2 | v0.10.0 架构图（ADR-082 + MUL/DIV FSM + RV32C + ICache） |
| §3.5.3 | v1.0.0 架构图（S/U + AMO + BTB + PLIC/CLINT） |
| §3.5.4 | v1.2.0 架构图（4-way RRIP + PMP + Spike lockstep） |
| §3.5.5 | v1.3.0 架构图（Linux-on-FPGA + chip-selector） |
| §3.5.6 | 4 版本节点架构演进对比表（13 维度） |
| §3.5.7 | 与现有文档衔接（ADR + 章节锚点） |

#### 2. `docs/roadmap/strategy/execution-roadmap.md` 新增 §6 架构演进章节

- **位置**: §5 待启动决策点后 / §7 关联文档前
- **范围**: 覆盖 Phase 6d → v0.8.0 → v0.9.0
- **章节重编号**: 原 §6 关联文档 → §7, 原 §7 变更日志 → §8
- **内容**: 6 个子章节（演进全景 + 3 个阶段架构图 + 4 个并行轨道 + 对比表 + 文档衔接）

| 子章节 | 内容 |
|--------|------|
| §6.1 | Phase 6d → v0.8.0 → v0.9.0 架构演进全景 |
| §6.2 | Phase 6d 架构图（5-stage Pipeline CH_MEM + Verilator + MMU FSM） |
| §6.3 | v0.8.0 架构图（ADR-049 PADDR 真消费 + cycle-precision + MUL/DIV FSM） |
| §6.3.1-6.3.4 | 4 个并行轨道详细分解（P1#3 / P1#4 / P1#5 supersede / P1#6） |
| §6.4 | v0.9.0 架构图（CSR + exception + 4-way Cache + mispredict） |
| §6.5 | Phase 6d → v0.8.0 → v0.9.0 架构演进对比表（12 维度） |
| §6.6 | 与现有文档衔接（ADR + 章节锚点） |

#### 3. `soc/cpu/docs/roadmap/README.md` 增强导航

- **新增**: "这个目录解决什么问题"快速导航表（新会话第一件事）
- **新增**: §3.5 子章节内容索引
- **新增**: "与框架级 execution-roadmap 的区别"对照表 + 记忆口诀
- **新增**: 指向本文件的链接（`../../../../docs/MIGRATION_LOG.md`）

#### 4. 顶部元信息更新

- `soc/cpu/docs/roadmap/README.md`: 最后更新日期 2026-09-28
- `docs/roadmap/strategy/execution-roadmap.md`: 头部"Status"字段标注 2026-09-28 新增 §5/§6
- 关联 `soc/cpu/docs/roadmap/execution-roadmap.md` 作为"下游衔接"列入顶部

### 为什么需要 4 项变更（而不是只做 1 项）

**架构演进的可视化需求**驱动**文档结构明确化需求**:

1. 用户最初要求"把架构演进内容写入文档" → 我必须先决定**写到哪里**
2. 发现项目里有**两个** `execution-roadmap.md` → 必须先明确**各自职责**
3. 没有 README.md 级别的"读哪个文件"导航 → 新会话会困惑
4. 没有 MIGRATION_LOG.md → 未来的"为什么是这样"问题无法溯源

**触发链**: 需求 → 结构澄清 → 导航增强 → 迁移记录

### 决策记录

| 决策点 | 选项 | 选中 | 理由 |
|--------|------|------|------|
| 文档分叉处理 | (A) 合并两个 execution-roadmap / (B) 维持分叉 + 明确衔接 / (C) 重命名其中一个 | **(B) 维持分叉 + 明确衔接** | 两个文件的问题域（框架 vs SoC）确实不同，分叉合理；§3.5 / §6 衔接 + README 对照表足够澄清 |
| Phase 文档拆分粒度 | (A) 每个版本独立 phase doc / (B) 仅 Phase 6d 等已确定阶段独立 / (C) 全部留 execution-roadmap | **(B) 仅已确定阶段独立** | 全部独立会变成 6+ 文档维护负担；预测式文档会腐烂 |
| 导航表放哪里 | (A) AGENTS.md / (B) soc/cpu/docs/roadmap/README.md / (C) 顶层 docs/roadmap/README.md | **(B)+(C)** | 局部导航放局部 README，全局导航放全局 README |
| MIGRATION_LOG 创建位置 | (A) docs/MIGRATION_LOG.md / (B) docs/roadmap/MIGRATION_LOG.md | **(A) docs/MIGRATION_LOG.md** | 顶层可被所有子目录引用 |

### 验证结果

| 验证项 | 结果 |
|--------|------|
| `doc_link_check.sh` 修复后 | 我新增的 5 个链接全部修复（22 → 17 失效链接），剩余 17 个是**预先存在**的失效链接（历史归档文档路径漂移，非本次引入） |
| 章节结构（§3.5 插入位置） | ✅ 正确（§3.4 后 / §4 前） |
| 章节结构（§6 插入位置） | ✅ 正确（§5 后 / §7 前） |
| 章节编号连续性 | ✅ §1-§8 连续，无重复 |
| ADR-040/046/047/048/049/082 引用 | ✅ 全部 6 个 ADR 文件存在 |
| ADR 相对路径 | ✅ 全部有效（`../../architecture/adr/...`） |
| 跨文档锚点 | ✅ 符合 GitHub anchor 规则 |

### 未来会话遇到这个变更怎么办

**如果你读到本文件发现某些链接指向不存在的文档**:
- 大概率是文档已迁移到 `archive/` 子目录（如 `soc/cpu/docs/roadmap/archive/phase-1-tlm-foundation.md`）
- 检查 `soc/cpu/docs/roadmap/archive/` 是否有归档版本
- 检查 `docs/roadmap/README.md` 是否有等价 phase doc

**如果你想新增一个文档到路线结构**:
1. 先确定: 是"框架级"还是"SoC 级"? 这决定放 `docs/roadmap/` 还是 `soc/cpu/docs/roadmap/`
2. 如果是"已确定阶段"（有 OpenSpec change + tasks.md）→ 独立成 `phases/phase-X-name.md`
3. 如果是"还在规划阶段"（仅 PoC 列表）→ 留在 execution-roadmap.md 里
4. 任何变更必须更新本文件 + AGENTS.md

---

## 变更历史

| 日期 | 版本 | 变更摘要 | 作者 |
|------|------|---------|------|
| 2026-09-28 | v1.0 | 初版：4 项变更（§3.5 / §6 / README 增强 / 元信息更新）+ 决策记录 + 验证结果 | Sisyphus (per user request) |

---

## 模板（下次新增变更时复制）

```markdown
## YYYY-MM-DD: [变更标题]

### 背景

[什么驱动了这个变更]

### 变更内容

#### 1. [文档 A 变更]
- 位置: ...
- 范围: ...
- 内容: ...

#### 2. [文档 B 变更]
...

### 为什么需要 [N] 项变更

[触发链说明]

### 决策记录

| 决策点 | 选项 | 选中 | 理由 |
|--------|------|------|------|

### 验证结果

| 验证项 | 结果 |
|--------|------|

### 未来会话遇到这个变更怎么办

[预期问题 + 解决方法]
```
