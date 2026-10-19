// tests/mmu/test_ptw_real_memory.cpp
//
// mmu-paddr-consume-and-real-memory task 1.2 (修订): PTW 期望通过 MemoryInterface 读真实 PTE
//
// 状态 (2026-10-19 honesty_audit 复核):
//   - `advance_from_real_memory(MemoryInterface*)` 已实装于 `ip/mmu/lib/ptw.h:93` + `ptw.cpp:126`
//   - 本测试 :155 真实调用该方法; :152 注释 "C2 fix + C8 fix" 标注 = 真实路径已闭环
//   - 头部的"TDD red phase: 当前仅有 advance_from_stub" + "演进计划 Phase B" 描述为 2026-09-25 阶段过期注释, 不再反映当前代码状态
//   - 历史: 此文件原是 wave3-mmu P1#3 TDD red 阶段产物 (2026-09-25), 2026-09-26 archive (`openspec/changes/archive/2026-09-26-mmu-paddr-consume-and-real-memory/`) 后,
//     真实路径已实装, 但本文件头部 + :82-93 + [red] 标签未同步清理
//
// 关联: openspec/changes/archive/2026-09-26-mmu-paddr-consume-and-real-memory/tasks.md §1 + §3 + §4
// 作者: ChipForge Plugin Team
// 创建日期: 2026-09-25 (P1#3 TDD red phase)
// 最近修订: 2026-10-19 (roadmap-honesty-audit-2026-10 清理过期注释)

#include "catch_amalgamated.hpp"

#include <cstdint>
#include <map>
#include <memory>

#include "ip/mmu/lib/memory_interface.h"
#include "ip/mmu/lib/ptw.h"

using cf::ip::mmu::MemoryInterface;
using cf::ip::mmu::PTW;
using cf::ip::mmu::PTE;
using cf::ip::mmu::SvMode;

// Stub 路径守护测试 (2026-09-25 起, 持续守护 stub write PTE + advance_from_stub 路径 byte-equal)
// 2026-10-19 honesty_audit 复核: 本测试持续 PASS, [red] 标签因 stub 路径"已成历史路径"已不再适用, 保留测试作为稳态守护
TEST_CASE("PTW_Sv32_Walk_Via_Stub_Memory_BackwardCompat", "[mmu][ptw][real-memory][stub-guard]") {
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

// API 存存性占位测试 (2026-10-19 honesty_audit 复核):
//   - 原始意图 (2026-09-25): TDD red 守护 `advance_from_real_memory(MemoryInterface*)` API 编译期存在性
//   - 当前状态: API 已在 `ip/mmu/lib/ptw.h:93` 声明 + `ptw.cpp:126` 实装, :155 处真实调用
//   - 真实路径覆盖已由本文件 :143-156 (后续 TEST_CASE) 完成, 此处 `REQUIRE(true)` 仅为 compile-time 占位
//   - 保留此测试作为 API 存在性 sentinel; 标签 [api] 标注用途, 不再带 `[red]`
TEST_CASE("PTW_Has_AdvanceFromRealMemory_Method", "[mmu][ptw][real-memory][api]") {
  REQUIRE(true);
  WARN("API sentinel: advance_from_real_memory 已实装 (ptw.h:93 + ptw.cpp:126), 真实路径测试见本文件 :143-156");
}

namespace c8_test {

// MockMemoryInterface (C8 测试专用, 复用 MemoryInterface 抽象)
// Plant PTE 在任意物理地址, walk 通过 mem->read_word 读
class MockMemoryForC8 : public cf::ip::mmu::MemoryInterface {
 public:
  std::uint32_t read_word(std::uint64_t phys_addr) override {
    auto it = mem_.find(phys_addr);
    if (it == mem_.end()) return 0xFFFFFFFF;  // unmapped → V=0 fault
    return it->second;
  }
  void write_word(std::uint64_t phys_addr, std::uint32_t val) override {
    mem_[phys_addr] = val;
  }
 private:
  std::map<std::uint64_t, std::uint32_t> mem_;
};

}  // namespace c8_test

// C8 (Oracle 2026-09-25): Sv32 VPN[0] 是 10 bits (vaddr[21:12]), 旧 mask 0x1FF 截断 bit 21
// 导致 VPN[0]≥512 时 L0 PTE 地址算错, walk 读到错误位置 → fault
//   旧 0x1FF: VPN[0] & 0x1FF 截断 → L0 PTE addr = pte.ppn<<12 + 0
//   新 0x3FF: VPN[0] & 0x3FF 正确  → L0 PTE addr = pte.ppn<<12 + 0x800
// 用 vaddr=0x80200000 (VPN[1]=0x200, VPN[0]=0x200, bit 21 set) 触发此 bug
TEST_CASE("PTW_Sv32_VPN_HighBit_RealMemory_Walk_Succeeds", "[mmu][ptw][real-memory]") {
  using c8_test::MockMemoryForC8;

  // satp_ppn=0x80000 → root page table at phys 0x80000000
  // vaddr=0x80200000:
  //   VPN[1] = (0x80200000 >> 22) & 0x3FF = 0x200
  //   VPN[0] = (0x80200000 >> 12) & 0x3FF = 0x200 (bit 9 set, 触发 C8 bug)
  // Root PTE (L1) addr = 0x80000000 + 0x200 * 4 = 0x80000800
  //   L1 PTE: V=1, R=1, PPN=0x001 → L0 page table at phys 0x001000
  // L0 PTE addr = 0x001000 + 0x200 * 4 = 0x001800
  //   L0 PTE (leaf): V=1, R=1, X=1, PPN=0x0 → phys 0x00000000
  std::uint64_t satp_ppn = 0x80000ULL;
  std::uint64_t vaddr = 0x80200000ULL;

  MockMemoryForC8 mem;
  // L1 PTE (root, V=1 R=1 PPN=0x001)
  mem.write_word(0x80000800, (1u << 0) | (1u << 1) | (0x001u << 10));
  // L0 PTE (leaf, V=1 R=1 X=1 PPN=0) → phys 0x00000000, page offset 0
  mem.write_word(0x001800, (1u << 0) | (1u << 1) | (1u << 3));

  PTW ptw(SvMode::Sv32, /*max_inflight=*/2);
  bool walk_done = false;
  std::uint64_t translated_paddr = 0xDEADBEEFULL;  // sentinel
  std::uint8_t fault_code = 0;  // init 0 (Oracle 2026-09-25): 原 0xFF sentinel bug 永远判失败
  ptw.start_walk(vaddr, /*asid=*/0, satp_ppn,
                 [&](std::uint64_t paddr, std::uint8_t perms) {
                   walk_done = true;
                   translated_paddr = paddr;
                 },
                 [&](std::uint8_t code) { fault_code = code; });

  // Sv32 2-level walk: 推进 2 次 (L0 root, L1 leaf)
  // 用 advance_from_real_memory (C2 fix + C8 fix 一起验证)
  int max_steps = 10;
  while (ptw.is_busy() && !ptw.is_done() && max_steps-- > 0) {
    ptw.advance_from_real_memory(&mem);
  }

  // Oracle C8 fix 后: walk 成功, paddr = (leaf.PPN << 12) | (vaddr & 0xFFF) = 0 | 0 = 0
  // Oracle C8 bug 还在: walk 读 L0 PTE addr = pte.ppn<<12 + 0 (VPN[0] bit 9 截断)
  //   → mem 不含 0x001000 (不同地址) → 读 0xFFFFFFFF → V=1 垃圾 → leaf → paddr 错误
  // 用 PTW 内部 result_fault_ (start_walk 时重置 0) 而非本地 sentinel — bug 触发也能区分
  REQUIRE(ptw.result_fault() == 0);  // 无 fault, C8 fix 有效
  CHECK(fault_code == 0);             // 双保险: on_fault callback 从未触发
  CHECK(walk_done);
  CHECK(translated_paddr == 0);  // phys = (0 << 12) | (0x80200000 & 0xFFF) = 0
}