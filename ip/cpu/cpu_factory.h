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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cf/plugin/pipe_builder.h"
#include "ip/cpu/plugins/reg_file.h"  // M4G D.2 (G.2.10): smoke test 需要

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
};

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
  static std::unique_ptr<cf::plugin::PipeBuilder> build_cpu(
      const CPUConfig& config) {
    auto pb = std::make_unique<cf::plugin::PipeBuilder>();

    // 1. EARLY 阶段: fetch 相关 Plugin
    // (M2 stub, 实际由 CpuFactory 调度)
    register_early_plugins<T>(*pb, config);

    // 2. NORMAL 阶段: decode + execute
    register_normal_plugins<T>(*pb, config);

    // 3. LATE 阶段: writeback
    register_late_plugins<T>(*pb, config);

    // M4G-extend G.X: 注入 per-cycle dispatch 线程数
    // n_threads=1 默认 byte-identical; >1 走 SMT/超标量路径
    pb->set_n_threads(config.n_threads);

    return pb;
  }

 private:
  // EARLY 阶段: fetch
  template <typename U>
  static void register_early_plugins(cf::plugin::PipeBuilder& pb,
                                     const CPUConfig& /*config*/) {
    // IBusPlugin: fetch 阶段读指令
    // BranchPredictorPlugin: fetch 阶段预测分支
    // 注: 实际 Plugin 实例化由 build_cpu 调用方持有, 工厂只调度
    // M4 stub: 当前仅注册阶段, 不实例化 Plugin
    (void)pb;
    (void)sizeof(U);
  }

  // NORMAL 阶段: decode + execute
  template <typename U>
  static void register_normal_plugins(cf::plugin::PipeBuilder& pb,
                                      const CPUConfig& /*config*/) {
    // decode: RiscvDecodePlugin, HazardPlugin
    // execute: RiscvIntAluPlugin, RiscvBranchPlugin, RiscvMulPlugin,
    //          RiscvLsuPlugin, RiscvCsrPlugin
    (void)pb;
    (void)sizeof(U);
  }

  // LATE 阶段: writeback
  template <typename U>
  static void register_late_plugins(cf::plugin::PipeBuilder& pb,
                                    const CPUConfig& /*config*/) {
    // M4G D.2 (G.2.10): 编译期 smoke test, 验证模板参数化 RegFilePlugin 实例化链
    // 不调用 build() 也不注册到 PipeBuilder, 仅声明局部变量触发模板实例化
    // M4-DSE 启动时删除此 smoke test
    cf::cpu::plugins::RegFilePlugin<U> rf_smoke_{};
    (void)rf_smoke_;

    // writeback: RegFilePlugin (M4-DSE 实施)
    // memory: DBusPlugin
    (void)pb;
    (void)sizeof(U);
  }
};

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CPU_FACTORY_H
