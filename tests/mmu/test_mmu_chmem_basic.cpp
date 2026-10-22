// tests/mmu/test_mmu_chmem_basic.cpp
//
// openspec change mmu-chmem-phase-b-c-launch / Phase B (B.4, 路线 C 收窄版)
//
// ============================================================================
// 范围: MMUPluginChmem 的 **TLB 命中路径** 端到端验证
// ============================================================================
// 2026-10-22 用户裁决路线 C: 本增量只交付 TLB 翻译路径, PTW 页表 walk 推迟到
// 后续 change (理由见 ip/mmu/plugins/MMUPlugin_chmem.h 头部 §范围)。
// 因此 TLB 由**测试直接装载** (refill 端口 = entry_* 的 ch_reg 句柄),
// 不做「TLB miss → 逐级读 PTE → refill」。
//
// 验证 7 件事:
//   1. TLB 命中 ⇒ 非恒等翻译 (PADDR 来自 entry 的页基址 + vaddr 的页内偏移)
//   2. TLB miss ⇒ PADDR_VALID=0 且 EXCEPTION_CODE=0
//      (miss **不是** fault: 消费端回退 vaddr, 与 Bare 路径同构 ⇒ 不误触发 trap)
//   3. Bare 隔离 ⇒ 即使 TLB 有命中项也不翻译 (sv_mode_ 门控)
//   4. entry.valid 被真正参与判定 (仅 tag 相同不够) —— 变异对照可检测性
//   5. ifetch / loadstore 两个 lookup 端口共享同一 TLB
//   6. 未绑定端口惰性 (PADDR_VALID=0), 不产生悬空节点
//   7. payload cell 里是**活节点**而非 elaborated 常量 —— 逐拍翻转 ≥4 次
//      (常量折叠实现 ⇒ 0 翻转 ⇒ RED)
//
// ============================================================================
// 夹具纪律 (踩过的坑, 见 .rddf/plans/mmu-chmem-phase-b-c-launch 相关注记)
// ============================================================================
//  - `ch::core::ctx_swap` 不可拷贝/不可移动 ⇒ 只能作**函数作用域**守卫, 不可是
//    fixture 数据成员 (本文件所有 ch 节点创建都发生在 fixture 构造函数体内)。
//  - refill (`ch_reg::operator<<=`) 必须在 `create_simulator()` **之前**
//    (Simulator 构造时快照寄存器链)。
//  - **探针插件必须先于 MMU 插件注册**: 二者都挂 ("fetch", EARLY) /
//    ("memory", EARLY); 同一 (stage, phase) 桶内按插入序执行 (pipe_builder.h:157
//    setup/build 按注册序遍历; 桶内稳定序), 而 MMU 的 EARLY 闭包会解引用探针的
//    `ch_uint<32>*` ⇒ 探针不先跑则读到空句柄。
//  - 探针信号在 `setup()` 里创建 (早于 `pb->elaborate()`), 这样 fixture 可以在
//    `build()` 之后、`elaborate()` 之前完成 `bind_*` (绑定晚于 elaborate 无效 ——
//    MMU 闭包那时已捕获了 nullptr)。
//
// 编译条件: CF_PLUGIN_USE_CH_MEM (chipforge_tests_chmem target)

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/mmu/plugins/MMUPlugin_chmem.h"

using namespace ch;
using namespace ch::core;

namespace cfc = cf::plugin;
namespace cfmmu = cf::ip::mmu;

namespace {

// 与消费端 (ibus_chmem.h:121 / dmem_chmem.h:142) 必须同实例化
using K = cf::ip::mmu::payload::mmu_keys<std::uint64_t>;

// =========================================================================
// VaddrProbe — 测试内探针: 创建两个可 set_input_value 的 vaddr 信号
// (仅测试内定义, 不进生产代码)
// =========================================================================
class VaddrProbe : public cfc::PluginBase {
 public:
  void setup(cfc::PipeBuilder& /*pb*/) override {
    ifetch_ = std::make_unique<ch_uint<32>>(ch_uint<32>(ch_literal<0, 32>{}), "probe_ifetch_vaddr");
    loadstore_ =
        std::make_unique<ch_uint<32>>(ch_uint<32>(ch_literal<0, 32>{}), "probe_loadstore_vaddr");
  }

  void build(cfc::PipeBuilder& /*pb*/) override {}

  ch_uint<32>& ifetch_vaddr() { return *ifetch_; }
  ch_uint<32>& loadstore_vaddr() { return *loadstore_; }

 private:
  std::unique_ptr<ch_uint<32>> ifetch_;
  std::unique_ptr<ch_uint<32>> loadstore_;
};

// =========================================================================
// 夹具
// =========================================================================
struct TlbInit {
  std::size_t idx = 0;
  std::uint32_t vaddr = 0;
  std::uint32_t paddr = 0;
  bool valid = true;
};

struct MmuCfg {
  cfmmu::SvMode mode = cfmmu::SvMode::Sv32;
  std::uint32_t satp = 0x80000000u;  // Sv32 (bit31=1) + PPN=0
  bool bind_ports = true;
  std::vector<TlbInit> entries;
};

// sv32 页基址 (paddr[31:12]) 与页内偏移 (vaddr[11:0]) 拼装 —— 断言的期望值算法
constexpr std::uint64_t expected_paddr(std::uint32_t entry_paddr, std::uint32_t vaddr) {
  return (static_cast<std::uint64_t>(entry_paddr) & 0xFFFFF000ull) |
         (static_cast<std::uint64_t>(vaddr) & 0xFFFull);
}

struct MmuFixture {
  ch::core::context ctx;
  std::unique_ptr<cfc::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  VaddrProbe* probe = nullptr;
  cfmmu::MMUPluginChmem* mmu = nullptr;

  explicit MmuFixture(const MmuCfg& cfg)
      : ctx("mmu_chmem_basic_ctx"), pb(std::make_unique<cfc::PipeBuilder>(&ctx)) {
    ch::core::ctx_swap guard(&ctx);

    // 探针先注册 (同 phase 桶内先执行 ⇒ MMU 闭包可安全解引用其信号)
    auto pr = std::make_unique<VaddrProbe>();
    probe = pr.get();
    pb->register_plugin(std::move(pr));

    auto m = std::make_unique<cfmmu::MMUPluginChmem>(cfg.mode, cfg.satp);
    mmu = m.get();
    pb->register_plugin(std::move(m));

    cfc::auto_throw(pb->build());

    // 绑定必须在 elaborate() 之前 (elaborate 期闭包才读取绑定指针)
    if (cfg.bind_ports) {
      mmu->bind_ifetch_vaddr(&probe->ifetch_vaddr());
      mmu->bind_loadstore_vaddr(&probe->loadstore_vaddr());
    }

    pb->elaborate();

    // refill (必须在 create_simulator() 之前 —— Simulator 构造时快照寄存器链)
    for (const auto& e : cfg.entries) {
      mmu->entry_valid(e.idx) <<=
          e.valid ? ch_uint<1>(ch_literal<1, 1>{}) : ch_uint<1>(ch_literal<0, 1>{});
      mmu->entry_vaddr(e.idx) <<= ch_uint<32>(e.vaddr);
      mmu->entry_paddr(e.idx) <<= ch_uint<32>(e.paddr);
    }

    sim = cfc::auto_throw(pb->create_simulator());
    sim->reset();
  }

  cfc::PipeNode* ifetch_node() { return pb->node_of_logic_stage("tlb_lookup_ifetch").get(); }
  cfc::PipeNode* loadstore_node() { return pb->node_of_logic_stage("tlb_lookup_loadstore").get(); }

  void drive_ifetch(std::uint64_t vaddr) {
    sim->set_input_value(probe->ifetch_vaddr(), vaddr);
    sim->tick();
  }
  void drive_loadstore(std::uint64_t vaddr) {
    sim->set_input_value(probe->loadstore_vaddr(), vaddr);
    sim->tick();
  }

  std::uint64_t rd(cfc::PipeNode* n, const cfc::Payload<cfc::uint_t<64>>& k) {
    return static_cast<std::uint64_t>(sim->get_value(n->operator()(k)));
  }
  std::uint64_t rd8(cfc::PipeNode* n, const cfc::Payload<cfc::uint_t<8>>& k) {
    return static_cast<std::uint64_t>(sim->get_value(n->operator()(k)));
  }
  std::uint64_t rd1(cfc::PipeNode* n, const cfc::Payload<cfc::uint_t<1>>& k) {
    return static_cast<std::uint64_t>(sim->get_value(n->operator()(k)));
  }

  std::uint64_t if_paddr() { return rd(ifetch_node(), K::PADDR); }
  std::uint64_t if_vaddr() { return rd(ifetch_node(), K::MMU_VADDR); }
  std::uint64_t if_valid() { return rd1(ifetch_node(), K::PADDR_VALID); }
  std::uint64_t if_exc() { return rd8(ifetch_node(), K::EXCEPTION_CODE); }
  std::uint64_t if_ptw_active() { return rd1(ifetch_node(), K::PTW_ACTIVE); }
  std::uint64_t ls_paddr() { return rd(loadstore_node(), K::PADDR); }
  std::uint64_t ls_valid() { return rd1(loadstore_node(), K::PADDR_VALID); }
};

constexpr std::uint32_t kEntryVaddr = 0x80001000u;
constexpr std::uint32_t kEntryPaddr = 0x20001000u;
constexpr std::uint32_t kProbeVaddr = 0x80001ABCu;  // 同页, 偏移 0xABC (非恒等)

}  // namespace

// =========================================================================
// 1. TLB 命中 ⇒ 非恒等翻译
// =========================================================================
TEST_CASE("mmu_chmem_tlb_hit_translates_page", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, true}};
  MmuFixture f(cfg);

  f.drive_ifetch(kProbeVaddr);

  REQUIRE(f.if_valid() == 1);
  REQUIRE(f.if_vaddr() == kProbeVaddr);
  REQUIRE(f.if_paddr() == expected_paddr(kEntryPaddr, kProbeVaddr));
  REQUIRE(f.if_paddr() != kProbeVaddr);  // 非恒等 (页基址被替换)
  REQUIRE(f.if_exc() == 0);
  REQUIRE(f.if_ptw_active() == 0);  // PTW 未落地 ⇒ 恒 0
  SUCCEED("TLB hit: 0x80001ABC -> 0x20001ABC");
}

// =========================================================================
// 2. TLB miss ⇒ PADDR_VALID=0 且不报 fault (回退 vaddr, 与 Bare 同构)
// =========================================================================
TEST_CASE("mmu_chmem_tlb_miss_keeps_paddr_invalid", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;  // 不预载任何 entry
  MmuFixture f(cfg);

  f.drive_ifetch(kProbeVaddr);

  REQUIRE(f.if_valid() == 0);
  REQUIRE(f.if_exc() == 0);  // miss ≠ fault
  REQUIRE(f.if_vaddr() == kProbeVaddr);
  SUCCEED("TLB miss: PADDR_VALID=0, EXCEPTION_CODE=0");
}

// =========================================================================
// 3. Bare 隔离 —— 即使 TLB 有命中项也不翻译
// =========================================================================
TEST_CASE("mmu_chmem_bare_mode_is_isolated", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.mode = cfmmu::SvMode::Bare;
  cfg.satp = 0;  // Bare
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, true}};
  MmuFixture f(cfg);

  f.drive_ifetch(kProbeVaddr);

  REQUIRE(f.if_valid() == 0);  // sv_mode_=Bare ⇒ 恒不翻译
  REQUIRE(f.if_vaddr() == kProbeVaddr);
  SUCCEED("Bare: 命中项被 sv_mode_ 门控屏蔽");
}

// =========================================================================
// 4. entry.valid 真正参与判定 (变异对照: 只比 tag 的实现 ⇒ RED)
// =========================================================================
TEST_CASE("mmu_chmem_invalid_entry_does_not_translate", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, /*valid=*/false}};
  MmuFixture f(cfg);

  f.drive_ifetch(kProbeVaddr);

  // tag 与 paddr 都对, 只有 valid=0 ⇒ 必须不命中
  REQUIRE(f.if_valid() == 0);
  SUCCEED("entry.valid=0: tag 相同也不翻译");
}

// =========================================================================
// 5. ifetch / loadstore 两个端口共享同一 TLB
// =========================================================================
TEST_CASE("mmu_chmem_loadstore_port_shares_tlb", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, true}};
  MmuFixture f(cfg);

  f.drive_loadstore(kProbeVaddr);

  REQUIRE(f.ls_valid() == 1);
  REQUIRE(f.ls_paddr() == expected_paddr(kEntryPaddr, kProbeVaddr));
  SUCCEED("loadstore 端口命中同一 entry");
}

// =========================================================================
// 6. 未绑定端口惰性 (不产生悬空节点, PADDR_VALID=0)
// =========================================================================
TEST_CASE("mmu_chmem_unbound_ports_are_inert", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.bind_ports = false;  // 不调 bind_*
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, true}};
  MmuFixture f(cfg);

  // 未绑定时 slot 走 null_vaddr() (字面量 0) ⇒ 不命中任何 entry
  REQUIRE(f.if_valid() == 0);
  REQUIRE(f.ls_valid() == 0);
  SUCCEED("未绑定端口: 惰性 0, 无悬空句柄");
}

// =========================================================================
// 7. payload cell 是**活节点** —— 逐拍翻转 ≥4 次
//
// 若实现把 PADDR_VALID 折成 elaboration 期常量 (本 change 要消灭的缺陷类),
// 本 case 见到的每次读数都相同 ⇒ 0 翻转 ⇒ RED。
// =========================================================================
TEST_CASE("mmu_chmem_payload_is_live_node", "[mmu][chmem][mmu-chmem-basic]") {
  MmuCfg cfg;
  cfg.entries = {{0, kEntryVaddr, kEntryPaddr, true}};
  MmuFixture f(cfg);

  constexpr std::uint64_t kUnmapped = 0x40000000ull;  // 不同页 ⇒ miss
  int flips = 0;
  std::uint64_t prev = f.if_valid();

  for (int i = 0; i < 6; ++i) {
    f.drive_ifetch((i % 2 == 0) ? kProbeVaddr : kUnmapped);
    const std::uint64_t cur = f.if_valid();
    if (cur != prev) {
      ++flips;
    }
    prev = cur;
  }

  REQUIRE(flips >= 4);  // 常量折叠实现 ⇒ 0
  SUCCEED("PADDR_VALID 随输入逐拍翻转 (活节点)");
}

#endif  // CF_PLUGIN_USE_CH_MEM
