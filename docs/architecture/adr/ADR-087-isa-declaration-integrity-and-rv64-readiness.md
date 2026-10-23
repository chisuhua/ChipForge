# ADR-087: ISA 声明真实性与 RV64 就绪度门禁（ISA Declaration Integrity & RV64 Readiness Gate）

> **Status**: 🚧 Proposed（设计提案 2026-10-23，RV64 架构符合性审查触发）
> **类别**: IP 架构 / ISA 契约
> **关联 ADR**: ADR-037 v2.0（Plugin 范式）· ADR-040 v2.0（TLM→HDL 移植性约束）· ADR-047（Result 范式）· ADR-048（注册规范序）· ADR-083（Plugin-style 单一 source of truth）· ADR-085 / ADR-086（MMU PTW / L0-L1 TLB 架构）
> **关联变更**: 待立项（建议 3 个 change，见 §Implementation Plan）
> **缺口表 SSOT**: [`ip/cpu/docs/rv64_readiness_gap.md`](../../../ip/cpu/docs/rv64_readiness_gap.md)
> **拟批准触发**: P0-1 声明纠偏 change archive 时同步批准

---

## Context

### 1.1 审查起因

2026-10-23 对 `ip/cpu/` 做 RV64 架构符合性审查，结论：**当前实现 = RV32IM + Zicsr(subset)，不符合 RV64**。

但这个结论本身不是问题 —— 路线图已明确把 RV64 推迟：

> `.rddf/roadmap/archive/phase-2-baremetal.md:5`
> 「rv64ui/um/ua/uf/ud/uc/si/mi 8 子集 | v0.10.0（rv32ui+um+uc）+ v1.0.0（rv32ua+si）；**rv64 子集推迟到 v1.x 之后远期**」

`docs/architecture/roadmap-evolution.md` 全部版本节点（v0.10.0 ~ v1.3.0）均为 RV32IMAC / RV32C / RV32A 形态。

### 1.2 真正的问题：声明层越界

**声明层全线宣称 RV64GC，实现层全线是 RV32，两侧之间没有任何交叉校验**：

| 位置 | 声明 |
|------|------|
| `ip/cpu/README.md:13` | 「支持 RV32I/RV64GC 指令集」 |
| `ip/cpu/cpu_factory.h:127` | `CPUConfig::isa = "rv64gc"`（**默认值**） |
| `ip/cpu/configs/{cpu_default,cpu_deep_pipeline,cpu_superscalar}.json:5` | `"isa": "rv64gc"` |
| `ip/cpu/configs/cpu_params_schema.json:18-23` | enum 含 rv64i/rv64gc、default rv64gc（**且无 `xlen` 属性**） |
| `ip/cpu/docs/blueprint.md:92,156` | 「RV32I/RV64I 整数运算」 |
| `ip/cpu/docs/status.md:109` | 「已有 (5 级 RV64GC)」 |
| `ip/cpu/docs/multi_isa_architecture.md:734,1204` | `isa` 是「唯一权威字段 (字符串, factory dispatch)」 |
| `soc/docs/integration-guide.md:17` | `"isa": "rv64imac"`（第四种取值，连 schema enum 都不在） |

**并且 `isa` 字段完全不参与 dispatch** —— `ip/cpu/cpu_factory.h` 全文无任何 `isa ==` 判断，`T`（XLEN 类型）由调用方决定（`cpu_factory.h:310` `template <typename T = std::uint32_t>`）。

代码自身已经承认这个不一致：

> `tools/cpu_sim/main.cpp:16`
> 「isa="rv32i" 显式 pin（修复既有 `CpuFactory<uint32_t>` vs config.isa="rv64gc" 不一致）」

### 1.3 第二重风险：模板化制造的"假 RV64 就绪"

`ip/cpu/` 的骨架层**确实是参数化的**（见缺口表 §3）：`keys<T, XLEN>` 支持 64-bit、`RegFilePlugin<T>` 模板化、`stage_link.h` 完全参数化、内存 bundle 地址 64-bit、`satp` 编码已支持 Sv39/Sv48。

**但 ISA 语义层写死 RV32**（缺口表 §4）。这意味着：

> 如果有人把 `CpuFactory<uint64_t>` 实例化出来，**不会编译失败，不会运行报错，会静默产出错误结果**。

具体地（TLM 侧）：

- `int_alu.h:92,97,100` 移位量 `& 0x1F` ⇒ RV64 下 shamt=32..63 全部算错
- `int_alu.h:93-94` / `branch.h:87-90,115-118` 用 `int32_t` 比较 ⇒ 负数比较错
- `mul.h:102-119` MULH 族 = `uint32_t(r >> 32)` ⇒ RV64 语义完全不同（需 128-bit 高 64 位）
- `mul_div_fsm.h:148,231,241,244` radix-2 除法写死 32 拍 ⇒ 需重写，不是放开模板参数

CH_MEM 侧另有 9 处 `kXlenBits = 32` 定义点各自硬编码（缺口表 §5，含跨 IP 的 `ip/mmu/plugins/MMUPlugin_chmem.h:81`），Phase 6d / Verilator 主战场整体锁死。

### 1.4 问题的性质

这是**治理缺陷**，不是架构缺陷：

1. 实现层与 roadmap 一致 ⇒ 无需返工
2. 声明层与实现层脱节 ⇒ **集成面契约不实**（下游按 `isa="rv64gc"` 做假设 → 集成踩坑）
3. 模板化诱导 ⇒ **未来切 XLEN=64 会静默算错**（最危险的形态：无异常、无断言）

---

## Decision

### D1 — ISA 声明三态语义

`isa` 字段不再是自由字符串，取值分三态：

| 态 | 语义 | 允许取值（RV32 期） | 配置校验 |
|----|------|--------------------|-----------|
| `implemented` | 该 ISA 已实现，可被 dispatch | `rv32i` / `rv32im` | 允许 |
| `declared-reserved` | 规划中，**未实现**，配置时必须显式接受降级 | `rv32imac` / `rv64i` / `rv64gc` | 允许但**必须 warn**（不得静默） |
| `unsupported` | 不在规划内 | 其他任何字符串 | **拒绝**（schema `enum` 收窄 + 运行时 fail-fast） |

**默认值为 `rv32im`**（当前实际实现的最宽子集），不再是 `rv64gc`。

### D2 — `isa` → `xlen` 单一映射 + 运行时 fail-fast 一致性检查

引入唯一映射函数（位置待定，倾向 `ip/cpu/core/payload_common.h`）：

```
isa 含 "rv64" ⇒ xlen = 64
isa 含 "rv32" ⇒ xlen = 32
```

并规定：

1. `CPUConfig` 增加 `std::uint8_t xlen = 32;` 字段
2. `isa` 与 `xlen` 不一致 ⇒ **运行时 fail-fast**（`load_config()` / `CpuFactory::build_cpu()` 在配置装载后、实例化前抛异常/返回错误，形态与 D1 的 `unsupported` 一致）
   **不可用编译期 `static_assert`**：`CPUConfig::isa` 是运行期 `std::string`（`ip/cpu/cpu_factory.h:127`），其值来自 JSON 装载，编译期不可见 —— 任何声称"编译期校验 isa↔xlen"的实现要么恒真要么无法编译
3. `CpuFactory::build_cpu` 按 `xlen` 选择 `uint32_t` / `uint64_t` 特化；`isa` 首次真正参与 dispatch
4. `declared-reserved` 态的 `isa` ⇒ 降级到 `rv32im` 并 **emit 一次显式 warning**（不是静默）

> D2 是「声明可信」的最终形态；D1 是 D2 落地前的**止血**步骤（两者可分属不同 change，见 §Implementation Plan）。
> 与 D4 的分工：**运行期 fail-fast 管运行期字符串（isa↔xlen）；编译期 `static_assert` 只管模板参数本身（`sizeof(T)`）** —— 两类守卫覆盖面不同，不得混称。

### D3 — XLEN 单一来源（消除 9 处 `kXlenBits = 32` 定义点）

在 `ip/cpu/core/payload_common.h` 暴露唯一常量：

```cpp
template <typename T>
inline constexpr unsigned xlen_bits = sizeof(T) * 8;
```

CH_MEM 后端全部 9 处定义点（缺口表 §5 的 3.1–3.8、3.10；其中 4 处为多行 `#ifdef` 形式，验收 grep 必须用宽松正则）改为引用该常量，**不再各自定义**。

**约束**：CH_MEM 路径的 `ch_uint<N>` 宽度不能直接用 `sizeof(T)*8`（`ch_uint<32>` 的 `sizeof` ≠ 4，见 `int_alu_chmem.h:98-103` 现有注释），因此需要一层「TLM 类型 → CH_MEM 位宽」的显式映射，而不是简单替换。

### D4 — 语义层编译期护栏（把"静默算错"变成"编译失败"）

在 Layer 2 的语义点加 `static_assert(sizeof(T) == 4)`。**承载 `T` 模板的运算入口文件**是护栏对象；`isa` 字符串不在其覆盖面（那是 D2 的运行期职责）：

- `ip/cpu/arch/riscv/int_alu.h` / `branch.h` — **随 Change 1 提前落地**（见 §Implementation Plan Change 1 任务 1.9/1.10；与"是否退役 TLM"正交，窗口期不裸奔）
- `ip/cpu/arch/riscv/mul.h` / `mul_div_fsm.h` — 随 Change 2 落地（M 扩展语义点 2.4–2.7）

`static_assert` 必须放在**模板运算入口函数体首行**（实例化点触发），不得放文件作用域（否则 include 即失败的误伤）。

**理由**：RV64 实现是独立 change（§G1–G9）。在它落地前，宁可**编译期拒绝** `T = uint64_t`，也不接受静默算错。

**约束**：`static_assert` 必须带明确信息串，指明"RV64 语义未实现，见 ADR-087 §8 G3"。

### D5 — RV64 实施准入清单（Gate）

RV64 实施 change 的**准入条件**由缺口表 §8 的 G1–G9 定义，判定权归本 ADR，不归单个 code change。

**当前状态：0/9 满足。** 在 G1–G9 全部满足前：

- 不得实例化 `CpuFactory<uint64_t>`
- 不得声称支持 RV64
- `isa` 含 rv64 的配置一律走 D1 的 `declared-reserved` 降级路径

### D6 — 缺口表 SSOT 归属

RV64 缺口的唯一登记处为 [`ip/cpu/docs/rv64_readiness_gap.md`](../../../ip/cpu/docs/rv64_readiness_gap.md)。本 ADR **只引用不复制**。

新增 XLEN 假设（无论 TLM / CH_MEM / 工具链）必须登记到缺口表，**不得只写在代码注释里**。当前全仓唯一的显式 RV64 TODO 是 `ip/cpu/plugins/hazard_chmem.h:39`，其余全是隐性缺口 —— 这是本次审查最需要修复的治理缺口。

### D7 — 声明纠偏的责任划分

| 范围 | 归属 | 本 ADR 状态 |
|------|------|-------------|
| 架构 / 声明类文档（README / blueprint / status / multi_isa 文档 / integration-guide） | architect | ✅ **本 ADR 落地时同步完成** |
| 实现代码（`cpu_factory.h` / schema / JSON / `tools/cpu_sim`） | builder | ⬜ 待立项 change |
| 语义层护栏 / XLEN 单一来源 | builder | ⬜ 待立项 change |

---

## Consequences

### 正面

1. **集成面契约恢复可信**：`isa` 不再撒谎，下游按 `rv32im` 做假设不会踩坑
2. **未来 RV64 改造的踩坑面收敛**：从"改了 9 处漏 1 处"变为"改 1 处"
3. **静默算错变为编译失败**：Layer 2 的 5 个语义点不再可能在 XLEN=64 下无声出错
4. **RV64 何时可做有了判据**：G1–G9 把"感觉差不多了"变成可核对的清单
5. **缺口不再隐性**：缺口表把散落 8+ 处的 XLEN 假设集中登记

### 负面

1. **默认 `isa` 变更影响配置**：`rv64gc` → `rv32im` 会改变 3 个 JSON 配置的语义，需要评估 DSE / SoC 拓扑的连带影响
2. **D4 的 `static_assert` 会挡住 `RegFilePlugin<uint64_t>`**：`ip/cpu/plugins/reg_file.cpp:34` 有该显式实例化（当前是死代码，未进任何 CMakeLists）。落地 D4 时需**同步删除或修正该文件**，否则编译直接失败
3. **`declared-reserved` 的 warn 机制需要落点**：ADR-047 的 Result 范式是自然载体，但 `CPUConfig` 是纯 struct、无 Result 通道 ⇒ 需要约定 warn 输出位置
4. **RV64 本身仍未做**：本 ADR 不改变任何 ISA 覆盖面，只把"何时能做"的判据固定下来

### 中性

1. **不影响双模**：所有决策同时作用于 TLM 与 CH_MEM，无单边约束
2. **不改变 roadmap**：RV64 仍按 `.rddf/roadmap/archive/phase-2-baremetal.md:5` 推迟到 v1.x 之后；本 ADR 只做声明纠偏与就绪度登记

---

## Alternatives Considered

### 替代 A — 直接实现 RV64（现在就做）

- **优点**：一次性解决全部缺口
- **否决原因**：与 roadmap 直接冲突（RV64 已推迟到 v1.x 之后），且 Layer 2 的 M 扩展需**重写**（非参数化），工作量与当前 v0.11.0 收官主线冲突。ADR-087 明确"不做 RV64，只固定判据"。

### 替代 B — 保留 `isa = "rv64gc"` 默认，仅在文档里加免责声明

- **优点**：零代码改动
- **否决原因**：`isa` 字段本身就是集成面契约，配置消费者（SoC 拓扑 / DSE / 第三方集成）读到的是**值**不是免责声明。`tools/cpu_sim` 已经用 pin `rv32i` 的方式证明了"免责声明挡不住实际误用"。

### 替代 C — 删掉 `isa` 字段，只保留 `xlen`

- **优点**：单一事实源，无三态语义负担
- **否决原因**：`isa` 已被 3 个 JSON + schema + SoC 文档引用，删除是破坏性变更；且 `isa` 表达的是**扩展子集**（IMAC / G），与 `xlen`（32/64）是正交的两个维度 —— `rv32imac` 与 `rv64imac` 的差别不在 `xlen`。**保留 `isa` + 增加 `xlen`** 才是正确分解。

### 替代 D — 只做 Layer 2/3 的参数化，暂不动声明层

- **优点**：技术债最小
- **否决原因**：声明层是**本次审查发现的唯一越界项**。技术债（RV64 缺口）是 roadmap 已接受的既定事实，不构成阻塞；虚假声明才是。**先止血，再还债**。

---

## Implementation Plan

### Change 1 — `rv64-isa-declaration-correction`（P0，声明纠偏）

| # | 任务 | 归属 |
|---|------|------|
| 1.1 | `CPUConfig::isa` 默认 `rv64gc` → `rv32im` | `ip/cpu/cpu_factory.h:127` |
| 1.2 | `CPUConfig` 增加 `xlen` 字段（默认 32）+ `isa`→`xlen` 映射 + **运行时 fail-fast** 一致性检查（`isa` 是运行期 `std::string`，编译期校验不可行，见 §D2） | `ip/cpu/cpu_factory.h:124-176` |
| 1.3 | schema 加 `xlen` 属性（enum 32/64）；`isa` enum 收窄 + `declared-reserved` 标注 | `ip/cpu/configs/cpu_params_schema.json:18-23` |
| 1.4 | 3 个 JSON 的 `isa` 改 `rv32im`；`cpu_embedded.json` 的 `rv32imac` → `rv32im`（A 扩展未实现） | `ip/cpu/configs/*.json:5` |
| 1.5 | `tools/cpu_sim` 移除 `isa="rv32i"` pin（改为跟随 config） | `tools/cpu_sim/main.cpp:16,87,188` |
| 1.6 | `soc/docs/integration-guide.md` 的 `rv64imac` 纠正 | `soc/docs/integration-guide.md:17` |
| 1.7 | `ip/cpu/plugins/reg_file.cpp`（死代码）删除或修正 `uint64_t` 显式实例化 | `ip/cpu/plugins/reg_file.cpp:34,47,52,58,69,72` |
| 1.8 | `ip/cpu/docs/dse_architecture.md` 的 `isa` 表述 + 代码样例默认值标注 | `ip/cpu/docs/dse_architecture.md:126,231,251` |
| 1.9 | `int_alu.h` 模板运算入口函数体首行加 `static_assert(sizeof(T) == 4, ...)`（实例化点触发，把窗口期静默错算变编译错；D4 提前落地，与 TLM 退役决策正交） | 缺口表 §4 的 2.1–2.2 载体 |
| 1.10 | `branch.h` 同 1.9（BLT/BGE `int32_t` 符号比较的载体） | 缺口表 §4 的 2.3 载体 |

**验收**：`grep -rn '"rv64' ip/cpu/configs/ soc/` 返回 0；`CPUConfig{}` 默认 `isa == "rv32im"`；`CpuFactory<uint64_t>` 实例化**编译失败**（1.9/1.10 生效）；全部既有测试零退化。

### Change 2 — `rv64-xlen-width-guardrails`（P0，技术护栏）

| # | 任务 | 归属 |
|---|------|------|
| 2.1 | `xlen_bits<T>` 单一来源落地 | `ip/cpu/core/payload_common.h` |
| 2.2 | CH_MEM **9 处** `kXlenBits = 32` 定义点改为引用单一来源（需 TLM 类型 → CH_MEM 位宽映射层；含跨 IP 的 `ip/mmu/plugins/MMUPlugin_chmem.h:81`） | 缺口表 §5 的 3.1–3.8、3.10 |
| 2.3 | `mul.h` / `mul_div_fsm.h` 的 M 扩展加 `static_assert` 护栏（`int_alu.h` / `branch.h` 已随 Change 1 落地；明确标注"RV64 需重写，见 ADR-087 G4"） | 缺口表 §4 的 2.4–2.7 |

**验收**：`grep -rnE "kXlenBits *=" --include='*.h' ip/ | grep -v payload_common.h` 返回 0（单一来源生效）；`CpuFactory<uint64_t>` 实例化**编译失败**（护栏生效）。

### Change 3 — `rv64-readiness-gap-tracker`（P1，缺口治理）

| # | 任务 |
|---|------|
| 3.1 | `ip/cpu/docs/rv64_readiness_gap.md` 入 `ip/cpu/docs/README.md` 索引 |
| 3.2 | `docs/architecture/adr.md` 注册 ADR-087 |
| 3.3 | 建立"新增 XLEN 假设必须登记缺口表"的 CI 提示（软门禁，warn） |

---

## Validation & Verification

### 静态验证

- `bash tools/verify_adr.sh` — ADR 注册表一致性（本 ADR 登记后 0 fail）
- `bash tools/doc_link_check.sh` — 新文档无断链
- `grep -rn "rv64" ip/cpu/README.md ip/cpu/docs/ soc/docs/integration-guide.md` — 声明层无遗留虚假宣称
- `grep -rnE "kXlenBits *= *(32)" --include='*.h' ip/` — 单行定义 5 处；`grep -rnE "kXlenBits *=" --include='*.h' ip/` — **定义点合计 9 处**（与缺口表 §5 3.1–3.10 一一对应；**不得用 `ip/cpu/` 前缀**，会漏 `ip/mmu/` 的 3.10；多行 `#ifdef` 形式只有宽松正则能命中）
- `grep -n "std::string isa" ip/cpu/cpu_factory.h` — 确认 :127 为运行期值（D2 的编译期校验不可行的物证）

### 动态验证

- Change 1 落地后：`[cpu]` / `[cpu-integration]` / `[riscv-tests]` / `[chmem]` 全部零退化；新增编译期负向测试（`CpuFactory<uint64_t>` 实例化**必须编译失败**，1.9/1.10 守卫）
- Change 2 落地后：护栏扩展到 `mul.h` / `mul_div_fsm.h`；单一来源 grep 验收通过

---

## Risks & Rollback

### 风险 R1 — `isa` 默认值变更影响下游

- **信号**：Change 1 落地后 SoC 拓扑 / DSE 配置校验失败
- **缓解**：`declared-reserved` 降级路径保证 rv64 配置仍可加载（带 warn）；JSON 变更与代码变更同 change 提交，不拆

### 风险 R2 — `static_assert` 挡住已有的 `uint64_t` 显式实例化

- **信号**：`ip/cpu/plugins/reg_file.cpp` 编译失败
- **缓解**：该文件当前是死代码（未进任何 CMakeLists），Change 2 落地时同步删除或修正

### 风险 R3 — `xlen_bits` 单一来源在 CH_MEM 侧不适用

- **信号**：`ch_uint<32>` 的 `sizeof` ≠ 4 导致推导错误
- **缓解**：D3 已明确要求"显式映射层"，不是简单替换；映射层需自带单测

### 风险 R4 — 窗口期：Change 1 已落地、Change 2/D5 强制门禁未落地

在 `rv64-isa-declaration-correction`（Change 1）合入后、`rv64-xlen-width-guardrails`（Change 2，phase-5）与 D5 准入清单强制化落地前，存在"**声明已收紧、护栏未全覆盖**"的窗口：

1. `CpuFactory<uint64_t>`（模板 XLEN=64）在 TLM 路径的 `mul.h` / `mul_div_fsm.h` 语义点**仍无** `sizeof(T)==4` 守卫 —— 该两点护栏归 Change 2；`int_alu.h` / `branch.h` 已由 Change 1 任务 1.9/1.10 提供**编译期兜底**（任一处实例化 `uint64_t` 即编译失败，静默算错窗口在编译层关闭）
2. `CPUConfig::isa` 为运行期 `std::string`（`ip/cpu/cpu_factory.h:127`），D2 的 isa↔xlen 一致性只能是运行时 fail-fast，无法编译期拦截
3. 与"是否退役 TLM"完全正交：即便 feat-tlm-backend-retirement B3 删除 TLM，phase-4 → phase-5 窗口内 TLM 仍可编译、可被实例化

**缓解**：(a) ✅ **已采纳** —— Change 1 并入 TLM 侧 `static_assert(sizeof(T)==4)`（int_alu.h / branch.h 两个模板入口），窗口期风险在编译层退化；`mul.h` / `mul_div_fsm.h` 的残余窗口至 Change 2 关闭；(b) 窗口内任何涉及 `CpuFactory<uint64_t>` 的用例必须在 PR 描述显式声明并链接本 R4；(c) 变异对照：把 `branch.h` 的断言临时改成 `sizeof(T) == 8`，`[cpu]` 相关用例应编译失败 —— 证明守卫在实例化点生效，不是死代码

**关闭条件**：Change 2 落地（`mul.h` / `mul_div_fsm.h` 护栏 + 单一来源 grep 验收）或 TLM B3 删除（护栏对象消失，切 CH_MEM 侧 CI grep 门禁 —— 见 feat-rv64-isa-declaration-integrity 的 D4 形态切换）

### 回退

- D1（isa 默认值）：改回 `rv64gc` 即可，无结构依赖
- D3（单一来源）：常量保留，各 plugin 改回本地定义（但会丢失 D3 的漂移防护）
- D4（`static_assert`）：删断言即回退（但恢复"静默算错"风险）

---

## Related

- [`ip/cpu/docs/rv64_readiness_gap.md`](../../../ip/cpu/docs/rv64_readiness_gap.md) — 缺口表 SSOT（含全部 file:line 证据 + G1–G9 准入清单）
- **ADR-085 / ADR-086**（MMU PTW / L0-L1 TLB 架构，已归档设计）— MMU 侧已按 Sv39/Sv48 规划，CPU 侧 CH_MEM PTW 仍是 sv32（缺口表 §7 跨模块不一致）
- **ADR-047**（Result 范式）— `declared-reserved` 降级 warn 的自然载体
- **ADR-083**（Plugin-style 单一 source of truth）— TLM / CH_MEM 双模下 `xlen` 单一来源的可行性前提
