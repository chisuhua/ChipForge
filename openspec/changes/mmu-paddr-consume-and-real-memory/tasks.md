---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
---

# Tasks — mmu-paddr-consume-and-real-memory

## 1. failing test (TDD red)

- [x] 1.1 新建 `tests/cpu/test_mmu_paddr_propagation.cpp` —— IBus 期望读 PADDR 而非 vaddr, 故意构造 vaddr=0x80001000 + PADDR=0x1000 场景, 当前读 vaddr 0x80001000 → 测试期望读 PADDR 0x1000 fail
- [x] 1.2 新建 `tests/mmu/test_ptw_real_memory.cpp` —— PTW 期望读真实 PTE, 当前 `advance_from_stub` 用 mock → 测试期望 walk 走真内存 fail
- [x] 1.3 验证 fail: `[cpu]` 新测试 fail (期望 PADDR, 实际 vaddr); `[mmu]` 新测试 fail (期望真实 PTE, 实际 stub)
  - **状态**: ✅ TDD red 验证成功 (2026-09-25):
    - `IBusPlugin_Consumes_PADDR_Over_VADDR`: REQUIRE(`last_paddr_read == 0x1000`) FAIL → `0 == 4096` (IBus 当前不读 PADDR, 装饰性问题确认)
    - `PTW_Sv32_Walk_Via_Stub_Memory_BackwardCompat`: CHECK(`result_fault == 0`) FAIL → `15 == 0` (PTW 当前仅 stub 路径, 真实路径缺失确认)
  - **下一步**: 实装 task 5 (IBus PADDR consumption) + task 3-4 (MemoryInterface + advance_from_real_memory) 后, 重新跑这两个测试验证 PASS (TDD green).

## 2. ADR-049 起草

- [x] 2.1 新建 `docs/architecture/adr/ADR-049-mmu-paddr-consumption-contract.md` (Context / Decision PADDR-first / Consequences / 5 风险表)
  - **状态**: ✅ 已完成 (commit pending) — 5 Decisions (D1 PADDR-first / D2 MemoryInterface 抽象 / D3 PicolibcHostMemory 继承 / D4 MMUPlugin 构造接 mem=nullptr / D5 SoC JSON 字段) + Open Questions + §后续项 已合入 P1#6 adr-049-followup-section.md 内容
- [x] 2.2 `docs/architecture/adr.md` 注册 ADR-049
  - **状态**: ✅ 已完成 — §2.1 已实现决策表行 + 计数 "37 → 38 条" 同步

## 3. MemoryInterface 抽象

- [x] 3.1 新建 `ip/mmu/lib/memory_interface.h` (~40 LOC) —— 抽象类: read_word/write_word
  - **状态**: ✅ 已完成 — 纯 lib/ 层 (零 cf::plugin::* 依赖), 与 ptw.h 同层
- [x] 3.2 `PicolibcHostMemory` 继承 `MemoryInterface` (修改构造 + 添加 override)
  - **状态**: ✅ 已完成 — read_word 移除 const + write_word/read_word 加 override; 既有 4 callers (dbus.h:84, ibus.h:76, test_picolibc_memory_base_window.cpp:35/84) 兼容性已验证 (build 成功, [cpu-integration] 81/81 PASS 0 回归)

## 4. PTW advance_from_real_memory

- [x] 4.1 `ip/mmu/lib/ptw.h/.cpp` 新增 `advance_from_real_memory(MemoryInterface* mem)` 方法
  - **状态**: ✅ 已完成 (Oracle C1+C2+§4.1 一并修复)
  - **C1 fix**: 修 reserved encoding 检查 (旧 r&&w&&x spec-wrong → 新 w&&!r per RISC-V spec)
  - **C2 fix**: start_walk + next_pte_paddr 加 mode-correct addressing (Sv32: 4-byte PTE + VPN[1]/VPN[0]; Sv39/48: 8-byte + 3/4-level walk)
  - **新增方法**: `advance_from_real_memory(MemoryInterface* mem)` 内部 null 检查 → stub fallback; 越界读 (PicolibcHostMemory 返回 0) → decode 后 V=0 → advance() 走 fault 12 路径
- [x] 4.2 `ip/mmu/tlm/MMUPlugin.h/.cpp` 构造函数接 `MemoryInterface* mem_ = nullptr` (向后兼容)
  - **状态**: ✅ 已完成 — 构造加 `MemoryInterface* mem` 默认 nullptr; `RiscvMMUPlugin` 透传 mem 到基类 (Oracle C7 mitigation)
- [x] 4.3 MMUPlugin at_stage 闭包优先用 `mem_->advance_from_real_memory()`, fallback `advance_from_stub()` (当 mem_ == nullptr)
  - **状态**: ✅ 已完成 — ptw_l0/l1/l2 at_stage 全部调 `ptw_->advance_from_real_memory(mem_)`, 内部 null 检查统一处理 (DRY)
  - **Oracle C6 fix**: PADDR + PADDR_VALID 在 do_lookup 同一 closure 同一 phase (NORMAL) 原子写, 镜像 PTW_ACTIVE 先例 (hit: true, fault: false)

## 5. IBusPlugin/DBusPlugin PADDR consumption

- [x] 5.1 `ip/cpu/plugins/ibus.h` `at_stage("fetch", NORMAL)` 加 `paddr_valid` 标志读取, 优先用 `pl::PADDR`
  - **状态**: ✅ 已完成 — 读 tlb_lookup_ifetch 节点的 PADDR + PADDR_VALID (Oracle C4 design.md D1 注记), has() 守卫防 PayloadStore fail-fast (v0.3.1 M6), 节点 nullptr / PADDR_VALID=false → fallback PC
- [x] 5.2 `ip/cpu/plugins/dbus.h` `at_stage("memory", NORMAL)` 同样改造 (LOAD + STORE)
  - **状态**: ✅ 已完成 — LOAD + STORE 都优先用 tlb_lookup_loadstore 节点的 PADDR (镜像 §5.1 模式), fallback MEM_ADDR (vaddr)
- [x] 5.3 验证 `paddr_valid` 标志由 MMU 写入 (RiscvMMUPlugin::at_stage "tlb_lookup_*" LATE 阶段写 `valid=true`)
  - **状态**: ✅ 已完成 (Oracle C6: 与 PADDR 同点同 phase NORMAL 原子写; hit→true, fault→false)
  - **额外修复**: RiscvMMUPlugin::setup() 现在调基类 MMUPlugin::setup() 声明 tlb_lookup_ifetch + tlb_lookup_loadstore substage (P0#1 之前缺失此调用导致 §5.1/§5.2 节点为 nullptr)

## 6. SoC JSON 集成

- [x] 6.1 `soc/cpu_l1_mmu_demo.json` 加 `mmu.memory_interface: "picolibc_host_memory"` 字段
  - **状态**: ✅ 已完成 — JSON 声明式字段 + description 标注 runner C++ 端透传 (Oracle C7 重述 scope)
- [x] 6.2 CpuFactory 解析该字段, 注入 MemoryInterface*
  - **状态**: ✅ 已完成 (rescoped per Oracle C7) — CpuFactory::register_early_plugins 1 行 static_cast<MemoryInterface*>(mem) 透传给 RiscvMMUPlugin (既有 mem 形参, 不需 JSON 解析器)

## 7. 验证 pass

- [ ] 7.1 `[cpu]` test_mmu_paddr_propagation 4/4 PASS
- [ ] 7.2 `[mmu]` test_ptw_real_memory 3/3 PASS + 既有 47 PASS 无回归 (合计 50/50)
- [ ] 7.3 `[soc]` test_cpu_l1_mmu_demo 8/8 PASS (升级 2 用例)
- [ ] 7.4 `[riscv-tests]` 40/40 PASS 无回归
- [ ] 7.5 `[cpu-integration]` 4/4 PASS 无回归
- [ ] 7.6 TLM baseline 0 回归

## 8. CI 门禁

- [ ] 8.1 `bash tools/verify_adr.sh` PASS（含 ADR-049）
- [ ] 8.2 `bash tools/verify_plugin_decision.sh` PASS（D4 合规）
- [ ] 8.3 `bash tools/check_plugin_portability.sh` PASS
- [ ] 8.4 `bash tools/doc_link_check.sh` PASS

## 9. 文档同步

- [ ] 9.1 `soc/cpu/docs/dse/mmu-paddr-propagation-matrix.csv` 新建 (真 PADDR 翻译矩阵)
- [ ] 9.2 `soc/cpu/docs/architecture.md` §3 MMU 状态更新（"装饰性" → "真集成"）
- [ ] 9.3 `ip/mmu/STATUS.md` "deferred 承诺" 段划掉 (`advance_from_real_memory()` 已实装)

## Design Notes (P0#1 传递)

> **MMU 配置 JSON 驱动** (P0#1 实施后记录, 2026-09-24):
> - 当前 `register_early_plugins()` 中 TLB 几何 (`{"L0", 8, 8, 1, 1, "LRU"}`, `{"L1", 8, 8, 1, 2, "LRU"}`) 和 `SvMode` 枚举映射由字符串 switch 完成, 三层都是 C++ 硬编码
> - `ip/mmu/configs/params_schema.json` 已存在, 但未在 `CPUConfig` / MMUPlugin 构造处数据化
> - ADR-048 §后续项: 建议 P1#3 或 P1#4 时改为从 `soc/*.json` 配置读取 TLB 层级数/每级大小/替换策略/`ptw_max_inflight`
> - 预期收益: 消除 C++ 中的配置描述代码, 支持 DSE 参数扫描

## 10. commit + archive

- [ ] 10.1 1 个原子 commit (含 test + ADR + MemoryInterface + PTW 改造 + IBus/DBus 改造 + SoC JSON + 文档)
- [ ] 10.2 `CHANGELOG.md` v0.5.0 段本 change 条目
- [ ] 10.3 `openspec archive mmu-paddr-consume-and-real-memory -y`

## Acceptance

- [ ] 1.1-1.3 failing test 写出 + 验证 fail
- [ ] 2.1-2.2 ADR-049 起草 + 注册
- [ ] 3.1-3.2 MemoryInterface 抽象
- [ ] 4.1-4.3 PTW real memory
- [ ] 5.1-5.3 IBus/DBus PADDR consumption
- [ ] 6.1-6.2 SoC JSON 集成
- [ ] 7.1-7.6 verify pass 全绿
- [ ] 8.1-8.4 CI 门禁全 PASS
- [ ] 9.1-9.3 文档同步
- [ ] 10.1-10.3 commit + archive + status 派生
