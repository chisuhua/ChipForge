// tests/soc/test_mmu_minimal_json.cpp (mmu-cache-integration commit 5/9)
//
// 功能描述: soc/mmu_minimal.json 结构验证
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-13
//
// 验证 mmu-cache-integration commit 5 全链拓扑:
//   traffic_gen → mmu (MMUTLMBridge) → l1 (L1CacheTLMBridge) → memory
//
// 范围限制 (mmu-cache-integration Decision 7 + W-4):
//   - 本测试只做结构验证 (top-level fields, modules, connections, params)
//   - 不做 full instantiateAll() 仿真 (RETRY 语义需要 traffic_gen 重发支持, deferred)
//   - 仿真端到端集成推迟到 mmu-cache-integration 后续 change 或 Phase 5
//
// 详见:
//   - openspec/changes/mmu-cache-integration/specs/mmu-soc-minimal-topology/spec.md
//   - soc/mmu_minimal.json

#include "catch_amalgamated.hpp"
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

const char* kSocMmuJsonPath = "soc/mmu_minimal.json";

nlohmann::json load_mmu_soc_json() {
  std::ifstream f(kSocMmuJsonPath);
  REQUIRE(f.is_open());
  return nlohmann::json::parse(f);
}

TEST_CASE("soc_mmu_json_has_required_top_level_fields", "[soc][MMUMinimalJson]") {
  auto j = load_mmu_soc_json();
  REQUIRE(j.contains("name"));
  REQUIRE(j["name"].is_string());
  REQUIRE(j["name"] == "MMUCacheMinimalSoC");
  REQUIRE(j.contains("description"));
  REQUIRE(j.contains("modules"));
  REQUIRE(j["modules"].is_array());
  REQUIRE(j.contains("connections"));
  REQUIRE(j["connections"].is_array());
}

TEST_CASE("soc_mmu_json_modules_match_full_chain_topology", "[soc][MMUMinimalJson]") {
  auto j = load_mmu_soc_json();
  const auto& modules = j["modules"];
  REQUIRE(modules.size() == 4);

  std::set<std::string> expected_types = {
      "TrafficGenTLM", "MMUTLMBridge", "L1CacheTLMBridge", "MemoryTLM"};
  std::set<std::string> expected_names = {"tg", "mmu", "l1", "mem"};

  for (const auto& m : modules) {
    REQUIRE(m.contains("name"));
    REQUIRE(m.contains("type"));
    REQUIRE(m["name"].is_string());
    REQUIRE(m["type"].is_string());
    REQUIRE(expected_names.count(m["name"].get<std::string>()) == 1);
    REQUIRE(expected_types.count(m["type"].get<std::string>()) == 1);
  }
}

TEST_CASE("soc_mmu_json_connections_match_full_chain_pipeline",
          "[soc][MMUMinimalJson]") {
  auto j = load_mmu_soc_json();
  const auto& connections = j["connections"];
  REQUIRE(connections.size() == 3);

  bool found_tg_to_mmu = false;
  bool found_mmu_to_l1 = false;
  bool found_l1_to_mem = false;
  for (const auto& c : connections) {
    REQUIRE(c.contains("src"));
    REQUIRE(c.contains("dst"));
    REQUIRE(c.contains("latency"));
    std::string src = c["src"];
    std::string dst = c["dst"];
    if (src == "tg" && dst == "mmu") found_tg_to_mmu = true;
    if (src == "mmu" && dst == "l1") found_mmu_to_l1 = true;
    if (src == "l1" && dst == "mem") found_l1_to_mem = true;
  }
  REQUIRE(found_tg_to_mmu);
  REQUIRE(found_mmu_to_l1);
  REQUIRE(found_l1_to_mem);
}

TEST_CASE("soc_mmu_json_params_validate_vipt_safe_geometry",
          "[soc][MMUMinimalJson]") {
  auto j = load_mmu_soc_json();
  for (const auto& m : j["modules"]) {
    if (m["type"] == "MMUTLMBridge") {
      REQUIRE(m.contains("params"));
      const auto& p = m["params"];
      REQUIRE(p.contains("sv_mode"));
      REQUIRE(p["sv_mode"] == "sv39");
      REQUIRE(p.contains("levels"));
      REQUIRE(p["levels"].is_array());
      REQUIRE(p["levels"].size() == 2);

      // VIPT-safe 8/8 全关联 (idx=0 + offset=12=12 SAFE)
      for (const auto& lvl : p["levels"]) {
        REQUIRE(lvl["entries"] == 8);
        REQUIRE(lvl["associativity"] == 8);
      }
      REQUIRE(p["ptw_max_inflight"] == 2);
      return;
    }
  }
  INFO("MMUTLMBridge module not found");
  REQUIRE(false);
}
