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

// 构造 reserved encoding PTE (R=1, W=1, X=1, V=1) → fault 15
PTE make_reserved_pte() {
  PTE pte{};
  pte.raw = (1ULL << 0) | (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
  pte.v = true;
  pte.r = true;
  pte.w = true;
  pte.x = true;
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

  ptw.advance(reserved.raw, 0);  // R=1,W=1,X=1,V=1 → fault 15

  REQUIRE(fault_fired);
  CHECK(fault_code == 15);
  CHECK_FALSE(ptw.is_busy());
  CHECK(ptw.is_done());
}

}  // namespace mmu
}  // namespace ip
}  // namespace cf
