// tests/soc/test_soc_l1_cache_minimal_json.cpp
//
// 功能描述: soc/l1_cache_minimal.json 结构验证 (Phase 1.3b)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 验证 v2 决策草案 §4 (D1=C + D1') 拓扑:
//   traffic_gen → l1_cache (L1CacheTLMBridge) → memory
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §3-4
//   - soc/l1_cache_minimal.json (Phase 1.3b 主交付物)

#include "catch_amalgamated.hpp"
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

const char* kSocJsonPath = "soc/l1_cache_minimal.json";

nlohmann::json load_soc_json() {
  std::ifstream f(kSocJsonPath);
  REQUIRE(f.is_open());
  return nlohmann::json::parse(f);
}

TEST_CASE("soc_json_has_required_top_level_fields", "[soc]") {
  auto j = load_soc_json();
  REQUIRE(j.contains("name"));
  REQUIRE(j["name"].is_string());
  REQUIRE(j["name"] == "L1CacheMinimalSoC");
  REQUIRE(j.contains("description"));
  REQUIRE(j.contains("modules"));
  REQUIRE(j["modules"].is_array());
  REQUIRE(j.contains("connections"));
  REQUIRE(j["connections"].is_array());
}

TEST_CASE("soc_json_modules_match_v2_topology", "[soc]") {
  auto j = load_soc_json();
  const auto& modules = j["modules"];
  REQUIRE(modules.size() == 3);

  std::set<std::string> expected_types = {
      "TrafficGenTLM", "L1CacheTLMBridge", "MemoryTLM"};
  std::set<std::string> expected_names = {"tg", "l1", "mem"};

  for (const auto& m : modules) {
    REQUIRE(m.contains("name"));
    REQUIRE(m.contains("type"));
    REQUIRE(m["name"].is_string());
    REQUIRE(m["type"].is_string());
    REQUIRE(expected_names.count(m["name"].get<std::string>()) == 1);
    REQUIRE(expected_types.count(m["type"].get<std::string>()) == 1);
  }
}

TEST_CASE("soc_json_connections_match_v2_pipeline", "[soc]") {
  auto j = load_soc_json();
  const auto& connections = j["connections"];
  REQUIRE(connections.size() == 2);

  bool found_tg_to_l1 = false;
  bool found_l1_to_mem = false;
  for (const auto& c : connections) {
    REQUIRE(c.contains("src"));
    REQUIRE(c.contains("dst"));
    REQUIRE(c.contains("latency"));
    std::string src = c["src"];
    std::string dst = c["dst"];
    if (src == "tg" && dst == "l1") found_tg_to_l1 = true;
    if (src == "l1" && dst == "mem") found_l1_to_mem = true;
  }
  REQUIRE(found_tg_to_l1);
  REQUIRE(found_l1_to_mem);
}

TEST_CASE("soc_json_l1_bridge_params_match_plugin_geometry", "[soc]") {
  auto j = load_soc_json();
  for (const auto& m : j["modules"]) {
    if (m["type"] == "L1CacheTLMBridge") {
      REQUIRE(m.contains("params"));
      const auto& p = m["params"];
      REQUIRE(p["num_sets"] == 256);
      REQUIRE(p["tag_bits"] == 20);
      REQUIRE(p["idx_bits"] == 8);
      REQUIRE(p["line_data_bits"] == 512);
      return;
    }
  }
  REQUIRE(false && "L1CacheTLMBridge module not found");
}