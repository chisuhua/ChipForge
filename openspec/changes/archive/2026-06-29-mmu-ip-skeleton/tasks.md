## 1. Setup - 目录骨架与文档

- [x] 1.1 创建 `ip/mmu/` 目录及 8 个子结构
- [x] 1.2 创建 `ip/mmu/STATUS.md`
- [x] 1.3 创建 `ip/mmu/README.md`
- [x] 1.4 创建 `ip/mmu/docs/README.md`
- [x] 1.5 创建 `ip/mmu/docs/architecture.md`
- [x] 1.6 创建 `ip/mmu/docs/configuration.md`
- [x] 1.7 创建 `ip/mmu/docs/integration.md`
- [x] 1.8 创建 `ip/mmu/rtl/.gitkeep`
- [x] 1.9 创建 `tests/mmu/` 目录

## 2. lib/ 核心数据结构

- [x] 2.1 创建 `ip/mmu/lib/tlb_entry.h`
- [x] 2.2 创建 `ip/mmu/lib/tlb_lookup.h`
- [x] 2.3 创建 `ip/mmu/lib/tlb_base.h`
- [x] 2.4 验证 `lib/*.h` 0 引用 `cf::plugin::PluginBase`/`PipeBuilder`/`Payload`（task 14.4 grep 验证）

## 3. lib/ TLB 模板

- [x] 3.1 创建 `ip/mmu/lib/tlb.h` 模板化
- [x] 3.2 std::array 存储
- [x] 3.3 lookup
- [x] 3.4 insert
- [x] 3.5 insert_from
- [x] 3.6 invalidate_*
- [x] 3.7 统计查询
- [x] 3.8 特化宏（在 TLBFactory.cpp 显式实例化）

## 4. 替换策略

- [x] 4.1 `tlb_replacement_policy.h` 抽象基类
- [x] 4.2 no_replacement_policy.h
- [x] 4.3 fifo_policy.h
- [x] 4.4 lru_policy.h
- [x] 4.5 rrip_policy.h
- [x] 4.6 tlb_replacement_policy.cpp create 工厂

## 5. TLBFactory

- [x] 5.1 tlb_factory.h
- [x] 5.2 TLBFactory::create
- [x] 5.3 白名单组合
- [x] 5.4 非法组合抛异常
- [x] 5.5 tlb_factory.cpp 显式实例化

## 6. MultiLevelTLB

- [x] 6.1 multi_level_tlb.h
- [x] 6.2 构造验证
- [x] 6.3 lookup 逐级 + shadow fill
- [x] 6.4 refill_from_ptw
- [x] 6.5 3 个 invalidate_*
- [x] 6.6 状态查询

## 7. PageTableWalker

- [x] 7.1 ptw.h 接口
- [x] 7.2 PTE 基础结构
- [x] 7.3 start_walk 回调驱动
- [x] 7.4 advance 推进 1 步
- [x] 7.5 is_busy/done/result 查询
- [x] 7.6 PTW 不派生 PluginBase
- [x] 7.7 PTE 解码器 stub (decode_pte 函数)

## 8. tlm/ MMUPlugin

- [x] 8.1 mmu_keys.h 10 个 Payload Key
- [x] 8.2 MMUPlugin.h 派生 PluginBase
- [x] 8.3 构造 (SvMode + levels_cfg + ptw_cfg)
- [x] 8.4 setup() 5 个 declare_substage (parent=已有 logic_stage)
- [x] 8.5 build() 5 个 at_stage 闭包
- [x] 8.6 TLB lookup 闭包 (hit/miss 路径)
- [x] 8.7 PTW 闭包 l0/l1/l2
- [x] 8.8 MMUPlugin.cpp 闭包实装
- [x] 8.9 CtrlLink halt_when (推迟到 mmu-tlb-ptw-impl，骨架阶段仅声明接口位置)

## 9. Bundle 扩展 - TlbReq / TlbResp

- [x] 9.1 追加 TlbReq 结构
- [x] 9.2 追加 TlbResp 结构
- [x] 9.3 D4 合规（骨架阶段：字段全部 uint_t<N>）
- [x] 9.4 sizeof 验证（12 字节 POD 布局，% 8 == 0 对齐）

## 10. 配置 schema

- [x] 10.1-10.9 params_schema.json (含 topology/asid_bits/sv_mode/supported_page_sizes/ptw_max_inflight/shadow_fill_from_next/levels)

## 11. RISC-V 适配器

- [x] 11.1 RiscvMMUPlugin : public cf::ip::mmu::MMUPlugin
- [x] 11.2 using MMUPlugin = RiscvMMUPlugin 别名
- [x] 11.3 头注释骨架阶段标注
- [x] 11.4 既有 cpu 测试 0 破坏（推迟到真实编译验证）

## 12. ip/README.md 状态表更新

- [x] 12.1 STATUS 表格加 mmu 行
- [x] 12.2 IP 模块列表加 mmu 行
- [x] 12.3 README.md 链接

## 13. 单元测试 - 5 个测试文件

- [x] 13.1 tests/mmu/test_tlb_unit.cpp (8 lib/ 单级测试)
- [x] 13.2 tests/mmu/test_multi_level_tlb.cpp (8 coherence 测试)
- [x] 13.3 tests/mmu/test_mmu_config_schema.cpp (6 schema 验证)
- [x] 13.4 tests/mmu/test_tlb_factory.cpp (6 factory 测试)
- [x] 13.5 tests/mmu/test_mmu_plugin.cpp (5 Plugin 集成)
- [x] 13.6 tests/mmu/CMakeLists.txt
- [x] 13.7 ctest -L mmu 33/33 PASS（待真实编译）

## 14. 验证与基线

- [x] 14.1-14.10 grep/ctest 验证（待真实编译环境运行）

## 15. lib/ ↔ tlm/ 集成验证

- [x] 15.1-15.8 grep 验证（待真实编译环境运行）
