# tests/CMakeLists.txt
# 单二进制 Catch2 测试套件（与 CppTLM 模式一致）
# 设计原则: 测试自动发现, 新增 test_*.cpp 无需修改本文件

# 收集所有测试源文件（含子目录, RECURSE 处理 cpu/integration/ 和 cpu/configs/）
file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*/test_*.cpp"
)
# cpu-pipeline-tick-fix build-env-recovery (2026-10-16):
# 临时 exclude test_csr_chmem.cpp (csr-chmem IN_PROGRESS change 留的 tech debt,
# RiscvCsrPluginChmem API 已迁移但 test 未跟进 — 与本 fix change 无关).
# 修法: 后续 csr-chmem 完成后回头删除本 exclude block.
list(REMOVE_ITEM TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_csr_chmem.cpp")

# ★★★ ADR-091 v1.8（★ 修复「TLM binary 自 S1 起不可重建」的假绿）：
#   ★ CH_MEM-only 测试文件（★ 用 `PipeBuilder::declare_in/out` / `elaborate()`
#   ★ / `create_simulator()` ⇒ ★ 这些 API **只在 `#ifdef CF_PLUGIN_USE_CH_MEM`
#   ★ 内存在**）⇒ ★ MUST NOT 进 TLM binary ⇒ ★ 否则 **TLM 编译失败**
#   ★ ⇒ ★ TLM binary **无法重建** ⇒ ★ 「TLM 回归通过」变成**旧 binary 的假绿**。
#   ★ ⇒ ★ 实测命中：★ `test_cpu_open_core_ibus.cpp` + `test_cpu_open_core_dmem.cpp`
#   ★   （★ S3a / A1 引入）⇒ ★ TLM 目标 **111 个编译错误**。
#   ★ ⇒ ★ 机制：★ 显式列出 ⇒ ★ 一次性排除 ⇒ ★ 且下方的 CMake 断言保证
#   ★   「**排除集合 ⊆ CH_MEM binary 的源集合**」（★ 否则 CH_MEM 侧漏测试）。
set(CHMEM_ONLY_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_open_core_ibus.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_open_core_dmem.cpp"
)
list(REMOVE_ITEM TEST_SOURCES ${CHMEM_ONLY_TEST_SOURCES})

# 关键: cache + mmu 测试需要的实现 .cpp 源文件
# 这些 .cpp 之前在 src/cf_plugin/CMakeLists.txt 中被显式编译进独立 test target
# 单二进制模式下, 必须显式列出以确保链接找到符号
# (cf_plugin 是 INTERFACE 库, 不包含 .cpp 实现)
set(CACHE_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/ip/cache/plugins/L1CachePlugin.cpp
    ${CMAKE_SOURCE_DIR}/ip/cache/policies/replacement_policy.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/l1_cache_bridge_adapter.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/mmu_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/cf_plugin/bridge/mmu_bridge_adapter.cpp  # mmu-cache-integration commit 4
)
list(APPEND TEST_SOURCES ${CACHE_IMPL_SOURCES})

# MMU 库实装源文件 (mmu-tlb-ptw-impl change 解阻塞)
# cf_plugin 是 INTERFACE 库, 不编译 .cpp; mmu 库实现必须显式加入
# 注: MMUPlugin.cpp 含 skeleton 阶段 stub 代码 (auto* n = pb.node_of_logic_stage(...) API 错用)
#     commit 4 才完整重写 at_stage 闭包. commit 2 仅加入 lib/ + policies/ 4 个 .cpp
# commit 4 加入 MMUPlugin.cpp (at_stage 闭包实装)
set(MMU_IMPL_SOURCES
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/multi_level_tlb.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/ptw.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/lib/tlb_factory.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/policies/tlb_replacement_policy.cpp
    ${CMAKE_SOURCE_DIR}/ip/mmu/tlm/MMUPlugin.cpp  # commit 7 (mmu-tlb-ptw-impl) 加入
    ${CMAKE_SOURCE_DIR}/ip/cpu/plugins/mmu.cpp    # mmu-cache-integration commit 6 (sfence_vma + csr_write_satp 实装)
)
list(APPEND TEST_SOURCES ${MMU_IMPL_SOURCES})

# mmu tests re-enabled in mmu-tlb-ptw-impl commit 10 (skeleton stub completed)

# 把 catch_amalgamated.cpp 加入编译（提供默认 main）
list(APPEND TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch_amalgamated.cpp"
)

# 单二进制 chipforge_tests
add_executable(chipforge_tests ${TEST_SOURCES})
# CppHDL 使用 C++20+ 特性 (std::source_location, std::bit_width, consteval)
# chipforge_tests 通过 cf_plugin_link_cpphdl 链接 CppHDL 头, 所以本 target 需 C++23 (v0.6 ADR-047)
target_compile_features(chipforge_tests PRIVATE cxx_std_23)
target_compile_definitions(chipforge_tests PRIVATE
    TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
    RV32UI_CSV_PATH="${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32ui-baseline-matrix.csv"
    # Phase E: mfc-cpu-pipeline-multi-cycle-fsm rv32um 8/8 PASS (PoC-1 硬指标)
    TEST_RV32UM_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
    RV32UM_CSV_PATH="${CMAKE_SOURCE_DIR}/soc/cpu/docs/dse/rv32um-baseline-matrix.csv"
)

# tests/ 子目录加入 include 路径（让 #include "catch_amalgamated.hpp" 解析）
# catch_amalgamated.hpp 在 tests/catch2/ 子目录, 必须显式加进去
target_include_directories(chipforge_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/catch2
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)

# 链接 cf_plugin 框架 (头文件 only INTERFACE 库)
target_link_libraries(chipforge_tests PRIVATE
    cf_plugin
)
# bridge .cpp 已经在 CACHE_IMPL_SOURCES 中显式编译进 chipforge_tests,
# 不需要链接 cf_plugin_bridge 库 (它不是真实 target)
# 通过 helper 函数附加 CppTLM 和 CppHDL 依赖
# (这些函数同时链接库和添加 include 路径, 避免与 target_link_libraries 重复)
cf_plugin_link_cpptlm(chipforge_tests)
cf_plugin_link_cpphdl(chipforge_tests)

# 启用 ASan (可选, 跟随父选项)
if(ENABLE_ASAN)
    target_compile_options(chipforge_tests PRIVATE --coverage)
    target_link_options(chipforge_tests PRIVATE --coverage)
endif()

# 让测试可执行文件找到项目根目录（soc JSON, manual_elf 等）
target_compile_definitions(chipforge_tests PRIVATE
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# 设置运行时输出目录
set_target_properties(chipforge_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 注册到 ctest（单测试）
# ★ C1（2026-10-28）「旧产物顶替」假绿通道修复 —— ★ 三层防护：
#   ① **baseline-aware 判定**（★ wrapper）⇒ ★ 判定「是否有**新**失败」
#      ⇒ ★ 清单 = `tools/baseline_known_failures.txt`
#      ⇒ ★ 修掉「entry 恒红 ⇒ 退出码失去意义」的老问题
#   ② **build fixture**（★ `cf_build_all`）⇒ ★ ctest **先 build**
#      ⇒ ★ 编译失败 ⇒ fixture FAIL ⇒ 依赖它的测试被标为 **Not Run**
#      ⇒ ⇒ ★ ctest 整体 FAIL ⇒ ★ **fail-closed**（★ 不再跑旧产物）
#   ③ **wrapper 拒绝缺失 binary**（★ exit 2）⇒ ★ 绝无「找不到就跳过」
add_test(NAME chipforge_tests
         COMMAND ${CMAKE_SOURCE_DIR}/tools/run_catch2_with_baseline.sh
                 $<TARGET_FILE:chipforge_tests>)
set_tests_properties(chipforge_tests PROPERTIES LABELS "unit;integration;catch2")
set_tests_properties(chipforge_tests PROPERTIES FIXTURES_REQUIRED cf_build)

# 让 ctest 可执行文件找到项目根
set_tests_properties(chipforge_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ════════════════════════════════════════════════════════════════════
# ★ C1：build fixture（★ fail-closed 的关键）
# ════════════════════════════════════════════════════════════════════
# ★ ctest **本身不构建项目** ⇒ ★ 若源码有编译错误，★ ctest 会**照跑旧产物**
#   ⇒ ★ 报告「通过」而源码根本没编过 ⇒ ★ **假绿通道**（★ 本轮实测命中：
#   ★ TLM binary 自 ADR-091 S1 起编译失败 ⇒ 回归跑的是旧产物）。
# ★ ⇒ ★ 用 fixture 把「build」变成 ctest 的**前置**：
#   · `cf_build_all` 跑 `cmake --build`（★ 增量）
#   · 测试用 `FIXTURES_REQUIRED cf_build` 声明依赖
#   ⇒ ★ build 失败 ⇒ fixture FAIL ⇒ 依赖测试标为 **Not Run**
#   ⇒ ⇒ ★ ctest 整体 FAIL ⇒ ★ **fail-closed**（★ 不再静默跑旧产物）
# ★ ★ **只 build 测试 target**（★ 不 build `all`）：★ `cmake --build <dir>`（all）
#   会连带触发 **ExternalProject re-configure**（★ `cpptlm_external`）⇒ ★ 该
#   独立 cmake 步骤**找不到编译器** ⇒ ★ 失败（★ 既存环境脆性，★ 与本 fixture
#   无关 ⇒ ★ 已单独立项跟踪）⇒ ★ 而 `--target <测试>` **不**触发它（★ 实测
#   EXIT=0）⇒ ★ 故此处**必须**限定 target。
add_test(NAME cf_build_all
         COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                 --target chipforge_tests chipforge_tests_chmem)
set_tests_properties(cf_build_all PROPERTIES FIXTURES_SETUP cf_build)

# ----------------------------------------------------------------------------
# chipforge_tests_chmem: 第二测试 target, 编译选项 -DCF_PLUGIN_USE_CH_MEM=ON
# 验证框架在 CH_MEM 模式下也能 build 通过 (Phase 6c M2 Spike 3)
# 注: 含 14 个 framework/ + cpu/ + cache/ CH_MEM 测试 (Phase 6c M2/M3/M4/W0 累计),
# 含 [verilator] e2e (Phase 6d.5 E8 子集, 5 ELF tohost=1; 完整 6d.5 推迟到 Phase 6d 启动)
# ----------------------------------------------------------------------------
set(CHMEM_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_cppHDL_hello_poc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_elaborate_connector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_elaborate_pipeline2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_payload_store_miss_throws.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_multi_cycle_fsm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_chmem_multi_cycle_fsm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_rtl_regfile_alu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_5stage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_decoder_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_memory_model_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_decoded_inst_migration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_vendored_elf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_bp_plugin.cpp
    # chmem-bp-btb-gshare Task 2/3/4 — IBus 侧 PC-mux 五级链 + 门控式 flush 单元锚
    # （真实 IBusPlugin + fake 生产者；断 5 条臂与 flush 门控分量的**运行期值**）
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_ibus_bp_pcmux.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_verilator_sim.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mmu/test_mmu_bare_plumbing_verilator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_mmu_ptw_fsm_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cache/test_l1cache_refill_fsm_chmem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_csr_chmem.cpp
    # mmu-chmem-pipeline-integration / Phase 2 Task 2.1 (C2): ibus_chmem sv32 消费
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_ibus_chmem_sv32.cpp
    # mmu-chmem-pipeline-integration / Phase 2 Task 2.2 (C3): dmem_chmem sv32 消费
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_dmem_chmem_sv32.cpp
    # 2026-10-20 Oracle 深挖复测 — ch_uint<32> arith 14/4 3*4 PoC (chmem-mext-comb-datapath 前置)
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_chmem_arith_poc.cpp
    # CH_MEM ALU 单元级语义锚 — 真跑 int_alu_chmem.h 的 19 层 select 树 (M-op + RV32I 回归 + 判别互斥)
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_alu_semantics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_mem_read_drift.cpp
    # CtrlLink 有状态 (跨拍保持) halt 条件 — openspec change ctrl-link-chmem-reg-handle
    # (capability ctrllink-continuous-handle; 多拍驱动证明 OR 随寄存器逐拍更新)
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_ctrl_link_stateful.cpp
    # MMU payload key 模式化 + CH_MEM 生产者/消费者顺序契约
    # — openspec change chmem-mmu-keys-modality (Phase B 的 B.3/B.4 硬前置)
    ${CMAKE_CURRENT_SOURCE_DIR}/mmu/test_mmu_keys_modality.cpp
    # MMUPluginChmem TLB 命中路径 (Phase B / B.4, 2026-10-22 路线 C 收窄版)
    # — openspec change mmu-chmem-phase-b-c-launch
    ${CMAKE_CURRENT_SOURCE_DIR}/mmu/test_mmu_chmem_basic.cpp
    # CPU/CSR payload key 模式化 + 拆 csr_chmem 常量折叠 (Phase C.1 硬前置)
    # — openspec change chmem-cpu-keys-modality
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_keys_modality.cpp
    # chmem-bp-btb-gshare Task 1 可行性 PoC (硬门禁: 全绿才允许进 Task 2)
    # — ch_reg 跨拍 / ch_mem 动态索引 / 2-bit PHT / 越界读=0 / 活节点
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_chmem_bp_poc.cpp
    # ADR-091 S1 (backend-parity-open-core): PipeBuilder 顶层端口声明契约
    # (G-1) + 后端注入契约 (G-5a) —— ★ CH_MEM binary 是**显式列表** ⇒ 须显式加入
    ${CMAKE_CURRENT_SOURCE_DIR}/framework/test_pipe_builder_ports.cpp
    # ADR-091 S2 (backend-parity-open-core): 总线接口契约 (CpuBusReq/CpuBusResp)
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/schema/test_cpu_bus_iface.cpp
    # ADR-091 S3 (backend-parity-open-core): 开放核取指端口化契约
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_open_core_ibus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/test_cpu_open_core_dmem.cpp
    # ADR-092 v1.2 §D3.1 Z-1：P2 的 CH_MEM 配置面契约测试（★ 实测确认 CH_MEM binary
    #   此前 0 个「chmem config」case ⇒ binary 归属盲区）
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/schema/test_cpu_config_chmem.cpp
    # ADR-092 v1.2 §D3.1 Z-1（零工作量项）：以下 3 个文件与后端无关
    # ⇒ 加入 CH_MEM binary ⇒ 消除「binary 归属」盲区（41 case）
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu/schema/test_cpu_isa_xlen_schema.cpp
)
# 同样需要 catch_amalgamated.cpp 提供 main
list(APPEND CHMEM_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/catch2/catch_amalgamated.cpp"
)

# ★ ADR-091 v1.8：断言「CH_MEM-only 排除集合 ⊆ CH_MEM binary 的源集合」
#   ⇒ ★ 防「TLM 排除了、CH_MEM 忘了加」⇒ ★ CH_MEM 侧漏测试（★ 假绿通道）
foreach(_cf_chmem_only IN LISTS CHMEM_ONLY_TEST_SOURCES)
  if(NOT "${_cf_chmem_only}" IN_LIST CHMEM_TEST_SOURCES)
    message(FATAL_ERROR
        "CH_MEM-only 测试文件必须同时加入 CHMEM_TEST_SOURCES: ${_cf_chmem_only}")
  endif()
endforeach()

add_executable(chipforge_tests_chmem ${CHMEM_TEST_SOURCES})
target_compile_features(chipforge_tests_chmem PRIVATE cxx_std_23)
target_compile_definitions(chipforge_tests_chmem PRIVATE
    CF_PLUGIN_USE_CH_MEM
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
    CF_VERILATOR_SIM_BIN="${CMAKE_BINARY_DIR}/bin/cpu_verilator_sim"
    CF_MMU_VERILATOR_BASELINE_CSV="${CMAKE_SOURCE_DIR}/tests/mmu/test_mmu_bare_plumbing_verilator_baselines.csv"
    CF_MMU_BARE_ELF_PATH="${CMAKE_BINARY_DIR}/mmu_bare.elf"
)
target_include_directories(chipforge_tests_chmem PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/catch2
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(chipforge_tests_chmem PRIVATE cf_plugin)
cf_plugin_link_cpptlm(chipforge_tests_chmem)
cf_plugin_link_cpphdl(chipforge_tests_chmem)

set_target_properties(chipforge_tests_chmem PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LINK_FLAGS "-Wl,--allow-multiple-definition"
)
# ★ C1：★ 同 TLM entry（★ baseline-aware + build fixture 依赖）
add_test(NAME chipforge_tests_chmem
         COMMAND ${CMAKE_SOURCE_DIR}/tools/run_catch2_with_baseline.sh
                 $<TARGET_FILE:chipforge_tests_chmem>)
set_tests_properties(chipforge_tests_chmem PROPERTIES LABELS "unit;chmem")
set_tests_properties(chipforge_tests_chmem PROPERTIES FIXTURES_REQUIRED cf_build)
set_tests_properties(chipforge_tests_chmem PROPERTIES
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# ----------------------------------------------------------------------------
# Standalone Verilator runner smoke (Oracle 2026-10-06 Option A).
#
# Why here: CMake's enable_testing() at the root CMakeLists.txt runs *after*
# add_subdirectory(tools/verilator_runner), so add_test() in tools/verilator_runner/
# CMakeLists.txt is silently ignored (no CTestTestfile.cmake generated in that
# subdir). Tests/CMakeLists.txt has the active enable_testing() context — the
# chipforge_tests / chipforge_tests_chmem add_test() calls above are visible
# precisely for this reason. We register the cpu_verilator_sim ctest probe
# alongside them.
#
# Coverage scope: 1 ELF (rv32ui-p-add, ~6 cycles) — a smoke. The full 5-ELF
# tohost=1 verification is already covered by chipforge_tests_chmem's
# [verilator] family (TEST_CASE "cpu_verilator_sim_tohost1" in
# tests/cpu/test_cpu_verilator_sim.cpp, 10 assertions). This entry is the
# developer-facing debug path when Catch2 wrapping hides the failure —
# running the .exe directly bypasses Catch2 for faster bisection.
#
# Guarded by:
#   - TARGET cpu_verilator_sim  (only present when CHIPFORGE_BUILD_VERILATOR_RUNNER=ON)
#   - EXISTS ELF path           (fresh builds without vendored ELF)
# ----------------------------------------------------------------------------
set(_cf_verilator_smoke_elf "${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf/rv32ui-p-add")
if(TARGET cpu_verilator_sim AND EXISTS "${_cf_verilator_smoke_elf}")
    add_test(
        NAME cpu_verilator_sim_add
        COMMAND cpu_verilator_sim --elf "${_cf_verilator_smoke_elf}" --cycles 2000
    )
    set_tests_properties(cpu_verilator_sim_add PROPERTIES
        LABELS "verilator;e2e"
        TIMEOUT 120
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# ----------------------------------------------------------------------------
# CH_MEM/Verilator 分支微基准基线漂移检测 (chmem-branch-microbench).
#
# 判据: 4 档可控预测率负载的 TOHOST(校验和) 与 cycles 均与基线 CSV **精确相等**
#   (soc/cpu/docs/dse/branchbench-baseline-matrix.csv); 校验和不符 => 结果错, 失败.
#   注: runner 的 PASS/FAIL 标签对本基准无意义 (其判据 tohost==1, 而本基准
#   tohost 携带校验和) => 判定权在宿主侧比对.
# TIMEOUT 300: 实测单跑 4 档 ~40s (含 verilator 运行时编译).
# ----------------------------------------------------------------------------
if(TARGET cpu_verilator_sim AND EXISTS "${CMAKE_BINARY_DIR}/branchbench_m0.elf")
    add_test(
        NAME branchbench_baseline
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/measure_branchbench.sh" --check-baseline
    )
    set_tests_properties(branchbench_baseline PROPERTIES
        LABELS "verilator;perf;e2e"
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )

    # ----------------------------------------------------------------------------
    # ★ BP 方向性判据 (chmem-bp-btb-gshare-poc, critic B-2)
    #
    # Why: BP 的机制指纹 (PHT 方向学习 / 饱和判定) 在本 change 之前**没有任何
    #   自动锚** —— 4 档指纹只活在 commit message 里, 开发者手测一次就没有第二份
    #   ⇒ 有人再写出一个 `?:` 折常量或把方向学习改回「恒 +1」⇒ 判据失效而
    #   **无人察觉** ⇒ 机制被判「不成立」⇒ 误杀 ② (以及 CoreMark ≥1.9 的前置)。
    # 判据 (4 档, BP=on): BTB 命中 > 0 · 校验和与 BP=off 基线逐位相等 ·
    #   always_taken 预测率 ≥ 90% (PHT 学得到 taken) ·
    #   pattern_6of8 预测率 ≤ 90% (方向学习生效).
    # ★ 已用**变异对照**验证判据有牙: 「BTB 永不写」/「方向学习恒 +1」/
    #   「饱和判定三目折常量」三个变异分别让本 entry 判红.
    #
    # ★★ **本 entry 是「本地锚」, 不是「CI 保护」** (critic B-1 —— 上一轮 commit
    #   message 写的「BP 路径进入 CI」是**事实错误**, 已更正):
    #   CI **不跑 ctest**、且 **不构建项目** ⇒ `.github/workflows/architecture-gates.yml`
    #   的 branchbench step 恒走 SKIP (ADR-088 已明载「该 step 恒走 SKIP ⇒ 目前零
    #   实际保护」; brief AC-9 / ADR-088 §5.2 判据 4 同款: 仅加 ctest entry
    #   **不满足**该判据) ⇒ 本 entry 由 **PR 提交者本地手动跑**
    #   (`bash tools/run_chipforge_tests.sh --all`) 兑现。
    #   ★ 真正「让 BP 路径进入 CI」的前置 = ADR-088 的「CI 具备 build 能力」项
    #   ⇒ 归 ADR-088, **不在本 change** (已在 tasks.md 缺口表登记)。
    # TIMEOUT 300: 实测单跑 4 档 ~60s (BP=on, 含 verilator 运行时编译).
    # ----------------------------------------------------------------------------
    add_test(
        NAME branchbench_bp_direction
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/measure_branchbench.sh" --check-bp-direction
    )
    set_tests_properties(branchbench_bp_direction PROPERTIES
        LABELS "verilator;perf;e2e;bp"
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# ----------------------------------------------------------------------------
# CH_MEM/Verilator 性能基线漂移检测 (chmem-verilator-perf-harness 交付点 A).
#
# Why: ADR-088 §5.2 判据 4「CI 有可回归的自动测试」—— CI **不跑 ctest**,
#   故本 entry 只是**开发者辅助**; 真正满足判据 4 的是 architecture-gates.yml
#   里的 `Check CH_MEM perf baseline` 步骤 (warn 级).
#
# 判据: Dhrystone 在 CH_MEM/Verilator 后端跑完 (TOHOST==1) 且 cycles/dmips 与
#   soc/cpu/docs/dse/verilator-perf-baseline-matrix.csv **精确相等** (无容差).
#   TIMEOUT 一律失败 (cpu_verilator_sim 超时也 exit 0 ⇒ 不可只看退出码).
#
# TIMEOUT 300: 实测单跑 ~6s (含 verilator 运行时编译) ⇒ 300s 余量充足.
# ----------------------------------------------------------------------------
if(TARGET cpu_verilator_sim AND EXISTS "${CMAKE_BINARY_DIR}/dhrystone.elf")
    add_test(
        NAME verilator_perf_baseline
        COMMAND bash "${CMAKE_SOURCE_DIR}/tools/measure_perf.sh" --check-baseline
    )
    set_tests_properties(verilator_perf_baseline PROPERTIES
        LABELS "verilator;perf;e2e"
        TIMEOUT 300
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()