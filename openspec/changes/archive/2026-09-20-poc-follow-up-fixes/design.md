# Phase 6c PoC Follow-up Fixes — Design

> 详细设计文档

## Fix #1: M3/W6 byte-equal 完整版 设计

### 根因分析 (Phase 6c Oracle M3/W8 #95)

**`reg_file_chmem.h::get_regs()` 跨 context 安全性**:
- Phase 6c 修复: 提升 `static thread_local regs` 为 plugin 实例成员 `std::unique_ptr<regs_array_t> regs_`
- **遗留风险**: 5-stage 集成下多个 RegFilePlugin 实例 + 嵌套 elaboration (test 嵌套 + factory 嵌套) 是否完全安全?
- **测试方案**:
  1. Test 1: 单 RegFilePlugin 单 context → 32 ch_reg 全部 in context A
  2. Test 2: 两个 RegFilePlugin 实例 (栈分配复用同一地址) → 各自 32 ch_reg in 各自 context, 不串扰
  3. Test 3: RegFilePlugin + HazardPlugin + IntAluPlugin 共同 elaborate → 各自 regs/regs_ 独立

**`PayloadStore` cell put/get byte-equal**:
- 已知 issue: `n->operator()(KeyType::X) = val;` 写后, 另一闭包 `n->operator()(KeyType::X)` 读在 CH_MEM 模式下**偶尔**返回 null impl 或 0
- v0.3.1 M6 修复: `const T& get(key)` fail-fast 抛异常
- **遗留风险**: 5-stage 集成下 stage linking connector 跨 stage 复制 cell 时, 复制目标 cell 是否保留 ch 代理引用
- **测试方案**:
  1. 跑 `cpu_factory_chmem.h::build_cpu()` 完整 5-stage elaboration
  2. 在每个 stage 的 EARLY at_stage 闭包预填充占位符 (v0.3.1 M6 fix 沿用)
  3. 在每个 stage 的 NORMAL/LATE at_stage 闭包读 cell, 验证 byte-equal
  4. 跑 10+ cycle Simulator tick, 验证每 cycle cell 值与 TLM 参考 trace byte-equal

### 实现细节

```cpp
// tests/cpu/test_cpu_rtl_regfile_alu.cpp 新增
TEST_CASE("m3_poc_5stage_byte_equal", "[framework][chmem][m3][poc][byte-equal]") {
  // 1. TLM 参考 trace (纯 C++ 模拟 5-stage)
  auto tlm_trace = tlm_5stage_simulate(/* n_cycles=20, ... */);
  
  // 2. CH_MEM CpuFactoryChmem 完整 5-stage elaboration
  ch::core::context ctx("m3_poc_5stage_ctx");
  ctx.set_as_current_context();
  PipeBuilder pb(&ctx);
  auto factory = std::make_unique<CpuFactoryChmem>();
  factory->build_cpu(pb, /* PicolibcHostMemory* */ nullptr);
  pb.elaborate(ctx);
  pb.to_verilog("/tmp/cpu_5stage.v");
  
  // 3. CH_MEM Simulator tick 20 cycle
  ch::Simulator sim(&ctx);
  sim.reset();
  auto chmem_trace = chmem_5stage_simulate(pb, sim, /* n_cycles=20 */);
  
  // 4. Byte-equal 验证
  REQUIRE(tlm_trace.size() == chmem_trace.size());
  for (size_t i = 0; i < tlm_trace.size(); ++i) {
    INFO("Cycle " << i);
    REQUIRE(tlm_trace[i] == chmem_trace[i]);  // byte-equal
  }
}
```

## Fix #2: chbool context pollution 测试隔离 设计 (Metis 2026-09-20 修正)

### 根因分析 (Phase 6c Oracle)

`chipforge_tests_chmem` 全量跑时, `cpphdl_poc_chbool_contextual_conversion` 失败的根因:
- 测试套件中其他 TEST_CASE (e.g. `cpphdl_poc_ch_device_construct`) 先执行, 创建 `ch::ch_device<Hello> dev;`, dev 构造时 `ctx_curr_ = dev's ctx`
- dev 析构时 `ctx_swap` 还原 `ctx_curr_ = nullptr`
- `cpphdl_poc_chbool_contextual_conversion` 进入时 `ctx_curr_ = nullptr`, SECTION 内 `ch_bool a(false, "a_bool")` 调用 `node_builder::build_literal` → `[ERROR] No active context for literal creation` → 返回 nullptr → `if(a)` 静默 false

**v0.3.1 (`5fe72fe`) 部分修复**: 在每个 SECTION 入口前 `set_as_current_context()` 提供 active ctx, 但仅 SECTION 级隔离, TEST_CASE 级仍可能污染。

### 实现细节 (Metis 修正: 方案 B 直接)

**方案 B (推荐, Metis 决策)**: TEST_CASE_METHOD fixture

```cpp
// tests/framework/test_cppHDL_hello_poc.cpp
class ChmemPocFixture {
 public:
  ChmemPocFixture() {
    ctx_ = std::make_unique<ch::core::context>("chmem_poc_ctx");
    ctx_->set_as_current_context();
  }
  ~ChmemPocFixture() {
    ctx_.reset();  // ctx_curr_ 还原 (nullptr)
  }
 private:
  std::unique_ptr<ch::core::context> ctx_;
};

TEST_CASE_METHOD(ChmemPocFixture, "cpphdl_poc_ch_device_construct", "[framework][cpphdl][poc]") {
  // ctx 已 active (fixture 构造时 set)
  ch::ch_device<HelloComponent> dev;
  // ...
}

TEST_CASE_METHOD(ChmemPocFixture, "cpphdl_poc_chbool_contextual_conversion", "[framework][cpphdl][poc]") {
  // ctx 已 active
  ch_bool a(false, "a_bool");
  // ...
}
```

**Metis 决策**: 方案 A (catch2 `[.]` tag) 是 catch2 v3 隐藏测试 (默认不包含), 全量跑时如果 catch2 默认 filter 不匹配 `[.]` 标签, 测试被静默跳过, 全量跑可能显示"0 tests ran"被误解为 PASS。方案 B TEST_CASE_METHOD fixture 在每个 TEST_CASE 入口显式管理 context, 验证可靠。

## Fix #3: HazardPlugin 完整 CH_MEM 版 设计

### 现状 (Phase 6c W7)

`hazard_chmem.h` 仅 PoC 6 条件 OR-merge:
```cpp
ch_bool HazardPlugin::raw_hazard(...) {
  return (id_rs1 == ex_rd) || (id_rs2 == ex_rd) ||
         (id_rs1 == mem_rd) || (id_rs2 == mem_rd) ||
         (id_rs1 == wb_rd) || (id_rs2 == wb_rd);
}
```

**问题**:
- 缺 x0 屏蔽: `ex_rd == 0` / `mem_rd == 0` / `wb_rd == 0` 时不应 halt (写 x0 是 noop)
- 9 条件 = 6 数据 + 3 屏蔽

### 完整版

```cpp
ch_bool HazardPlugin::raw_hazard_complete(
    ch_uint<5> id_rs1, ch_uint<5> id_rs2,
    ch_uint<5> ex_rd, ch_uint<5> mem_rd, ch_uint<5> wb_rd) {
  // 数据路径 RAW 检测
  ch_bool ex_match  = (id_rs1 == ex_rd)  || (id_rs2 == ex_rd);
  ch_bool mem_match = (id_rs1 == mem_rd) || (id_rs2 == mem_rd);
  ch_bool wb_match  = (id_rs1 == wb_rd)  || (id_rs2 == wb_rd);
  
  // x0 屏蔽 (写 x0 是 noop, 不算 RAW)
  ch_bool ex_valid  = (ex_rd != ch_uint<5>(0_d));
  ch_bool mem_valid = (mem_rd != ch_uint<5>(0_d));
  ch_bool wb_valid  = (wb_rd != ch_uint<5>(0_d));
  
  // OR-merge: 任一阶段 RAW 且目的非 x0
  return (ex_match && ex_valid) || (mem_match && mem_valid) || (wb_match && wb_valid);
}
```

**与 CtrlLink 集成** (不变):
```cpp
pb.register_ctrl_link("decode", std::make_shared<CtrlLink>(
    halt_when(hazard.raw_hazard_complete(id_rs1, id_rs2, ex_rd, mem_rd, wb_rd))));
```

### 测试 PoC

```cpp
// tests/cpu/test_cpu_5stage.cpp 新增
TEST_CASE("hazard_chmem_complete_elaborate", "[framework][chmem][hazard][poc]") {
  ch::core::context ctx("hazard_complete_ctx");
  ctx.set_as_current_context();
  
  PipeBuilder pb(&ctx);
  HazardPlugin hazard;
  pb.register_plugin(...);
  pb.elaborate(ctx);
  pb.to_verilog("/tmp/hazard_complete.v");
  
  // 验证 hazard.v 含 9 个 mux_select
  std::ifstream f("/tmp/hazard_complete.v");
  REQUIRE(f.is_open());
  std::string v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  size_t mux_count = 0;
  size_t pos = 0;
  while ((pos = v.find("mux_select", pos)) != std::string::npos) {
    ++mux_count;
    pos += 10;
  }
  REQUIRE(mux_count >= 9);
}
```
