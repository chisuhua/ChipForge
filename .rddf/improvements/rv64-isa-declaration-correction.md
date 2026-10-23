---
id: rv64-isa-declaration-correction
name: rv64-isa-declaration-correction
status: draft
initiative: wave5-isa-coverage-and-bp
priority: P0
phase: phase-4
category: core-impl
type: fix
manual_deps: []
review_by: 2026-10-30
主题: RV64 ISA 声明真实性 + XLEN 就绪度门禁 (ADR-087 实施)
依赖: ADR-087
来源: ADR-087 §Implementation Plan Change 1
生成时间: 2026-10-23
---

# rv64-isa-declaration-correction

**主题**: RV64 ISA 声明真实性 + XLEN 就绪度门禁 (ADR-087 实施)

> 对应 [ADR-087](../../docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md) §D1 / §D2 的 **Change 1**（声明纠偏）。
> 缺口证据 SSOT：[`ip/cpu/docs/rv64_readiness_gap.md`](../../ip/cpu/docs/rv64_readiness_gap.md)。

## 架构依据

ADR-083（Plugin-style 单一 source of truth）要求"同一份业务 Plugin 经两个编译模式产出不同后端"，其隐含前提是**声明面与实现面一致**。ADR-040 v2.0 已把 CH_MEM 定为"新正道"。当前实现层完全合规（RV32IM + Zicsr(subset)，与路线图一致 —— `.rddf/roadmap/archive/phase-2-baremetal.md:5` 明确 RV64 推迟到 v1.x 之后），**但声明层全线越界**。

实测越界点（HEAD `3775224`）：

| 位置 | 声明 | 实测事实 |
|------|------|---------|
| `ip/cpu/README.md:13` | 「支持 RV32I/RV64GC 指令集」 | 实现覆盖 = RV32I 40 条 + RV32M 8 条 + 8 个 M-mode CSR |
| `ip/cpu/cpu_factory.h:127` | `CPUConfig::isa = "rv64gc"` | **默认值即谎言** |
| `ip/cpu/cpu_factory.h`（全文） | — | **无任何 `isa ==` dispatch**；`T` 由调用方定（`:310` `template <typename T = std::uint32_t>`）⇒ `isa` 是装饰字段 |
| `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar}.json:5` | `"isa": "rv64gc"` | 同上 |
| `ip/cpu/configs/cpu_embedded.json:5` | `"isa": "rv32imac"` | A 扩展零实现 |
| `ip/cpu/configs/cpu_params_schema.json:18-23` | enum 含 rv64i/rv64gc + default rv64gc | **无 `xlen` 属性**；`:204` 的 sv32 条件约束无代码消费 |
| `soc/docs/integration-guide.md:17` | `"isa": "rv64imac"` | 第四种取值，**连 schema enum 都不在** |
| `tools/cpu_sim/main.cpp:16,188` | 显式 `cfg.isa = "rv32i"` | 注释自认「修复既有 `CpuFactory<uint32_t>` vs config.isa="rv64gc" 不一致」⇒ **代码自证** |

**第二重风险（隐患层）**：骨架层真参数化（`payload_common.h:112-120` `keys<T, XLEN>` 支持 64-bit、`reg_file.h:68` 模板化、`stage_link.h:55` 完全参数化、`mem_bundles.h:45-46` 64-bit 地址），但 ISA 语义层写死 RV32（`int_alu.h:92,97,100` 移位掩码 `& 0x1F`、`int_alu.h:93-94` / `branch.h:87-90,115-118` `int32_t` 有符号比较、`mul.h:102-119` MULH = `uint32_t(r >> 32)`、`mul_div_fsm.h:148,231,241,244` radix-2 写死 32 拍）⇒ **切 `CpuFactory<uint64_t>` 会静默产出错误结果，无异常无断言**。

## 范围

**In Scope**:

1. `ip/cpu/cpu_factory.h` —— `CPUConfig::isa` 默认 `rv64gc` → `rv32im`；新增 `std::uint8_t xlen = 32;`；新增 `isa → xlen` 映射 + **运行时 fail-fast** 一致性检查（`isa` 为运行期 `std::string`（`:127`，值来自 JSON 装载），编译期校验不成立；fail 落点在 `load_config()`/`build_cpu()`，形态与 D1 `unsupported` 同）
2. `ip/cpu/configs/cpu_params_schema.json` —— 新增 `xlen` 属性（enum 32/64）；`isa` enum 三态标注（implemented / declared-reserved / unsupported）；default `rv32im`
3. `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar}.json` —— `isa` → `rv32im`
4. `ip/cpu/configs/cpu_embedded.json` —— `rv32imac` → `rv32im`（A 扩展未实现）
5. `tools/cpu_sim/main.cpp` —— 移除 `isa="rv32i"` 显式 pin（`:16` 注释 + `:188` 赋值），改为跟随 config
6. `ip/cpu/plugins/reg_file.cpp` —— 死代码删除（显式实例化 `RegFilePlugin<std::uint64_t>` 却用 `pl::keys_rv32::*`，`:34,47,52,58,69,72`；该文件不在任何 CMakeLists —— 实测 `grep -rn "reg_file" --include=CMakeLists.txt` = 0 命中）
7. **D4 提前落地**（TLM 侧护栏，与 TLM 退役决策正交）—— `ip/cpu/arch/riscv/int_alu.h` / `branch.h` 模板运算入口函数体首行加 `static_assert(sizeof(T) == 4, "RV32 TLM path assumes 32-bit XLEN; RV64 requires the phase-5 block gate see ADR-087 G3")`；**必须放模板入口内**（实例化点触发），不得放文件作用域（否则 include 即误伤）；`mul.h` / `mul_div_fsm.h` 归 Change 2（phase-5）
8. `ip/cpu/docs/rv64_readiness_gap.md` —— 入 `ip/cpu/docs/README.md` 索引
9. `docs/architecture/adr.md` —— ADR-087 注册（已完成，见 §Impact）
10. CI 软门禁 —— "新增 XLEN 假设必须登记缺口表" 提示（warn 级）

**Out of Scope**:

- **不做 `isa → T` 真 dispatch**（留 phase-5，与 `feat-tlm-backend-retirement` B1 合并做 —— 否则要先定 XLEN 单一来源，见 ADR-087 §D3）
- **不做任何 RV64 语义实现**（XLEN 参数化 / 128-bit 乘积 / 64 拍除法 / LD-SD / W 指令 / CSR 64-bit / `misa`-MXL —— 全部属 ADR-087 G1–G9 准入清单，当前 0/9 满足）
- **不扩展 CH_MEM 侧护栏** —— `mul.h` / `mul_div_fsm.h` 的 `static_assert` 与 CH_MEM `kXlenBits = 32` 定义点（**9 处**，含 `ip/mmu/plugins/MMUPlugin_chmem.h:81`）收敛均属 ADR-087 Change 2（phase-5）
- **不改 SoC 拓扑 JSON 的 `isa` 语义**（`soc/docs/integration-guide.md` 仅改示例值）
- **不建 OpenSpec change** —— 由 rdd-builder P0 立项时创建

## Why

**声明面是集成契约，不是注释。** `isa` 字段被 3 个 JSON + schema + SoC 文档 + `tools/cpu_sim` 消费，下游读到的是**值**而非免责声明。`tools/cpu_sim` 已经用 pin `rv32i` 的方式证明了"免责声明挡不住实际误用" —— 它必须绕过配置才能正常工作。

**沉默的失败模式最贵。** 当前如果有人实例化 `CpuFactory<uint64_t>`，**不会编译失败、不会运行报错，只会静默算错**（移位量 ≥32 全错、负数比较有符号截断、MULH 取错半部）。本 change 落地后该风险在两层关闭：编译层（`int_alu.h`/`branch.h` 的 `sizeof(T)==4` 守卫）+ 配置层（`isa`↔`xlen` 运行时 fail-fast）；`mul.h`/`mul_div_fsm.h` 的残余窗口由 ADR-087 Risks **R4** 显式登记，至 Change 2 关闭。

**治理债已经产生实际成本。** `tools/cpu_sim` 的 pin、`reg_file.cpp` 的 `uint64_t` 显式实例化、schema 无 `xlen` 属性 —— 三处都是同一个根因（`isa` 无权威语义）的不同症状。分别打补丁会继续产生第四、第五处。

**与路线图的关系。** RV64 本身推迟到 v1.x 之后是既定决策，本 change **不做 RV64**，只保证"当决策到来时，不会从错误的起点出发"。它也是 `feat-tlm-backend-retirement` 的 Gate 1 前置（G1.5）—— 若在虚假声明未纠正时就删除 TLM，等于把两条语义都不准的实现里更少的那条留下来。

## What Changes

### 变更清单（10 个文件：8 改 + 2 护栏）

| 文件 | 动作 | 具体 |
|------|------|------|
| `ip/cpu/cpu_factory.h` | 改 | `isa` 默认值；新增 `xlen` 字段；新增映射 + **运行时 fail-fast** |
| `ip/cpu/configs/cpu_params_schema.json` | 改 | `xlen` 属性；`isa` 三态标注；default |
| `ip/cpu/configs/cpu_default.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_deep_pipeline.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_superscalar.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_embedded.json` | 改 | `rv32imac` → `rv32im` |
| `tools/cpu_sim/main.cpp` | 改 | 移除 `isa="rv32i"` pin |
| `ip/cpu/plugins/reg_file.cpp` | 删 | 死代码（`uint64_t` 显式实例化 + `keys_rv32::*` 混用） |
| `ip/cpu/arch/riscv/int_alu.h` | 改 | 模板运算入口加 `static_assert(sizeof(T)==4)`（D4 提前落地） |
| `ip/cpu/arch/riscv/branch.h` | 改 | 同上（BLT/BGE 载体） |

### 文档（3 个文件）

| 文件 | 动作 | 具体 |
|------|------|------|
| `ip/cpu/docs/README.md` | 改 | 缺口表入索引（**architect 阶段已落地**） |
| `docs/architecture/adr.md` | 改 | ADR-087 注册 + 修订历史 v1.5（**architect 阶段已落地**） |
| `ip/cpu/docs/rv64_readiness_gap.md` | 改 | §2 状态列更新（0.1 / 0.2 / 0.3 / 0.4 / 0.5 / 0.6 / 0.9 / 0.10 标 ✅） |

### `isa` 三态语义定义（ADR-087 §D1）

| 态 | 语义 | 取值 | 配置校验 |
|----|------|------|-----------|
| `implemented` | 已实现，可被 dispatch | `rv32i` / `rv32im` | 允许 |
| `declared-reserved` | 规划中，**未实现** | `rv32imac` / `rv64i` / `rv64gc` | 允许但**必须 warn**（不得静默） |
| `unsupported` | 不在规划内 | 其他任何字符串 | **拒绝**（schema `enum` 收窄 + 运行时 fail-fast） |

**默认值为 `rv32im`**（当前实际实现的最宽子集）。

### `isa → xlen` 映射（ADR-087 §D2）

```
isa 含 "rv64" ⇒ xlen = 64
isa 含 "rv32" ⇒ xlen = 32
```

一致性检查：`isa` 含 rv64 ⇒ `xlen == 64`，不一致 ⇒ **运行时 fail-fast**（`load_config()`/`build_cpu()` 抛异常；`isa` 是运行期 `std::string`，编译期校验不成立 —— ADR-087 §D2 已按此降级）。本 change **只做映射 + fail-fast + TLM 模板护栏，不做 dispatch**。

编译期护栏（仅对模板参数可行）：`int_alu.h` / `branch.h` 加 `static_assert(sizeof(T) == 4)` —— 与"是否退役 TLM"正交，phase-4→5 窗口期不裸奔；`mul.h` / `mul_div_fsm.h` 归 Change 2，残余窗口由 ADR-087 R4 登记。

## 验收标准

> 同 `## Acceptance` 清单（下节）；中英文标题并存 —— 「验收标准」为 rdd-workflow 5 段 HARD-GATE 口径，`Acceptance` 为 OpenSpec / rdd-verifier 口径，内容相同。

## Acceptance

- [ ] `CPUConfig{}` 默认 `isa == "rv32im"` 且 `xlen == 32`；`isa` 含 rv64 且 `xlen == 32` ⇒ **运行时 fail-fast**（配置装载/工厂实例化抛异常或返回错误，非静默）
- [ ] `CpuFactory<uint64_t>` 实例化**编译失败**（`int_alu.h`/`branch.h` 守卫生效；变异对照：把 `branch.h` 断言临时改 `8` 应仍失败 `[cpu]` 编译 —— 证明守卫在实例化点生效）
- [ ] `grep -rn '"rv64' ip/cpu/configs/ soc/` 返回 **0**；schema 中 `rv64i` / `rv64gc` 不再是默认值（可保留于 enum 的 reserved 态）
- [ ] `tools/cpu_sim` 不再出现 `isa="rv32i"` 硬编码；`--isa` 参数（若存在）行为与 config 一致
- [ ] `ip/cpu/plugins/reg_file.cpp` 不存在；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` 返回 0
- [ ] `ip/cpu/docs/rv64_readiness_gap.md` §2 的 0.1–0.10 状态列全部更新为 ✅（0.7 / 0.8 保持 ✅）
- [ ] CI 软门禁上线："新增 XLEN 假设必须登记缺口表"（warn 级，不阻塞）
- [ ] 5 个架构门禁全 PASS：`verify_adr.sh` 0 fail · `verify_plugin_decision.sh` 8/8 · `check_plugin_portability.sh` 13/13 · `doc_link_check.sh` 0 broken · `roadmap-r9-check.sh` 0 critical
- [ ] 0 退化：`[cpu]` 129/129 · `[cpu-integration]` 36/36 · `[riscv-tests]` 48/48 · `[chmem]` 130/130 · `[mmu]` 59/59 全 PASS
- [ ] `openspec change validate <change>` 0 error（change 由 rdd-builder P0 立项时创建）

## Capabilities

- **`isa` 三态语义**（`implemented` / `declared-reserved` / `unsupported`）：配置字段首次具备可判定的可信度语义，取代"自由字符串 + 装饰字段"
- **`isa → xlen` 单一映射 + 双层一致性**（运行时 fail-fast 管运行期字符串、编译期模板护栏管 `sizeof(T)`）：消除"声明 64-bit + 实现 32-bit"的静默组合 —— 配置层抛异常、实例层编译失败
- **`xlen` 显式字段**：为 ADR-087 §D3（XLEN 单一来源）与后续 RV64 dispatch 提供落点 —— `xlen` 与 `isa` 正交（`rv32imac` 与 `rv64imac` 的差别不在 `xlen`），二者并存是正确分解
- **声明可信度**：`isa` 从"记录性字符串"升级为"可校验契约"；集成方按 `rv32im` 做假设不会踩坑
- **死代码归零**：`reg_file.cpp` 的 `uint64_t` 显式实例化（用 `keys_rv32::*`）是 ADR-087 缺口表 C.11 登记的"MUST be fixed for Phase 4 (RV64 integration)"项 —— 消除一颗定时炸弹

## Impact

### 受影响文件

| 文件 | 动作 | 影响 |
|------|------|------|
| `ip/cpu/cpu_factory.h` | 改 | `CPUConfig` 加 1 字段；`isa` 默认值变更；新增映射 + `static_assert` |
| `ip/cpu/configs/cpu_params_schema.json` | 改 | 加 `xlen` 属性；`isa` enum 收窄 + default |
| `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar,cpu_embedded}.json` | 改 | `isa` 值 |
| `tools/cpu_sim/main.cpp` | 改 | 移除 pin；行为改为跟随 config |
| `ip/cpu/plugins/reg_file.cpp` | **删** | 81 行死代码；无 CMake 引用（实测 `grep -rn "reg_file" --include=CMakeLists.txt` = 0） |
| `ip/cpu/arch/riscv/int_alu.h` / `branch.h` | 改 | 模板入口 `static_assert(sizeof(T)==4)`；0 断言语义变化、纯护栏 |
| `ip/cpu/docs/rv64_readiness_gap.md` | 改 | §2 状态列 |
| `ip/cpu/docs/README.md` | 改 | 索引（已落地） |
| `docs/architecture/adr.md` | 改 | ADR-087 注册（已落地） |

### 风险

- **R1（默认 `isa` 变更影响下游）**：`rv64gc` → `rv32im` 改变 3 个 JSON 的语义，可能触发 SoC 拓扑 / DSE 配置校验失败。缓解：`declared-reserved` 降级路径保证 rv64 配置仍可加载（带显式 warn）；JSON 与代码同 change 提交，不拆
- **R2（`reg_file.cpp` 删除漏引用）**：若有 include 遗漏 → 编译失败。缓解：编译即验证（该文件本就不在任何 CMakeLists，删除后必然能验证）
- **R3（`tools/cpu_sim` 移除 pin 后行为变化）**：若某配置仍写 `rv64gc`，`cpu_sim` 会因 `declared-reserved` 降级而走 RV32 路径 —— 行为与 pin 相同，但**必须 warn**（否则又是一次静默降级）。缓解：降级路径强制 warn，不静默
- **R4（`xlen` 字段与既有 `CPUConfig` 聚合初始化冲突）**：若某处用聚合初始化按位置传参，加字段会错位。缓解：`CPUConfig` 全部用具名赋值（实测现状即如此），加字段安全
- **回退**：单 commit，`git revert` 即可（无持久化 schema / 无序列化格式变更）

### 下游依赖

- `feat-rv64-isa-declaration-integrity`（phase-4）—— 本 change 是其 Change 1
- `objective-ip-cpu-tlm-backend-retirement` —— Gate 1 判据 **G1.5** 依赖本 change 落地
- **ADR-087** §D2 的"真 dispatch"留 phase-5，与 `feat-tlm-backend-retirement` B1 合并实施（避免两处同时动 `cpu_factory.h`）
- `chmem-tlm-backend-freeze`（同批次提案）—— 二者都属 phase-4 B0，互不重叠文件
