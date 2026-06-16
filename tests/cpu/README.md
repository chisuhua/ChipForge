# CPU IP 业务测试

> **家族**: CPU IP 业务 (M1-M5 实施)
> **数量**: 1 个测试 (M1 收官时)
> **预测增长**: M2 = +5, M3 = +6, M4 = +2, M5 = +1 → 共 14+ 个测试

## 1. 测试列表

| 文件 | 测什么 | Phase | 备注 |
|------|--------|-------|------|
| `test_payload_common.cpp` | `ip/cpu/core/payload_common.h` 8 Payload Key + DecodePayload | **1.5 M1.7** | 🆕 M1 收官新增 |

## 2. M2-M5 增长预测

| 阶段 | 新增测试 | 路径示例 |
|------|----------|----------|
| **M2** | RegFilePlugin / HazardPlugin / IBusPlugin / DBusPlugin / BranchPredictorPlugin | `tests/cpu/test_reg_file.cpp` `test_hazard.cpp` 等 |
| **M3** | RiscvDecodePlugin / RiscvIntAluPlugin / RiscvMulPlugin / RiscvBranchPlugin / RiscvLsuPlugin / RiscvCsrPlugin | `tests/cpu/test_riscv_*.cpp` |
| **M4** | CpuFactory + JSON + 集成测试 (2) | `tests/cpu/test_cpu_factory.cpp` `test_5stage_riscv.cpp` 等 |
| **M5** | 联调测试 | `tests/cpu/test_demo_soc.cpp` |

## 3. 命名约定

- `test_<plugin_name>.cpp` 测单个 Plugin (Level A 单元测试)
- `test_<feature>.cpp` 测 ISA 特性 (如 `test_rv32i_decode.cpp`)
- `test_<integration>.cpp` 测集成 (如 `test_5stage_riscv.cpp`)

## 4. 与 IP 测试的边界

- **IP 自身测试** (L1CachePlugin) 在 `tests/cache/`
- **CPU IP 测试** (本目录) 与 cache 测试是**两个独立家族**
- CPU 业务测试可能 mock cache (用 M5 联调时), 不直接依赖 L1CachePlugin

## 相关文档

- **M1 实施计划**: `ip/cpu/docs/implementation-plan/M1-cpu-skeleton.md`
- **总体实施规划**: `ip/cpu/docs/implementation-plan/README.md`
- **M2-M5 详细**: `ip/cpu/docs/implementation-plan/M[2-5]-*.md`
