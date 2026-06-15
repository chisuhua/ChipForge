# M2 — ISA 无关 Plugin 套件 (5 个 P0)

> **本文件位置**: `ip/cpu/docs/implementation-plan/M2-core-plugins.md`
> **状态**: 🟡 待启动 (依赖 M1 完成)
> **估算**: 2-3 d
> **总体任务清单**: 见 [`README.md` §6 M2 行](README.md)

## 1. 目标

实施 5 个 ISA 无关 Plugin (RegFile / Hazard / IBus / DBus / BranchPredictor) + 3 个 P3+ 占位 Plugin (FPU / MMU / Exception)。所有 Plugin 写入 `ip/cpu/plugins/`。这些 Plugin 不绑定 RISC-V, 未来 ARM 复用零修改。

## 2. 任务清单

| # | 任务 | 文件 | 验收 | 估时 |
|---|------|------|------|------|
| **M2.1** | 实施 `RegFilePlugin` (议题 3 选 B+C: array_store 模板参数化 xlen) | `ip/cpu/plugins/reg_file.h` + `.cpp` | test_reg_file 4-6 用例 PASS | 0.7d |
| **M2.2** | 实施 `HazardPlugin` (RAW/WAW/WAR 检测, CtrlLink.halt_when) | `ip/cpu/plugins/hazard.h` + `.cpp` | test_hazard 4-6 用例 PASS | 0.7d |
| **M2.3** | 实施 `BranchPredictorPlugin` (P1: BTB / Bimodal / GShare) | `ip/cpu/plugins/branch_predictor.h` + `.cpp` | test_branch_predictor 4-6 用例 PASS | 0.5d |
| **M2.4** | 实施 `IBusPlugin` (取指总线, MemReqBundle 外发 + INSTRUCTION 回收) | `ip/cpu/plugins/ibus.h` + `.cpp` | test_ibus 4-6 用例 PASS | 0.4d |
| **M2.5** | 实施 `DBusPlugin` (数据总线, LSU_REQ 外发 + LSU_RESP 回收) | `ip/cpu/plugins/dbus.h` + `.cpp` | test_dbus 4-6 用例 PASS | 0.4d |
| **M2.6** | 创建 `fpu.h` P3+ 占位 (.cpp 写 `// TODO: M3+`) | `ip/cpu/plugins/fpu.h` + `.cpp` | 占位存在 | 0.05d |
| **M2.7** | 创建 `mmu.h` P3+ 占位 | `ip/cpu/plugins/mmu.h` + `.cpp` | 占位存在 | 0.05d |
| **M2.8** | 创建 `exception.h` P3+ 占位 | `ip/cpu/plugins/exception.h` + `.cpp` | 占位存在 | 0.05d |
| **M2.9** | 5/5 P0 单元测试 PASS (4-6 用例/Plugin) | `ip/cpu/plugins/tests/unit/` | ctest 5/5 PASS | (累计) |

## 3. 依赖

- ✅ M1 完成 (cf_plugin 扩展点 + DecodePayload + 通用 Payload Key)

## 4. 完成判据

- [ ] M2.1-M2.5 全部 5 个 P0 Plugin 代码 + 单元测试 commit
- [ ] M2.6-M2.8 全部 3 个 P3+ 占位文件存在
- [ ] M2.9: ctest 5/5 P0 单元测试 PASS
- [ ] 16/16 ctest 全局不退化

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| `HazardPlugin` 数据冒险检测逻辑复杂 | 复用 L1Cache 6 维度方法学 D2 范式合规; 引用 multi_isa v2.0 §5.3 RAW/WAW/WAR 定义 |
| `BranchPredictorPlugin` P1 实施范围不明确 (本期仅 stub?) | 议题 2 选 B: P1 必做, 不只是 stub; 测试覆盖 bimodal / gshare 切换 |
| `IBusPlugin` / `DBusPlugin` 与 cf::bundles::MemReq/MemResp 集成 | M2 启动前确认 bundles 接口; 若变动需修订 M1.7 的 payload_common.h |

## 6. 任务编号约定

`M2.x` 其中 x = 1..9 (与本文件 §2 表格 # 列对应)

## 相关文档

- 总体任务: [`README.md`](README.md) §6 M2 行
- Plugin 套件: [`../blueprint.md`](../blueprint.md) §4.1
- payload_common.h (M1 产出): `ip/cpu/core/payload_common.h`
- 任务状态: [`../status.md`](../status.md)
