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
      // Phase 6d.4: fetch.FLUSH must exist before branch NORMAL reads it
      // (branch_compute gates its decision on the previous cycle's branch).
      if (std::string(stage_name) == "fetch") {
        n->operator()(cf::cpu::plugins::IBusPlugin<T>::FLUSH) =
            ch::core::ch_bool(false);
      }
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
    ibus->set_initial_pc(initial_pc);

    // DMemPlugin（可选预载 ELF）
    auto dmem = std::make_unique<DMem>();

    // Phase 6d.4: ELF 解析 + PT_LOAD segment 路由
    //   解析 ELF32 header + program headers, 把 PT_LOAD (p_flags&1=PF_X) 段
    //   写入 IBus, (p_flags&2=PF_W) 段写入 DMem, 偏移 = (p_vaddr-0x80000000)/4.
    //   非 ELF 原始字节 (mock 测试) 回退到旧 preload_words 行为.
    if (preload_elf && !elf_image.empty()) {
      bool is_elf = elf_image.size() >= 52 && elf_image[0] == 0x7F &&
                    elf_image[1] == 'E' && elf_image[2] == 'L' &&
                    elf_image[3] == 'F' && elf_image[4] == 1;
      if (is_elf) {
        std::uint32_t phoff = static_cast<std::uint32_t>(elf_image[28])
                            | (static_cast<std::uint32_t>(elf_image[29]) << 8)
                            | (static_cast<std::uint32_t>(elf_image[30]) << 16)
                            | (static_cast<std::uint32_t>(elf_image[31]) << 24);
        std::uint16_t phentsize = static_cast<std::uint16_t>(elf_image[42])
                                | (static_cast<std::uint16_t>(elf_image[43]) << 8);
        std::uint16_t phnum = static_cast<std::uint16_t>(elf_image[44])
                            | (static_cast<std::uint16_t>(elf_image[45]) << 8);
        constexpr std::uint32_t kElfBase = 0x80000000;
        for (std::uint16_t i = 0; i < phnum; ++i) {
          std::size_t off = phoff + static_cast<std::size_t>(i) * phentsize;
          if (off + 32 > elf_image.size()) continue;
          std::uint32_t p_type = static_cast<std::uint32_t>(elf_image[off])
                               | (static_cast<std::uint32_t>(elf_image[off+1]) << 8)
                               | (static_cast<std::uint32_t>(elf_image[off+2]) << 16)
                               | (static_cast<std::uint32_t>(elf_image[off+3]) << 24);
          std::uint32_t p_offset = static_cast<std::uint32_t>(elf_image[off+4])
                                 | (static_cast<std::uint32_t>(elf_image[off+5]) << 8)
                                 | (static_cast<std::uint32_t>(elf_image[off+6]) << 16)
                                 | (static_cast<std::uint32_t>(elf_image[off+7]) << 24);
          std::uint32_t p_vaddr = static_cast<std::uint32_t>(elf_image[off+8])
                                | (static_cast<std::uint32_t>(elf_image[off+9]) << 8)
                                | (static_cast<std::uint32_t>(elf_image[off+10]) << 16)
                                | (static_cast<std::uint32_t>(elf_image[off+11]) << 24);
          std::uint32_t p_filesz = static_cast<std::uint32_t>(elf_image[off+16])
                                 | (static_cast<std::uint32_t>(elf_image[off+17]) << 8)
                                 | (static_cast<std::uint32_t>(elf_image[off+18]) << 16)
                                 | (static_cast<std::uint32_t>(elf_image[off+19]) << 24);
          std::uint32_t p_flags = static_cast<std::uint32_t>(elf_image[off+24])
                                | (static_cast<std::uint32_t>(elf_image[off+25]) << 8)
                                | (static_cast<std::uint32_t>(elf_image[off+26]) << 16)
                                | (static_cast<std::uint32_t>(elf_image[off+27]) << 24);
          if (p_type != 1) continue;
          if (p_vaddr < kElfBase) continue;
          std::size_t word_offset = static_cast<std::size_t>(p_vaddr - kElfBase) / 4;
          std::size_t nwords = p_filesz / 4;
          if (nwords == 0) continue;
          std::vector<uint32_t> words;
          words.reserve(nwords);
          for (std::size_t j = 0; j < nwords; ++j) {
            std::size_t bo = static_cast<std::size_t>(p_offset) + j * 4;
            if (bo + 4 > elf_image.size()) { words.clear(); break; }
            words.push_back(static_cast<std::uint32_t>(elf_image[bo])
                          | (static_cast<std::uint32_t>(elf_image[bo+1]) << 8)
                          | (static_cast<std::uint32_t>(elf_image[bo+2]) << 16)
                          | (static_cast<std::uint32_t>(elf_image[bo+3]) << 24));
          }
          bool is_exec = (p_flags & 1u) != 0u;
          bool is_writable = (p_flags & 2u) != 0u;
          if (is_exec && !is_writable) {
            ibus->preload_segment(word_offset, words);
          } else if (is_writable) {
            dmem->preload_segment(word_offset, words);
          }
        }
        // 6d.4: tohost 固定位于 dmem word 0x1000/4 = 0x400 (0x80001000)
        dmem->set_tohost_probe_word(0x400);
      } else {
        // legacy: mock add.elf 原始指令字节 (无 ELF header)
        std::vector<uint32_t> words;
        words.reserve(elf_image.size() / 4);
        for (std::size_t i = 0; i + 4 <= elf_image.size(); i += 4) {
          words.push_back(static_cast<std::uint32_t>(elf_image[i])
                        | (static_cast<std::uint32_t>(elf_image[i+1]) << 8)
                        | (static_cast<std::uint32_t>(elf_image[i+2]) << 16)
                        | (static_cast<std::uint32_t>(elf_image[i+3]) << 24));
        }
        ibus->preload_words(words);
        dmem->preload(elf_image);
      }
    }

    pb->register_plugin(std::make_unique<Decoder>());
    pb->register_plugin(std::make_unique<Hazard>());
    pb->register_plugin(std::move(ibus));
    pb->register_plugin(std::make_unique<RegFile>());
    pb->register_plugin(std::make_unique<IntAlu>());
    pb->register_plugin(std::make_unique<Branch>());
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
        // Phase 6d.4: JAL link — rd = pc+4 (return address). The ALU does
        // not match JAL's opcode, so RD_DATA defaults to 0; override it here
        // using the execute stage's PC.
        const auto& exe_dec = exe->operator()(DecodePlugin::DECODED_INST);
        auto is_jal = (exe_dec.opcode ==
                       ch::core::ch_uint<7>(ch::core::ch_literal<0x6F, 7>{}));
        auto link = exe->operator()(KT::PC) +
                    ch::core::ch_uint<32>(ch::core::ch_literal<4, 32>{});
        mem->operator()(KT::RD_DATA) = select(is_jal, link, mem->operator()(KT::RD_DATA));
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

    // Phase 6d.4: fetch.PC exposes pc_lag (previous cycle's PC) to stay
    // aligned with the 1-cycle-latent imem aread (see IBusPlugin). All
    // stages then share the same instruction in the same cycle, so the
    // datapath must be fully combinational (no stage pipeline registers):
    // a reg written at cycle N's writeback is read at cycle N+1's decode,
    // giving correct 1-cycle-latency dependencies (AUIPC, ADDI t5,t5,...).
    (void)make_connector;

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