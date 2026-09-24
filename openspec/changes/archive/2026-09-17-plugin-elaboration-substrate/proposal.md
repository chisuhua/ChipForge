# Phase 6c: cf::plugin elaboration 重解释（Plugin → CppHDL 落地）

## Why

### 现状（v0.2.x，2026-09）

`cf::plugin` 是 TLM 仿真框架：
- `Payload<T>` 用 `std::any` 做运行期类型擦除（payload.h:153）
- `PipeBuilder::run()` 把 `at_stage` 闭包当作每周期执行的仿真回调（pipe_builder.h:114-142）
- `array_store` 后端是 `std::array`，`commit()` 是 no-op（ADR-040）
- `CtrlLink::halt_when(std::function<bool()>)` 在运行期求值

`ip/*/rtl/` 全是占位 README（`ip/cpu/rtl/README.md:7` 写"Phase 5 规划中"）。
`README.md:1` 承诺的"CppTLM + CppHDL-based"**HDL 侧 0% 兑现**。
D4 决策（`decision-plugin-framework-2026-06-08.md:289`）"最不可逆"承诺"业务代码不被破坏"，但 HDL 路径从未启动。

### 核心洞察（来自 Phase 6c 调研）

VexRiscv 能编译到 Verilog，**不是因为 Scala 翻译了 Plugin.scala，是因为"执行即布线"**：
- Scala 没有"运行期/编译期"之分
- Plugin.build() 里的每一行代码 elaboration 时执行一次
- 操作符重载把表达式录成 SpinalHDL AST/netlist
- `addPrePopTask` 调度 elaboration 阶段（C++ 已有等价物：setup/build 两阶段）

`cf::plugin` 的根本问题不是 API 形态，是**底层语义**：lambda 闭包被当作"每周期解释"而非"elaboration 一次发射"。

### Why now

- **P0 任务**：本项目"基于 Plugin 风格的硬件设计"承诺 2 年未兑现，是项目根本目标
- **W0 审计已通过**：`docs/audit/cppHDL-maturity-audit.md` 确认 CppHDL 基础设施完整可支撑
- **范围纪律**：9 周 PoC + 单点突破，不贪多（MMU/Cache/总线推迟）
- **不发明翻译器**：复用 `ch::toVerilog()` 后端，零 codegen 工作量

## What Changes

### 设计原则

| 原则 | 内容 |
|------|------|
| **保留 API 表面** | `PluginBase::setup/build`、`at_stage`、`Payload<T>`、`CtrlLink` API 形态保留 |
| **翻转底层语义** | "每周期执行闭包" → "elaboration 一次，发射 lnode DAG" |
| **复用 CppHDL 后端** | `ch::toVerilog()` 复用，零自定义 codegen |
| **范围纪律** | 9 周只覆盖 RegFile + IntALU + 5 级流水线算术子集 |

### 1. cf::plugin 底层翻转

**`uint_t.h` 翻转**（W1-2）：
```cpp
// 旧（uint_t.h:42-46）
template <unsigned N> using uint_t = typename uint_t_impl<N>::type;  // → uint32_t
using bool_t = bool;

// 新
template <unsigned N> using uint_t = ch::core::ch_uint<N>;
using bool_t = ch::core::ch_bool;
```

**`PayloadStore` cell 改造**：
```cpp
// 旧（payload.h:153）
std::map<const PayloadKeyBase*, std::any> cells_;  // 装 POD 值

// 新：cell 装 ch 代理对象（std::any 保留，类型擦除发生在 elaboration）
// get<T>/set<T> 接口不变，内部 get() 仍做 typeid 校验
```

**`PipeBuilder` 改造**：
```cpp
// 旧（pipe_builder.h:114-142）
void run();  // 每周期执行所有 at_stage 闭包

// 新
void elaborate();  // 执行所有 at_stage 闭包一次，发射 lnode DAG
void run() = delete;  // 或标记 [[deprecated]]
```

### 2. Stage plumbing（M2，W3-4）

**借鉴 VexRiscv `Stageable.insert/input/output` 语义**：
- 每个 PipeNode（per-stage）持有独立信号实体
- stage 间自动插 `ch_reg`（`next_proxy->next = prev_output`，带 stall/flush gating）

**`CtrlLink` ch_bool 化**：
```cpp
// 旧（ctrl_link.h:26）
using Condition = std::function<bool()>;
CtrlLink& halt_when(Condition cond);

// 新
CtrlLink& halt_when(ch_bool cond);   // OR 合并到 stage valid/ready 门控
CtrlLink& flush_when(ch_bool cond);
```

**`array_store` 后端切 ch_mem**（ADR-040 §4 步骤 5 立即执行）：
```cpp
// storage.h 增加 ch_mem 特例化（TLM 模式保留 std::array 后端）
template <typename T, std::size_t N>
class array_store {
#ifdef CF_PLUGIN_USE_CH_MEM
    ch::core::ch_mem<T, N> mem_;  // RTL 后端
#else
    std::array<T, N> data_;      // TLM 后端（Phase 6c 兼容）
);
}
```

### 3. 业务代码改写模式（M3-M4）

**唯一主战场：if/else 链 → select 树**。`ch_bool` 提供 `explicit operator bool()`，**但 C++17 contextual conversion（`if`/`while`/`!`/`?:`）会使用 explicit 转换函数**——`if(ch_bool)` **会编译通过且静默取 false**（DAG 静默截断），**类型系统不提供任何编译期保护**。纪律靠 `tools/check_plugin_portability.sh` v2.0 Check 5 的 CI grep 强制执行（详见 `docs/audit/cppHDL-maturity-audit.md` §3 勘误）。

**示例**（`int_alu.h` 改写）：
```cpp
// 旧
if (rv.opcode == opcode::OP_AUIPC) { result = pc_val + rv.imm; }
else if (rv.opcode == opcode::OP_LUI) { result = rv.imm; }
else { result = compute(op, rs1_val, op2); }

// 新
auto is_auipc = (rv.opcode == ch_uint<7>(opcode::OP_AUIPC));   // ch_bool
auto is_lui   = (rv.opcode == ch_uint<7>(opcode::OP_LUI));
result = select(is_auipc, pc_val + rv.imm,
        select(is_lui,   rv.imm,
                         compute_mux(op, rs1_val, op2)));
```

### 4. ADR 修订

- **ADR-040 修订**：Tier-1 #5 改为"允许 ch 类型渗透"（因为 ch 是 elaboration 正道）；新增"at_stage 回调内禁止运行期 if(ch_bool)"（编译期已强制，加 CI 冗余）
- **ADR-037 修订**：D4 范式在 elaboration 语义下兑现（不只是 TLM 仿真）
- **新增 ADR**：多周期协议引擎（MMU PTW、Cache refill FSM）豁免 D4 "无状态机"禁令，但必须用 `chlib::ch_state_machine` 而非手写 `enum class + switch`

### 5. CHANGELOG 声明

v0.2.x 增加条目：
> "TLM 仿真层将于 v0.3.0 废弃（Phase 6c）；cf::plugin 改为 elaboration DSL；仿真由 CppHDL simulator 或 Verilator 承担。"

## Acceptance

### M1（W1-2）退出标准
- [ ] `uint_t.h` 翻转完成，`tests/framework/test_uint_t.cpp` PASS
- [ ] `PayloadStore` cell 改造完成，`tests/framework/test_payload.cpp` PASS
- [ ] `PipeBuilder::elaborate()` 新增，`run()` 删除/标记 deprecated
- [ ] **第一个 hello.v PoC**：10 行 at_stage lambda（`c = a + b → ch_reg`）生成 `hello.v`，端口/位宽正确

### M2（W3-4）退出标准
- [ ] Per-stage 信号实体（insert/input/output 语义）
- [ ] 自动 stage 间 `ch_reg` 插入 + stall/flush 门控
- [ ] `CtrlLink::halt_when(ch_bool)` + OR 合并借鉴 `chlib::stream_halt_when`
- [ ] `array_store` 后端切 `ch_mem`（TLM 兼容 std::array 双模）
- [ ] **2-stage 流水线带 stall** 生成 Verilog，CppHDL sim 跑 10 周期与参考 trace 逐拍一致

### M3（W5-6）退出标准

> **2026-09-17 修订 v2 (Oracle/Metis 审查后)**: M3 范围降级. 不跑 add.elf 端到端 (超出 RegFile+ALU 范围); 改为 RegFile+ALU 单元级 sim + TLM↔CH_MEM 字节对标.

- [ ] **M3/Prereq-1..6 完成** (6 项前置: 4 框架 API + 2 .disabled 缺陷 + 1 范围降级, 详见 tasks.md)
- [ ] `RegFilePlugin` 改写：32 个独立 `ch_reg` (不用 array_store) + 读/写端口 + x0 屏蔽用 `select`
- [ ] `RiscvIntAluPlugin` 改写：if/else → 11 op select 树 (借鉴 `CppHDL/examples/riscv-mini/src/rv32i_alu.h:172`)
- [ ] `regfile.v` + `alu.v` 生成
- [ ] **M3-PoC**: RegFile+ALU 单元级 sim (手喂 payload, 不跑 add.elf 端到端)
  + TLM↔CH_MEM 字节对标 (复用 M2/Spike-6 协议)
  - 降级原因: RegFile+ALU 无 fetch/无 instruction memory/cpu_factory 骨架明示
    memory=nullptr; 跑 ELF 超出 M3 范围, 推到 M5
- [ ] `tests/CMakeLists.txt` 扩展: `chipforge_tests_chmem` 显式列表加 `test_cpu_rtl_regfile_alu.cpp`

### M4（W7-8）退出标准

> **2026-09-17 修订 v2 (Oracle/Metis 审查后)**:
> 1. 范围明确: **最小** RV32I 算术子集 (5 指令) 而非"完整 RV32I" (原 spec 失准)
> 2. DecoderPlugin 改写: **select 树分发表** (不是 2^32 ROM, 原 spec 4 GB 不可能)
> 3. HazardPlugin 是 W7 **必须** 不可推迟 (无 hazard 5-stage 跑不出 rv32ui tohost=1)
> 4. riscv-tests 端到端 **推 M5** (M4 接受降级为"5-stage sim tick 不 crash")

- [ ] `IntAluPlugin` 内嵌 Decoder: 译码表 mux 化 (select 树, 不是 ROM)
- [ ] `BranchPlugin` 改写：branch decision select 树 (新建 `branch_chmem.h`)
- [ ] **HazardPlugin 改写**: RAW 检测组合逻辑 + `CtrlLink::halt_when(ch_bool)` 接到 stall 门控 (新建 `hazard_chmem.h`, 借鉴 `hazard_unit.h`)
- [ ] **最小 RV32I 算术子集 5 级 Verilog** (add/addi/auipc/jal/beq 5 指令, **不是完整 RV32I**)
- [ ] **M4-PoC**: 5-stage elaborate + `toVerilog` + CppHDL Simulator tick 不 crash (riscv-tests 推 M5)
- [ ] (可选) `cpu.v` Yosys 综合: `yosys -p "read_verilog cpu.v; synth; stat"` (Fallback: `iverilog -t null` / `verilator --lint-only`)

### M5（W9）退出标准

> **2026-09-17 修订 v2**: M5 范围扩展. riscv-tests 端到端从 M4 推到 M5. 不再仅是文档收口.

- [ ] `[cpu-integration]` 测试从 `pb.run()` 迁到 CppHDL sim runner
- [ ] `tools/check_plugin_portability.sh` 增 Check 7 (TLM-only 文件不应含 ch 类型实例化)
- [ ] ADR-040/037 修订 + 新增"多周期豁免" ADR-046 完整化
- [ ] `ctest` 全绿 (双 target: TLM 47+ + CH_MEM 8+)
- [ ] `ch::toVerilog("cpu.v", ctx)` 生成**最小** RV32I 算术子集核 (不是"完整 RV32I", 原 spec 失准)
- [ ] **新增**: 重启用 `tests/cpu/test_cpu_chmem_riscv_tests.cpp.disabled` (7.9 KB), 在 CppHDL Simulator 下跑 riscv-tests add/addi/auipc/jal/beq 5 个 ELF **tohost=1** (对齐 `[cpu-l1-mmu-demo]` 5 用例) — **Phase 6c 最终硬证据**
- [ ] CHANGELOG v0.3.0 标记 TLM 仿真层废弃 + 列出 M1-M5 commit hash

### Capabilities（影响规范）
- 修改 `cf-plugin` capability：at_stage 从"仿真回调"改为"elaboration 闭包"
- 修改 `cf-tlm-rtl-bridge` capability：`array_store` 后端切换条件

## Impact

### 战略影响
- ✅ **兑现 README 承诺**：CppTLM + CppHDL-based 双层
- ✅ **兑现 D4 决策**：Plugin 范式在 elaboration 语义下落地
- ✅ **兑现 ADR-037**：Plugin 作为设计范式不可逆

### 风险
- ⚠️ **pb.run() 死亡是静默范式断裂**：[cpu-integration] 测试 harness 必须迁移到 CppHDL sim
- ⚠️ **MMU/PTW 与 D4 冲突**：必须 ADR 豁免（ch_state_machine 实现是简化版）
- ⚠️ **行为基线失效**：riscv-tests baseline matrix 需要重基线化（不是回归，看起来像回归）
- ⚠️ **M5 范围内 IBus/DBus LOAD width 推迟**：相关 LOAD 测试用例继续走原 TLM 路径（脱机分支）

### 不影响
- `tools/cpu_sim/main.cpp`（CPU 端到端测试 CLI）：M5 迁移到 CppHDL sim 后保留 CLI 入口
- `[cpu]` 单元测试：plugin 体改写后测试用例同步更新
- 业务算法层（`ip/mmu/lib/`）：零改动（lib/tlm 分层本来就是这个方向）
- `src/cf_plugin/bridge/`（Bridge Adapter）：独立路径，不在 Phase 6c 范围

## Tasks（实现任务列表）

详见 `tasks.md`。

## 关联决策

- **D4**（decision-plugin-framework-2026-06-08.md）：Plugin-style 强制，本 change 兑现 elaboration 语义
- **ADR-037**：Plugin 作为设计范式，本 change 落地 RTL
- **ADR-040**：TLM→HDL 移植性约束，本 change 翻转 #5，新增"运行期 if 禁令"

## OpenSpec 标准

按 `openspec/AGENTS.md` 规范：`proposal.md` + `tasks.md` + `specs/cf-plugin/spec.md`（如有 capability 修改）。