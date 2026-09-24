---
initiative: wave3-mmu-real-memory-and-cycle
priority: P1
version_target: v0.8.0
---

# Tasks — mmu-paddr-consume-and-real-memory

## 1. failing test (TDD red)

- [ ] 1.1 新建 `tests/cpu/test_mmu_paddr_propagation.cpp` —— IBus 期望读 PADDR 而非 vaddr, 故意构造 vaddr=0x80001000 + PADDR=0x1000 场景, 当前读 vaddr 0x80001000 → 测试期望读 PADDR 0x1000 fail
- [ ] 1.2 新建 `tests/mmu/test_ptw_real_memory.cpp` —— PTW 期望读真实 PTE, 当前 `advance_from_stub` 用 mock → 测试期望 walk 走真内存 fail
- [ ] 1.3 验证 fail: `[cpu]` 新测试 fail (期望 PADDR, 实际 vaddr); `[mmu]` 新测试 fail (期望真实 PTE, 实际 stub)

## 2. ADR-049 起草

- [ ] 2.1 新建 `docs/architecture/adr/ADR-049-mmu-paddr-consumption-contract.md` (Context / Decision PADDR-first / Consequences / 5 风险表)
- [ ] 2.2 `docs/architecture/adr.md` 注册 ADR-049

## 3. MemoryInterface 抽象

- [ ] 3.1 新建 `ip/mmu/lib/memory_interface.h` (~40 LOC) —— 抽象类: read_word/write_word
- [ ] 3.2 `PicolibcHostMemory` 继承 `MemoryInterface` (修改构造 + 添加 override)

## 4. PTW advance_from_real_memory

- [ ] 4.1 `ip/mmu/lib/ptw.h/.cpp` 新增 `advance_from_real_memory(MemoryInterface* mem)` 方法
- [ ] 4.2 `ip/mmu/tlm/MMUPlugin.h/.cpp` 构造函数接 `MemoryInterface* mem_ = nullptr` (向后兼容)
- [ ] 4.3 MMUPlugin at_stage 闭包优先用 `mem_->advance_from_real_memory()`, fallback `advance_from_stub()` (当 mem_ == nullptr)

## 5. IBusPlugin/DBusPlugin PADDR consumption

- [ ] 5.1 `ip/cpu/plugins/ibus.h` `at_stage("fetch", NORMAL)` 加 `paddr_valid` 标志读取, 优先用 `pl::PADDR`
- [ ] 5.2 `ip/cpu/plugins/dbus.h` `at_stage("memory", NORMAL)` 同样改造 (LOAD + STORE)
- [ ] 5.3 验证 `paddr_valid` 标志由 MMU 写入 (RiscvMMUPlugin::at_stage "tlb_lookup_*" LATE 阶段写 `valid=true`)

## 6. SoC JSON 集成

- [ ] 6.1 `soc/cpu_l1_mmu_demo.json` 加 `mmu.memory_interface: "picolibc_host_memory"` 字段
- [ ] 6.2 CpuFactory 解析该字段, 注入 MemoryInterface*

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
