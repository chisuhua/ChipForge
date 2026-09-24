## ADDED Requirements

### Requirement: ADR-037 v2.0 修订

ADR-037 "Plugin 作为设计范式" MUST 修订为 v2.0, 记录 D4 范式在 elaboration 语义下兑现。

#### Scenario: ADR-037 v2.0 内容

- **WHEN** 起草 `docs/architecture/adr/ADR-037-plugin-as-design-paradigm.md` v2.0
- **THEN** 新增章节: §v2 翻转摘要 (7 大借鉴点 SpinalHDL/VexRiscv/CppHDL → cf::plugin) + §elaboration 纪律 (8 项 CI 检查清单) + §CH_MEM 模式契约 (双文件分离 + `uint_t<N>` 双模 + `array_store` 双缓冲 + `CtrlLink` ch_bool)

#### Scenario: docs/architecture/adr.md 注册表更新

- **WHEN** `docs/architecture/adr.md` ADR-037 注册行更新
- **THEN** 版本字段改 v2.0, 关联 ADR-040 v2.0 + ADR-046

#### Scenario: cross-reference 一致性

- **WHEN** `bash tools/verify_adr.sh` 跑
- **THEN** ADR-037 v2.0 PASS, cross-reference 无 broken
