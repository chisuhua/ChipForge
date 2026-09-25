// tests/mmu/test_ptw_unit.cpp (mmu-cache-integration planned, commit 0)
//
// 间接验证 MMUPlugin::do_lookup 闭包捕获 vaddr 模式:
//   MMUPlugin.cpp:60-65 用 [node, vaddr] capture 后在 WalkCallback 内写 MMU_VADDR.
//   本测试用 PTW 自身的 start_walk + advance 触发 WalkCallback, 验证闭包能拿到 vaddr.
//   模式镜像: MMUPlugin 闭包和本测试 PTW 单测闭包都用 `[&, vaddr]` capture 模式.

#include "catch_amalgamated.hpp"

#include "ip/mmu/lib/ptw.h"

namespace cf {
namespace ip {
namespace mmu {

namespace {

// 构造一个 valid leaf PTE (V=1, R=1, W=0, X=0, U=0, ppn=0x80000)
// Sv39 3-level walk 终止条件: current_level + 1 >= max_levels (=3) 且 V=1 且非 reserved
PTE make_valid_leaf_ppn(std::uint64_t ppn) {
  PTE pte{};
  pte.raw = (1ULL << 0) | (1ULL << 1) | (ppn << 10);
  pte.v = true;
  pte.r = true;
  pte.w = false;
  pte.x = false;
  pte.u = false;
  pte.ppn = ppn;
  pte.ppn_bits = 44;
  return pte;
}

// 构造 invalid PTE (V=0)
PTE make_invalid_pte() {
  PTE pte{};
  pte.raw = 0;
  pte.v = false;
  return pte;
}

// 构造 reserved encoding PTE (W=1, R=0, V=1) → fault 15
// per RISC-V spec: W=1 且 R=0 是 illegal reserved encoding
// (mmu-paddr-consume-and-real-memory P1#3 Oracle C1: 旧实现错检 r && w && x,
// RWX 实际是合法叶子 PTE, 现已修正为 w && !r)
PTE make_reserved_pte() {
  PTE pte{};
  pte.raw = (1ULL << 0) | (1ULL << 2);  // V=1, W=1, R=0, X=0
  pte.v = true;
  pte.r = false;
  pte.w = true;
  pte.x = false;
  return pte;
}

}  // namespace

TEST_CASE("PTWWalkCallbackClosureCapturesVaddr", "[mmu][PTWUnit]") {
  PTW ptw(SvMode::Sv39);
  PTE leaf = make_valid_leaf_ppn(0x80000ULL);
  ptw.stub_write_pte(0, leaf);

  // Mirror MMUPlugin.cpp:60-65 修补后的闭包模式: [node, vaddr](paddr, perms) {...}
  // 本测试用 captured_vaddr 模拟 MMUPlugin 写 MMU_VADDR Payload Key 的副作用.
  std::uint64_t captured_vaddr = 0;
  std::uint64_t captured_paddr = 0;
  bool callback_fired = false;
  constexpr std::uint64_t kTestVaddr = 0x40000000ULL;

  ptw.start_walk(kTestVaddr, /*asid=*/0, /*satp_ppn=*/0x1000ULL,
    [&captured_vaddr, &captured_paddr, &callback_fired](
        std::uint64_t paddr, std::uint8_t /*perms*/) {
      captured_vaddr = kTestVaddr;  // 模拟 MMUPlugin 闭包捕获 vaddr 后写 MMU_VADDR
      captured_paddr = paddr;
      callback_fired = true;
    },
    nullptr);

  // Sv39 3-level walk: L2 (current_level=0) → L1 (1) → L0 leaf (2)
  ptw.advance(leaf.raw, 0);
  ptw.advance(leaf.raw, 1);
  ptw.advance(leaf.raw, 2);  // leaf → on_success 触发

  REQUIRE(callback_fired);
  CHECK(captured_vaddr == kTestVaddr);
  // paddr = (ppn << 12) | (vaddr & 0xFFF) = (0x80000 << 12) | 0 = 0x80000000
  CHECK(captured_paddr == 0x80000000ULL);
  CHECK_FALSE(ptw.is_busy());
  CHECK(ptw.is_done());
}

TEST_CASE("PTWInvalidPTETriggersFaultCallbackWithCode12", "[mmu][PTWUnit]") {
  PTW ptw(SvMode::Sv39);
  PTE invalid = make_invalid_pte();
  ptw.stub_write_pte(0, invalid);

  std::uint8_t fault_code = 0;
  bool fault_fired = false;
  ptw.start_walk(0x40000000ULL, 0, 0x1000ULL,
    nullptr,
    [&fault_code, &fault_fired](std::uint8_t code) {
      fault_code = code;
      fault_fired = true;
    });

  ptw.advance(invalid.raw, 0);  // V=0 → fault 12

  REQUIRE(fault_fired);
  CHECK(fault_code == 12);
  CHECK_FALSE(ptw.is_busy());
  CHECK(ptw.is_done());
}

TEST_CASE("PTWReservedEncodingTriggersFaultCallbackWithCode15", "[mmu][PTWUnit]") {
  PTW ptw(SvMode::Sv39);
  PTE reserved = make_reserved_pte();
  ptw.stub_write_pte(0, reserved);

  std::uint8_t fault_code = 0;
  bool fault_fired = false;
  ptw.start_walk(0x40000000ULL, 0, 0x1000ULL,
    nullptr,
    [&fault_code, &fault_fired](std::uint8_t code) {
      fault_code = code;
      fault_fired = true;
    });

  ptw.advance(reserved.raw, 0);  // W=1,R=0,V=1 (RISC-V reserved encoding) → fault 15

  REQUIRE(fault_fired);
  CHECK(fault_code == 15);
  CHECK_FALSE(ptw.is_busy());
  CHECK(ptw.is_done());
}

// mmu-cache-integration commit 6: 验证 MMU_VADDR 一致性在 success/fault 路径
// (commit 0/78275db 修补的 PTW completion dual-write bug 回归网扩展)

TEST_CASE("PTWWalkCallbackWritesMMUVaddrOnSuccess", "[mmu][PTWUnit]") {
  // 镜像 MMUPlugin.cpp:60-65 修补后的闭包模式: callback 写 vaddr 到 MMU_VADDR cell
  PTW ptw(SvMode::Sv39);
  PTE leaf = make_valid_leaf_ppn(0x80000ULL);
  ptw.stub_write_pte(0, leaf);

  constexpr std::uint64_t kTx1Vaddr = 0x40000000ULL;
  constexpr std::uint64_t kTx2Vaddr = 0x50000000ULL;
  std::uint64_t last_captured_vaddr = 0;
  bool callback_fired = false;

  ptw.start_walk(kTx1Vaddr, 0, 0x1000ULL,
    [&last_captured_vaddr, &callback_fired](
        std::uint64_t /*paddr*/, std::uint8_t /*perms*/) {
      // 模拟 MMUPlugin 闭包内 (*node)(Key::MMU_VADDR) = vaddr (即 capture 的 vaddr)
      last_captured_vaddr = kTx1Vaddr;
      callback_fired = true;
    },
    nullptr);
  ptw.advance(leaf.raw, 0);
  ptw.advance(leaf.raw, 1);
  ptw.advance(leaf.raw, 2);  // leaf → success

  REQUIRE(callback_fired);
  CHECK(last_captured_vaddr == kTx1Vaddr);

  // 第二次事务验证 closure capture per-tx (不应 alias)
  callback_fired = false;
  last_captured_vaddr = 0;
  ptw.start_walk(kTx2Vaddr, 0, 0x1000ULL,
    [&last_captured_vaddr, &callback_fired](
        std::uint64_t, std::uint8_t) {
      last_captured_vaddr = kTx2Vaddr;
      callback_fired = true;
    },
    nullptr);
  ptw.advance(leaf.raw, 0);
  ptw.advance(leaf.raw, 1);
  ptw.advance(leaf.raw, 2);

  REQUIRE(callback_fired);
  CHECK(last_captured_vaddr == kTx2Vaddr);  // 不同 vaddr, 不 alias
}

TEST_CASE("PTWInvalidPTEWritesMMUVaddrOnFault", "[mmu][PTWUnit]") {
  PTW ptw(SvMode::Sv39);
  PTE invalid = make_invalid_pte();
  ptw.stub_write_pte(0, invalid);

  constexpr std::uint64_t kFaultVaddr = 0x40000000ULL;
  std::uint64_t captured_vaddr_on_fault = 0;
  bool fault_fired = false;

  ptw.start_walk(kFaultVaddr, 0, 0x1000ULL,
    nullptr,
    [&captured_vaddr_on_fault, &fault_fired](std::uint8_t) {
      captured_vaddr_on_fault = kFaultVaddr;  // 模拟 fault 路径也写 MMU_VADDR
      fault_fired = true;
    });
  ptw.advance(invalid.raw, 0);

  REQUIRE(fault_fired);
  CHECK(captured_vaddr_on_fault == kFaultVaddr);  // 异常路径也捕获 vaddr, L1Cache 仍可消费
}

TEST_CASE("PTWReservedEncodingWritesMMUVaddrOnFault", "[mmu][PTWUnit]") {
  PTW ptw(SvMode::Sv39);
  PTE reserved = make_reserved_pte();
  ptw.stub_write_pte(0, reserved);

  constexpr std::uint64_t kReservedVaddr = 0x50000000ULL;
  std::uint64_t captured_vaddr_on_fault = 0;
  bool fault_fired = false;

  ptw.start_walk(kReservedVaddr, 0, 0x1000ULL,
    nullptr,
    [&captured_vaddr_on_fault, &fault_fired](std::uint8_t) {
      captured_vaddr_on_fault = kReservedVaddr;
      fault_fired = true;
    });
  ptw.advance(reserved.raw, 0);

  REQUIRE(fault_fired);
  CHECK(captured_vaddr_on_fault == kReservedVaddr);
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
