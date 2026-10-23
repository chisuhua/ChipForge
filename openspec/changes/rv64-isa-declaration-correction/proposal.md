---
initiative: wave5-isa-coverage-and-bp
priority: P0
version_target: v0.11.0-prep
status: ready
# P0 立项 (2026-10-23, rdd-builder case-1 approve, auto-decision per ADR-0049):
#   advisory=complex (planner) + LLM=complex + AC=10 → 完整 P1-P3。
#   上游 = ADR-087 (commit 11225a3) + improvement 5 段 (commit 2617fb2)。
#   dispatch 交接: isa→T 真 dispatch 不在本 change（A550 Out of Scope）。
created_from: .rddf/improvements/rv64-isa-declaration-correction.md
adr:
  - ADR-087
  - ADR-083
---

# rv64-isa-declaration-correction

## Why

**声明面是集成契约，不是注释。** `isa` 字段被 3 个 JSON + schema + SoC 文档 + `tools/cpu_sim` 消费，下游读到的是**值**而非免责声明。`tools/cpu_sim` 已经用 pin `rv32i` 的方式证明了"免责声明挡不住实际误用" —— 它必须绕过配置才能正常工作。

**沉默的失败模式最贵。** 当前如果有人实例化 `CpuFactory<uint64_t>`，**不会编译失败、不会运行报错，只会静默算错**（移位量 ≥32 全错、负数比较有符号截断、MULH 取错半部）。本 change 落地后该风险在两层关闭：编译层（`int_alu.h`/`branch.h` 的 `sizeof(T)==4` 守卫）+ 配置层（`isa`↔`xlen` 运行时 fail-fast）；`mul.h`/`mul_div_fsm.h` 的残余窗口由 ADR-087 Risks **R4** 显式登记，至 Change 2 关闭。

**治理债已经产生实际成本。** `tools/cpu_sim` 的 pin、`reg_file.cpp` 的 `uint64_t` 显式实例化、schema 无 `xlen` 属性 —— 三处都是同一个根因（`isa` 无权威语义）的不同症状。分别打补丁会继续产生第四、第五处。

**与路线图的关系。** RV64 本身推迟到 v1.x 之后是既定决策，本 change **不做 RV64**，只保证"当决策到来时，不会从错误的起点出发"。它也是 `feat-tlm-backend-retirement` 的 Gate 1 前置（G1.5）—— 若在虚假声明未纠正时就删除 TLM，等于把两条语义都不准的实现里更少的那条留下来。

### 架构依据（越界点实测，HEAD `c4d548f`）

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

**第二重风险（隐患层）**：骨架层真参数化（`payload_common.h:112-120` `keys<T, XLEN>` 支持 64-bit、`reg_file.h:68` 模板化、`stage_link.h:55` 完全参数化、`mem_bundles.h:45-46` 64-bit 地址），但 ISA 语义层写死 RV32 —— 缺口 SSOT 见 `ip/cpu/docs/rv64_readiness_gap.md`（ADR-087 §D6）。

## What Changes

### In Scope（10 项）

1. `ip/cpu/cpu_factory.h` —— `CPUConfig::isa` 默认 `rv64gc` → `rv32im`；新增 `std::uint8_t xlen = 32;`；新增 **三层 fail-fast** 校验函数 `cf::cpu::detail::validate_isa_xlen(config, sizeof(T)*8)`（唯一 SSOT，`build_cpu()` 首行接线；`isa` 为运行期 `std::string`（`:127`，值来自 JSON 装载），编译期校验不成立）。三层语义见 design.md Decision 1：unsupported ⇒ throw / implemented 态与 `xlen` 冲突 ⇒ throw / `xlen != sizeof(T)*8` ⇒ throw / declared-reserved ⇒ 接受 + 一次 cerr warning
2. `ip/cpu/configs/cpu_params_schema.json` —— 新增 `xlen` 属性（enum 32/64）；`isa` enum 收窄至 `["rv32i","rv32im","rv32imac"]`，`rv64i`/`rv64gc` 从 enum 移到 description（**declared-reserved 值文档化**；运行时 reserved-warn 语义不变，机器校验层只覆盖可生效取值）；default `rv32im`
3. `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar}.json` —— `isa` → `rv32im`；**`mmu_mode` → `sv32`**（32-bit XLEN 下只有 Sv32 合法；不改则触发 schema `:204` 条件约束 `isa ∈ {rv32i,rv32im,rv32imac} ⇒ mmu_mode const "sv32"` ⇒ 改后配置变 schema 非法）
4. `ip/cpu/configs/cpu_embedded.json` —— `rv32imac` → `rv32im`（A 扩展未实现）
5. `tools/cpu_sim/main.cpp` —— 移除 `isa="rv32i"` 显式 pin（`:16` 注释 + `:188` 赋值），改为跟随 config
6. `ip/cpu/plugins/reg_file.cpp` —— 死代码删除（显式实例化 `RegFilePlugin<std::uint64_t>` 却用 `pl::keys_rv32::*`，`:34,47,52,58,69,72`；该文件不在任何 CMakeLists —— 实测 `grep -rn "reg_file" --include=CMakeLists.txt` = 0 命中）
7. **D4 提前落地**（TLM 侧护栏，与 TLM 退役决策正交）—— `ip/cpu/arch/riscv/int_alu.h` / `branch.h` 模板运算入口函数体首行加 `static_assert(sizeof(T) == 4, "RV32 TLM path assumes 32-bit XLEN; RV64 requires the phase-5 block gate see ADR-087 G3")`；**必须放模板入口内**（实例化点触发），不得放文件作用域（否则 include 即误伤）；`branch.h` 需**两处**（静态入口 `evaluate_branch` + `build()` 闭包内的真实分支判定路径 —— 只护静态入口则真实 BLT/BGE 路径不受覆盖）；`mul.h` / `mul_div_fsm.h` 归 Change 2（phase-5）
8. `ip/cpu/docs/rv64_readiness_gap.md` —— 入 `ip/cpu/docs/README.md` 索引
9. `docs/architecture/adr.md` —— ADR-087 注册（architect 阶段已落地，见 §Impact）
10. CI 软门禁 —— "新增 XLEN 假设必须登记缺口表" 提示（warn 级；新脚本 `tools/check_xlen_hygiene.sh`，以缺口表 §5 的 **9 处定义点 / 9 个文件**为 allowlist —— 实测 `grep -rnE "kXlenBits *=" --include='*.h' ip/` = 9 命中 9 文件，含 §5 描述未点名 `kXlenBits` 的 3.5 `ibus_chmem.h` / 3.6 `dmem_chmem.h`；出现新硬编码假设 → warn 输出 `文件:行`）
11. **连带纠偏**（design D4）：`tools/dse/sweep_config.example.json:8` 的 sweep grid `["rv32i","rv64i"]` → `["rv32i","rv32im"]`（DSE 示例不再诱导 reserved 值）
12. **T64 实例化点迁移**（critic B1 + 用户裁决：保留编译期护栏）—— 实测 `RiscvIntAluPlugin<unsigned long>` / `RiscvBranchPlugin<unsigned long>` 今天已被 `tests/cpu/test_cpu_factory.cpp` 与 `tests/cpu/test_branch_predictor_runtime_btb.cpp` 实例化（`nm -C` 扫 `.o` 实证）⇒ 与 AC2「`CpuFactory<uint64_t>` 编译失败」互斥。处置：删除 RV64 构建探测 case `build_cpu_rv64`；其余 T64 调用点改 T32（保留 BTB 覆盖）；integration 侧 `cfg.isa` 值纠偏（`rv64gc`/`rv32imac` → `rv32im`，共 10 处）。**RV64 真实路径归 ADR-087 Change 2（phase-5）**
13. **测试值纠偏**（设计后果，非缺陷修复）—— 既有 `cfg.isa = "rv64gc"`（test_7stage×3 / test_10stage×3 / test_cpu_factory×2 / test_topology_builder×1）与 `rv32imac`（test_3stage×1 / test_cpu_factory×1）原按 declared-reserved warn 路径「零改动」，现按 item 12 统一纠偏为 `rv32im` ⇒ **配置/声明面与 tests/ 均无 reserved 值残留**（reserved 语义由新增专用测试覆盖）

### Out of Scope

- **不做 `isa → T` 真 dispatch**（留 phase-5，与 `feat-tlm-backend-retirement` B1 合并做 —— 否则要先定 XLEN 单一来源，见 ADR-087 §D3）
- **不做任何 RV64 语义实现**（XLEN 参数化 / 128-bit 乘积 / 64 拍除法 / LD-SD / W 指令 / CSR 64-bit / `misa`-MXL —— 全部属 ADR-087 G1–G9 准入清单，当前 0/9 满足）
- **不扩展 CH_MEM 侧护栏** —— `mul.h` / `mul_div_fsm.h` 的 `static_assert` 与 CH_MEM `kXlenBits = 32` 定义点（**9 处**，含 `ip/mmu/plugins/MMUPlugin_chmem.h:81`）收敛均属 ADR-087 Change 2（phase-5）
- **不改 SoC 拓扑 JSON 的 `isa` 语义**（`soc/docs/integration-guide.md` 仅改示例值）

### 变更清单（16 个文件：13 改 + 1 删 + 2 新增）

| 文件 | 动作 | 具体 |
|------|------|------|
| `ip/cpu/cpu_factory.h` | 改 | `isa` 默认值；新增 `xlen` 字段；新增 `cf::cpu::detail::validate_isa_xlen()`（后置重开 namespace）+ **三层运行时 fail-fast** |
| `ip/cpu/configs/cpu_params_schema.json` | 改 | `xlen` 属性；`isa` enum 收窄 + default；rv64* 移入 description |
| `ip/cpu/configs/cpu_default.json` | 改 | `isa` → `rv32im`；`mmu_mode` → `sv32` |
| `ip/cpu/configs/cpu_deep_pipeline.json` | 改 | `isa` → `rv32im`；`mmu_mode` → `sv32` |
| `ip/cpu/configs/cpu_superscalar.json` | 改 | `isa` → `rv32im`；`mmu_mode` → `sv32` |
| `ip/cpu/configs/cpu_embedded.json` | 改 | `rv32imac` → `rv32im` |
| `tools/cpu_sim/main.cpp` | 改 | 移除 `isa="rv32i"` pin（含头注释）；`load_config()` 补 `xlen` 解析 |
| `ip/cpu/plugins/reg_file.cpp` | 删 | 死代码（`uint64_t` 显式实例化 + `keys_rv32::*` 混用） |
| `ip/cpu/arch/riscv/int_alu.h` | 改 | 模板入口加 `static_assert(sizeof(T)==4)`（D4 提前落地） |
| `ip/cpu/arch/riscv/branch.h` | 改 | 同上，**两处**（静态入口 + `build()` 闭包内真实判定） |
| `tools/dse/sweep_config.example.json` | 改 | sweep grid `rv64i` → `rv32im`（design D4 连带纠偏） |
| `tools/check_xlen_hygiene.sh` | **新增** | CI 软门禁（warn-only；allowlist = 缺口表 §5 的 9 处定义点 / **9 个文件**） |
| `.github/workflows/architecture-gates.yml` | 改 | 接入 hygiene 脚本（`continue-on-error: true`） |
| `tests/cpu/test_cpu_factory.cpp` | 改 | 删 `build_cpu_rv64` case；余下 T64 → T32；`isa` 断言纠偏 |
| `tests/cpu/test_branch_predictor_runtime_btb.cpp` | 改 | 3 个 case 的 `uint64_t` → `uint32_t`（保留 BTB 覆盖） |
| `tests/cpu/integration/test_{3,7,10}stage_riscv.cpp` | 改 | `cfg.isa` 值纠偏（`rv64gc`/`rv32imac` → `rv32im`，7 处） |
| `tests/cpu/test_cpu_factory_isa_xlen.cpp` | **新增** | `[cpu][isa-xlen]` 5 用例（默认值契约 / unsupported throw / implemented 冲突 throw / T↔xlen 交叉 throw / reserved 单发 warn） |

### 文档（3 个文件，architect 阶段已落地 / 本 change 只更新状态列）

| 文件 | 动作 | 具体 |
|------|------|------|
| `ip/cpu/docs/README.md` | 改 | 缺口表入索引（**architect 阶段已落地**） |
| `docs/architecture/adr.md` | 改 | ADR-087 注册 + 修订历史（**architect 阶段已落地**） |
| `ip/cpu/docs/rv64_readiness_gap.md` | 改 | §2 状态列更新（0.1 / 0.2 / 0.3 / 0.4 / 0.5 / 0.6 / 0.9 / 0.10 标 ✅） |

### `isa` 三态语义定义（ADR-087 §D1）

| 态 | 语义 | 取值 | 配置校验 |
|----|------|------|-----------|
| `implemented` | 已实现，可被 dispatch | `rv32i` / `rv32im` | 允许 |
| `declared-reserved` | 规划中，**未实现** | `rv32imac` / `rv64i` / `rv64gc` | 允许但**必须 warn**（不得静默） |
| `unsupported` | 不在规划内 | 其他任何字符串 | **拒绝**（schema `enum` 收窄 + 运行时 fail-fast） |

**默认值为 `rv32im`**（当前实际实现的最宽子集）。

### `isa → xlen` 映射与一致性规则（ADR-087 §D2，语义分解见 design.md Decision 1）

```
isa 含 "rv64" ⇒ xlen = 64
isa 含 "rv32" ⇒ xlen = 32
```

**一致性规则（三层，与 design.md Decision 1 一一对应）**：

| 情形 | 行为 |
|------|------|
| `isa` 落在 implemented 集合（`rv32i`/`rv32im`）且 `xlen != 32` | **运行时 fail-fast**（`build_cpu()` 抛 `std::invalid_argument`） |
| `isa` 落在 declared-reserved 集合（`rv32imac`/`rv64i`/`rv64gc`） | **接受 + 一次 cerr warning**（不 throw —— reserved 态不可 dispatch，一致性校验无判据意义） |
| `build_cpu<T>` 中 `xlen != sizeof(T)*8` | **运行时 fail-fast**（模板 T ↔ 配置 xlen 的交叉校验） |
| `isa` 为其他任何字符串（unsupported） | **运行时 fail-fast** |

> ⚠️ 口径澄清（critic SHOULD_FIX 7 修正）：「`isa` 含 rv64 ⇒ `xlen == 64`，不一致即 fail-fast」**不作为独立规则**——rv64* 属 declared-reserved，其不一致由上表第 2 行以 warn 覆盖、由第 3 行以 T↔xlen 交叉校验兜底。ADR-087 §D1 三态表为语义权威。

本 change **只做映射 + 三层 fail-fast + TLM 模板护栏，不做 dispatch**。

编译期护栏（仅对模板参数可行）：`int_alu.h` / `branch.h` 加 `static_assert(sizeof(T) == 4)` —— 与"是否退役 TLM"正交，phase-4→5 窗口期不裸奔；`mul.h` / `mul_div_fsm.h` 归 Change 2，残余窗口由 ADR-087 R4 登记。

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
| `ip/cpu/cpu_factory.h` | 改 | `CPUConfig` 加 1 字段；`isa` 默认值变更；新增映射 + **运行时 fail-fast** |
| `ip/cpu/configs/cpu_params_schema.json` | 改 | 加 `xlen` 属性；`isa` enum 收窄 + default |
| `ip/cpu/configs/cpu_default.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_deep_pipeline.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_superscalar.json` | 改 | `isa` → `rv32im` |
| `ip/cpu/configs/cpu_embedded.json` | 改 | `rv32imac` → `rv32im` |
| `tools/cpu_sim/main.cpp` | 改 | 移除 pin；行为改为跟随 config |
| `ip/cpu/plugins/reg_file.cpp` | **删** | 81 行死代码；无 CMake 引用（实测 `grep -rn "reg_file" --include=CMakeLists.txt` = 0） |
| `ip/cpu/arch/riscv/int_alu.h` / `branch.h` | 改 | 模板入口 `static_assert(sizeof(T)==4)`；0 断言语义变化、纯护栏 |
| `tools/dse/sweep_config.example.json` | 改 | sweep grid `rv64i` → `rv32im`（design D4 连带纠偏） |
| `ip/cpu/docs/rv64_readiness_gap.md` | 改 | §2 状态列更新 |
| `ip/cpu/docs/README.md` | 改 | 缺口表入索引（已落地） |
| `docs/architecture/adr.md` | 改 | ADR-087 注册（已落地） |

### 风险

1. **R1（默认值变更波及面）**：`rv64gc` → `rv32im` 改变 3 个 JSON 的语义；连带 `mmu_mode` `sv39/sv48` → `sv32`（32-bit XLEN 下只有 Sv32 合法；`enable_mmu=false` 下路径 inert，行为不变）。缓解：TLM 测试全部 C++ 构造 config（不读 JSON 默认值），DSE CSV 消费点赛前 grep 确认；回归 `[mmu]` / `[cpu-l1-mmu-demo]` 兜底
2. **R2（static_assert 误伤 CH_MEM binary）**：实测 CH_MEM 侧**不 include** `int_alu.h`/`branch.h`（`cpu_factory_chmem.h` 只 include `*_chmem.h`；`nm chipforge_tests_chmem` 只有 `compute_static`/`safe_divisor_u`）⇒ 护栏不触发。但 `int_alu_chmem.h` 是**同名类**（`#ifdef` 隔离），护栏落点若误改到 chmem 变体会打断 CH_MEM 编译。缓解：Change 收口必须双 binary 全量 build + `[chmem]` 抽查
3. **R3（cpu_sim CLI 行为变化）**：5 个 e2e 用例依赖 `exec_cmd` 约定，移除 `isa="rv32i"` pin 后需回归验证；`load_config()` 必须补 `xlen` 解析（否则 schema 声明的 `xlen` 被静默丢弃 —— `cpu-pipeline-tick-fix` F1 同型 bug）
4. **R4（窗口期）**：`mul.h` / `mul_div_fsm.h` 无护栏至 Change 2（phase-5）—— ADR-087 Risks R4 已登记，残余窗口显式可接受
5. **R5（T64 迁移导致计数变化）**：删除 `build_cpu_rv64` case 后 `[cpu]` 从 129 → 128，再 +5 新用例 = **133**（AC9 已按此校准；非退化）
6. **R6（schema 有效性基线）**：4 个配置 JSON 中 3 个因**顶层 `_comment`** 撞 schema `additionalProperties: false` 而**今天已非法**（pre-existing；`tools/test_doc_checker.py:129-132` 捕获 `ValidationError` 后 pass ⇒ CI 从不报）。本 change 只保证**不新增** schema 违规（`isa`/`mmu_mode` 组合满足 `:204` 条件），`_comment` 问题登记为基线、不在本 change 修

## Acceptance

- [ ] 三层 fail-fast 语义生效（design.md Decision 1，ADR-087 §D1 为准）：① 默认 `CPUConfig{}` `isa == "rv32im"` 且 `xlen == 32`（不 throw 不 warn）；② `isa="rv128i"`（unsupported）⇒ **throw**；③ implemented 态冲突（`isa="rv32im"` + `xlen=64`）⇒ **throw**；④ `build_cpu<T>` 中 `xlen != sizeof(T)*8` ⇒ **throw**；⑤ declared-reserved（`rv32imac`）⇒ **接受 + 一次 warning**（std::cerr，非静默）
- [ ] `CpuFactory<uint64_t>` 实例化**编译失败**（`int_alu.h`/`branch.h` 守卫生效）；**前置条件**：现存 T64 实例化点已按 In Scope item 12 迁移（`grep -rn "uint64_t>::build_cpu\|CpuFactory<T64>" tests/` = 0）⇒ 否则 AC2 与 0 退化互斥；变异对照：把 `branch.h` 断言临时改 `8` 应使 `[cpu]` 编译失败 —— 证明守卫在实例化点生效
- [ ] `grep -rn '"rv64' ip/cpu/configs/ soc/ tools/dse/sweep_config.example.json` 返回 **0**；schema `isa` enum 只含 `rv32i`/`rv32im`/`rv32imac`、default `rv32im`；`rv64i`/`rv64gc` 仅以 **description 文本**形式保留（declared-reserved 文档化，非 enum 项）
- [ ] `tools/cpu_sim` 不再出现 `isa="rv32i"` 硬编码（`grep -n 'isa="rv32i"' tools/cpu_sim/main.cpp` = 0，含注释）；`--isa` 参数（若存在）行为与 config 一致；`load_config()` 已解析 `xlen`
- [ ] `ip/cpu/plugins/reg_file.cpp` 不存在；`grep -rn "RegFilePlugin<std::uint64_t>" ip/` 返回 0
- [ ] `ip/cpu/docs/rv64_readiness_gap.md` §2 的 0.1–0.10 状态列全部更新为 ✅（0.7 / 0.8 保持 ✅）
- [ ] CI 软门禁上线："新增 XLEN 假设必须登记缺口表"（warn 级，不阻塞；`tools/check_xlen_hygiene.sh` 基线 `warn=0`）
- [ ] 6 个门禁全 PASS：`verify_adr.sh` 0 fail · `verify_plugin_decision.sh` 8/8 · `check_plugin_portability.sh` 13/13 · `doc_link_check.sh` 0 broken · `roadmap-r9-check.sh` 0 critical · `check_xlen_hygiene.sh` warn=0
- [ ] 0 退化（**HEAD `9b45fac` 实测基线口径**）：`[cpu]` **137**（129 − 1 删 `build_cpu_rv64` + 5 契约用例 + 4 完备性补全用例）· `[cpu-integration]` **96 cases / 95 PASS / 1 FAIL**（唯一 FAIL = `tests/cpu/integration/test_7stage_riscv.cpp:124` node_count `9 == 17`，**pre-existing**，口径 = **不新增失败**；TLM binary 全量 493 = 492 + 1）· `[riscv-tests]` 48/48 · `[mmu]` 59/59 · `[cache]` 25/25 · `[cpu-configs]` 13/13（175 assertions）· `chipforge_tests_chmem` binary **130**（其中 `[chmem]` tag 89/89）全 PASS
- [ ] `openspec change validate rv64-isa-declaration-correction` 0 error（change 已由 rdd-builder P0 立项创建）

## References

- ADR-087: `docs/architecture/adr/ADR-087-isa-declaration-integrity-and-rv64-readiness.md`（§D1 三态 / §D2 映射+fail-fast / §D4 护栏 / §D6 缺口表 SSOT / Risks R4）
- 缺口表 SSOT: `ip/cpu/docs/rv64_readiness_gap.md`（3 层缺口 + §8 G1–G9 准入清单）
- Improvement 5 段: `.rddf/improvements/rv64-isa-declaration-correction.md`
- Roadmap: `.rddf/roadmap/features/feat-rv64-isa-declaration-integrity.md`（Change 1 段）
