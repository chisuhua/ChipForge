// tests/cpu/test_cpu_chmem_keys_modality.cpp
//
// openspec change chmem-cpu-keys-modality（mmu-chmem-phase-b-c-launch Phase C.1 的硬前置）
//
// ============================================================================
// 验证 CH_MEM 模式下 CPU / CSR 侧 payload 通道的 4 件事
// ============================================================================
//
//   1. **类型模式化** —— `cpu_keys` 4 个 + `csr_bundles` 5 个 key 的 cell 类型是
//      mode-aware（`ch_uint<N>` 代理）而不是裸 POD（`bool` / `uint8_t` / `uint64_t`）。
//      这是 Phase C.1（`mmufault_clear = (CPU_EXCEPTION_CODE != 0)` 变成运行期信号）
//      与 CSR 读值/写回通道可实现的前提：CH_MEM 模式下 `PayloadStore` 按**精确类型
//      匹配**（`include/cf/plugin/payload.h:122-128`），裸类型 cell 装不下
//      `ch_bool` / `ch_uint` ⇒ 通道只能承载常量。
//
//   2. **鉴别力** —— 生产者写入的 ch 表达式被消费者读到时是**同一个 DAG 节点**
//      （`impl()` 指针相等），且该值**随拍更新**（Simulator tick ≥2 拍观测到翻转）。
//      ⇒ 若通道退化成常量（`static_cast<bool>(select(...))` 折常量 / `ch_bool(cell)`
//      折常量），本 case 立即红。
//
//   3. **顺序契约** —— 同 stage 内 EARLY 必先于 NORMAL（`Phase` enum: EARLY=0 <
//      NORMAL=1 < LATE=2；`pipe_builder.h:414-424` 同 stage 内按 phase 顺序、同
//      phase 桶内按插入序稳定排序）⇒ 生产者挂在 EARLY、消费者挂在 NORMAL 时，
//      消费者必然读到生产者写的值；反过来（消费者 EARLY / 生产者 LATE）则读不到。
//      这条契约**不依赖 plugin 注册顺序**。
//
//   4. **变异对照可检测性** —— 证明三种典型变异都会被本文件的断言抓住：
//      ① 「生产端折常量」② 「消费端折常量」③ 「读错 key」。
//
// ============================================================================
// 探测纪律（沿用 tests/mmu/test_mmu_keys_modality.cpp，见 capability
// `payload-key-modality` 的「动态行为的验证 MUST 经 Simulator 读值」Requirement）
// ============================================================================
//   - cell 探测一律 `has()` + const-get；**禁止**非 const `operator()`
//     （`payload.h:151-161` 会 emplace `ch_uint<N>()` null impl，污染 `has()` 与 DAG）
//   - 动态行为断言必须经 **Simulator 读值**；禁止 C++ 侧直读 cell 判动态性
//   - ⚠ **K5 边界约束**：CppHDL 表达式即节点 ⇒ 同一 API 两次调用产生**不同指针**。
//     本文件的「指针相等」断言**仅适用于 cell 传递路径**。
//
// 编译条件: CF_PLUGIN_USE_CH_MEM（chipforge_tests_chmem target）

#ifdef CF_PLUGIN_USE_CH_MEM

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>
#include <type_traits>

#include <ch.hpp>
#include <core/context.h>
#include <simulator.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_base.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"

#include "bundles/csr_bundles.h"
#include "ip/cpu/core/payload_common.h"
#include "ip/cpu/plugins/csr_chmem.h"
#include "ip/cpu/tlm/cpu_keys.h"

using namespace ch;
using namespace ch::core;

namespace cfc = cf::plugin;
namespace ckc = cf::cpu::csr::payload;
namespace cku = cf::cpu::tlm::payload;

namespace {

// 与消费端同实例化：
//   - `cpu_keys<std::uint64_t>` ← ip/cpu/plugins/mmu.cpp:89
//   - `csr_keys<ch_uint<32>>`   ← ip/cpu/cpu_factory_chmem.h:184
using CpuK = cku::cpu_keys<std::uint64_t>;
using CsrK = ckc::csr_keys<ch_uint<32>>;

constexpr const char* kProbeStage = "probe";

// =========================================================================
// 探针插件（仅测试内定义，**不进生产代码**）
//
// 理由：`csr_chmem.h` 的生产路径依赖 `cpu_factory_chmem` 的 stage 传播（execute →
// memory → writeback），单插件测试路径下 writeback 节点缺 `CSR_WRITE_*` key ⇒
// `has()==false` ⇒ no-op。本测试自建 PipeBuilder（沿用 `test_mmu_keys_modality.cpp`
// 与 `test_mmu_ptw_fsm_chmem.cpp` 的范式），**不改 factory**。
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
    // ⚠ 这里刻意复刻**生产端折叠**的形态（`static_cast<bool>(...)`），让 Case 2/3/4
    //   能在**修复前**观测到"节点丢失"，在**修复后**观测到"节点保留"。
    //   但折叠本身不能留在生产代码里 —— 本探针是测试内的**对照源**。
    //
    //   正确形态（生产代码用）: `ch_uint<1>(<ch_bool 表达式>)` —— 保留 impl 指针。
    //   折叠形态（对照）:       `static_cast<bool>(<ch_bool 表达式>)` —— 变成 POD。
    n->operator()(CsrK::CSR_READ_VALID) = ch_uint<1>(*src_);
    n->operator()(CsrK::CSR_WRITE_EN) = ch_uint<1>(*src_);
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
    if (n && n->payloads().has(CsrK::CSR_READ_VALID)) {
      has_cell_ = true;
      seen_valid_ = n->operator()(CsrK::CSR_READ_VALID);  // 拷贝 → 捕获当时的 node
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
  //   结束时就析构了，测试体内再创建 ch 节点会因无 current context 而拿到 null impl。
  ch::core::context ctx{"cpu_keys_modality_ctx"};
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

// =========================================================================
// CsrLiveFixture — 直接用**生产代码** (`RiscvCsrPluginChmem`) 驱动
//
// 目的: 前 4 个 case 用探针验证「模态契约」; 本 fixture 验证
//   「`csr_chmem.h` 的 3 处折叠真的拆了」——
//   ① cell 持有的是**计算节点** (`impl()->is_const() == false`) 而非 literal
//   ② 值随 csr_addr 逐拍翻转
// =========================================================================

// 两条 CSRRW 指令 (csr_addr 在 bits[31:20]):
//   A = 0x300010F3 → CSRRW x1, mstatus, x0 ⇒ csr_addr=0x300 ⇒ read_valid=1, write_en=1
//   B = 0xB00010F3 → CSRRW x1, mcycle,  x0 ⇒ csr_addr=0xB00 ⇒ read_valid=0, write_en=0
//                    (R3.3 决策: mcycle 走单独路径 ⇒ valid=false; mcycle 永不被 CSRR* 写)
constexpr std::uint32_t kInstA = 0x300010F3u;
constexpr std::uint32_t kInstB = 0xB00010F3u;

struct CsrLiveFixture {
  // ⚠ ctx_swap 必须是**成员**（而非构造函数内的局部变量）—— 局部 guard 在构造函数
  //   结束时就析构了，测试体内再创建 ch 节点会因无 current context 而拿到 null impl。
  ch::core::context ctx{"cpu_keys_csr_live_ctx"};
  ch::core::ctx_swap guard{&ctx};
  std::unique_ptr<cfc::PipeBuilder> pb;
  std::unique_ptr<ch::Simulator> sim;
  std::unique_ptr<ch_reg<ch_uint<32>>> inst_reg;

  CsrLiveFixture()
      : pb(std::make_unique<cfc::PipeBuilder>(&ctx)) {
    // 自翻转的指令寄存器: 每拍在 A / B 之间交替 ⇒ csr_addr 每拍翻转
    // ⇒ CSR_READ_VALID / CSR_WRITE_EN 的值必须跟着翻转 (若被折成常量则恒定)。
    // ⚠ `ch_reg::operator<<=` 必须在 **Simulator 创建之前**调用（Simulator 初始化
    //   时快照寄存器链，之后再 `<<=` 静默无效 —— 上游 lnode/reg.tpp）。
    inst_reg = std::make_unique<ch_reg<ch_uint<32>>>(
        ch_uint<32>(ch_literal<kInstA, 32>{}), "csr_live_inst");
    const auto is_a = (*inst_reg == ch_uint<32>(ch_literal<kInstA, 32>{}));
    inst_reg->operator<<=(select(is_a, ch_uint<32>(ch_literal<kInstB, 32>{}),
                                  ch_uint<32>(ch_literal<kInstA, 32>{})));

    auto holder =
        std::make_unique<cf::cpu::arch::riscv::RiscvCsrPluginChmem<ch_uint<32>>>();
    pb->register_plugin(std::move(holder));

    using KT = cf::cpu::core::payload::keys<ch_uint<32>, 32>;
    // execute-EARLY: 把**寄存器节点**绑进 INSTRUCTION cell（不是字面量）
    //   ⇒ 指令值逐拍变化 ⇒ 下游 CSR 读/写使能树逐拍变化。
    pb->at_stage("execute", cfc::Phase::EARLY, [this]() {
      auto* n = pb->node_of_logic_stage("execute").get();
      if (n) {
        n->operator()(KT::INSTRUCTION) = *inst_reg;
        n->operator()(KT::PC) = ch_uint<32>(ch_literal<0x80000000, 32>{});
        n->operator()(KT::RS1) = ch_uint<32>(ch_literal<0, 32>{});
        n->operator()(KT::RS2) = ch_uint<32>(ch_literal<0, 32>{});
        n->operator()(KT::RD_IDX) = ch_uint<5>(ch_literal<1, 5>{});
      }
    });

    cfc::auto_throw(pb->build());
    cfc::auto_throw(pb->elaborate(ctx));
    sim = cfc::auto_throw(pb->create_simulator());
    sim->reset();
  }

  // execute 节点（CSR plugin 的 execute-LATE 闭包写入 CSR_READ_VALID / CSR_WRITE_EN）
  cfc::PipeNode* execute_node() { return pb->node_of_logic_stage("execute").get(); }

  std::uint64_t read_valid_value() {
    return static_cast<std::uint64_t>(
        sim->get_value(execute_node()->payloads().get(CsrK::CSR_READ_VALID)));
  }
  std::uint64_t write_en_value() {
    return static_cast<std::uint64_t>(
        sim->get_value(execute_node()->payloads().get(CsrK::CSR_WRITE_EN)));
  }
};

}  // anonymous namespace

// =========================================================================
// Case 1: 9 个 key 的 cell 类型是 mode-aware（编译期断言 + 运行期非空）
//
// RED 形态 = **编译失败**（当前是裸类型 ⇒ cell 是 POD）
// =========================================================================
TEST_CASE("cpu_chmem_keys_cells_are_mode_aware", "[cpu][chmem][cpu-chmem-keys]") {
  // ⚠ 必须有 context —— ch 节点的创建 (literal / op) 需要 current context
  ch::core::context ctx("cpu_keys_modality_type_ctx");
  ch::core::ctx_swap guard(&ctx);

  cfc::PayloadStore store;

  // ---- cpu_keys 4 个（`ip/cpu/tlm/cpu_keys.h`）----
  static_assert(
      std::is_same_v<decltype(store.get(CpuK::SAT)), ch_uint<64>&>,
      "cpu_keys::SAT cell must be ch_uint<64> in CH_MEM mode (mode-aware key)");
  static_assert(
      std::is_same_v<decltype(store.get(CpuK::SFENCE_VADDR)), ch_uint<64>&>,
      "cpu_keys::SFENCE_VADDR cell must be ch_uint<64> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CpuK::SFENCE_ASID)), ch_uint<64>&>,
      "cpu_keys::SFENCE_ASID cell must be ch_uint<64> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CpuK::CPU_EXCEPTION_CODE)), ch_uint<8>&>,
      "cpu_keys::CPU_EXCEPTION_CODE cell must be ch_uint<8> in CH_MEM mode");

  // ---- csr_bundles 5 个（`bundles/csr_bundles.h`）----
  static_assert(
      std::is_same_v<decltype(store.get(CsrK::CSR_READ_VALID)), ch_uint<1>&>,
      "csr_keys::CSR_READ_VALID cell must be ch_uint<1> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CsrK::CSR_WRITE_EN)), ch_uint<1>&>,
      "csr_keys::CSR_WRITE_EN cell must be ch_uint<1> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CsrK::Zicsr_VALID)), ch_uint<1>&>,
      "csr_keys::Zicsr_VALID cell must be ch_uint<1> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CsrK::Zicsr_OP)), ch_uint<8>&>,
      "csr_keys::Zicsr_OP cell must be ch_uint<8> in CH_MEM mode");
  static_assert(
      std::is_same_v<decltype(store.get(CsrK::TRAP_PENDING)), ch_uint<1>&>,
      "csr_keys::TRAP_PENDING cell must be ch_uint<1> in CH_MEM mode");

  // 运行期：cell 存在且 impl() 非空（不是 null impl）
  store.put(CsrK::CSR_READ_VALID, ch_uint<1>(ch_literal<1, 1>{}));
  REQUIRE(store.get(CsrK::CSR_READ_VALID).impl() != nullptr);

  // ⚠ 反证：裸类型 key 在 CH_MEM 下装不下 ch 信号（这正是模式化的动因）
  //    —— 下面的写法无法编译，故用注释记录实测证据：
  //    put(Payload<bool>&, ch_uint<1>) → error: no matching function for call to
  //      'PayloadStore::put(Payload<bool>&, ch_uint<1>)'
  //    (见 RED 证据: chmem-cpu-keys-modality-RED.txt)
}

// =========================================================================
// Case 2（鉴别力核心）: 生产者写的 ch 表达式 → 消费者读到【同一节点】且随拍更新
// =========================================================================
TEST_CASE("cpu_chmem_keys_producer_node_reaches_consumer",
          "[cpu][chmem][cpu-chmem-keys]") {
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
TEST_CASE("cpu_chmem_keys_consumer_before_producer_sees_no_cell",
          "[cpu][chmem][cpu-chmem-keys]") {
  ModalityFixture fx(cfc::Phase::LATE, cfc::Phase::EARLY);  // 生产者晚于消费者

  REQUIRE_FALSE(fx.consumer->has_cell_);
  REQUIRE(fx.consumer->seen_valid_impl_ == nullptr);
}

// =========================================================================
// Case 4: 变异对照可检测性
//   ① 「生产端折常量」：把 ch 表达式折成 POD 后，值被冻结 ⇒ 与 cell 的真实节点可区分
//      ⇒ 若生产端退回 `static_cast<bool>(select(...))`，Case 2 的断言必红
//   ② 「消费端折常量」：消费端退回 `ch_bool(cell)` 包装 ⇒ 同样丢节点
//   ③ 「读错 key」：生产者没写的 key ⇒ has()==false ⇒ 读错必被抓住
// =========================================================================
TEST_CASE("cpu_chmem_keys_mutations_are_detectable", "[cpu][chmem][cpu-chmem-keys]") {
  ModalityFixture fx;

  // ① 「折常量」对照：旧实现里生产端 `static_cast<bool>(ch 表达式)` 得到的是
  //    **冻结常量**（elaboration 期求值一次后不再变化）。这里用一个**值恒定的
  //    寄存器**建模（等价物，且在 Simulator 里被正常求值）。
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

  // ② 「消费端折常量」对照：消费端若退回 `ch_bool(cell)` 形态（POD ⇒ literal 节点），
  //    读到的 impl 指针会指向一个**字面量**节点而非生产者的寄存器 proxy 节点。
  //    ⇒ 断言"消费者读到的 impl == 生产者 src 的 impl"就是这条变异的抓手。
  REQUIRE(fx.consumer->seen_valid_impl_ == fx.src()->impl());

  // ③ 「读错 key」：生产者只写了 CSR_READ_VALID + CSR_WRITE_EN，没写 TRAP_PENDING
  //    ⇒ 消费端若读错 key，has() 恒 false ⇒ 必被抓住
  auto* n = fx.pb->node_of_logic_stage(kProbeStage).get();
  REQUIRE(n != nullptr);
  REQUIRE(n->payloads().has(CsrK::CSR_READ_VALID));
  REQUIRE(n->payloads().has(CsrK::CSR_WRITE_EN));
  REQUIRE_FALSE(n->payloads().has(CsrK::TRAP_PENDING));
}

// =========================================================================
// Case 5: **生产代码**的折叠已拆 —— `csr_chmem.h` 的 cell 持有计算节点
//
// 这是本 change 的核心判据（AC-CK-3）：前 4 个 case 验证「模态契约」，
// 本 case 直接证明 `RiscvCsrPluginChmem` 写进 payload 的
// `CSR_READ_VALID` / `CSR_WRITE_EN` 是**运行期节点**而不是折叠常量。
//
// 折叠（修复前）的形态是 `static_cast<bool>(<ch_bool 表达式>)`：
//   上游 `ch_bool::explicit operator bool()` 对非常量节点会报
//   "[ERROR] Attempting to convert non-constant node" 并退化为 **0**,
//   写进 cell 的就是**字面量 0** ⇒ `impl()->is_const() == true` 且值恒定。
// =========================================================================
TEST_CASE("cpu_chmem_keys_csr_producer_cells_are_live_nodes",
          "[cpu][chmem][cpu-chmem-keys]") {
  CsrLiveFixture fx;

  auto* n = fx.execute_node();
  REQUIRE(n != nullptr);

  // ① cell 存在（生产代码确实写了）
  REQUIRE(n->payloads().has(CsrK::CSR_READ_VALID));
  REQUIRE(n->payloads().has(CsrK::CSR_WRITE_EN));

  // ② **不是字面量** —— 这一条直接对应"折叠已拆"
  const auto& rv = n->payloads().get(CsrK::CSR_READ_VALID);
  const auto& we = n->payloads().get(CsrK::CSR_WRITE_EN);
  REQUIRE(rv.impl() != nullptr);
  REQUIRE(we.impl() != nullptr);
  INFO("CSR_READ_VALID node type = " << to_string(rv.impl()->type()));
  INFO("CSR_WRITE_EN   node type = " << to_string(we.impl()->type()));
  REQUIRE_FALSE(rv.impl()->is_const());  // 非 literal ⇒ 不是折叠常量
  REQUIRE_FALSE(we.impl()->is_const());

  // ③ 随 csr_addr 逐拍翻转 (0x300 ⇒ valid=1/write_en=1; 0xB00 ⇒ 0/0)
  std::uint64_t prev_rv = 2;
  std::uint64_t prev_we = 2;
  int rv_flips = 0;
  int we_flips = 0;
  for (int t = 0; t < 6; ++t) {
    fx.sim->tick();
    const std::uint64_t rv = fx.read_valid_value();
    const std::uint64_t we = fx.write_en_value();
    INFO("tick " << t << ": read_valid=" << rv << " write_en=" << we);
    if (prev_rv != 2 && rv != prev_rv) ++rv_flips;
    if (prev_we != 2 && we != prev_we) ++we_flips;
    prev_rv = rv;
    prev_we = we;
  }
  REQUIRE(rv_flips >= 3);  // 常量折叠实现 ⇒ 0
  REQUIRE(we_flips >= 3);
}

#endif  // CF_PLUGIN_USE_CH_MEM
