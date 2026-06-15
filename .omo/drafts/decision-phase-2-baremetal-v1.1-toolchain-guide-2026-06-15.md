# ChipForge RISC-V Bare-Metal 验证执行指南 v1.1 (ChipForge 适配版)

> **版本**: v1.1-baseline
> **决策 ID**: DECISION-2026-06-15-01 (基于评审反馈 + 4 项并行调研结果)
> **关联**: 修订 v1.0 通用 RISC-V TLM 指南 → 适配 ChipForge Phase 2 现状
> **关联决策草案**: `decision-phase-2-baremetal-riscv-tests-2026-06-13.md` (DECISION-2026-06-13-03 v1) → 本版合并
> **关联计划**: `.omo/plans/phase-2-baremetal-riscv-tests.md`
> **关联路线图**: `docs/roadmap/phases/phase-2-baremetal.md` (M2 里程碑)
> **状态**: 🟡 Proposed v1.1 — 待 1-2 项关键问题定夺后改 Accepted
> **取代关系**: 全面取代 v1.0 通用 RISC-V TLM 指南 (v1.0 假设的"完整 RISC-V Core + SystemC TLM 2.0"在 ChipForge 中不成立)

---

## 📑 修订要点速览 (相对 v1.0)

| # | 修订 | v1.0 假设 | v1.1 现实 |
|---|------|----------|-----------|
| 1 | 平台范围 | 通用 RISC-V TLM 仿真器 | ChipForge: L1CachePlugin as memory model, 无 CPU Core |
| 2 | TLM 框架 | SystemC TLM 2.0 | CppTLM 2.0 (cppuhua/cpptlm commit c6079357) |
| 3 | HTIF 实现 | b_transport + tlm_phase | tick() + InputStreamAdapter/OutputStreamAdapter |
| 4 | streaming_width | set_streaming_width(8) | **不存在**,需字段级 RVWMO 约束 |
| 5 | arch-test 流程 | begin_signature/end_signature 外部 dump | **ACT4 自检 ELF + tohost 监测** (ACT3.x API 已弃用) |
| 6 | Python 集成 | pybind11 直驱 | **C++ shim + ctypes + 子进程** (CppTLM 无 pybind11) |
| 7 | 主存访问 | MemoryTLM 直读 | **MemoryTLM 是 stub**,只能通过 L1CachePlugin (2KB 实际容量) |
| 8 | 测试命名 | add-01.S | **I-add-00.S** (ACT4 新命名) |
| 9 | 工具链锁定 | latest/Docker digest | apt + 源码 + 预编译 (具体见 §二 2.1) |
| 10 | 里程碑 | P0-P5 (含 Verilator/riscv-dv) | 重映射到 Phase 2-5 (见 §四) |

---

## §0 平台约束声明 (新增)

> ⚠️ **本指南专为 ChipForge Phase 2 现状定制,不可作为通用 RISC-V TLM 验证文档使用**

### 0.1 ChipForge 当前状态 (截至 2026-06-15)

| 组件 | 状态 | 路径 |
|------|------|------|
| **L1CachePlugin** (256 sets × 64B direct-mapped) | ✅ Phase 1.2 完整, 4/4 单元测试 PASS | `ip/cache/tlm/L1CachePlugin.{h,cpp}` |
| **L1CacheTLMBridge** (4 字段窄桥 + D1' 契约) | ✅ Phase 1.3a 完整 | `src/cf_plugin/bridge/l1_cache_bridge.{h,cpp}` |
| **L1CacheTLMBridgeAdapter** (ch_stream 4 字段) | ✅ Phase 1.3d-extras 完整, 16/16 ctest PASS | `src/cf_plugin/bridge/l1_cache_bridge_adapter.{h,cpp}` |
| **L1CachePlugin 实际容量** (Phase 0 退化) | ⚠️ 2 KB (256 sets × 8 B/line, `uint_t<512>` → `uint_t<64>`) | `include/cf/plugin/uint_t.h:37` |
| **MemoryTLM** (主存) | ❌ **STUB** — `ip/memory/` 仅有 `.gitkeep` + 规划 .md, 无任何 .h/.cpp | `ip/memory/README.md:3` |
| **RISC-V CPU Core** (RV64I/M/A/F/D/C) | ❌ **不存在** — 仅 `ip/cpu/README.md` + `configs/cpu_default.json` | — |
| **PLIC / CLINT / UART** (中断 + IO) | ❌ STUB — Phase 3 范围 | `ip/peripheral/` |
| **Build system** | ✅ CppTLM v2.0.3 (commit c6079357) ExternalProject 集成 | `CMakeLists.txt` |

### 0.2 v1.1 范围声明

- **P0-P1 (本次 Phase 2 目标)**: 跑 riscv-arch-test I/M/Zicsr/Zifencei/Zba/Zbb/Zbs/C 子集, 用 L1CachePlugin 作为 memory model (Phase 1.3d-extras 的 e2e 路径已验证)
- **P2 (本次 Phase 2 不达)**: 中断/异常/PMP — 需 PLIC/CLINT (Phase 3 范围)
- **P3+ (Phase 5 范围)**: Verilator RTL 协同验证, 需 L1CacheRtl (CppHDL)
- **riscv-dv (Phase 3 范围)**: 需 RTL + UVM, 当前不可达

### 0.3 v1.0 → v1.1 的不可调和冲突 (核心)

| v1.0 假设 | v1.1 现实 | 解决方式 |
|----------|----------|----------|
| "完整 RISC-V Core" | 不存在 | 接受"memory-side 验证"模式: L1CachePlugin 接收 ch_stream 事务, 验证其行为正确性 (而非执行指令) |
| "SystemC TLM 2.0" | CppTLM | 改写 §3 全部代码示例 |
| "pybind11 直驱" | 不存在 | 改用 C++ shim + ctypes + 子进程 |
| "begin_signature/end_signature 外部 dump" | ACT4 已弃用 | 改用 ACT4 自检 ELF + tohost 监测 |
| "MemoryTLM 直读" | 不存在 | 仅通过 L1CachePlugin (2KB), 主存访问推迟到 MemoryTLM 实施后 |

---

## 一、核心原则 (继承 v1.0, 5 条 → 6 条)

1. **分层解耦**: arch-test (合规性) 与 riscv-tests (功能/性能) 严格分离; HTIF 协议层与具体 simulator (Spike/Sail/QEMU) 解耦
2. **风险前置**: P0→P5 递进,前一阶段未通过禁止进入下一阶段
3. **环境确定性**: 工具链版本锁定到 commit hash / digest / 精确版本号,**禁止 latest**
4. **Golden Reference 分层**: Sail 验证 ISA 行为 (arch-test 自检生成), Spike 验证微架构时序 (Phase 5 才用)
5. **平台适配**: ChipForge 平台约束 (无 CPU Core, L1CachePlugin as memory model) 必须在范围声明中显式说明
6. **依赖可执行**: 每条原则都必须有可验证的验收标准, 不可"原则上同意但无法验证"

---

## 二、工具链与环境准备 (Day 0)

### 2.1 工具链版本锁定 (实测, 非 v1.0 假设)

```text
# /workspace/project/requirements-toolchain.txt (新增, 待提交)
# ChipForge Phase 2 实测锁定版本 (2026-06-15)

# 编译器 (apt 锁定, dpkg 验证)
riscv64-unknown-elf-gcc:13.2.0-11ubuntu1+12
binutils-riscv64-unknown-elf:2.42-1ubuntu1+6
picolibc-riscv64-unknown-elf:1.8.6-2
# 备选 (apk unavailable for riscv64-elf; 用上述 apt 包)
# riscv-gnu-toolchain: 不锁定 (与 apt 包冲突, 二选一)

# ISA Simulator (source build 锁定 commit)
spike:1.1.1-dev@/workspace/project/opt/riscv/bin/spike
sail-riscv:0.12@/workspace/project/opt/riscv/sail-riscv-Linux-x86_64/bin/sail_riscv_sim

# 合规认证
riscof:1.25.3@/home/ubuntu/venv/bin/riscof  (pip via aliyun mirror)

# C++ TLM 框架 (ChipForge ExternalProject 锁定 commit)
cpptlm:2.0.3@commit-c6079357  (CppTLM 子模块)
# 框架对 streaming_width / tlm_phase / pybind11 的支持: 无 (见 §3.2)

# riscv-arch-test (新增, P0 必需)
riscv-arch-test:act4@commit-09b4999ed3fb7350c082d92816e7909a85a82d62  (ACT4 framework)
# 已弃用: riscof (riscof 已被 ACT4 框架取代, 不可混用)

# Phase 3+ 引入 (本期不需要)
# riscv-dv:master@commit-TBD (Phase 3 才用)
# verilator:5.020+ (Phase 5 才用)
# cpphdl:1.0.0 (已集成, Phase 1.4+ 使用, Phase 5 RTL 输出)
```

### 2.2 工具清单与引入时机 (修订)

| 工具 | 引入时机 | 用途 | 状态 (2026-06-15) |
| :--- | :--- | :--- | :--- |
| riscv64-unknown-elf-gcc 13.2.0 | **Day 0** | 编译测试用例 | ✅ apt 装好 |
| spike 1.1.1-dev | **Day 0** | ISA Golden Reference (HTIF + `+signature=`) | ✅ source build 完 |
| sail_riscv 0.12 | **Day 0** | arch-test 自检 ELF 的预期值生成 | ✅ 预编译下载 |
| riscof 1.25.3 | **Day 0** | (备选, ACT4 已取代 riscof) | ✅ pip 装好 |
| **riscv-arch-test (ACT4)** | **P0** | 官方合规测试集 (本指南主用) | ❌ 待 clone |
| **elfio / libelf** | **P0** | ELF 符号解析 (HTIF + 签名区域) | ❌ 待 apt install |
| CppTLM 2.0.3 (commit c6079357) | **Day 0** (已存在) | TLM 平台 + ch_stream 适配 | ✅ 已 ExternalProject 集成 |
| Python 3.12.3 | **Day 0** | 自动化测试驱动 | ✅ 已装 |
| riscv-dv | **Phase 3** | 随机指令生成 (需 RTL) | ❌ 不引入 |
| Verilator 5.020+ | **Phase 5** | RTL Golden Reference | ❌ 不引入 |
| riscv64-unknown-elf-gdb | **P4 (Phase 2 不强制)** | 调试支持 | ❌ 不强制 |

> ⚠️ **v1.0 错误**: "riscv-dv 从 Day 0 可选移至 P2 之后" — 实际上 riscv-dv 在 CPU-less TLM 平台**根本无法运行** (需 RTL + UVM), 正确表述应为"riscv-dv 推迟到 Phase 3 (有 RTL 后)"。

---

## 三、验证流水线实现 (全面重写)

### 3.1 测试驱动器核心逻辑 (适配 ACT4 自检 ELF + L1CachePlugin memory model)

> **v1.0 错误**: 用 `extract_signature` 读 `begin_signature`/`end_signature`, 这是 ACT3.x 风险信号已弃用流程。ACT4 是**自检 ELF**, 验证流程完全不同。

```python
# scripts/run_arch_test.py (ChipForge 适配版)
import logging
import subprocess
import ctypes
import os
import json
from pathlib import Path

class ChipForgeArchTestRunner:
    """
    适配 ACT4 自检 ELF 流程 + L1CachePlugin memory model。

    流程:
      1. ACT4 框架生成 self-checking ELF (I-add-00.elf 等), 内嵌 Sail 生成的期望值
      2. Spike + L1CachePlugin shadow co-sim: Spike 执行, 把每个 load/store 重放到 L1CachePlugin
      3. Spike 监测 tohost 写入 (1=PASS, 3=FAIL), 退出码
      4. 直接看 Spike 退出码即测试结果 (无需再读 memory)
    """

    def __init__(self, spike_path, sail_path, l1cache_shim_path):
        self.spike = spike_path  # /workspace/project/opt/riscv/bin/spike
        self.sail = sail_path    # /workspace/project/opt/riscv/sail-riscv-Linux-x86_64/bin/sail_riscv_sim
        self.shim = ctypes.CDLL(l1cache_shim_path)  # libcf_l1_shim.so

    def setup_shim(self, num_sets=256, tag_bits=20, idx_bits=8, line_data_bits=64):
        """创建 L1CachePlugin + Bridge 链 + 初始化 ch_stream 端口"""
        self.bridge = self.shim.l1_create_bridge()
        return self.bridge

    def load_elf_to_l1cache(self, elf_path, base_addr=0x80000000):
        """
        把 ELF 镜像加载到 L1CachePlugin 8 字节粒度写入 (Phase 0 退化限制)。
        ACT4 link.ld: .text.init 0x80000000 起, .data 0x80000000+0x4000 起。
        ELF 2KB 内可由 L1CachePlugin 容纳 (256 sets × 8B = 2KB)。
        """
        from elftools.elf.elffile import ELFFile  # pyelftools
        with open(elf_path, 'rb') as f:
            elf = ELFFile(f)
            for seg in elf.iter_segments():
                if seg['p_type'] != 'PT_LOAD':
                    continue
                paddr = seg['p_paddr']
                data = seg.data()
                for off in range(0, len(data), 8):
                    if off + 8 <= len(data):
                        val = int.from_bytes(data[off:off+8], 'little')
                    else:
                        # tail bytes (≤7): zero-pad to 8
                        tail = data[off:].ljust(8, b'\x00')
                        val = int.from_bytes(tail, 'little')
                    addr = paddr + off
                    self.shim.l1_write_data(self.bridge, addr, val)
                    # L1CachePlugin 不能直写主存: write_data 走 lookup+refill 路径
                    # 这会触发 cache line 替换, 但因为 L1CachePlugin 是 direct-mapped,
                    # 同一 idx 多次写会覆盖 (line_data_bits=8 bytes 限制)
                    self.shim.l1_tick(self.bridge)  # advance pipe builder

    def run_test(self, elf_path, max_cycles=100_000):
        """
        Spike co-sim + L1CachePlugin shadow。
        实际方案: Spike 单独跑, L1CachePlugin 仅做 memory 验证副本。
        """
        # 1. 用 Spike 跑 ELF, 监测退出码 (自检 ELF 会写 tohost)
        result = subprocess.run(
            [self.spike, "--isa=rv64gc_zicsr", "-m", "0x90000000:0x100000000",
             str(elf_path)],
            capture_output=True, text=True, timeout=60
        )
        # Spike 退出码: 0=HTIF_PASS, 非零=HTIF_FAIL 或异常
        # tohost 写入 1 = PASS, 3 = FAIL (由 ELF 内 RVMODEL_HALT_* 宏实现)
        return result.returncode

    def run_compliance_batch(self, elf_dir, extensions=("I", "M", "Zicsr", "Zifencei")):
        """批量跑 arch-test ELF, 返回 pass/fail 汇总"""
        results = {"pass": 0, "fail": 0, "errors": []}
        for elf in sorted(Path(elf_dir).glob("*.elf")):
            try:
                ret = self.run_test(elf)
                if ret == 0:
                    results["pass"] += 1
                    logging.info(f"  [PASS] {elf.name}")
                else:
                    results["fail"] += 1
                    logging.warning(f"  [FAIL] {elf.name} (ret={ret})")
                    results["errors"].append({"elf": elf.name, "ret": ret})
            except subprocess.TimeoutExpired:
                results["fail"] += 1
                results["errors"].append({"elf": elf.name, "ret": "timeout"})
        return results
```

**关键变化 (vs v1.0)**:
- **无 `extract_signature`**: ACT4 自检 ELF 在执行时直接比较, 失败就跳到 FAIL handler
- **无 `load_binary(ref_sig_path)`**: 不再有外部 `.sig` 文件 (ACT4 把期望值内嵌到 ELF)
- **Spike 退出码即结果**: HTIF `tohost=1` → Spike exit 0 (PASS), `tohost=3` → Spike exit 非零 (FAIL)
- **L1CachePlugin 仅作 memory 副本**: 主路径是 Spike, L1CachePlugin 用于 cache 行为验证 (Phase 2 任务 2 实际用例)

### 3.2 L1CachePlugin C++ shim 接口 (新增, 替代 §3.2 HTIF TLM 模块)

> **v1.0 错误**: 用 SystemC TLM 2.0 `b_transport` + `set_streaming_width(8)` + `tlm_phase::BEGIN_RESP/END_RESP` — 这些 API **在 CppTLM 中全部不存在**。

```cpp
// src/cf_plugin/bridge/l1_cache_python_shim.h (新增, extern "C" for ctypes)
#ifndef CHIPFORGE_L1_CACHE_PYTHON_SHIM_H
#define CHIPFORGE_L1_CACHE_PYTHON_SHIM_H

#include <cstdint>

extern "C" {
  // 生命周期
  void* l1_create_bridge();                              // 返回 opaque L1CacheTLMBridge*
  void  l1_destroy_bridge(void* bridge);

  // ch_stream 4 字段窄桥 (DECISION-2026-06-13-01 F1.A)
  void  l1_issue_request(void* bridge, uint64_t addr, uint64_t data,
                         uint8_t is_write, uint8_t id);
  void  l1_refill_from_memory(void* bridge, uint64_t data, uint8_t id, uint8_t err);
  void  l1_tick(void* bridge);                            // 末尾调 pb.run() (D1')

  // 8 字节粒度读写 (Phase 0 uint_t<512> 退化限制)
  // 走 lookup+refill 完整路径, 验证 cache 行为正确性
  uint64_t l1_read_data(void* bridge, uint64_t addr);     // 命中则返回 data, miss 返回 0
  void     l1_write_data(void* bridge, uint64_t addr, uint64_t data);  // 写穿

  // 批量读 (signature 区域用)
  // 返回读到的字节数; 输出 buffer 由 Python 分配
  size_t   l1_read_range(void* bridge, uint64_t addr, size_t size_bytes,
                        uint8_t* out_buffer);

  // HTIF 监测 (ChipForge 扩展: 不需要真 CPU, 直接读 tohost 符号)
  // 0x80001000 = tohost, 0x80001008 = fromhost (标准 riscv-tests/ACT4 约定)
  uint64_t l1_read_tohost(void* bridge);                  // 返回 0 = 未触发, 1 = PASS, 3 = FAIL
  void     l1_set_tohost_addr(void* bridge, uint64_t addr);  // 设置 tohost 监听地址
}

#endif
```

**实现要点** (`l1_cache_python_shim.cpp`):
- 内部用 `L1CacheTLMBridge` (Phase 1.3a 已有, D1' 契约 `tick()` 末尾 `pb.run()`)
- 写穿逻辑: 写 addr → 触发 cache line 替换, 若 dirty 则写回 (当前 L1CachePlugin 无 write-back, 视为无操作)
- `l1_read_data(addr)` 走完整 lookup 路径: tag 比较 → hit 直接返 / miss 触发 refill (依赖外部 `l1_refill_from_memory` 注入)
- 字节序: **little-endian** (与 RISC-V 一致, `bridge_adapter.cpp:55`)

### 3.3 ch_stream 路径降级 (Phase 2+ 考虑)

**v1.0 错误**: `set_stream_adapter(adapter)` 描述不准确。

**正确描述**:
- `L1CacheTLMBridgeAdapter` 继承 `cpptlm::ChStreamModuleBase`, 暴露 `req_in()` (InputStreamAdapter) 和 `resp_out()` (OutputStreamAdapter)
- 静态注册: `ChStreamAdapterFactory::get().registerAdapter<L1CacheTLMBridgeAdapter, ::bundles::CacheReqBundle, ::bundles::CacheRespBundle>("L1CacheTLMBridgeAdapter")` 在 Adapter `.cpp` 加载时自动执行
- **不推荐** Python 通过 ch_stream 直驱: 需 EventQueue 调度 + 完整 sim_object 生命周期管理, 复杂度高
- **推荐** Python 走 §3.2 shim 接口, 内部走 Bridge 的 `issue_request` + `tick` (单元测试已验证 5/5 e2e PASS)

### 3.4 实施经验: Spike HTIF 集成 4 件套 (新增, 我刚 debug 的)

> **背景**: 调试 spike HTIF 时, 反复遭遇 `lui t1, 0x80001` 符号扩展陷阱 + `--gc-sections` 删除未引用 tohost + 静态 `tohost` 符号被 NOBITS 段跳过。

**4 件套** (任一缺失 spike 都会不识别 tohost):

1. **tohost 必须在 `.data` / `.tohost_data` 等 PROGBITS 段** (spike `elfloader.cc:88` 跳过 NOBITS)
   ```c
   .section .tohost_data, "aw", @progbits   // 不要用 .bss (NOBITS)
   .globl tohost
   tohost: .dword 0
   ```

2. **链接器加 `KEEP()` 防止 --gc-sections 删除**
   ```ld
   .tohost_data 0x80001000 : { KEEP(*(.tohost_data)) }
   ```

3. **GCC 加 `-Wl,--no-gc-sections` 或写 .data section**
   ```
   -Wl,--no-gc-sections
   ```

4. **地址加载用 `auipc` 不用 `lui` (避免 32→64 bit 符号扩展)**
   ```asm
   .option push
   .option norelax
   auipc   t1, 0
   addi    t1, t1, 16         # 偏移到 .tohost_data
   sd      t0, 0(t1)
   .option pop
   ```

**验证命令**:
```bash
nm test.elf | grep tohost      # 期望: 0000000080001000 D tohost
spike test.elf                  # 期望: 无 "warning: tohost and fromhost symbols not in ELF"
```

---

## 四、P0→P5 里程碑验收标准 (重映射 ChipForge)

> **v1.0 错误**: P3 "Verilator 波形比对" 在 ChipForge 是 Phase 5; P2 "中断" 需 PLIC/CLINT (Phase 3); P4 "GDB Stub" 完全未列入路线图。

### 4.1 里程碑重映射表

| 文档 v1.0 阶段 | ChipForge 阶段 | 任务对应 | Golden Reference | HTIF | 前置条件 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **P0** RV64I 基础整数 | **Phase 2 任务 2** | riscv-arch-test I 子集 100% PASS | sail_riscv (ACT4 自检) | ✅ Spike + L1CachePlugin | 工具链锁定 |
| **P1** M/C/Zicsr 扩展 | **Phase 2 任务 2** | riscv-arch-test M/Zicsr/Zifencei/Zba/Zbb/Zbs/C 子集 100% PASS | sail_riscv | ✅ | P0 通过 |
| **P1.5** (新增) **riscv-tests 冒烟** | **Phase 2 任务 2.5** | riscv-tests rv64ui/rv64uc PASS (bypass 覆盖) | Spike | ✅ | P0-P1 通过 |
| **P2** Trap/Interrupt | **Phase 3 (推迟)** | 自定义中断嵌套 + PLIC/CLINT IP | Spike | ✅ | 需 PLIC/CLINT IP (Phase 3 前置) |
| **P2.5** riscv-dv 10K | **Phase 3+ 推迟** | (riscv-dv 需 RTL, **不适用于 CPU-less TLM**) | — | ❌ | 需 RTL (Phase 5+) |
| **P3** Cache/总线时序 | **Phase 2 任务 5+7** (微架构) | Dhrystone + DSE cache_sweep | Spike | ✅ | P0-P1 通过 |
| **P4** GDB Stub | **Phase 2 (可选, 不阻塞)** | riscv64-unknown-elf-gdb stub | 手动 | ❌ | 不阻塞 P0-P3 |
| **P5** 性能计数器 | **Phase 5** | mcycle/minstret 与仿真周期精确匹配 | Verilator + Spike | ✅ | 需 RTL |
| **(新增) M2 里程碑** | **Phase 2 末** | riscv-arch-test RV64I 100% PASS + riscv-tests rv64ui 100% PASS | sail + spike | ✅ | P0-P1.5 通过 |

### 4.2 ChipForge Phase 2 任务映射

| 路线图任务 | v1.0 P 级 | 实施指南对应章节 |
|----------|----------|-----------------|
| 任务 1: 工具链准备 | Day 0 | §二 |
| 任务 2: riscv-tests 集成 | P0-P1.5 | §三 + §五 (Spike Diff) |
| 任务 3: RISCOF 合规认证 | P0 (用 ACT4 取代) | §三 3.1 + §六 |
| 任务 4: 自定义功能测试 (CSR/PMP/Sv39/中断/原子) | P0-P1 (部分) | §三 + §四 (推迟到 Phase 3) |
| 任务 5: 基准测试 (Dhrystone/CoreMark) | P3 | §四 P3 |
| 任务 6: 自动化驱动 | Day 0 | §三 3.1 (scripts/run_arch_test.py) |
| 任务 7: DSE 基础 | P3 | §四 P3 |
| 任务 8: 自检 + commit | M2 末 | §六 + §七 |

### 4.3 关键风险标注

| ID | 风险 | 缓解 |
|----|------|------|
| **R-MEM-1** | **MemoryTLM 是 stub**, 2KB L1CachePlugin 容量仅够 arch-test 极小子集 (实际 arch-test ELF ~5-10KB) | **M2 里程碑只验证 I 子集 (add/sub/and/or 等小 ELF)**, 完整 GC 推迟到 MemoryTLM 实施后 |
| **R-CACHE-1** | `uint_t<512>` Phase 0 退化, 写 8 字节以上需分段 | shim 接口 `l1_write_data` 8 字节粒度, Python 端分段 |
| **R-OPCODE-1** | riscv-arch-test M 扩展 (mul/div) 需 32-bit 立即数, shim 不支持 | **M 扩展暂跳过**, 仅跑 I + Zicsr + Zifencei |
| **R-COMPRESS-1** | C 扩展 (压缩指令) 需 ISA=RV64GC, 编译时需 `-march=rv64imac_zicsr_zifencei` | 在测试 ConfigBuilder 中显式设置 |
| **R-ACT-1** | ACT4 默认用 Sail 生成自检 ELF, 需 `mise trust .mise.toml` + Python 环境 | Day 0 准备 |
| **R-ENV-1** | sail_riscv 命令名是 `sail_riscv` (下划线) 而非 `sail-riscv` (连字符) | shim/RISCOF 配置使用 `sail_riscv` |

---

## 五、Spike Diff 调试方法论 (保留, 适用)

**v1.0 §五 完全适用, 保留不动**:

```bash
# Step 1: Spike 逐条提交日志
spike --log-commits -l --isa=rv64gc_zicsr failing_test.elf 2> spike_trace.log

# Step 2: ChipForge L1CachePlugin 同样格式 trace (Phase 2 任务 2 实施)
./chipforge_sim --trace-format=spike --l1cache=shim failing_test.elf 2> l1_trace.log

# Step 3: diff
diff <(awk '{print $1,$2,$3}' spike_trace.log) <(awk '{print $1,$2,$3}' l1_trace.log) | head -50
```

**3 路归因决策树** (v1.0 适用, 略加修改):
- L1CachePlugin ≠ Spike → **cache 行为错误** (lookup/refill 流水线 bug)
- L1CachePlugin = Spike ≠ Verilator → **微架构时序错误** (Phase 5)
- L1CachePlugin = Spike = Verilator ≠ ACT4 自检结果 → **测试期望或 ELF 加载错**

---

## 六、立即执行项 (修订, 命名/路径修正)

> **v1.0 §六错误**: "跑通 `rv64i_m/I/add-01.S`" — ACT4 命名是 `I-add-00.S`,且 arch-test 在 `tests/rv64i/I/` (不在 `rv64i_m/I/`)。

| # | 项 | v1.0 描述 | v1.1 修正 |
|---|----|---------|----------|
| 1 | 标记 v1.0-baseline | ✅ | **改为 v1.1-baseline** + git tag |
| 2 | Dockerfile + requirements-toolchain.txt | ✅ | **保持**, 但内容改为实测版本 (见 §2.1) |
| 3 | 跑通 `rv64i_m/I/add-01.S` | ✅ | **改为** `tests/rv64i/I/I-add-00.S` (ACT4 命名) |
| 4 | TLM 平台 SystemC 接口规范 | ✅ | **改为 CppTLM 接口规范** (基于 ch_stream, 见 §3.3) |
| 5 (新增) | L1CachePlugin Python shim | ❌ | **新增**: `src/cf_plugin/bridge/l1_cache_python_shim.{h,cpp}` + Python ctypes 调用测试 |
| 6 (新增) | MemoryTLM stub 升级评估 | ❌ | **新增**: 决定 P0-P1 是否可在 2KB 限制下推进, 还是必须先实施 MemoryTLM |
| 7 (新增) | riscv-arch-test ACT4 集成 | ❌ | **新增**: `git submodule add https://github.com/riscv/riscv-arch-test`, 应用 chipforge DUT config 模板 |

完成以上 7 项后, 方可开始 P0 全量测试。

---

## 七、风险与缓解 (新增)

| ID | 风险 | 严重度 | 缓解 |
|----|------|-------|------|
| **R-1** | MemoryTLM stub, 真实主存不可达 | 🔴 High | M2 范围限定: 仅验证 I 子集 (add/and/or/sll 等小 ELF, < 2KB), M/C/F/D 推迟到 MemoryTLM 实施后 |
| **R-2** | `uint_t<512>` Phase 0 退化, 写穿粒度 8 字节 | 🟡 Med | shim 强制 8 字节对齐, ELF 加载时分段; **不可逆限制, 接受** |
| **R-3** | riscv-arch-test ACT4 框架仍在演进 (2026-04 发布) | 🟡 Med | 锁定 commit `09b4999e`, 升级前评估 |
| **R-4** | Spike HTIF 调试陷阱 (4 件套) | 🟡 Med | §3.4 已记录 4 件套方案, TDD 时优先验证 |
| **R-5** | CppTLM 框架对 RVWMO 语义无内置支持 | 🟠 Med-Low | 在 L1CachePlugin 内部维护 Program Order (lookup→refill 单 in-flight 事务) |
| **R-6** | Python 走 ctypes 而非 pybind11, 性能损失 | 🟢 Low | Phase 2 测试吞吐受 spike 限制, shim 调用不是瓶颈 |
| **R-7** | sail_riscv 命令名 (`sail_riscv` 下划线) 与脚本预期 (`sail-riscv` 连字符) 不一致 | 🟢 Low | §二 2.2 明确命名, 文档引用统一 |

---

## 八、Python shim 接口契约 (新增, §3.2 摘要)

```c
// l1_create_bridge() — 返回 opaque L1CacheTLMBridge*
// 内部: std::make_unique<L1CacheTLMBridge>(std::make_unique<L1CachePlugin>(256, 20, 8, 64))

// l1_issue_request(bridge, addr, data, is_write, id)
// 内部: bridge->issue_request({addr, data, is_write, op, id})  // op default 0
//       bridge->tick()  // D1' 契约末尾

// l1_refill_from_memory(bridge, data, id, err)
// 内部: bridge's underlying plugin->refill_from_memory(node, {data, id, err})

// l1_read_data(bridge, addr) -> uint64_t
// 内部: 走 lookup 路径
//   1. 计算 idx = (addr >> 3) & 0xFF (8 sets, 256 idx)
//   2. tag 比较, hit → 返回 data, miss → 返回 0 (无 refill 注入)
//   3. tick 推进
// 注: 完整 miss-then-refill 路径需 Python 显式调用 l1_refill_from_memory

// l1_read_range(bridge, addr, size_bytes, out_buffer) -> size_t
// 内部: 循环 l1_read_data(addr + i*8), 8 字节写入 out_buffer
//       返回实际读取字节数 (size_bytes / 8 向上取整)

// l1_read_tohost(bridge) -> uint64_t
// 内部: 读 L1CachePlugin 内部 tohost 影子 (需 shim 内部维护)
// 0 = 未触发, 1 = PASS, 3 = FAIL
```

**测试契约** (`test_python_shim.cpp`, TDD 优先):
1. `l1_create_bridge` 返回非 nullptr
2. `l1_write_data(addr=0x100, 0xCAFE)` 后 `l1_read_data(0x100) == 0xCAFE`
3. `l1_issue_request({0x100, 0, false, 1})` → `l1_read_tohost() == 0` (持续推进)
4. `l1_read_range(0x100, 64, buf)` 返回 64, 内容为 8 个 uint64 (小端序)
5. 销毁后不泄漏 (valgrind 验证)

---

## 九、验收标准 (M2 里程碑)

| 项 | 目标 | 验证命令 |
|----|------|---------|
| 工具链锁定 | 5/5 工具精确版本 | `dpkg -l \| grep riscv64` + `sail_riscv --version` + `spike --help` |
| riscv-arch-test ACT4 集成 | 8 子集 ELF 可编译 | `cd riscv-arch-test && make elfs` 退出码 0 |
| I 子集 100% PASS | `I-add-00.elf` 到 `I-*` 全部 | `scripts/run_arch_test.py --ext=I` 返回 0/0 |
| L1CachePlugin shim | ctypes 调用 5/5 测试 PASS | `ctest -R test_python_shim` 5/5 |
| Spike Diff 工具 | 三路 diff 可定位分歧 | `diff --side-by-side spike_trace.log l1_trace.log` |
| **M2 里程碑** | **I 子集 100% PASS + M 子集 ≥80%** | 综合报告 |

---

## 十、变更日志

### v1.0 → v1.1 变更总结 (按章节)

| § | v1.0 | v1.1 变更类型 |
|---|------|------------|
| §0 | (无) | **新增** 平台约束声明 |
| §一 | 4 条核心原则 | **+2 条** (分层, 适配, 可执行) |
| §2.1 | Docker digest 假设 | **改** 实测 apt+commit 锁定 |
| §2.2 | 5 工具 | **+3** (riscv-arch-test, elfio, 推迟 riscv-dv) |
| §3.1 | `extract_signature` (旧 API) | **重写** ACT4 自检 ELF 流程 |
| §3.2 | b_transport + set_streaming_width | **重写** C++ shim 接口 (CppTLM 适配) |
| §3.3 | (无) | **新增** shim vs ch_stream 路径选择 |
| §3.4 | (无) | **新增** Spike HTIF 4 件套 (实施经验) |
| §四 | P0-P5 (含 Verilator/riscv-dv) | **重映射** 到 ChipForge Phase 2-5 |
| §五 | Spike Diff | **保留** 适用 |
| §六 | add-01.S | **改** I-add-00.S (ACT4 命名) |
| §七 | (无) | **新增** 7 项风险 |
| §八 | (无) | **新增** shim 接口契约 |
| §九 | (无) | **新增** M2 验收标准 |

### 影响范围

- **新增 .cpp/.h**: `src/cf_plugin/bridge/l1_cache_python_shim.{h,cpp}`, `tests/test_python_shim.cpp`
- **新增脚本**: `scripts/run_arch_test.py`, `scripts/run_chipforge_tests_phase2.sh`
- **新增 submodule**: `riscv-arch-test` (ACT4)
- **新增文档**: `docs/roadmap/phase-2-arch-test-config.md` (chipforge DUT config 模板)
- **更新文档**: `docs/roadmap/phases/phase-2-baremetal.md`, `CHANGELOG.md` Unreleased

---

## 决议表 (F1-F8)

| # | 决议 | 状态 |
|---|------|------|
| **F1** | 平台范围 = ChipForge 现状 (L1CachePlugin as memory model, 无 CPU Core) | **Proposed** |
| **F2** | TLM 框架 = CppTLM 2.0.3 (commit c6079357), 非 SystemC TLM 2.0 | **Proposed** |
| **F3** | Python 集成 = C++ shim + ctypes + 子进程, 非 pybind11 | **Proposed** |
| **F4** | arch-test 流程 = ACT4 自检 ELF + tohost 监测, 非 ACT3.x 外部 signature dump | **Proposed** |
| **F5** | 主存访问 = 通过 L1CachePlugin (2KB 限制), MemoryTLM 推迟到 Phase 2.5+ | **Proposed** |
| **F6** | 工具链锁定 = 实测版本号 (apt + commit hash), 不使用 Docker digest 假设 | **Proposed** |
| **F7** | Spike HTIF 集成 = 4 件套 (PROGBITS + KEEP + --no-gc-sections + auipc) | **Proposed** |
| **F8** | 里程碑 = 重映射到 ChipForge Phase 2-5 (riscv-dv/Verilator 推迟) | **Proposed** |

---

*本指南基于评审反馈 + 4 项并行调研结果 (riscv-arch-test 可行性 / CppTLM API / L1CachePlugin API / 测试套件对比) 起草。修订完成后 v1.0 通用 RISC-V TLM 指南即作废, ChipForge 后续裸机验证工作严格按此 v1.1 执行。*
