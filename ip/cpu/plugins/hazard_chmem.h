// ip/cpu/plugins/hazard_chmem.h
//
// HazardPlugin CH_MEM 版 (Phase 6c M4 W7) — RAW 数据冒险检测
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17
//
// 设计: RAW (Read-After-Write) 组合逻辑检测 + CtrlLink::halt_when(ch_bool) 接到 stall 门控
// 借鉴: CppHDL/examples/riscv-mini/src/hazard_unit.h (188 行, RAW detection pattern)
//
// 原理 (每周期求值):
//   1. 从 decode stage 的 DECODE Payload 读 rs1_idx/rs2_idx/reads_rs1/reads_rs2
//   2. 从 execute/memory/writeback stage 的 DECODE Payload 读 rd_idx/writes_rd
//   3. 条件: rsN == rd && rd != 0 && reads_rsN && writes_rd
//   4. EX/MEM/WB 三路 OR 合并, 任一匹配则 stall decode stage (指令停顿, 不前进)
//   5. x0 (rd==0) 排除: RISC-V spec x0 永远是 0, 无需 stall
//
// 实现: M4/W7 从 skeleton (ch_bool(false)) 升级为真实 RAW 检测逻辑
//   1. 读取 decode/execute/memory/writeback 四阶段的 DECODE struct
//   2. 6 路 ch_bool 条件: rs1_ex || rs1_mem || rs1_wb || rs2_ex || rs2_mem || rs2_wb
//   3. 每条条件: ch_bool(reads_rs) && (rs == rd) && ch_bool(writes_rd) && (rd != 0)
//   4. ch_bool() 包装 C++ bool 操作数避免 operator&& 歧义
//   5. OR 合并结果 → CtrlLink::halt_when(raw_hazard) → elaboration 接 decode stage stall
//
// 阶段命名 (项目约定):
//   - "decode"   = ID  stage (rs1/rs2 读取, RAW 检测发生)
//   - "execute"  = EX  stage (1 cycle ahead)
//   - "memory"   = MEM stage (2 cycles ahead)
//   - "writeback"= WB  stage (3 cycles ahead)
//
// 编译条件: #ifdef CF_PLUGIN_USE_CH_MEM (启用时编译)
//   在 CH_MEM 模式下, CtrlLink::halt_when(ch_bool) 注册硬件信号句柄,
//   PipeBuilder::elaborate() 阶段将其连入 stage stall OR 树.
//
// 限制:
//   - 仅 RAW 数据 hazard, 不实现控制 hazard (jal/branch)
//   - 不实现 load-use hazard 优化 (M4 W7 HazardPlugin 完整化时补)
//   - 不实现 WAR / WAW (in-order pipeline 不需要)
//   - 不实现数据前推 (forwarding) — 当前纯 stall 策略, 无旁路
//   - 仅 RV32 PoC (XLEN=32 硬编码), RV64 需 M4/W8 泛化
//
// 与 TLM HazardPlugin (hazard.h) 的区别:
//   - TLM 版: 软件 scoreboard (bool 数组) + std::function<bool()> 运行期谓词
//   - CH_MEM 版: 组合逻辑 RAW 检测 + ch_bool 硬件信号 (elaboration 期建 DAG)
//   - TLM 版 stall "execute" stage, CH_MEM 版 stall "decode" stage (硬件语义正确)
//   - 两版共存于不同 namespace, 互不冲突

#ifndef CHIPFORGE_HAZARD_CHMEM_H
#define CHIPFORGE_HAZARD_CHMEM_H

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <memory>
#include <utility>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/core/payload_common.h"

// 注: ctrl_link.h 在 CH_MEM 模式下已包含 <ch.hpp> + <core/bool.h>,
//     因此 ch::core::ch_bool / ch::core::ch_uint / ch::core::ch_literal 在此可用.

namespace ip {
namespace cpu {

// ============================================================================
// HazardPlugin (CH_MEM) — RAW 数据冒险检测 (M4/W7 实装)
//
// 模板参数:
//   T    — xlen 类型 (CH_MEM 模式 = ch_uint<XLEN>, 默认 std::uint32_t 供编译)
//   XLEN — RV 位宽 (CH_MEM PoC: 32 硬编码)
//
// 生命周期 (CH_MEM):
//   1. CpuFactory::build_cpu() 创建 plugin 实例并注册到 PipeBuilder
//   2. setup(): 创建 CtrlLink, register_ctrl_link("decode", link)
//   3. build(): 注册 at_stage("decode", NORMAL, detect_raw_hazard)
//   4. PipeBuilder::elaborate(): 运行 at_stage 闭包 → halt_when(ch_bool)
//      → CtrlLink::halt_condition() 被 elaborate() 读入 decode stage stall 信号
//
// RAW 检测公式:
//   raw_hazard =
//     (reads_rs1 && id_rs1 == ex_rd  && ex_writes_rd  && ex_rd  != 0) ||
//     (reads_rs2 && id_rs2 == ex_rd  && ex_writes_rd  && ex_rd  != 0) ||
//     (reads_rs1 && id_rs1 == mem_rd && mem_writes_rd && mem_rd != 0) ||
//     (reads_rs2 && id_rs2 == mem_rd && mem_writes_rd && mem_rd != 0) ||
//     (reads_rs1 && id_rs1 == wb_rd  && wb_writes_rd  && wb_rd  != 0) ||
//     (reads_rs2 && id_rs2 == wb_rd  && wb_writes_rd  && wb_rd  != 0)
// ============================================================================
template <typename T = std::uint32_t, unsigned XLEN = 32>
class HazardPlugin : public cf::plugin::PluginBase {
 public:
  HazardPlugin() = default;
  ~HazardPlugin() override = default;

  HazardPlugin(const HazardPlugin&) = delete;
  HazardPlugin& operator=(const HazardPlugin&) = delete;

  // --------------------------------------------------------------------------
  // setup — 创建 CtrlLink 并注册到 decode stage
  //
  // CH_MEM 模式: CtrlLink 的 halt_when(ch_bool) 会在 elaboration 期
  // 被 PipeBuilder::elaborate() 读入 OR 合并, 连到 decode stage 的 stall 信号.
  // 阶段名"decode"对齐项目命名约定 (cpu_factory.h TopologyBuilder<5>).
  // --------------------------------------------------------------------------
  void setup(cf::plugin::PipeBuilder& pb) override {
    stall_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
    pb.register_ctrl_link("decode", stall_ctrl_);
  }

  // --------------------------------------------------------------------------
  // build — 注册 RAW 检测组合逻辑
  //
  // CH_MEM 模式: at_stage 闭包在 PipeBuilder::elaborate() 期间执行一次,
  // 在 decode stage 的 DECODE Payload 就绪后, 建立 ch_bool 比较网络.
  // --------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("decode", cf::plugin::Phase::NORMAL, [this, &pb] {
      this->detect_raw_hazard(pb);
    });
  }

 private:
  // --------------------------------------------------------------------------
  // detect_raw_hazard — RAW 检测组合逻辑 (M4/W7 实装)
  //
  // 原理 (映射 hazard_unit.h describe()):
  //   Step 1: 读 decode stage DECODE (rs1_idx, rs2_idx, reads_rs1, reads_rs2)
  //   Step 2: 读 execute/memory/writeback stage DECODE (rd_idx, writes_rd)
  //   Step 3: 6 路 ch_bool 比较 OR 合并
  //           每条: ch_bool(reads_rs) && (rs == rd) && ch_bool(writes_rd) && (rd != 0)
  //   Step 4: halt_when(raw_hazard) 注册到 CtrlLink
  //
  // ch_bool 注意事项:
  //   - ch_uint<N> == ch_uint<N> 直接返回 ch_bool (CppHDL operator 重载)
  //   - C++ bool 进 ch_bool 上下文必须显式 ch_bool(bool_val) 避免歧义
  //   - static_cast<uint32_t> 桥接 uint8_t (结构体字段) 到 ch_uint<5>
  //   - ch::core::ch_literal<V, W> 构造字面值, 不使用 _d 后缀
  // --------------------------------------------------------------------------
  void detect_raw_hazard(cf::plugin::PipeBuilder& pb) {
    using KeyType = cf::cpu::core::payload::keys<T, XLEN>;

    // ── Step 1: ID/decode stage rs1/rs2 ─────────────────────────────────
    auto* id_node = pb.node_of_logic_stage("decode").get();
    if (!id_node) {
      // Node 不存在 (3-stage 等场景): 无操作, halt_when(false)
      if (stall_ctrl_) stall_ctrl_->halt_when(ch::core::ch_bool(false));
      return;
    }

    const auto& dec = id_node->operator()(KeyType::DECODE);

    // 5-bit 源寄存器索引 (从 DecodePayload uint8_t 字段转换)
    auto id_rs1 = static_cast<ch::core::ch_uint<5>>(
        static_cast<std::uint32_t>(dec.rs1_idx));
    auto id_rs2 = static_cast<ch::core::ch_uint<5>>(
        static_cast<std::uint32_t>(dec.rs2_idx));
    bool id_reads_rs1 = dec.reads_rs1;
    bool id_reads_rs2 = dec.reads_rs2;

    // ── Step 2: EX/MEM/WB stage rd ─────────────────────────────────────
    // 辅助 lambda: 读某 stage 的 rd_idx + writes_rd
    auto get_stage_rd = [&](const std::string& stage_name)
        -> std::pair<ch::core::ch_uint<5>, bool> {
      auto* n = pb.node_of_logic_stage(stage_name).get();
      if (!n) {
        return {ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{}), false};
      }
      const auto& d = n->operator()(KeyType::DECODE);
      return {static_cast<ch::core::ch_uint<5>>(
                  static_cast<std::uint32_t>(d.rd_idx)),
              d.writes_rd};
    };

    auto [ex_rd,  ex_writes]  = get_stage_rd("execute");
    auto [mem_rd, mem_writes] = get_stage_rd("memory");
    auto [wb_rd,  wb_writes]  = get_stage_rd("writeback");

    // ── Step 3: 6 路 RAW detection ─────────────────────────────────────
    // x0 排除常数 (RISC-V: x0 始终保持 0, stall x0 无意义)
    auto zero5 = ch::core::ch_uint<5>(ch::core::ch_literal<0, 5>{});

    // 非零写回检测 (rd != 0)
    ch::core::ch_bool ex_ok  = ch::core::ch_bool(ex_writes)  && (ex_rd  != zero5);
    ch::core::ch_bool mem_ok = ch::core::ch_bool(mem_writes) && (mem_rd != zero5);
    ch::core::ch_bool wb_ok  = ch::core::ch_bool(wb_writes)  && (wb_rd  != zero5);

    // rs1/match: reads_rs1 && writes_rd && rd != 0 && rs_idx == rd_idx
    ch::core::ch_bool ex_rs1  = ex_ok  && (id_rs1 == ex_rd)
                                         && ch::core::ch_bool(id_reads_rs1);
    ch::core::ch_bool ex_rs2  = ex_ok  && (id_rs2 == ex_rd)
                                         && ch::core::ch_bool(id_reads_rs2);
    ch::core::ch_bool mem_rs1 = mem_ok && (id_rs1 == mem_rd)
                                         && ch::core::ch_bool(id_reads_rs1);
    ch::core::ch_bool mem_rs2 = mem_ok && (id_rs2 == mem_rd)
                                         && ch::core::ch_bool(id_reads_rs2);
    ch::core::ch_bool wb_rs1  = wb_ok  && (id_rs1 == wb_rd)
                                         && ch::core::ch_bool(id_reads_rs1);
    ch::core::ch_bool wb_rs2  = wb_ok  && (id_rs2 == wb_rd)
                                         && ch::core::ch_bool(id_reads_rs2);

    // ── OR 合并 → 任意条件满足则产生 RAW hazard ─────────────────────────
    ch::core::ch_bool raw_hazard =
        ex_rs1 || ex_rs2 || mem_rs1 || mem_rs2 || wb_rs1 || wb_rs2;

    if (stall_ctrl_) {
      stall_ctrl_->halt_when(raw_hazard);
    }
  }

  // CtrlLink shared_ptr: setup() 中创建并注册到 PipeBuilder
  // detect_raw_hazard() 中用此句柄调用 halt_when(ch_bool)
  std::shared_ptr<cf::plugin::CtrlLink> stall_ctrl_;
};

}  // namespace cpu
}  // namespace ip

#endif  // CF_PLUGIN_USE_CH_MEM
#endif  // CHIPFORGE_HAZARD_CHMEM_H