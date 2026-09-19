// ip/cpu/cpu_factory_chmem.h
//
// 功能描述: CpuFactory (CH_MEM 模式) — 5 级流水线 RISC-V CPU 集成 (M4/W8)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17 (Phase 6c M4 W8: 4 plugin 集成 + 阶段链接 + pipeline_reg)
//
// M4/W8 变更:
//   - 从 .disabled 更名为 .h (M3/W5 后解锁)
//   - 集成 4 个 CH_MEM Plugin: RegFilePlugin + RiscvIntAluPlugin +
//     BranchPlugin + HazardPlugin (RAW detection)
//   - 5 级流水线阶段链接: decode → execute → memory → writeback
//     (data flow via at_stage LATE copy + pipeline_reg connectors)
//   - CtrlLink 注册: Hazard halt (decode) + Branch flush (branch)
//   - D4 合规修复: static_assert(is_unsigned) 仅在 TLM,
//     kXlenBits=32 在 CH_MEM (RV32 PoC), ch_literal 替代 _d 字面值
//
// 设计:
//   - 仅在 CF_PLUGIN_USE_CH_MEM 下编译 (elaboration 路径)
//   - 5 级流水线阶段: fetch → decode → execute → memory → writeback
//     (额外 "branch" 阶段由 BranchPlugin 声明)
//   - Plugin 注册顺序: Hazard → RegFile → IntAlu → Branch
//   - 阶段链接通过 at_stage(LATE) 闭包 + register_stage_payload_connector
//   - 模板参数化 <typename T>: T = xlen 类型 (TLM=uint32/uint64, CH_MEM=ch_uint<32>)
//
// 阶段链接归纳 (Stage Linking Summary):
//   ├─ decode(LATE): copy RS1, RS2, PC, DECODE, RISCV_DETAIL → execute
//   ├─ decode(LATE): copy RS1, RS2, PC, RISCV_DETAIL → branch
//   ├─ execute(LATE): copy RESULT, RD_DATA, DECODE → memory
//   └─ memory(LATE): copy RESULT, RD_DATA, DECODE → writeback
//   然后 pipeline_reg 插入:
//   ├─ execute.RS1  → pipeline_reg (ID/EX)
//   ├─ execute.RS2  → pipeline_reg (ID/EX)
//   ├─ execute.PC   → pipeline_reg (ID/EX)
//   ├─ writeback.RESULT  → pipeline_reg (EX/WB)
//   └─ writeback.RD_DATA → pipeline_reg (EX/WB)
//
// 借鉴:
//   - TLM 版本: ip/cpu/cpu_factory.h
//   - CppHDL/examples/riscv-mini/src/rv32i_pipeline.h (5-stage 参考)
//   - VexRiscv Pipeline.build() 调度顺序
//   - test_elaborate_connector.cpp (register_stage_payload_connector 模式)
//
// 约束:
//   - 必须 -DCF_PLUGIN_USE_CH_MEM
//   - D4 合规: 工厂只做 Plugin 注册 + 阶段链接, 不做业务逻辑
//   - static_assert(std::is_unsigned<T>) 在 CH_MEM 下跳过 (T=ch_uint<N>)
//   - ch_literal<V,W>{} 替代 _d 字面值 (D4 合规)
//   - ch_bool() 包装 C++ bool → ch_bool (D4 合规)
//
// PoC 简化:
//   - 无真实 Instruction Fetch (PC 恒值, initial_pc unused)
//   - Memory 阶段 pass-through (RESULT/RD_DATA 直通)
//   - HazardPlugin 的 RAW 检测 + BranchPlugin 的 flush 已注册,
//     但 pipeline_reg stall/flush 为 false (PoC 不触发)
//   - 完整 RTL sim + 端到端验证在 M5

#ifndef CF_IP_CPU_CPU_FACTORY_CHMEM_H
#define CF_IP_CPU_CPU_FACTORY_CHMEM_H

#ifndef CF_PLUGIN_USE_CH_MEM
#error "cpu_factory_chmem.h requires CF_PLUGIN_USE_CH_MEM"
#endif

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <chlib/pipeline.h>
#include <core/context.h>
#include <core/bool.h>
#include <core/uint.h>

#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/payload.h"
#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/uint_t.h"

#include "ip/cpu/plugins/reg_file_chmem.h"
#include "ip/cpu/arch/riscv/int_alu_chmem.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;
// 导入 riscv 命名空间: branch_chmem.h 使用未限定 payload_keys_riscv/opcode (M4/W7 预存遗留)
using namespace cf::cpu::arch::riscv;

#include "ip/cpu/plugins/branch_chmem.h"
#include "ip/cpu/plugins/hazard_chmem.h"

namespace cf {
namespace cpu {

// ============================================================================
// CpuFactoryChmem<T>: 5 级流水线 CPU 集成 (CH_MEM)
//
// 阶段名 (VexRiscv 风格):
//   - "fetch"      (IF  阶段): 取指 (PoC: 无真实 IF, PC 恒值)
//   - "decode"     (ID  阶段): 译码 + 读寄存器 (RegFilePlugin + HazardPlugin)
//   - "branch"     (BR  阶段): 分支决策 (BranchPlugin, 与 execute 并行的单独节)
//   - "execute"    (EX  阶段): ALU 计算 (RiscvIntAluPlugin)
//   - "memory"     (MEM 阶段): Memory 访问 (PoC: pass-through)
//   - "writeback"  (WB  阶段): 写回寄存器 (RegFilePlugin wb_writeback)
//
// canonical_order (由 at_stage 注册顺序决定):
//   decode → execute → memory → writeback → branch
//   全部 decode 阶段先跑(NORMAL→LATE), 然后 execute(NORMAL→LATE), 以此类推.
//   LATE 闭包在 NORMAL 之后执行, 保证
//     decode(LATE) copy → execute(NORMAL) 顺序正确
// ============================================================================
template <typename T>
class CpuFactoryChmem {
#ifndef CF_PLUGIN_USE_CH_MEM
  static_assert(std::is_unsigned<T>::value,
                "CpuFactoryChmem<T>: T must be unsigned (uint32/uint64)");
#endif
  static constexpr std::size_t kXlenBits =
#ifdef CF_PLUGIN_USE_CH_MEM
      32;  // CH_MEM PoC: 固定 RV32 (T=ch_uint<32> 时 sizeof(T)*8 ≠ 32)
#else
      sizeof(T) * 8;
#endif

 public:
  using RegFile = plugins::RegFilePlugin<T>;
  using IntAlu  = arch::riscv::RiscvIntAluPlugin<T>;
  using Branch  = plugins::BranchPlugin<T>;
  using Hazard  = ip::cpu::HazardPlugin<T>;

  // ==========================================================================
  // build_cpu — 主入口
  //
  // 阶段编排 (M4/W8):
  //   1. register_plugin (Hazard → RegFile → IntAlu → Branch)
  //   2. at_stage(LATE) 阶段链接闭包 (decode→execute, execute→memory, memory→writeback,
  //      decode→branch)
  //   3. register_stage_payload_connector pipeline_reg 插入
  //   4. pb->build(): 触发所有 Plugin 的 setup()+build()
  //   5. 返回 PipeBuilder
  //
  // 调用者后续: pb->elaborate() + pb->to_verilog() + create_simulator()
  // 便利包装: build_cpu_with_elaborate()
  // ==========================================================================
  static std::unique_ptr<PipeBuilder> build_cpu(
      ch::core::context* elaboration_ctx,
      T* /*memory*/ = nullptr,
      T initial_pc = T{0x80000000}) {
    (void)initial_pc;  // PoC: 无 fetch stage, PC 暂恒值
    auto pb = std::make_unique<PipeBuilder>(elaboration_ctx);

    // ========================================================================
    // 0. EARLY-stage payload pre-population (Phase 6c M6)
    //    每个 stage(EARLY) 在 NORMAL 之前执行, 用 literal 0 / 默认 struct 占位,
    //    防止阶段链接和插件从 null-impl cell 传播
    //    (5-stage Simulator SEGV 根因之一: PayloadStore emplace-on-miss)
    // ========================================================================
    auto populate_stage = [pb_ptr = pb.get()](const char* stage_name) {
      using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
      using RK = cf::cpu::arch::riscv::payload_keys_riscv<T>;
      auto* n = pb_ptr->node_of_logic_stage(stage_name).get();
      if (!n) return;
      n->operator()(KT::RS1)     = T(ch::core::ch_literal<0, kXlenBits>{});
      n->operator()(KT::RS2)     = T(ch::core::ch_literal<0, kXlenBits>{});
      n->operator()(KT::PC)      = T(ch::core::ch_literal<0, kXlenBits>{});
      n->operator()(KT::RESULT)  = T(ch::core::ch_literal<0, kXlenBits>{});
      n->operator()(KT::RD_DATA) = T(ch::core::ch_literal<0, kXlenBits>{});
      cf::cpu::core::payload::DecodePayload default_decode{};
      n->payloads().put(KT::DECODE, default_decode);
      cf::cpu::arch::riscv::RiscvDecodeDetail default_detail{};
      n->payloads().put(RK::RISCV_DETAIL, default_detail);
    };
    pb->at_stage("fetch",     Phase::EARLY, [populate_stage]() { populate_stage("fetch"); });
    pb->at_stage("decode",    Phase::EARLY, [populate_stage]() { populate_stage("decode"); });
    pb->at_stage("execute",   Phase::EARLY, [populate_stage]() { populate_stage("execute"); });
    pb->at_stage("memory",    Phase::EARLY, [populate_stage]() { populate_stage("memory"); });
    pb->at_stage("writeback", Phase::EARLY, [populate_stage]() { populate_stage("writeback"); });
    pb->at_stage("branch",    Phase::EARLY, [populate_stage]() { populate_stage("branch"); });

    // ========================================================================
    // 1. 注册 4 个 CH_MEM Plugin
    //    顺序: Hazard (setup→ctrl_link) → RegFile (at_stage decode+writeback)
    //          → IntAlu (at_stage execute) → Branch (at_stage branch + ctrl_link)
    // ========================================================================
    pb->register_plugin(std::make_unique<Hazard>());
    pb->register_plugin(std::make_unique<RegFile>());
    pb->register_plugin(std::make_unique<IntAlu>());
    pb->register_plugin(std::make_unique<Branch>());

    // ========================================================================
    // 2. 阶段链接 (stage linking) — 在 build() 前注册, 确保 canonical_order
    //    按流水线数据流: decode → execute → memory → writeback → branch
    //
    //    每个链接闭包注册为 at_stage(<stage>, LATE):
    //    - 在 decode(LATE): 复制 decode → execute, decode → branch
    //    - 在 execute(LATE): 复制 execute → memory
    //    - 在 memory(LATE): 复制 memory → writeback
    //
    //    LATE phase 在当前 stage 的 NORMAL 之后执行, 所以:
    //      decode(NORMAL: id_decode) → decode(LATE: copy) → execute(NORMAL: alu)
    //      数据流正确。
    // ========================================================================
    using KeyType = cf::cpu::core::payload::keys<T, kXlenBits>;
    using RvKey   = cf::cpu::arch::riscv::payload_keys_riscv<T>;

    // 2a. decode(LATE): decode → execute + branch
    //     (RS1, RS2, PC, DECODE, RISCV_DETAIL)
    pb->at_stage("decode", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
      using RK = cf::cpu::arch::riscv::payload_keys_riscv<T>;
      auto* dec = pb_ptr->node_of_logic_stage("decode").get();
      auto* exe = pb_ptr->node_of_logic_stage("execute").get();
      auto* brn = pb_ptr->node_of_logic_stage("branch").get();

      if (dec && exe) {
        exe->operator()(KT::RS1)    = dec->operator()(KT::RS1);
        exe->operator()(KT::RS2)    = dec->operator()(KT::RS2);
        exe->operator()(KT::PC)     = dec->operator()(KT::PC);
        exe->operator()(KT::DECODE) = dec->operator()(KT::DECODE);
        exe->operator()(RK::RISCV_DETAIL) = dec->operator()(RK::RISCV_DETAIL);
      }
      if (dec && brn) {
        brn->operator()(KT::RS1)    = dec->operator()(KT::RS1);
        brn->operator()(KT::RS2)    = dec->operator()(KT::RS2);
        brn->operator()(KT::PC)     = dec->operator()(KT::PC);
        brn->operator()(RK::RISCV_DETAIL) = dec->operator()(RK::RISCV_DETAIL);
      }
    });

    // 2b. execute(LATE): execute → memory
    //     (RESULT, RD_DATA, DECODE)
    pb->at_stage("execute", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
      auto* exe = pb_ptr->node_of_logic_stage("execute").get();
      auto* mem = pb_ptr->node_of_logic_stage("memory").get();
      if (exe && mem) {
        mem->operator()(KT::RESULT)  = exe->operator()(KT::RESULT);
        mem->operator()(KT::RD_DATA) = exe->operator()(KT::RD_DATA);
        mem->operator()(KT::DECODE)  = exe->operator()(KT::DECODE);
      }
    });

    // 2c. memory(LATE): memory → writeback
    //     (RESULT, RD_DATA, DECODE)
    pb->at_stage("memory", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
      auto* mem = pb_ptr->node_of_logic_stage("memory").get();
      auto* wb  = pb_ptr->node_of_logic_stage("writeback").get();
      if (mem && wb) {
        wb->operator()(KT::RESULT)   = mem->operator()(KT::RESULT);
        wb->operator()(KT::RD_DATA)  = mem->operator()(KT::RD_DATA);
        wb->operator()(KT::DECODE)   = mem->operator()(KT::DECODE);
      }
    });

    // ========================================================================
    // 3. Pipeline register connectors
    //
    //    register_stage_payload_connector<T>(stage, key, connector)
    //    在 elaborate() 的 commit_payload_map 阶段执行, 读取 target stage
    //    的 PayloadStore cell, 用 chlib::pipeline_reg<N>(...) 包装,
    //    写回 cell. 产生 always_ff @(posedge) 块 (Verilog 证据).
    //
    //    只用于 ch_uint 类型 (DECODE/RISCV_DETAIL 是 struct 不能 pipeline_reg).
    // ========================================================================
    auto make_connector =
        [](const std::string& suf) {
          return [suf](lnodeimpl* prev, ch_bool stall, ch_bool flush,
                       const std::string& name) -> lnodeimpl* {
            T prev_signal(prev);
            ch_bool rst(false);
            auto pipelined = chlib::pipeline_reg<kXlenBits>(
                prev_signal, rst, stall, flush, name + "_" + suf);
            return pipelined.impl();
          };
        };

    // ID/EX pipeline_reg: execute 的 RS1, RS2, PC
    pb->register_stage_payload_connector<T>(
        "execute", KeyType::RS1, make_connector("id_ex_rs1"));
    pb->register_stage_payload_connector<T>(
        "execute", KeyType::RS2, make_connector("id_ex_rs2"));
    pb->register_stage_payload_connector<T>(
        "execute", KeyType::PC,  make_connector("id_ex_pc"));

    // EX/WB pipeline_reg: writeback 的 RESULT, RD_DATA
    pb->register_stage_payload_connector<T>(
        "writeback", KeyType::RESULT,  make_connector("ex_wb_result"));
    pb->register_stage_payload_connector<T>(
        "writeback", KeyType::RD_DATA, make_connector("ex_wb_rd_data"));

    // ========================================================================
    // 4. pb->build(): 触发所有 Plugin 的 setup() → build()
    //    - HazardPlugin setup: create CtrlLink, register_ctrl_link("decode")
    //    - RegFilePlugin setup: at_stage("decode", NORMAL), at_stage("writeback", LATE)
    //    - IntAluPlugin build: at_stage("execute", NORMAL)
    //    - BranchPlugin setup: at_stage("branch", NORMAL), register_ctrl_link("branch")
    //
    //    此时 canonical_order 包含 decode(LATE), execute(LATE), memory(LATE)
    //    (已由第 2 步注册) + decode(NORMAL), writeback(LATE), execute(NORMAL),
    //    branch(NORMAL) (由 Plugin 注册).
    //    最终 canonical_order = [decode, execute, memory, writeback, branch]
    // ========================================================================
    pb->build();

    return pb;
  }

  // ==========================================================================
  // build_cpu_with_elaborate — 便利包装
  //
  //   1. build_cpu() — 注册 Plugin + 阶段链接 + pipeline_reg
  //   2. pb->elaborate() — 执行所有 at_stage 闭包 + commit_payload_map
  //   3. pb->to_verilog() — 生成 /tmp/cpu.v
  //
  //   M4-PoC 测试使用此函数快速验证:
  //     - build 不抛异常
  //     - elaborate 不 crash
  //     - cpu.v 含 module + always_ff
  // ==========================================================================
  static std::unique_ptr<PipeBuilder> build_cpu_with_elaborate(
      ch::core::context* ctx,
      const std::string& verilog_path = "/tmp/cpu.v",
      T* memory = nullptr,
      T initial_pc = T{0x80000000}) {
    auto pb = build_cpu(ctx, memory, initial_pc);
    pb->elaborate(*ctx);
    pb->to_verilog(verilog_path);
    return pb;
  }

  // ==========================================================================
  // Config — 简化版 (Phase 6c PoC; 完整 CPUConfig 见 cpu_factory.h)
  // ==========================================================================
  struct Config {
    T initial_pc = T{0x80000000};
    std::size_t pipeline_stages = 5;
    bool enable_mmu = false;
  };
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CPU_FACTORY_CHMEM_H