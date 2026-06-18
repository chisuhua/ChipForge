# IP STATUS 模板

> **版本**: 1.0 (2026-06-17)
> **来源**: CHANGE-005 (empty-directory-cleanup)
> **适用范围**: 所有 `ip/<name>/STATUS.md` 文件

每个 IP 根目录应有 `STATUS.md` 标明当前状态。本模板提供 **2 个变体**：

1. **PLANNED 变体**（默认）：零代码 IP 使用
2. **INITIAL DESIGN 变体**：有设计文档但无源码的 IP 使用（tilecore/tilecopy 等）

---

## 变体 1: PLANNED (0 LOC)

适用于**完全无源码**的 IP（memory/interconnect/peripheral 等）。

```markdown
# STATUS: PLANNED (0 LOC)

This IP is **planned but not yet implemented**. No source code exists.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PLANNED variant) -->

## Implementation Roadmap
- 实施预计: Phase X+ (见 docs/roadmap/phases/phase-X-name.md)
- 依赖: (从 ip/README.md 标准结构 / ip-catalog.md)
- 状态: 🔴 规划中
- (可选) 子模块: 列出未来要实施的子模块

## 已知限制
- 所有子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- 当前 IP 根仅含 `README.md` (无任何源码)
- Phase Y 之前不会有任何实施
```

---

## 变体 2: INITIAL DESIGN (0 LOC src, design docs exist)

适用于**有设计文档但无源码**的 IP（tilecore/tilecopy 等）。

```markdown
# STATUS: INITIAL DESIGN (0 LOC src, design docs exist)

This IP has **architectural documentation** but no source code yet.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (INITIAL DESIGN variant) -->

## Existing Assets
- 设计文档: `docs/architecture.md` (XX KB, 简要描述)
- 政策框架: `policies/` (空目录, 待实施)

## Implementation Roadmap
- 实施预计: Phase X+ (见 docs/roadmap/phases/phase-X-name.md)
- 依赖: (从 ip-catalog.md)
- 状态: 🟡 初始设计
- 角色: (从 ip-catalog.md)

## 已知限制
- 所有源码子目录 (`tlm/`/`rtl/`/`test/`/`configs/`) 待实施时创建
- 当前 IP 根仅含 `README.md` + `docs/architecture.md` (无任何源码)
```

---

## 变体 3: PARTIAL (有部分实现)

适用于**已开始实施**的 IP（cache 已实现 L1 unified 但 L1I/L1D/L2 未拆分）。

```markdown
# STATUS: PARTIAL (N LOC, X 已实现 / Y 未实现)

This IP has **partial implementation**.

<!-- Conforms to: docs/templates/IP_STATUS_TEMPLATE.md (PARTIAL variant) -->

## Existing Assets
- 实现文件: (列出 .h/.cpp 文件路径)
- 单元测试: (列出 test 路径)
- LOC: N

## Implementation Roadmap
- 下一里程碑: (e.g., "L2 Cache Phase 1.5")
- 待实现: (列出未实现的功能)

## 已知限制
- (列出当前实现的限制)
```

---

## 状态变更流程

IP 实施状态变化时（PLANNED → PARTIAL → 稳定）：

1. **更新 `STATUS.md`**：从当前变体替换为目标变体
2. **更新 `docs/architecture/ip-catalog.md`**：状态标记变化
3. **更新 `CHANGELOG.md`**：记录状态变更
4. **在 PR 中显式说明**：状态变更是可见的重大变更

## 不变式

- `STATUS.md` 永远位于 IP 根目录（与 `README.md` 同级）
- 状态变体只能是 PLANNED / INITIAL DESIGN / PARTIAL / 稳定 之一
- 状态变更必须与代码现状一致（无虚假声明"已稳定"）

---

*本模板由 CHANGE-005 (empty-directory-cleanup) 创建 (2026-06-17)*
