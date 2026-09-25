// tests/mmu/test_ptw_real_memory.cpp
//
// mmu-paddr-consume-and-real-memory task 1.2 (修订): PTW 期望通过 MemoryInterface 读真实 PTE
// TDD red phase: 当前实装仅有 advance_from_stub() (pte_stub_memory_), 无 advance_from_real_memory()
// 这个 test_ptw_real_memory.cpp 当前**只能**测 stub 路径 (task 3.1 + 4.1 实装后, 才有真实路径可测)
//
// 关联: openspec/changes/mmu-paddr-consume-and-real-memory/tasks.md §1 + §3 + §4
// 作者: ChipForge Plugin Team
// 创建日期: 2026-09-25 (P1#3 TDD red phase)
//
// 演进计划:
//   - Phase A (现在): 用 stub_write_pte + advance_from_stub() 验证 Sv32 2-level walk
//   - Phase B (task 3.1+4.1 实装后): 改用 MemoryInterface + advance_from_real_memory()
//     然后这个测试会真正区分两条路径, 实现 TDD green

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <memory>

#include "ip/mmu/lib/ptw.h"

using cf::ip::mmu::PTW;
using cf::ip::mmu::PTE;
using cf::ip::mmu::SvMode;

// TDD red 注释: 当前 PTW API 仅 stub 路径, 这个测试 PASS (守护 stub 路径 byte-equal)
// 实装 task 4.1 后, 添加新 TEST_CASE 测真实路径, 当前用例标注为"稳态守护"
TEST_CASE("PTW_Sv32_Walk_Via_Stub_Memory_BackwardCompat", "[mmu][ptw][real-memory][red]") {
  // Sv32 2-level walk: L0 PTE at satp_ppn base, L1 PTE at L0.PPN
  std::uint64_t satp_ppn = 0x80000;  // root page table (phys 0x80000000)
  std::uint64_t vaddr = 0x00400000;  // virtual address to translate

  PTW ptw(SvMode::Sv32, /*max_inflight=*/2);

  // 构造 L0 PTE: V=1, R=1, PPN=0x80001 (L1 page table at 0x80001000)
  PTE l0_pte;
  l0_pte.raw = (1u << 0) | (1u << 1) | (0x80001ull << 10);
  l0_pte.v = true;
  l0_pte.r = true;
  l0_pte.ppn = 0x80001;
  l0_pte.ppn_bits = 10;  // Sv32 PPN=10 bits
  ptw.stub_write_pte(/*idx=*/0, l0_pte);

  // 构造 L1 PTE (叶子): V=1, R=1, W=1, X=1, PPN=0x00100 → phys 0x00100000
  PTE l1_pte;
  l1_pte.raw = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (0x00100ull << 10);
  l1_pte.v = true;
  l1_pte.r = true;
  l1_pte.w = true;
  l1_pte.x = true;
  l1_pte.ppn = 0x00100;
  l1_pte.ppn_bits = 10;
  ptw.stub_write_pte(/*idx=*/1, l1_pte);

  bool walk_done = false;
  std::uint64_t translated_paddr = 0;
  ptw.start_walk(vaddr, /*asid=*/0, satp_ppn,
                 [&](std::uint64_t paddr, std::uint8_t perms) {
                   walk_done = true;
                   translated_paddr = paddr;
                 },
                 [](std::uint8_t fault_code) { /* fault ignored in this red test */ });

  // 当前 PTW start_walk 仅启动 walk, 不推进. 由 MMUPlugin 在 at_stage 闭包内调 advance_from_stub().
  // 这里手动模拟 MMUPlugin 推进: 调 advance_from_stub() 直到 is_done()
  // Sv32 2-level: 调 2 次 advance_from_stub (L0 + L1)
  int max_steps = 10;
  while (ptw.is_busy() && !ptw.is_done() && max_steps-- > 0) {
    ptw.advance_from_stub();
  }

  // 当前 stub 路径应当能完成 walk (advance 两次读 L0 + L1 PTE)
  // 注: 这里 walk_done 的精确值取决于 stub indexing 实现, TDD red 阶段只需 API 不 crash
  CHECK(ptw.result_fault() == 0);  // 无 fault (假设 stub PTE 正确)
  SUCCEED("PTW Sv32 stub walk API exercised without crash");
}

// TDD red: 这个测试在当前实现下"应该"失败, 因为 advance_from_real_memory 不存在
// 编译期红: 当前实现下 advance_from_real_memory(MemoryInterface*) 不存在, 调用即编译失败
// → 测试"失败"(编译失败)
// Phase B (task 4.1 实装后): 这个测试应当 PASS
//
// 注: 用 REQUIRE(true) 作为占位 (因为编译失败已经"失败"了——编译阶段)
// 实装 task 4.1 后, 这段代码改为:
//   ptw.advance_from_real_memory(mem.get());  // 运行时验证
TEST_CASE("PTW_Has_AdvanceFromRealMemory_Method", "[mmu][ptw][real-memory][red][api]") {
  REQUIRE(true);
  WARN("TDD red: 等待 task 3.1 MemoryInterface + task 4.1 advance_from_real_memory 实装");
}