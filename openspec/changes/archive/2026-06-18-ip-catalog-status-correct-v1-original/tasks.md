# Tasks: ip-catalog-status-correct

> **总工时**: ~3h
> **执行模式**: git worktree `change/ip-catalog-status-correct`
> **依赖**: CHANGE-001 (doc-code-realignment) 必须先完成

## 1. 重写 IP 状态表

- [ ] 1.1 在 `docs/architecture/ip-catalog.md` 表格增加"实现范围"列（8 个 IP 全部填写）
- [ ] 1.2 修正 L1Cache 状态为 "🟡 TLM 实现中 (Phase 1.3, L1 unified direct-mapped 32KB, L1I/L1D/L2 未拆分)"
- [ ] 1.3 5 个零代码 IP（memory/interconnect/peripheral/tilecore/tilecopy）的"实现范围"列填 "0 LOC, 仅 README + .gitkeep"
- [ ] 1.4 在表格增加"实施预计"列，零代码 IP 指向对应 roadmap 路径
- [ ] 1.5 验证 `grep -c "实施预计" /workspace/project/ChipForge/docs/architecture/ip-catalog.md` ≥ 1
- [ ] 1.6 验证 `grep -c "实现范围" /workspace/project/ChipForge/docs/architecture/ip-catalog.md` ≥ 1

## 2. 创建 IP README 模板

- [ ] 2.1 创建 `docs/templates/IP_README_TEMPLATE.md`（含 实现状态/实施预计/依赖/测试/已知限制 5 段）
- [ ] 2.2 在 `ip/README.md` 顶部加链接指向模板："新 IP 须遵循 `docs/templates/IP_README_TEMPLATE.md`"

## 3. 同步现有 IP README

- [ ] 3.1 检查 7 个 IP README (`ip/cpu/README.md`, `ip/cache/README.md`, `ip/memory/README.md`, `ip/interconnect/README.md`, `ip/peripheral/README.md`, `ip/tilecore/README.md`, `ip/tilecopy/README.md`) 是否有"实现状态"和"实施预计"段
- [ ] 3.2 缺段的 IP README 补充相应段（每个 IP ~10 分钟）

## 4. 更新验证基线

- [ ] 4.1 在 `CHANGELOG.md` 顶部添加 v0.0.3 条目
- [ ] 4.2 验证 `tools/verify_no_ghost_refs.sh` 仍 PASS
- [ ] 4.3 验证 `tools/verify_adr.sh` 仍 PASS
- [ ] 4.4 验证现有 ctest 不破坏（仅修改文档，无需重 build）

## 5. PR 准备

- [ ] 5.1 `git diff --stat` 报告变更规模
- [ ] 5.2 PR description 引用 CHANGE-001 + CHANGE-002 的依赖关系
- [ ] 5.3 Request review
