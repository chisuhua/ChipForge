// tests/mmu/test_mmu_keys_modality.cpp
//
// openspec change chmem-mmu-keys-modality（Phase B 的 B.3/B.4 硬前置）
//
// 验证 CH_MEM 模式下 MMU payload 通道的 4 件事：
//
//   1. **类型模式化** —— `mmu_keys` 的目标 key 的 cell 类型是 mode-aware
//      （`ch_uint<N>` 代理）而不是裸 POD（`bool` / `uint64_t`）。
//      这是 Phase B 的 B.3（`MMUPlugin_chmem` 真翻译）可实现的前提：
//      CH_MEM 模式下 `PayloadStore` 按精确类型匹配（`payload.h:122-128`），
//      裸类型 cell 装不下 `ch_bool` / `ch_uint` ⇒ 通道只能承载常量。
//
//   2. **鉴别力** —— 生产者写入的 ch 表达式被消费者读到时是**同一个 DAG 节点**
//      （`impl()` 指针相等），且该值**随拍更新**（Simulator tick ≥2 拍观测到翻转）。
//      ⇒ 若通道退化成常量（`ch_bool(cell)` 折常量），本 case 立即红。
//
//   3. **顺序契约** —— 同 stage 内 EARLY 必先于 NORMAL（`Phase` enum: EARLY=0 <
//      NORMAL=1 < LATE=2；`pipe_builder.h:414-424` 同 stage 内按 phase 顺序、同
//      phase 桶内按插入序稳定排序）⇒ 生产者挂在 EARLY、消费者挂在 NORMAL 时，
//      消费者必然读到生产者写的值；反过来（消费者 EARLY / 生产者 LATE）则读不到。
//      这条契约**不依赖 plugin 注册顺序**（`canonical_stage_order()` 只看 stage
//      名字首现序，注册顺序根本不在排序输入里）。
//
//   4. **变异对照可检测性** —— 证明两种典型变异都会被本文件的断言抓住：
//      ① 「折常量」（消费端 `ch_bool(cell)`）② 「读错 key」。
//
// ⚠ 探测纪律（见 `.rddf/plans/chmem-mmu-keys-modality.md` D-6）：
//   - cell 探测一律 `has()` + const-get；**禁止**非 const `operator()`
//     （`payload.h:151-161` 会 emplace `ch_uint<N>()` null impl，污染 `has()` 与 DAG）
//   - 动态行为断言必须经 **Simulator 读值**；禁止 C++ 侧直读 cell 判动态性
//     （那是陈旧值，见 capability `ctrllink-continuous-handle` 的契约）
//   - ⚠ **K5 边界约束**：CppHDL 表达式即节点 ⇒ 同一 API 两次调用产生**不同指针**。
//     本文件的「指针相等」断言**仅适用于 cell 传递路径**；若涉及 API 生成的节点
//     （如 `OR_combined_halt_conds()`），必须断言**值等价**而非指针相等。
//
// 编译条件: CF_PLUGIN_USE_CH_MEM（chipforge_tests_chmem target）

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "ip/mmu/tlm/mmu_keys.h"

using namespace ch;
using namespace ch::core;

namespace cfc = cf::plugin;
namespace pk = cf::ip::mmu::payload;

namespace {

// 与消费端（`ibus_chmem.h:121` / `dmem_chmem.h:142`）必须同实例化
using K = pk::mmu_keys<std::uint64_t>;

constexpr const char* kProbeStage = "probe";

// =========================================================================
// 探针插件（仅测试内定义，**不进生产代码**）
//
// 理由：`cpu_factory_chmem.h:119` 当前**不注册任何 MMU plugin** ⇒ 从 factory
// 里找不到 MMU 是**已知正常态**（注册是 Phase C.4 的范围）。因此测试自建
// PipeBuilder，参考 `tests/cpu/test_mmu_ptw_fsm_chmem.cpp` 的范式。
// =========================================================================

// 生产者：在指定 phase 把 ch 表达式写进 payload cell
//   —— `n->operator()(key) = expr` 走 `logic_buffer<T>::operator=`
//      （`lnode/logic_buffer.tpp:21-26`），它**只重绑 node 指针**，不发射 assign；
//      即：cell 里的 C++ 代理被重绑到生产者构造的 DAG 节点上。
class ProbeProducer : public cfc::PluginBase {
 public:
  ProbeProducer(ch_reg<ch_bool>* src, cfc::Phase phase)
      : src_(src), phase_(phase) {}

  void setup(cfc::PipeBuilder& pb) override {
    pb.at_stage(kProbeStage, phase_, [this, &pb] { this->produce(pb); });
  }
  void build(cfc::PipeBuilder&) override {}

  void produce(cfc::PipeBuilder& pb) {
    auto* n = pb.node_of_logic_stage(kProbeStage).get();
    if (!n) return;
    // ch_reg<ch_bool> 是 ch_bool 的子类（`core/reg.h:32 class ch_reg : public T`）
    // ⇒ `ch_uint<1>(ch_bool)` 走 `ch_uint(const ch_bool&)`（`core/uint.h:69-72`,
    //    `requires(N == 1)`）⇒ node_impl_ = src 的 proxy 节点 ⇒ 随拍更新。
    n->operator()(K::PADDR_VALID) = ch_uint<1>(*src_);
    n->operator()(K::PADDR) = ch_uint<64>(ch_literal<0, 64>{});
  }

 private:
  ch_reg<ch_bool>* src_;
  cfc::Phase phase_;
};

// 消费者：在指定 phase 读 payload cell
class ProbeConsumer : public cfc::PluginBase {
 public:
  explicit ProbeConsumer(cfc::Phase phase) : phase_(phase) {}

  void setup(cfc::PipeBuilder& pb) override {
    pb.at_stage(kProbeStage, phase_, [this, &pb] { this->consume(pb); });
  }
  void build(cfc::PipeBuilder&) override {}

  void consume(cfc::PipeBuilder& pb) {
    auto* n = pb.node_of_logic_stage(kProbeStage).get();
    // ⚠ 一律 has() + const 路径；不用非 const operator() 探测
    if (n && n->payloads().has(K::PADDR_VALID)) {
      has_cell_ = true;
      seen_valid_ = n->operator()(K::PADDR_VALID);  // 拷贝 → 捕获当时的 node
      seen_valid_impl_ = seen_valid_.impl();
    } else {
      has_cell_ = false;
      seen_valid_impl_ = nullptr;
    }
  }

  bool has_cell_ = false;
  ch_uint<1> seen_valid_;
  lnodeimpl* seen_valid_impl_ = nullptr;

 private:
  cfc::Phase phase_;
};

// =========================================================================
// Fixture: 自建 PipeBuilder + 生产者/消费者 + Simulator
// =========================================================================
struct ModalityFixture {
  // ⚠ ctx_swap 必须是**成员**（而非构造函数内的局部变量）—— 局部 guard 在构造函数
  //   结束时就析构了，测试体内再创建 ch 节点（如 `ch_bool(true)`）会因无 current
  //   context 而拿到 **null impl**。
  ch::core::context ctx{"mmu_keys_modality_ctx"};
  ch::core::ctx_swap guard{&ctx};
  std::unique_ptr<cfc::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  ProbeConsumer* consumer = nullptr;

  // producer_phase / consumer_phase 决定先后；默认 = 契约要求 (EARLY → NORMAL)
  ModalityFixture(cfc::Phase producer_phase = cfc::Phase::EARLY,
                  cfc::Phase consumer_phase = cfc::Phase::NORMAL)
      : pb(std::make_unique<cfc::PipeBuilder>(&ctx)) {
    // 有状态源：每拍自翻转（`reg <<= !reg`）
    // ⚠ `ch_reg::operator<<=` 必须在 **Simulator 创建之前**调用（Simulator 初始化
    //   时快照寄存器链，之后再 `<<=` 静默无效 —— 上游 lnode/reg.tpp）。
    src_ = std::make_unique<ch_reg<ch_bool>>(ch_bool(false), "modality_src");
    src_->operator<<=(!(*src_));

    // 「折常量」对照源: 值**恒定**的寄存器 (等价于 elaboration 期冻结的常量)。
    // 用寄存器而非 literal —— literal 节点不在 Simulator 值表里, get_value 会
    // 走 "Value not found" 路径 (噪音 + 语义不清); 寄存器则被正常求值。
    frozen_ = std::make_unique<ch_reg<ch_bool>>(ch_bool(false), "frozen_const");
    frozen_->operator<<=(ch_bool(false));  // next 恒 false ⇒ 每拍求值都是 false

    auto prod = std::make_unique<ProbeProducer>(src_.get(), producer_phase);
    auto cons = std::make_unique<ProbeConsumer>(consumer_phase);
    consumer = cons.get();

    pb->register_plugin(std::move(prod));
    pb->register_plugin(std::move(cons));
    pb->build();
    pb->elaborate();
    sim = cfc::auto_throw(pb->create_simulator());
    sim->reset();
  }

  // 逐拍观测 cell 里的值（必须经 Simulator，禁止 C++ 侧直读）
  std::uint64_t observed() const {
    return static_cast<std::uint64_t>(sim->get_value(consumer->seen_valid_));
  }

  // 逐拍观测「冻结常量」对照源
  std::uint64_t frozen_observed() const {
    return static_cast<std::uint64_t>(sim->get_value(ch_bool(frozen_->impl())));
  }

  ch_reg<ch_bool>* src() const { return src_.get(); }

 private:
  std::unique_ptr<ch_reg<ch_bool>> src_;
  std::unique_ptr<ch_reg<ch_bool>> frozen_;
};

}  // anonymous namespace

// =========================================================================
// Case 1: key 的 cell 类型是 mode-aware（编译期断言 + 运行期非空）
//
// RED 形态 = **编译失败**（当前 `mmu_keys` 用裸类型 ⇒ cell 是 POD）
// =========================================================================
TEST_CASE("mmu_keys_modality_cells_are_mode_aware", "[mmu][chmem][keys]") {
  // ⚠ 必须有 context —— ch 节点的创建 (literal / op) 需要 current context
  ch::core::context ctx("mmu_keys_modality_type_ctx");
  ch::core::ctx_swap guard(&ctx);

  cfc::PayloadStore store;

  // 编译期：CH_MEM 模式下 cell 必须是 ch 代理，不是 POD
  static_assert(
      std::is_same_v<decltype(store.get(K::PADDR_VALID)), ch_uint<1>&>,
      "PADDR_VALID cell must be ch_uint<1> in CH_MEM mode (mode-aware key)");
  static_assert(
      std::is_same_v<decltype(store.get(K::PTW_ACTIVE)), ch_uint<1>&>,
      "PTW_ACTIVE cell must be ch_uint<1> in CH_MEM mode (mode-aware key)");
  static_assert(
      std::is_same_v<decltype(store.get(K::PADDR)), ch_uint<64>&>,
      "PADDR cell must be ch_uint<64> in CH_MEM mode (mode-aware key)");
  static_assert(
      std::is_same_v<decltype(store.get(K::MMU_VADDR)), ch_uint<64>&>,
      "MMU_VADDR cell must be ch_uint<64> in CH_MEM mode (mode-aware key)");
  static_assert(
      std::is_same_v<decltype(store.get(K::EXCEPTION_CODE)), ch_uint<8>&>,
      "EXCEPTION_CODE cell must be ch_uint<8> in CH_MEM mode (mode-aware key)");

  // 运行期：cell 存在且 impl() 非空（不是 null impl）
  store.put(K::PADDR_VALID, ch_uint<1>(ch_literal<1, 1>{}));
  REQUIRE(store.get(K::PADDR_VALID).impl() != nullptr);

  // ⚠ 反证：裸类型 key 在 CH_MEM 下装不下 ch 信号（这正是模式化的动因）
  //    —— 下面的写法无法编译，故用注释记录实测证据：
  //    put(Payload<bool>&, ch_uint<1>) → error: no matching function for call to
  //      'PayloadStore::put(Payload<bool>&, ch_uint<1>)'
  //    (见 RED 证据: chmem-mmu-keys-modality-RED.txt)
}

// =========================================================================
// Case 2（鉴别力核心）: 生产者写的 ch 表达式 → 消费者读到【同一节点】且随拍更新
// =========================================================================
TEST_CASE("mmu_keys_modality_producer_node_reaches_consumer",
          "[mmu][chmem][keys]") {
  ModalityFixture fx;

  // 消费者在 NORMAL 读到生产者（EARLY）写的 cell
  REQUIRE(fx.consumer->has_cell_);
  REQUIRE(fx.consumer->seen_valid_impl_ != nullptr);

  // 同一节点（cell 传递路径 ⇒ 指针相等成立；见 K5 边界约束）
  REQUIRE(fx.consumer->seen_valid_impl_ == fx.src()->impl());

  // 随拍更新：6 拍里至少 5 次翻转 ⇒ 确为运行期信号而非常量
  std::uint64_t prev = 2;  // 哨兵
  int flips = 0;
  for (int t = 0; t < 6; ++t) {
    fx.sim->tick();
    const std::uint64_t obs = fx.observed();
    INFO("tick " << t << ": cell=" << obs);
    if (prev != 2 && obs != prev) ++flips;
    prev = obs;
  }
  REQUIRE(flips >= 5);
}

// =========================================================================
// Case 3: 顺序契约 —— 消费者 EARLY / 生产者 LATE ⇒ 读不到（has()==false）
// =========================================================================
TEST_CASE("mmu_keys_modality_consumer_before_producer_sees_no_cell",
          "[mmu][chmem][keys]") {
  ModalityFixture fx(cfc::Phase::LATE, cfc::Phase::EARLY);  // 生产者晚于消费者

  REQUIRE_FALSE(fx.consumer->has_cell_);
  REQUIRE(fx.consumer->seen_valid_impl_ == nullptr);
}

// =========================================================================
// Case 4: 变异对照可检测性
//   ① 「折常量」：把 cell 折成 ch_bool 后，值被冻结 ⇒ 与 cell 的真实节点可区分
//      ⇒ 若消费端退回 `ch_bool(cell)`，Case 2 的「随拍更新」断言必红
//   ② 「读错 key」：生产者没写的 key ⇒ has()==false ⇒ 读错必被抓住
// =========================================================================
TEST_CASE("mmu_keys_modality_mutations_are_detectable", "[mmu][chmem][keys]") {
  ModalityFixture fx;

  // ① 「折常量」对照：旧实现里消费端 `ch_bool(POD cell)` 得到的是**冻结常量**
  //    （elaboration 期求值一次后不再变化）。这里用一个**值恒定的寄存器**建模
  //    （等价物，且在 Simulator 里被正常求值）。
  //    ⇒ 若 Case 2 的「随拍更新」判据无法区分常量与节点，它就没有鉴别力。
  REQUIRE(fx.consumer->has_cell_);
  REQUIRE(fx.consumer->seen_valid_impl_ != nullptr);

  std::uint64_t prev_cell = 2;
  std::uint64_t prev_frozen = 2;
  int cell_flips = 0;
  int frozen_flips = 0;
  for (int t = 0; t < 6; ++t) {
    fx.sim->tick();
    const std::uint64_t cell = fx.observed();
    const std::uint64_t frozen = fx.frozen_observed();
    INFO("tick " << t << ": cell=" << cell << " frozen=" << frozen);
    if (prev_cell != 2 && cell != prev_cell) ++cell_flips;
    if (prev_frozen != 2 && frozen != prev_frozen) ++frozen_flips;
    prev_cell = cell;
    prev_frozen = frozen;
  }
  REQUIRE(cell_flips >= 5);    // 真实 cell 节点随拍翻转
  REQUIRE(frozen_flips == 0);  // 冻结常量不翻转 ⇒ 两者可区分 ⇒ 变异必被抓住

  // ② 「读错 key」：生产者只写了 PADDR_VALID + PADDR，没写 MMU_VADDR
  //    ⇒ 消费端若读错 key，has() 恒 false ⇒ 必被抓住
  auto* n = fx.pb->node_of_logic_stage(kProbeStage).get();
  REQUIRE(n != nullptr);
  REQUIRE(n->payloads().has(K::PADDR_VALID));
  REQUIRE(n->payloads().has(K::PADDR));
  REQUIRE_FALSE(n->payloads().has(K::MMU_VADDR));
}

#endif  // CF_PLUGIN_USE_CH_MEM
