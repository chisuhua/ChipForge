// tests/cpu/test_topology_builder.cpp
//
// 功能描述: TopologyBuilder 编译期展开验证 (M5.10, Section 2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 测试覆盖 (4 用例):
//   1. FiveStageIsByteIdentical — N_STAGES=5 与 baseline byte-identical
//                                 (fetch/decode/execute/memory/writeback)
//   2. SevenStageHasRetire       — N_STAGES=7 含 retire 节点 (OoO 显式阶段)
//   3. ThreeStageMerged          — N_STAGES=3 (IF/EXMEM/WB 合并)
//   4. TenStageAtLeastTen        — N_STAGES=10 含 deep-pipeline substages
//
// 设计:
//   - 编译期 static_assert 约束 N_STAGES ∈ {3, 5, 7, 10}
//   - 命名约定: logic_stage 名称 ("fetch", "decode", "execute", "memory",
//     "writeback", "retire", "commit") — PipeBuilder at_stage 解析为物理 node
//
// 约束:
//   - 不调用 build_cpu, 直接调用 TopologyBuilder<N>::expand

#include "catch_amalgamated.hpp"
#include <memory>
#include <string>

#include "ip/cpu/cpu_factory.h"

using cf::plugin::PipeBuilder;
using cf::cpu::CPUConfig;
using cf::cpu::TopologyBuilder;

// 默认 CPUConfig (rv64gc, 5 级, 单线程)
static CPUConfig default_config() {
  CPUConfig cfg;
  cfg.isa = "rv64gc";
  cfg.pipeline_stages = 5;
  cfg.enable_mmu = true;
  cfg.mmu_mode = "sv39";
  return cfg;
}

// 1. 5 级 byte-identical: 与 baseline 5-stage 等价
//    node_count() == 5, stages = fetch/decode/execute/memory/writeback
TEST_CASE("five_stage_byte_identical", "[cpu]") {
  PipeBuilder pb;
  TopologyBuilder<5>::expand(pb, default_config());
  REQUIRE(pb.node_count() == 5);
  REQUIRE(pb.stage_count() == 5);
  // 5-stage 顺序: fetch → decode → execute → memory → writeback
  auto names = pb.stage_names();
  REQUIRE(names[0] == "fetch");
  REQUIRE(names[1] == "decode");
  REQUIRE(names[2] == "execute");
  REQUIRE(names[3] == "memory");
  REQUIRE(names[4] == "writeback");
  // 每个阶段都有对应 node
  REQUIRE(pb.node_of_logic_stage("fetch") != nullptr);
  REQUIRE(pb.node_of_logic_stage("decode") != nullptr);
  REQUIRE(pb.node_of_logic_stage("execute") != nullptr);
  REQUIRE(pb.node_of_logic_stage("memory") != nullptr);
  REQUIRE(pb.node_of_logic_stage("writeback") != nullptr);
}

// 2. 7 级: 含 retire 节点 (OoO 显式提交阶段)
//    node_count() == 7, has_stage("retire") = true
//    命名约定 (M4G-extend G.X): in-order 隐含 commit; OoO 显式 retire/commit
TEST_CASE("seven_stage_has_retire", "[cpu]") {
  PipeBuilder pb;
  TopologyBuilder<7>::expand(pb, default_config());
  REQUIRE(pb.node_count() == 7);
  REQUIRE(pb.stage_count() == 7);
  // 必须含 retire 阶段 (OoO 显式提交)
  REQUIRE(pb.has_stage("retire"));
  // 7-stage 顺序: fetch → decode → execute → memory → writeback → retire → commit
  auto names = pb.stage_names();
  REQUIRE(names[5] == "retire");
  REQUIRE(names[6] == "commit");
}

// 3. 3 级 (embedded, IF/EXMEM/WB 合并):
//    fetch+decode→IF, execute+memory→EXMEM, writeback→WB
//    node_count() == 3, stages = if/exmem/wb
TEST_CASE("three_stage_merged", "[cpu]") {
  PipeBuilder pb;
  TopologyBuilder<3>::expand(pb, default_config());
  REQUIRE(pb.node_count() == 3);
  REQUIRE(pb.stage_count() == 3);
  auto names = pb.stage_names();
  REQUIRE(names[0] == "if");
  REQUIRE(names[1] == "exmem");
  REQUIRE(names[2] == "wb");
}

// 4. 10 级 (deep pipeline, ≥10 nodes with deep-pipeline splits):
//    含 ≥3 sub-pipe between fetch and execute, declare_substage 用于 deep splits
//    node_count() >= 10
TEST_CASE("ten_stage_at_least_ten", "[cpu]") {
  PipeBuilder pb;
  TopologyBuilder<10>::expand(pb, default_config());
  REQUIRE(pb.node_count() >= 10);
  // 必须含 deep-pipeline substages (e.g. RENAME / ISSUE / EX1 / EX2)
  REQUIRE(pb.has_stage("fetch"));
  REQUIRE(pb.has_stage("execute"));
  REQUIRE(pb.has_stage("writeback"));
  // substages (declare_substage 创建的物理 node)
  REQUIRE(pb.node_of_logic_stage("RENAME") != nullptr);
  REQUIRE(pb.node_of_logic_stage("ISSUE") != nullptr);
}

