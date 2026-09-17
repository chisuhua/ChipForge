// ip/cpu/plugins/hazard_chmem.h
//
// HazardPlugin CH_MEM 版 (Phase 6c M4 W7, Oracle A4 必须)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-17
//
// 设计: RAW (Read-After-Write) 检测组合逻辑 + CtrlLink::halt_when(ch_bool) 接到 stall 门控
// 借鉴: CppHDL/examples/riscv-mini/src/hazard_unit.h (188 行, RAW detection pattern)
//
// 原理 (每周期求值):
//   1. 对比 ID stage 指令的 rs1/rs2 与 EX/MEM/WB stage 指令的 rd
//   2. 条件: rs1/rs2 == rd && rd != 0
//   3. EX/MEM/WB 三路 OR 合并, 任一匹配则 stall ID stage
//   4. x0 (rd==0) 排除: RISC-V spec x0 永远是 0, 无需 stall
//
// 状态: skeleton (M4 W7 集成测试待 M3 业务代码完成)
// 参考: hazard_unit.h describe() 的 rs1_match_ex/rs1_match_mem 组合逻辑
//
// 编译条件: #ifdef CF_PLUGIN_USE_CH_MEM (启用时编译)
//   在 CH_MEM 模式下, CtrlLink::halt_when(ch_bool) 注册硬件信号句柄,
//   PipeBuilder::elaborate() 阶段将其连入 stage stall OR 树.
//
// 限制:
//   - 骨架级: 仅 RAW 数据 hazard, 不实现控制 hazard (jal/branch 推迟 M4 W7)
//   - 不实现 load-use hazard 优化 (M4 W7 HazardPlugin 完整化时补)
//   - 不实现 WAR / WAW (in-order pipeline 不需要)
//   - 不接具体 Payload 字段 (M3 业务代码完成后对接 payload_common.h 的 rs1/rs2/rd 索引)
//
// 与 TLM HazardPlugin (hazard.h) 的区别:
//   - TLM 版: 软件 scoreboard (bool 数组) + std::function<bool()> 运行期谓词
//   - CH_MEM 版: 组合逻辑 RAW 检测 + ch_bool 硬件信号 (elaboration 期建 DAG)
//   - 两版共存于不同 namespace, 互不冲突
//
// M4 W7 集成测试路径:
//   1. M3 业务代码完成 → payload_common.h 字段就绪
//   2. 本插件从占位 ch_bool(false) 切换到 payload 读取 rs1_idx/rs2_idx/rd_idx
//   3. EX/MEM/WB 的 rd 通过 stage_payload_connector (pipe_builder.h) 读入
//   4. 组合逻辑: ch_bool raw = (id_rs1 == ex_rd && ex_rd != 0) || ...
//   5. CtrlLink::halt_when(raw) 连入 ID stage stall 信号

#ifndef CHIPFORGE_HAZARD_CHMEM_H
#define CHIPFORGE_HAZARD_CHMEM_H

#ifdef CF_PLUGIN_USE_CH_MEM

#include <memory>

#include "cf/plugin/ctrl_link.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/pipe_builder.h"

// 注: ctrl_link.h 在 CH_MEM 模式下已包含 <ch.hpp> + <core/bool.h>,
//     因此 ch::core::ch_bool 在此可用.

namespace ip {
namespace cpu {

// ============================================================================
// HazardPlugin (CH_MEM) — RAW 数据冒险检测骨架
//
// 生命周期 (CH_MEM):
//   1. CpuFactory::build_cpu() 创建 plugin 实例并注册到 PipeBuilder
//   2. setup(): 创建 CtrlLink, register_ctrl_link("ID", link)
//   3. build(): 注册 at_stage("ID", NORMAL, detect_raw_hazard)
//   4. PipeBuilder::elaborate(): 运行 at_stage 闭包 → halt_when(ch_bool)
//      → CtrlLink::halt_condition() 被 elaborate() 读入 ID stage stall 信号
//
// Skeleton 占位:
//   - detect_raw_hazard() 中暂时用 ch_bool(false) 表示"无 hazard"
//   - M4 W7 集成时替换为 payload_common.h 的 rs1_idx/rs2_idx/rd_idx 实际值
//     (cf::cpu::core::payload::keys<T>::DECODE 的 rs1_idx/rs2_idx/rd_idx)
// ============================================================================
class HazardPlugin : public cf::plugin::PluginBase {
 public:
  HazardPlugin() = default;
  ~HazardPlugin() override = default;

  HazardPlugin(const HazardPlugin&) = delete;
  HazardPlugin& operator=(const HazardPlugin&) = delete;

  // --------------------------------------------------------------------------
  // setup — 创建 CtrlLink 并注册到 ID stage
  //
  // CH_MEM 模式: CtrlLink 的 halt_when(ch_bool) 会在 elaboration 期
  // 被 PipeBuilder::elaborate() 读入 OR 合并, 连到 ID stage 的 stall 信号.
  // --------------------------------------------------------------------------
  void setup(cf::plugin::PipeBuilder& pb) override {
    stall_ctrl_ = std::make_shared<cf::plugin::CtrlLink>();
    pb.register_ctrl_link("ID", stall_ctrl_);
  }

  // --------------------------------------------------------------------------
  // build — 注册 RAW 检测组合逻辑 (每周期求值)
  //
  // CH_MEM 模式: at_stage 闭包在 PipeBuilder::elaborate() 期间执行一次,
  // 发射 ch_bool 信号到 CtrlLink 的 OR 合并网络.
  // 注: 不要在闭包内用 if(ch_bool) —— ch_bool 的 explicit operator bool()
  //     在编译期不会失败, 但运行期会发射"未知语义节点" (见 uint_t.h W3-4 M2 纪律).
  // --------------------------------------------------------------------------
  void build(cf::plugin::PipeBuilder& pb) override {
    pb.at_stage("ID", cf::plugin::Phase::NORMAL, [this] {
      detect_raw_hazard();
    });
  }

 private:
  // --------------------------------------------------------------------------
  // detect_raw_hazard — RAW 检测组合逻辑 (骨架占位)
  //
  // 原理 (映射 hazard_unit.h describe() 的 RAW 检测):
  //   rs1_match_ex   = (ex_rd == id_rs1) && (ex_rd != 0)
  //   rs1_match_mem  = (mem_rd == id_rs1) && (mem_rd != 0)
  //   rs1_match_wb   = (wb_rd == id_rs1) && (wb_rd != 0)
  //   rs2_match_ex   = (ex_rd == id_rs2) && (ex_rd != 0)
  //   rs2_match_mem  = (mem_rd == id_rs2) && (mem_rd != 0)
  //   rs2_match_wb   = (wb_rd == id_rs2) && (wb_rd != 0)
  //   raw_hazard     = rs1_match_ex || rs1_match_mem || rs1_match_wb
  //                  || rs2_match_ex || rs2_match_mem || rs2_match_wb
  //
  // Skeleton 占位 (ch_bool(false)):
  //   测试阶段不产生假 stall (M4 W7 集成时替换)
  //
  // TODO (M4 W7): 从 payload_common.h 读 ID/EX/MEM/WB 的寄存器索引:
  //   using KeyType = cf::cpu::core::payload::keys<T, XLEN>;
  //   auto* id_n  = pb.node_of_logic_stage("ID").get();
  //   auto* ex_n  = pb.node_of_logic_stage("execute").get();
  //   auto* mem_n = pb.node_of_logic_stage("memory").get();
  //   auto* wb_n  = pb.node_of_logic_stage("writeback").get();
  //   auto id_dec = id_n->operator()(KeyType::DECODE);
  //   ch_uint<5> id_rs1(id_dec.rs1_idx);
  //   ch_uint<5> id_rs2(id_dec.rs2_idx);
  //   ch_uint<5> ex_rd(id_n->operator()(KeyType::RD_IDX));   // 来自 stage_payload_connector
  //   ch_uint<5> mem_rd(mem_n->operator()(KeyType::RD_IDX));
  //   ch_uint<5> wb_rd(wb_n->operator()(KeyType::RD_IDX));
  //
  //   ch_bool ex_ok    = (ex_rd  != ch_uint<5>(0));
  //   ch_bool mem_ok   = (mem_rd != ch_uint<5>(0));
  //   ch_bool wb_ok    = (wb_rd  != ch_uint<5>(0));
  //
  //   ch_bool r1_ex    = ex_ok  && (ex_rd  == id_rs1);
  //   ch_bool r1_mem   = mem_ok && (mem_rd == id_rs1);
  //   ch_bool r1_wb    = wb_ok  && (wb_rd  == id_rs1);
  //   ch_bool r2_ex    = ex_ok  && (ex_rd  == id_rs2);
  //   ch_bool r2_mem   = mem_ok && (mem_rd == id_rs2);
  //   ch_bool r2_wb    = wb_ok  && (wb_rd  == id_rs2);
  //
  //   ch_bool raw = r1_ex || r1_mem || r1_wb || r2_ex || r2_mem || r2_wb;
  //   if (stall_ctrl_) stall_ctrl_->halt_when(raw);
  // --------------------------------------------------------------------------
  void detect_raw_hazard() {
    // Skeleton: 占位 ch_bool(false), 测试阶段不产生假 stall
    // M4 W7 集成时替换为上述 payload 读取 + 组合逻辑
    ch::core::ch_bool raw_hazard(false);

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