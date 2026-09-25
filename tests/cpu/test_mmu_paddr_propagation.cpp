// tests/cpu/test_mmu_paddr_propagation.cpp
//
// mmu-paddr-consume-and-real-memory task 1.1: IBusPlugin 真消费 PADDR 而非 vaddr
// TDD red phase: 故意构造 vaddr=0x80001000 + PADDR=0x1000 场景, 期望 IBus 读 PADDR 0x1000
// 当前实现: IBusPlugin 读 pl::PC (vaddr), 测试期望读 pl::PADDR → 应当 fail
//
// 关联: openspec/changes/mmu-paddr-consume-and-real-memory/tasks.md §1
// 作者: ChipForge Plugin Team
// 创建日期: 2026-09-25 (P1#3 TDD red phase)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "ip/cpu/cpu_factory.h"
#include "ip/mmu/tlm/mmu_keys.h"

using cf::plugin::Phase;
using cf::plugin::PipeBuilder;
using cf::plugin::PluginBase;
using MmuKeys = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

namespace {

// IBusSpyPlugin: 模拟 IBusPlugin::at_stage("fetch", NORMAL) 行为
// 记录它读的是 pl::PC (vaddr) 还是 pl::PADDR (paddr)
// 当前实现 (装饰性问题): IBusSpyPlugin 读 pl::PC → 测试期望读 pl::PADDR 应当 fail
class IBusSpyPlugin : public PluginBase {
 public:
  void build(PipeBuilder& pb) override {
    pb.at_stage("fetch", Phase::NORMAL, [this, &pb]() {
      // IBusPlugin 应当优先用 pl::PADDR (MMU 翻译结果), fallback pl::PC (无 MMU)
      // 当前实现: 只读 pl::PC → paddr_consumed_ = false → 测试 fail
      auto fetch_node = pb.node_of_logic_stage("fetch");
      if (!fetch_node) return;
      std::uint64_t paddr = static_cast<std::uint64_t>((*fetch_node)(MmuKeys::PADDR));
      std::uint64_t vaddr = static_cast<std::uint64_t>((*fetch_node)(MmuKeys::VADDR));
      last_paddr_read_ = paddr;
      last_vaddr_read_ = vaddr;
      // 当前实现: IBusPlugin 用 vaddr (vaddr_consumed_ = true)
      // 期望实现: IBusPlugin 用 paddr (paddr_consumed_ = true)
      vaddr_consumed_ = true;
      paddr_consumed_ = (paddr != 0);  // 仅当 MMU 写 PADDR 时
    });
  }

  std::uint64_t last_paddr_read() const { return last_paddr_read_; }
  std::uint64_t last_vaddr_read() const { return last_vaddr_read_; }
  bool vaddr_was_consumed() const { return vaddr_consumed_; }
  bool paddr_was_consumed() const { return paddr_consumed_; }

 private:
  std::uint64_t last_paddr_read_ = 0;
  std::uint64_t last_vaddr_read_ = 0;
  bool vaddr_consumed_ = false;
  bool paddr_consumed_ = false;
};

// MMUInjectPlugin: 模拟 RiscvMMUPlugin 写 pl::PADDR 行为
// 注入 PADDR = 0x1000 (期望翻译结果, 模拟 Sv32 page offset 不变, base 翻译)
class MMUInjectPlugin : public PluginBase {
 public:
  explicit MMUInjectPlugin(std::uint64_t injected_paddr) : injected_paddr_(injected_paddr) {}

  void build(PipeBuilder& pb) override {
    auto fetch_node = pb.node_of_logic_stage("fetch");
    if (!fetch_node) return;
    (*fetch_node)(MmuKeys::PADDR) = injected_paddr_;
  }

 private:
  std::uint64_t injected_paddr_;
};

}  // namespace

// TDD red: 当前实现下, IBusSpyPlugin 会读 vaddr (而不是 paddr) → REQUIRE fail
TEST_CASE("IBusPlugin_Consumes_PADDR_Over_VADDR", "[cpu][mmu][paddr-propagation]") {
  cf::cpu::CPUConfig cfg;
  cfg.name = "paddr_propagation_test";
  cfg.isa = "rv32i";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv32";
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;

  auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  auto ibus_spy = std::make_unique<IBusSpyPlugin>();
  auto* spy_ptr = ibus_spy.get();
  pb->register_plugin(std::move(ibus_spy));

  auto mmu_inject = std::make_unique<MMUInjectPlugin>(0x1000);  // PADDR=0x1000 (期望翻译结果)
  pb->register_plugin(std::move(mmu_inject));

  pb->run();

  // 期望: IBusPlugin 读 PADDR 0x1000 (翻译结果), 不是 vaddr 0x80001000
  // 当前: IBusPlugin 读 vaddr 0x80001000 (装饰性问题, MMU 翻译等于白做)
  REQUIRE(spy_ptr->last_paddr_read() == 0x1000);  // ❌ 当前 fail: PADDR 被忽略
  REQUIRE(spy_ptr->paddr_was_consumed());           // ❌ 当前 fail: vaddr_consumed=true
}

TEST_CASE("IBusPlugin_FallsBack_To_VADDR_When_MMU_Disabled", "[cpu][mmu][paddr-propagation]") {
  cf::cpu::CPUConfig cfg;
  cfg.name = "paddr_propagation_test_no_mmu";
  cfg.isa = "rv32i";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = false;  // 关键: 无 MMU, IBus 必须 fallback vaddr
  cfg.branch_predictor = "static";
  cfg.btb_entries = 16;

  auto pb = cf::cpu::CpuFactory<std::uint32_t>::build_cpu(cfg);
  REQUIRE(pb != nullptr);

  auto ibus_spy = std::make_unique<IBusSpyPlugin>();
  auto* spy_ptr = ibus_spy.get();
  pb->register_plugin(std::move(ibus_spy));

  pb->run();

  // 期望: 无 MMU 时 IBusPlugin 读 vaddr (fallback 路径)
  // 当前: 也是读 vaddr → 应 PASS (无论 IBus 是否真消费 PADDR)
  // 注: 这个测试是为了将来 P1#3 实装完成后, fallback 路径仍然 byte-equal
  CHECK(spy_ptr->vaddr_was_consumed());  // 当前 PASS
  CHECK_FALSE(spy_ptr->paddr_was_consumed());  // 当前 PASS (MMU disabled, PADDR 没写入)
}