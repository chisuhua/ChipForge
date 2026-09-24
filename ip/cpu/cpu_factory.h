// ip/cpu/cpu_factory.h
//
// 功能描述: CpuFactory — 集中 PluginOrder + build_cpu() 入口 (M4.1, P0)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-16
//
// 设计:
//   - 议题 5 选 B: CpuFactory 是 PluginOrder 单一真相源
//   - 接受 CPUConfig (或 JSON) 返回完整 PipeBuilder
//   - 11 个 Plugin 按 EARLY → NORMAL → LATE 顺序注册
//   - 模板参数化 <typename T>: T = xlen 类型 (uint32/uint64)
//
// 借鉴:
//   - multi_isa v2.0 §3.2 Plugin 调度顺序
//   - CpuFactory 是 Plugin 装配工厂
//
// 约束:
//   - D4 合规: 工厂只做 Plugin 注册, 不做业务逻辑

#ifndef CF_IP_CPU_CPU_FACTORY_H
#define CF_IP_CPU_CPU_FACTORY_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_error.h"
#include "cf/plugin/result_macros.h"
#include "ip/cpu/plugins/reg_file.h"
#include "ip/cpu/plugins/ibus.h"
#include "ip/cpu/plugins/branch_predictor.h"
#include "ip/cpu/plugins/hazard.h"
#include "ip/cpu/plugins/dbus.h"
#include "ip/cpu/plugins/stage_link.h"  // cpu-pipeline-stubs-replace commit B: 阶段间 Payload 传播
#include "ip/cpu/arch/riscv/decode.h"
#include "ip/cpu/arch/riscv/int_alu.h"
#include "ip/cpu/arch/riscv/mul.h"
#include "ip/cpu/arch/riscv/branch.h"
#include "ip/cpu/arch/riscv/lsu.h"
#include "ip/cpu/arch/riscv/csr.h"
#include "ip/cpu/plugins/mmu.h"  // mmu-cache-integration commit 6/9: RiscVMMUPlugin 接入 CPU pipeline

// ===== ADR-048: Plugin canonical ordering counters (C++17 inline variable) =====
// PLUGIN_SEQ increments at each register_plugin call site in build_cpu().
// MMU/IBUS/DBUS_REG_ORDER record each plugin's slot (0 = not registered).
// reset_canonical_counters() is called at the top of build_cpu().
// check_canonical_ordering() validates MMU < IBUS (unless MMU disabled).
namespace cf { namespace cpu { namespace detail {
inline int PLUGIN_SEQ = 0;
inline int MMU_REG_ORDER = 0;
inline int IBUS_REG_ORDER = 0;
inline int DBUS_REG_ORDER = 0;
inline void reset_canonical_counters() {
  PLUGIN_SEQ = 0;
  MMU_REG_ORDER = 0;
  IBUS_REG_ORDER = 0;
  DBUS_REG_ORDER = 0;
}
inline void check_canonical_ordering() {
  if (MMU_REG_ORDER == 0) return;  // MMU not enabled → no constraint
  if (MMU_REG_ORDER >= IBUS_REG_ORDER) {
    throw std::logic_error("MMUPlugin(seq=" + std::to_string(MMU_REG_ORDER) +
                           ") must be registered before IBusPlugin(seq=" +
                           std::to_string(IBUS_REG_ORDER) +
                           ") — ADR-048 canonical ordering violation");
  }
}
}}}  // namespace cf::cpu::detail

namespace cf {
namespace cpu {

// ----------------------------------------------------------------------------
// CPUConfig —— CPU 配置结构 (M4.1, 对应 cpu_default.json/cpu_embedded.json)
//
// 字段: 与 cpu_params_schema.json 一一对应
// 用途: 避免引入 JSON 解析依赖, 纯 C++ struct 即可
// ----------------------------------------------------------------------------
struct CPUConfig {
  // 基本信息
  std::string name = "RiscvCpu";
  std::string isa = "rv64gc";       // rv32i/rv32im/rv32imac/rv64i/rv64gc
  std::uint8_t pipeline_stages = 5; // 1-12, 默认 5 级
  std::uint32_t clock_freq_mhz = 100;

  // M4G-extend G.X: SMT 线程数 (1-4, 默认 1 = 单线程 byte-identical)
  std::uint8_t n_threads = 1;

  // 功能开关
  bool enable_pmp = true;
  bool enable_mmu = true;
  std::string mmu_mode = "sv39";    // sv32/sv39/sv48

  // 分支预测
  std::string branch_predictor = "gshare";  // static/bimodal/gshare/tournament
  std::uint16_t btb_entries = 64;  // 16/32/64/128/256

  // Cache 延迟 (cycle)
  std::uint8_t icache_latency = 1;
  std::uint8_t dcache_latency = 1;

  // M5-DSE Superscalar 字段 (M5.19, schema +9 fields)
  // 默认值与 cpu_params_schema.json 一致; 详见 design.md Decision 5
  std::uint8_t n_lanes = 1;
  std::uint8_t dispatch_width = 1;
  std::uint16_t issue_queue_size = 0;
  std::uint16_t rob_size = 0;
  std::uint16_t lsq_size = 0;
  std::uint16_t rename_table_size = 0;
  std::uint8_t retire_width = 1;
  std::uint8_t fetch_width = 1;
  std::uint8_t commit_width = 1;

  // M5-DSE M5.14: RiscvMulPlugin 多周期延迟 (mul_latency ∈ {1, 3, 5})
  // 默认 1 = 单周期 (byte-identical to baseline); 3/5 走多周期子流水
  // 详见 mul.h::RiscvMulPlugin<T, LATENCY> 模板参数化
  std::uint8_t mul_latency = 1;
};

// ----------------------------------------------------------------------------
// TopologyBuilder<N_STAGES> —— 编译期流水线拓扑展开器 (M5-DSE M5.10)
//
// 设计:
//   - 按 config.pipeline_stages 编译期实例化 4 个特化 (3/5/7/10)
//   - 每个特化按 multi_isa v2.0 §2.4 逻辑→物理节点映射展开 PipeBuilder
//   - 命名约定: at_stage() 使用 logic_stage 名称 ("fetch"/"decode"/...)
//     物理 node 由 declare_substage() 在 deep pipeline 中创建
//   - 5 级 byte-identical: 与 baseline register_*/at_stage 行为等价
//
// 借鉴:
//   - multi_isa v2.0 §2.4 拓扑表
//   - VexRiscv Pipeline.scala: 用 stageable + pluggable 描述流水线结构
//
// 约束:
//   - N_STAGES 必须在 {3, 5, 7, 10} 之内 (static_assert)
//   - 不调用 build_cpu, 直接操作 PipeBuilder& (D4 合规: 工厂只做 Plugin 注册)
//   - 与 mul_latency (Task 3) 正交: 此处只关心拓扑, 不关心执行延迟
// ----------------------------------------------------------------------------
template <std::size_t N_STAGES>
struct TopologyBuilder {
  static_assert(N_STAGES == 3 || N_STAGES == 5 ||
                N_STAGES == 7 || N_STAGES == 10,
                "TopologyBuilder supports 3/5/7/10 stages only");
  static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& cfg);
};

// ---- 特化: N_STAGES = 3 (embedded, IF/EXMEM/WB 合并) ----
// 拓扑: fetch+decode→IF, execute+memory→EXMEM, writeback→WB
// multi_isa v2.0 §2.4: 3-row 表
template <>
struct TopologyBuilder<3> {
  static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& /*cfg*/) {
    pb.at_stage("if",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("exmem", cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("wb",    cf::plugin::Phase::NORMAL, []() {});
  }
};

// ---- 特化: N_STAGES = 5 (baseline byte-identical) ----
// 拓扑: fetch→IF, decode→ID, execute→EX, memory→MEM, writeback→WB
// multi_isa v2.0 §2.4: 5-row 表, 与默认 RISC-V 5 级流水线对齐
// 重要: 此特化必须与 baseline byte-identical — 不改变节点数与命名
template <>
struct TopologyBuilder<5> {
  static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& /*cfg*/) {
    pb.at_stage("fetch",     cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("decode",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("execute",   cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("memory",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("writeback", cf::plugin::Phase::NORMAL, []() {});
  }
};

// ---- 特化: N_STAGES = 7 (OoO superscalar, 显式 retire/commit) ----
// 拓扑: fetch→IF, decode→ID, execute→EX, memory→MEM, writeback→WB,
//       retire (OoO 显式提交), commit (最终 commit 阶段)
// multi_isa v2.0 §2.4: 7-row 表 (含 RETIRE)
// M4G-extend G.X 命名: in-order 隐含 commit; OoO 显式 retire/commit 阶段
template <>
struct TopologyBuilder<7> {
  static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& /*cfg*/) {
    pb.at_stage("fetch",     cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("decode",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("execute",   cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("memory",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("writeback", cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("retire",    cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("commit",    cf::plugin::Phase::NORMAL, []() {});
  }
};

// ---- 特化: N_STAGES = 10 (deep pipeline, ≥3 sub-pipe between fetch and execute) ----
// 拓扑: fetch + IF1/IF2 子阶段, decode + ID 子阶段,
//       execute + RENAME/ISSUE/EX1/EX2/EX3 子阶段,
//       memory + MEM1/MEM2 子阶段, writeback, retire
// multi_isa v2.0 §2.4: 10-row 表 (deep pipeline, OoO + superscalar)
// ≥10 节点: 7 main stages + 8 substages (declared via declare_substage)
template <>
struct TopologyBuilder<10> {
  static void expand(cf::plugin::PipeBuilder& pb, const CPUConfig& /*cfg*/) {
    // fetch + 2 substages (IF1, IF2)
    pb.at_stage("fetch", cf::plugin::Phase::NORMAL, []() {});
    pb.declare_substage("fetch", "IF1", 1);
    pb.declare_substage("fetch", "IF2", 1);
    // decode + 1 substage (ID)
    pb.at_stage("decode", cf::plugin::Phase::NORMAL, []() {});
    pb.declare_substage("decode", "ID", 1);
    // execute + 5 substages (RENAME, ISSUE, EX1, EX2, EX3)
    pb.at_stage("execute", cf::plugin::Phase::NORMAL, []() {});
    pb.declare_substage("execute", "RENAME", 1);
    pb.declare_substage("execute", "ISSUE", 1);
    pb.declare_substage("execute", "EX1", 1);
    pb.declare_substage("execute", "EX2", 1);
    pb.declare_substage("execute", "EX3", 1);
    // memory + 2 substages (MEM1, MEM2)
    pb.at_stage("memory", cf::plugin::Phase::NORMAL, []() {});
    pb.declare_substage("memory", "MEM1", 1);
    pb.declare_substage("memory", "MEM2", 1);
    // writeback + retire (terminal stages)
    pb.at_stage("writeback", cf::plugin::Phase::NORMAL, []() {});
    pb.at_stage("retire",    cf::plugin::Phase::NORMAL, []() {});
  }
};

// ----------------------------------------------------------------------------
// stage_name_at_7 —— 7-stage superscalar 阶段名查找 (M5-DSE M5.18)
//
// 用途: lane 派发按 stage name 重新注册 at_stage 闭包.
// 7 阶段: fetch → decode → execute → memory → writeback → retire → commit
// 与 TopologyBuilder<7>::expand 顺序一致 (multi_isa v2.0 §2.4).
// 仅在 7-stage superscalar 路径 (dispatch_width > 1) 触发时使用.
// ----------------------------------------------------------------------------
inline const char* stage_name_at_7(std::size_t i) {
  static constexpr const char* kNames[7] = {
      "fetch", "decode", "execute", "memory", "writeback", "retire", "commit"};
  return kNames[i];
}

// ----------------------------------------------------------------------------
// CpuFactory —— 集中 PluginOrder + build_cpu() 入口
//
// 静态 API: CpuFactory::build_cpu<T>(config) → unique_ptr<PipeBuilder>
//
// Plugin 注册顺序 (multi_isa v2.0 §3.2):
//   EARLY   (fetch):    IBusPlugin, BranchPredictorPlugin
//   NORMAL  (decode):   RiscvDecodePlugin, HazardPlugin
//   NORMAL  (execute):  RiscvIntAluPlugin, RiscvBranchPlugin,
//                       RiscvMulPlugin, RiscvLsuPlugin, RiscvCsrPlugin
//   LATE    (writeback): RegFilePlugin
//   (memory):           DBusPlugin
// ----------------------------------------------------------------------------
template <typename T = std::uint32_t>
class CpuFactory {
 public:
  // 主入口: 接受 CPUConfig, 返回完整 PipeBuilder
  // cpu-pipeline-stubs-replace commit E: 可选 mem 参数 — 非空时注入 IBusPlugin/DBusPlugin
  // 真实访存 (取指/读写 PicolibcHostMemory). null (默认) 维持 stub 行为, 零回归.
  static std::unique_ptr<cf::plugin::PipeBuilder> build_cpu(
      const CPUConfig& config,
      cf::cpu::PicolibcHostMemory* mem = nullptr) {
    // ADR-048: reset canonical ordering counters at each build_cpu entry
    cf::cpu::detail::reset_canonical_counters();
    auto pb = std::make_unique<cf::plugin::PipeBuilder>();

    // cpu-pipeline-stubs-replace commit B: StageLinkPlugin 必须在 register_early_plugins 之前注册,
    // 保证 StageLinkPlugin 的 4 个 EARLY 闭包在所有业务 plugin 之前跑.
    pb->register_plugin(std::make_unique<cf::cpu::plugins::StageLinkPlugin<T>>());

    // ADR-048: MMUPlugin registered inside register_early_plugins, BEFORE IBusPlugin
    register_early_plugins<T>(*pb, config, mem);

    // 2. NORMAL 阶段: decode + execute
    register_normal_plugins<T>(*pb, config);

    // 3. LATE 阶段: writeback
    register_late_plugins<T>(*pb, config, mem);

    // M5-DSE M5.10: 编译期 TopologyBuilder 展开 (按 config.pipeline_stages)
    // 5-stage 路径必须 byte-identical to baseline (现 register_*/at_stage 行为)
    switch (config.pipeline_stages) {
      case 3:
        TopologyBuilder<3>::expand(*pb, config);
        break;
      case 5:
        TopologyBuilder<5>::expand(*pb, config);
        break;
      case 7:
        TopologyBuilder<7>::expand(*pb, config);
        break;
      case 10:
        TopologyBuilder<10>::expand(*pb, config);
        break;
      default:
        throw std::invalid_argument(
            "CpuFactory: unsupported pipeline_stages (must be 3/5/7/10)");
    }

    // M4G-extend G.X: 注入 per-cycle dispatch 线程数
    // n_threads=1 默认 byte-identical; >1 走 SMT/超标量路径
    pb->set_n_threads(config.n_threads);

    // M5-DSE M5.18: 2-wide superscalar lane 派发 (design.md Decision 3)
    //   触发条件: dispatch_width > 1 AND pipeline_stages == 7
    //   - 单发射 (dispatch_width=1): 完全 no-op, 5-stage baseline byte-identical
    //   - 非 7-stage (3/5/10): 当前 scope 不实现, 留待 Phase 2 扩展
    // 闭包: 每个 stage 注入一个 std::atomic<uint8_t> 计数器, 闭包内
    //   fetch_add(1) % n_lanes 决定 lane 0/1, 写到 node_of_logic_stage
    //   的 set_lane() 字段. per-build_cpu 栈帧, 闭包持有指针.
    if (config.dispatch_width > 1 && config.pipeline_stages == 7) {
      // Heap-allocate counters: lambdas outlive build_cpu's stack frame.
      auto lane_counters = std::make_shared<std::vector<std::atomic<std::uint8_t>>>(
          config.pipeline_stages);
      for (std::size_t s = 0; s < config.pipeline_stages; ++s) {
        (*lane_counters)[s].store(0);
      }
      for (std::size_t s = 0; s < 7; ++s) {
        const char* stage_name = stage_name_at_7(s);
        pb->at_stage(stage_name, cf::plugin::Phase::NORMAL,
                     [pb_ptr = pb.get(), counters = lane_counters,
                      n = config.n_lanes, stage_name, s]() {
                       const std::uint8_t my_lane =
                           static_cast<std::uint8_t>(
                               (*counters)[s].fetch_add(1) % n);
                       pb_ptr->node_of_logic_stage(stage_name)
                           ->set_lane(my_lane);
                     });
      }
    }

// ADR-048: canonical ordering check (MMUPlugin must register before IBusPlugin)
    cf::cpu::detail::check_canonical_ordering();

    // cpu-mmu-integration commit 1/6: 调 pb.build() 触发所有 plugin 的 setup() + build()
    // 此调用前 cpu_factory 只 调了 TopologyBuilder::expand 和 lane dispatch at_stage,
    // plugin 自身 (含 MMUPlugin) 的 declare_substage / at_stage 闭包都未运行.
    // 之前 baseline 不需要 plugin 闭包 (只 topology 正确); 加 MMU 后必须 build().
    pb->build();

    return pb;
  }

  // v0.6 ADR-047: Result 范式内部实现入口
  //   - 返回 Result<std::unique_ptr<PipeBuilder>>
  //   - 内部委托 build_cpu() (兼容既有行为); 失败统一转为 PluginException
  //   - P1 阶段: 后续逐点用 PB_TRY 替换 at_stage/register_plugin 调用以获得
  //     细粒度错误码 (NullPlugin / EmptyStageName / DuplicateStageName / etc.)
  //     本方法先提供 Result 兼容, 后续渐进迁移
  static cf::plugin::Result<std::unique_ptr<cf::plugin::PipeBuilder>>
  build_cpu_impl(const CPUConfig& config,
                 cf::cpu::PicolibcHostMemory* mem = nullptr) {
    try {
      return build_cpu(config, mem);
    } catch (const cf::plugin::PluginException& /*e*/) {
      return std::unexpected(cf::plugin::PluginError::BuildFailed);
    } catch (const std::exception& /*e*/) {
      return std::unexpected(cf::plugin::PluginError::BuildFailed);
    } catch (...) {
      return std::unexpected(cf::plugin::PluginError::BuildFailed);
    }
  }

 private:
// EARLY 阶段: fetch (ADR-048: MMUPlugin MUST register before IBusPlugin)
  template <typename U>
  static void register_early_plugins(cf::plugin::PipeBuilder& pb,
                                      const CPUConfig& config,
                                      PicolibcHostMemory* mem = nullptr) {
    // ADR-048: MMUPlugin 必须在 IBusPlugin 之前注册（canonical stall ordering）
    if (config.enable_mmu) {
      cf::ip::mmu::SvMode sv_mode = cf::ip::mmu::SvMode::Sv39;
      if (config.mmu_mode == "sv32")      sv_mode = cf::ip::mmu::SvMode::Sv32;
      else if (config.mmu_mode == "sv48") sv_mode = cf::ip::mmu::SvMode::Sv48;
      using mmu_cfg_t = cf::cpu::plugins::RiscvMMUPlugin::TLBConfig;
      std::vector<mmu_cfg_t> mmu_levels = {
        {"L0", 8, 8, 1, 1, "LRU"},
        {"L1", 8, 8, 1, 2, "LRU"}
      };
      cf::cpu::detail::MMU_REG_ORDER = ++cf::cpu::detail::PLUGIN_SEQ;
      pb.register_plugin(std::make_unique<cf::cpu::plugins::RiscvMMUPlugin>(
          sv_mode, mmu_levels, cf::ip::mmu::MMUPlugin::PTWConfig{2},
          /*satp_value=*/0));
    }

    cf::cpu::detail::IBUS_REG_ORDER = ++cf::cpu::detail::PLUGIN_SEQ;
    pb.register_plugin(
        std::make_unique<cf::cpu::plugins::IBusPlugin<U> >(mem));
    pb.register_plugin(
        std::make_unique<cf::cpu::plugins::BranchPredictorPlugin<U>>(
            config.btb_entries));
    (void)sizeof(U);
  }

  // NORMAL 阶段: decode + execute
  template <typename U>
  static void register_normal_plugins(cf::plugin::PipeBuilder& pb,
                                      const CPUConfig& config) {
    pb.register_plugin(
        std::make_unique<cf::cpu::arch::riscv::RiscvDecodePlugin<U> >());
    pb.register_plugin(std::make_unique<cf::cpu::plugins::HazardPlugin<U> >());
    pb.register_plugin(
        std::make_unique<cf::cpu::arch::riscv::RiscvIntAluPlugin<U> >());
    switch (config.mul_latency) {
      case 1:
        pb.register_plugin(
            std::make_unique<cf::cpu::arch::riscv::RiscvMulPlugin<U, 1> >());
        break;
      case 3:
        pb.register_plugin(
            std::make_unique<cf::cpu::arch::riscv::RiscvMulPlugin<U, 3> >());
        break;
      case 5:
        pb.register_plugin(
            std::make_unique<cf::cpu::arch::riscv::RiscvMulPlugin<U, 5> >());
        break;
      default:
        throw std::invalid_argument(
            "CpuFactory: unsupported mul_latency (must be 1/3/5)");
    }
    pb.register_plugin(
        std::make_unique<cf::cpu::arch::riscv::RiscvBranchPlugin<U> >());
    pb.register_plugin(
        std::make_unique<cf::cpu::arch::riscv::RiscvLsuPlugin<U> >());
    pb.register_plugin(std::make_unique<cf::cpu::arch::riscv::RiscvCsrPlugin>());
    (void)sizeof(U);
  }

// LATE 阶段: writeback
  template <typename U>
  static void register_late_plugins(cf::plugin::PipeBuilder& pb,
                                     const CPUConfig& /*config*/,
                                     PicolibcHostMemory* mem = nullptr) {
    cf::cpu::detail::DBUS_REG_ORDER = ++cf::cpu::detail::PLUGIN_SEQ;
    pb.register_plugin(std::make_unique<cf::cpu::plugins::DBusPlugin<U> >(mem));
    pb.register_plugin(std::make_unique<cf::cpu::plugins::RegFilePlugin<U> >());
    (void)sizeof(U);
  }
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CPU_FACTORY_H
