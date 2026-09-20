// ip/cpu/cpu_factory_chmem.h
//
// Phase 6d 6d.3: CpuFactory (CH_MEM) — 7-plugin 5-stage 流水线 CPU 集成
//   IBus + Decoder + RegFile + Hazard + IntAlu + Branch + DMem
//
// License: ChipForge Project (see top-level LICENSE file)

#ifndef CF_IP_CPU_CPU_FACTORY_CHMEM_H
#define CF_IP_CPU_CPU_FACTORY_CHMEM_H

#ifndef CF_PLUGIN_USE_CH_MEM
#error "cpu_factory_chmem.h requires CF_PLUGIN_USE_CH_MEM"
#endif

#include <cstdint>
#include <memory>
#include <vector>

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

#include "ip/cpu/plugins/ibus_chmem.h"
#include "ip/cpu/plugins/reg_file_chmem.h"
#include "ip/cpu/plugins/decode_chmem.h"
#include "ip/cpu/plugins/dmem_chmem.h"
#include "ip/cpu/arch/riscv/int_alu_chmem.h"
#include "ip/cpu/arch/riscv/decoder_table.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/arch/riscv/payload_riscv.h"

using namespace cf::plugin;
using namespace ch;
using namespace ch::core;
using namespace cf::cpu::arch::riscv;

// branch_chmem.h 和 hazard_chmem.h 在各自命名空间内, 放在 using 之后确保可见
#include "ip/cpu/plugins/branch_chmem.h"
#include "ip/cpu/plugins/hazard_chmem.h"

namespace cf {
namespace cpu {

template <typename T>
class CpuFactoryChmem {
  static constexpr std::size_t kXlenBits = 32;  // RV32 PoC

 public:
  using RegFile  = plugins::RegFilePlugin<T>;
  using IntAlu   = arch::riscv::RiscvIntAluPlugin<T>;
  using Branch   = plugins::BranchPlugin<T>;
  using Hazard   = ip::cpu::HazardPlugin<T>;
  using IBus     = plugins::IBusPlugin<T>;
  using Decoder  = plugins::RiscvDecodePluginChmem<T>;
  using DMem     = plugins::DMemPlugin<T>;

  // ==========================================================================
  // build_cpu — 7-plugin 5-stage
  //
  // 参数:
  //   memory — 保留向后兼容 (TLM 模式使用, CH_MEM 忽略)
  //   initial_pc — PC 起始地址 (默认 0x80000000, PoC 用 0)
  //   preload_elf — 启用 ELF 预载 (同时预载 IMem + DMem)
  //   elf_image — ELF 字节数组
  // ==========================================================================
  static std::unique_ptr<PipeBuilder> build_cpu(
      ch::core::context* elaboration_ctx,
      T* memory = nullptr,
      T initial_pc = T{0x80000000},
      bool preload_elf = false,
      const std::vector<uint8_t>& elf_image = {}) {
    (void)memory;
    auto pb = std::make_unique<PipeBuilder>(elaboration_ctx);

    using KT = cf::cpu::core::payload::keys<T, kXlenBits>;
    using RK = cf::cpu::arch::riscv::payload_keys_riscv<T>;

    // ======================================================================
    // 0. EARLY-stage payload pre-population
    //
    // 全部注册在 decode EARLY (而非各 stage 自身 EARLY):
    //   - 消费者 (hazard/regfile 在 decode NORMAL) 读 exe/mem/wb/branch 的
    //     DECODED_INST/RS1 等 cell 需先存在 (缺失 → null handle SEGV)
    //   - 生产者 LATE 链接 (decode LATE→exe/brn, execute LATE→mem, memory
    //     LATE→wb) 在 decode EARLY 之后执行, 覆写默认值为真实数据通路
    //   - 若 populate 注册在 exe/mem/wb/branch 自身 EARLY: execute EARLY 晚于
    //     decode LATE (stage 顺序), 会把 LATE 链接值覆写为 0 → 通路断裂
    // ======================================================================
    auto populate_stage = [pb_ptr = pb.get()](const char* stage_name) {
      using KT = cf::cpu::core::payload::keys<T, 32>;
      using RK = cf::cpu::arch::riscv::payload_keys_riscv<T>;
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      auto* n = pb_ptr->node_of_logic_stage(stage_name).get();
      if (!n) return;
      n->operator()(KT::RS1)        = T(ch::core::ch_literal<0, 32>{});
      n->operator()(KT::RS2)        = T(ch::core::ch_literal<0, 32>{});
      n->operator()(KT::PC)         = T(ch::core::ch_literal<0, 32>{});
      n->operator()(KT::RESULT)     = T(ch::core::ch_literal<0, 32>{});
      n->operator()(KT::RD_DATA)    = T(ch::core::ch_literal<0, 32>{});
      n->operator()(KT::INSTRUCTION)= T(ch::core::ch_literal<0, 32>{});
      cf::cpu::core::payload::DecodePayload default_decode{};
      n->payloads().put(KT::DECODE, default_decode);
      cf::cpu::arch::riscv::RiscvDecodeDetail default_detail{};
      n->payloads().put(RK::RISCV_DETAIL, default_detail);
      // DECODED_INST: 用真实 ch_literal 零值初始化 (v0.3.1 M6: 默认构造
      // DecodedInst{} 的 ch_uint 成员是 null impl → Hazard 比较 SEGV)
      cf::cpu::plugins::DecodedInst zero_decoded;
      zero_decoded.opcode    = ch::core::ch_uint<7>(ch::core::ch_literal<0, 7>{});
      zero_decoded.funct3    = ch::core::ch_uint<3>(ch::core::ch_literal<0, 3>{});
      zero_decoded.funct7    = ch::core::ch_uint<7>(ch::core::ch_literal<0, 7>{});
      zero_decoded.rd_idx    = ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{});
      zero_decoded.rs1_idx   = ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{});
      zero_decoded.rs2_idx   = ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{});
      zero_decoded.imm       = ch::core::ch_uint<32>(ch::core::ch_literal<0, 32>{});
      zero_decoded.reads_rs1 = ch::core::ch_bool(false);
      zero_decoded.reads_rs2 = ch::core::ch_bool(false);
      zero_decoded.writes_rd = ch::core::ch_bool(false);
      zero_decoded.op_class  = ch::core::ch_uint<8>(ch::core::ch_literal<0, 8>{});
      zero_decoded.instr_format = ch::core::ch_uint<3>(ch::core::ch_literal<0, 3>{});
      n->payloads().put(DecodePlugin::DECODED_INST, zero_decoded);
    };
    // fetch 在自身 EARLY populate (早于 IBus fetch NORMAL 覆写真实取指);
    // decode/exe/mem/wb/branch 在 decode EARLY 统一 populate (见上方说明)
    pb->at_stage("fetch", Phase::EARLY, [populate_stage]() { populate_stage("fetch"); });
    pb->at_stage("decode", Phase::EARLY, [populate_stage]() { populate_stage("decode"); });
    pb->at_stage("decode", Phase::EARLY, [populate_stage]() { populate_stage("execute"); });
    pb->at_stage("decode", Phase::EARLY, [populate_stage]() { populate_stage("memory"); });
    pb->at_stage("decode", Phase::EARLY, [populate_stage]() { populate_stage("writeback"); });
    pb->at_stage("decode", Phase::EARLY, [populate_stage]() { populate_stage("branch"); });

    // ======================================================================
    // 0b. fetch→decode INSTRUCTION forwarding (EARLY)
    //     在 EARLY 内执行, 在 NORMAL 之前把 fetch.INSTRUCTION 灌入 decode.
    //     注册在 populate_stage 之后, 以保证先初始化再覆写.
    // ======================================================================
    pb->at_stage("decode", Phase::EARLY, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, 32>;
      auto* fch = pb_ptr->node_of_logic_stage("fetch").get();
      auto* dec = pb_ptr->node_of_logic_stage("decode").get();
      if (fch && dec) {
        dec->operator()(KT::INSTRUCTION) = fch->operator()(KT::INSTRUCTION);
        dec->operator()(KT::PC)          = fch->operator()(KT::PC);
      }
    });

    // ======================================================================
    // 1. 注册 7 个 CH_MEM Plugin
    //    顺序: Decoder → Hazard → IBus → RegFile → IntAlu → Branch → DMem
    //    Decoder 必须最先注册: 其 build() 注册的 decode_closure (decode
    //    NORMAL) 必须早于 RegFile::build 的 id_decode 与 Hazard::build 的
    //    detect — 让 DECODED_INST 先被写入, 消费者再读 (elaboration 期
    //    DAG 构建顺序 = at_stage 注册顺序).
    // ======================================================================
    // 构建 IBusPlugin（可选预载 ELF）
    auto ibus = std::make_unique<IBus>();
    if (preload_elf && !elf_image.empty()) {
      std::vector<uint32_t> words;
      words.reserve(elf_image.size() / 4);
      for (std::size_t i = 0; i + 4 <= elf_image.size(); i += 4) {
        uint32_t w = static_cast<uint32_t>(elf_image[i])
                   | (static_cast<uint32_t>(elf_image[i+1]) << 8)
                   | (static_cast<uint32_t>(elf_image[i+2]) << 16)
                   | (static_cast<uint32_t>(elf_image[i+3]) << 24);
        words.push_back(w);
      }
      ibus->preload_words(words);
    }

    pb->register_plugin(std::make_unique<Decoder>());
    pb->register_plugin(std::make_unique<Hazard>());
    pb->register_plugin(std::move(ibus));
    pb->register_plugin(std::make_unique<RegFile>());
    pb->register_plugin(std::make_unique<IntAlu>());
    pb->register_plugin(std::make_unique<Branch>());

    // DMemPlugin（可选预载 ELF）
    auto dmem = std::make_unique<DMem>();
    if (preload_elf && !elf_image.empty()) {
      dmem->preload(elf_image);
    }
    pb->register_plugin(std::move(dmem));

    // ======================================================================
    // 2. Stage Linking (5 级流水线数据通路)
    //    每个闭包注册为 at_stage(<stage>, LATE):
    //    - decode(LATE): decode → execute (RS1/RS2/PC/DECODE/RV_DETAIL/INSTR)
    //    - decode(LATE): decode → branch  (RS1/RS2/PC/RV_DETAIL)
    //    - execute(LATE): execute → memory (RESULT/RD_DATA/DECODE/INSTR/RS2)
    //    - memory(LATE): memory → writeback (RESULT/RD_DATA/DECODE)
    // ======================================================================

    // 2a. decode(LATE): decode → execute + branch
    pb->at_stage("decode", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, 32>;
      using RK = cf::cpu::arch::riscv::payload_keys_riscv<T>;
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      auto* dec = pb_ptr->node_of_logic_stage("decode").get();
      auto* exe = pb_ptr->node_of_logic_stage("execute").get();
      auto* brn = pb_ptr->node_of_logic_stage("branch").get();
      if (dec && exe) {
        exe->operator()(KT::RS1)         = dec->operator()(KT::RS1);
        exe->operator()(KT::RS2)         = dec->operator()(KT::RS2);
        exe->operator()(KT::PC)          = dec->operator()(KT::PC);
        exe->operator()(KT::DECODE)      = dec->operator()(KT::DECODE);
        exe->operator()(RK::RISCV_DETAIL)= dec->operator()(RK::RISCV_DETAIL);
        exe->operator()(KT::INSTRUCTION) = dec->operator()(KT::INSTRUCTION);
        exe->operator()(DecodePlugin::DECODED_INST) = dec->operator()(DecodePlugin::DECODED_INST);
      }
      if (dec && brn) {
        brn->operator()(KT::RS1)         = dec->operator()(KT::RS1);
        brn->operator()(KT::RS2)         = dec->operator()(KT::RS2);
        brn->operator()(KT::PC)          = dec->operator()(KT::PC);
        brn->operator()(RK::RISCV_DETAIL)= dec->operator()(RK::RISCV_DETAIL);
        brn->operator()(DecodePlugin::DECODED_INST) = dec->operator()(DecodePlugin::DECODED_INST);
      }
    });

    // 2b. execute(LATE): execute → memory
    pb->at_stage("execute", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, 32>;
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      auto* exe = pb_ptr->node_of_logic_stage("execute").get();
      auto* mem = pb_ptr->node_of_logic_stage("memory").get();
      if (exe && mem) {
        mem->operator()(KT::RESULT)      = exe->operator()(KT::RESULT);
        mem->operator()(KT::RD_DATA)     = exe->operator()(KT::RD_DATA);
        mem->operator()(KT::DECODE)      = exe->operator()(KT::DECODE);
        mem->operator()(KT::INSTRUCTION) = exe->operator()(KT::INSTRUCTION);
        mem->operator()(KT::RS2)         = exe->operator()(KT::RS2);
        mem->operator()(DecodePlugin::DECODED_INST) = exe->operator()(DecodePlugin::DECODED_INST);
      }
    });

    // 2c. memory(LATE): memory → writeback
    pb->at_stage("memory", Phase::LATE, [pb_ptr = pb.get()]() {
      using KT = cf::cpu::core::payload::keys<T, 32>;
      using DecodePlugin = cf::cpu::plugins::RiscvDecodePluginChmem<T>;
      auto* mem_st = pb_ptr->node_of_logic_stage("memory").get();
      auto* wb    = pb_ptr->node_of_logic_stage("writeback").get();
      if (mem_st && wb) {
        wb->operator()(KT::RESULT)   = mem_st->operator()(KT::RESULT);
        wb->operator()(KT::RD_DATA)  = mem_st->operator()(KT::RD_DATA);
        wb->operator()(KT::DECODE)   = mem_st->operator()(KT::DECODE);
        wb->operator()(DecodePlugin::DECODED_INST) = mem_st->operator()(DecodePlugin::DECODED_INST);
      }
    });

    // ======================================================================
    // 3. Pipeline register connectors
    //    ID/EX: RS1, RS2, PC, INSTRUCTION
    //    EX/WB: RESULT, RD_DATA
    // ======================================================================
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

    pb->register_stage_payload_connector<T>("execute", KT::RS1,         make_connector("id_ex_rs1"));
    pb->register_stage_payload_connector<T>("execute", KT::RS2,         make_connector("id_ex_rs2"));
    pb->register_stage_payload_connector<T>("execute", KT::PC,          make_connector("id_ex_pc"));
    pb->register_stage_payload_connector<T>("execute", KT::INSTRUCTION, make_connector("id_ex_instr"));
    pb->register_stage_payload_connector<T>("writeback", KT::RESULT,    make_connector("ex_wb_result"));
    pb->register_stage_payload_connector<T>("writeback", KT::RD_DATA,   make_connector("ex_wb_rd_data"));

    pb->build();
    return pb;
  }

  // ==========================================================================
  // build_cpu_with_elaborate — 便利包装
  // ==========================================================================
  static std::unique_ptr<PipeBuilder> build_cpu_with_elaborate(
      ch::core::context* ctx,
      const std::string& verilog_path = "/tmp/cpu.v",
      T* memory = nullptr,
      T initial_pc = T{0x80000000},
      bool preload_elf = false,
      const std::vector<uint8_t>& elf_image = {}) {
    auto pb = build_cpu(ctx, memory, initial_pc, preload_elf, elf_image);
    pb->elaborate(*ctx);
    pb->to_verilog(verilog_path);
    return pb;
  }

  struct Config {
    T initial_pc = T{0x80000000};
    std::size_t pipeline_stages = 5;
    bool enable_mmu = false;
  };
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CPU_FACTORY_CHMEM_H