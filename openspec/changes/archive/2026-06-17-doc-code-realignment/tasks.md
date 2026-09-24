# Tasks: doc-code-realignment

> **总工时**: ~8h
> **执行模式**: git worktree `change/doc-code-realignment`
> **依赖**: 无（独立 change）

## 1. 删除幽灵文件（消除最大误导源）

- [x] 1.1 删除 `soc/riscv_virt.json`（7 个悬挂类引用 + impl_mode 字段无消费者）
- [x] 1.2 删除 `ip/cpu/cpu_factory.cpp`（12 行 stub，保留 `.h` 声明）
- [x] 1.3 验证删除后 `grep -rn "RiscvIssTlm\|LiscvCacheTlm\|BusMatrixTlm" /workspace/project/ChipForge --include="*.json"` 返回 0 行

## 2. 重写 overview.md 关键段落

- [x] 2.1 重写 `docs/architecture/overview.md` §"SoC 层是 IP 组合器" 段落（删除 `RiscvVirtSoC.h/cpp` 描述 + `REGISTER_MODULE` 示例，改为指向 `soc/l1_cache_minimal.json` 真实工作示例）
- [x] 2.2 重写 `docs/architecture/overview.md` §"ch_stream 接口即 ISA 无关层" 段落（改为 "Phase 1.4+ Future Work" 占位段，加 `> ⚠️ 推迟到 Phase 1.4+` 标记）
- [x] 2.3 验证 `grep -nE "RiscvVirtSoC\.h/cpp|RiscvIssTlm" /workspace/project/ChipForge/docs/architecture/overview.md` 返回 0 行

## 3. 跨文档幽灵类名清理（grep 全仓替换）

- [x] 3.1 列出所有 8 个幽灵类名的出现位置：`grep -rnE "RiscvIssTlm|L1CacheTlm|BusMatrixTlm|DramTlm|UartTlm|ClintTlm|PlicTlm|RiscvCoreRtl" /workspace/project/ChipForge --include="*.md" --include="*.json" --include="*.h" --include="*.cpp" 2>/dev/null`
- [x] 3.2 人工 review 3.1 列出的所有 hit，逐个替换为 Plugin 风格命名（`L1CachePlugin` / `L1CacheTLMBridgeAdapter` 等）或删除段落
- [x] 3.3 验证 `grep -rnE "RiscvIssTlm|L1CacheTlm|BusMatrixTlm|DramTlm|UartTlm|ClintTlm|PlicTlm|RiscvCoreRtl" /workspace/project/ChipForge --include="*.md" --include="*.json" --include="*.h" --include="*.cpp" 2>/dev/null | grep -v "/openspec/changes/" | grep -v "CHANGELOG.md"` 返回 0 行

## 4. 同步 interface-design.md 和 soc/README.md

- [x] 4.1 在 `docs/architecture/interface-design.md §1.0` 增加"已实现 POD vs 设计目标 bundle_base"对照表（标明 Phase 1 当前状态是 POD，与 §"所有 Bundle 继承 bundle_base" 段落对照）
- [x] 4.2 重写 `soc/README.md` 顶部说明（从"已实现 RISC-V virt"改为"目前 2 个 L1Cache 验证配置；RISC-V virt 推迟到 Phase 2+ 实施"）

## 5. 新增 ADR-041 桥接模式 ADR

- [x] 5.1 创建 `docs/architecture/adr/ADR-041-bridge-tick-pattern.md`（包含 Context / Decision / Consequences / References 4 段，引用 ADR-025 + ADR-037 + ADR-040）
- [x] 5.2 在 `docs/architecture/adr.md §2.1` 插入 ADR-041 摘要（`Accepted` 状态 + 链接到独立文件）
- [x] 5.3 在 `docs/architecture/adr.md §3 G/H 段`交叉引用 ADR-041（业务 Plugin 段落引用 ADR-025/037，Bridge 段落引用 ADR-041）

## 6. 新增 CI 防漂移脚本

- [x] 6.1 创建 `tools/verify_no_ghost_refs.sh`（可执行权限 755 + bash 严格模式 + 8 个类名 grep + 排除 `openspec/changes/` 和 `CHANGELOG.md`）
- [x] 6.2 验证脚本当前应返回 exit 0（3.3 已清理完成）
- [x] 6.3 验证脚本可执行：`chmod +x tools/verify_no_ghost_refs.sh` + `./tools/verify_no_ghost_refs.sh && echo "PASS" || echo "FAIL"`

## 7. 更新验证基线

- [x] 7.1 更新 `docs/architecture/code-framework-mapping.md §7.4` 漂移表（8 项从"待修复"移到"§7.6 已修复 (2026-06-17)"）
- [x] 7.2 更新 `code-framework-mapping.md §7.5 建议的修正优先级`（item 1-4 标"✅ 完成 2026-06-17"）
- [x] 7.3 在 `CHANGELOG.md` 顶部添加 v0.0.2 条目：`## v0.0.2 (2026-06-17) - doc-code-realignment` 列出 8 项漂移修复

## 8. 最终验证

- [x] 8.1 完整运行 `tools/verify_no_ghost_refs.sh` 应返回 exit 0
- [x] 8.2 完整运行 `tools/verify_adr.sh` 应返回 exit 0（确认 ADR-001~040 验证未破坏）
- [x] 8.3 完整运行 `cmake --build build && ctest --test-dir build --output-on-failure` 应保持 51 plugin + 4 L1Cache 测试 PASS
- [x] 8.4 `git status` 应仅显示预期变更（删除 2 文件 + 修改 ~7 文档 + 新增 3 文件）
- [x] 8.5 `git diff --stat` 报告变更规模
