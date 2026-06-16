// src/cf_plugin/tests/test_soc_l1_cache_minimal_json.cpp
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

#include <cassert>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace {

const char* kSocJsonPath = "soc/l1_cache_minimal.json";

nlohmann::json load_soc_json() {
  std::ifstream f(kSocJsonPath);
  if (!f.is_open()) {
    fprintf(stderr, "FAIL: cannot open %s\n", kSocJsonPath);
    return {};
  }
  return nlohmann::json::parse(f);
}

void test_soc_json_has_required_top_level_fields() {
  auto j = load_soc_json();
  assert(!j.is_null());
  assert(j.contains("name"));
  assert(j["name"].is_string());
  assert(j["name"] == "L1CacheMinimalSoC");
  assert(j.contains("description"));
  assert(j.contains("modules"));
  assert(j["modules"].is_array());
  assert(j.contains("connections"));
  assert(j["connections"].is_array());

  printf("  [PASS] test_soc_json_has_required_top_level_fields\n");
}

void test_soc_json_modules_match_v2_topology() {
  auto j = load_soc_json();
  const auto& modules = j["modules"];
  assert(modules.size() == 3);

  std::set<std::string> expected_types = {
      "TrafficGenTLM", "L1CacheTLMBridge", "MemoryTLM"};
  std::set<std::string> expected_names = {"tg", "l1", "mem"};

  for (const auto& m : modules) {
    assert(m.contains("name"));
    assert(m.contains("type"));
    assert(m["name"].is_string());
    assert(m["type"].is_string());
    assert(expected_names.count(m["name"].get<std::string>()) == 1);
    assert(expected_types.count(m["type"].get<std::string>()) == 1);
  }

  printf("  [PASS] test_soc_json_modules_match_v2_topology\n");
}

void test_soc_json_connections_match_v2_pipeline() {
  auto j = load_soc_json();
  const auto& connections = j["connections"];
  assert(connections.size() == 2);

  bool found_tg_to_l1 = false;
  bool found_l1_to_mem = false;
  for (const auto& c : connections) {
    assert(c.contains("src"));
    assert(c.contains("dst"));
    assert(c.contains("latency"));
    std::string src = c["src"];
    std::string dst = c["dst"];
    if (src == "tg" && dst == "l1") found_tg_to_l1 = true;
    if (src == "l1" && dst == "mem") found_l1_to_mem = true;
  }
  assert(found_tg_to_l1);
  assert(found_l1_to_mem);

  printf("  [PASS] test_soc_json_connections_match_v2_pipeline\n");
}

void test_soc_json_l1_bridge_params_match_plugin_geometry() {
  auto j = load_soc_json();
  for (const auto& m : j["modules"]) {
    if (m["type"] == "L1CacheTLMBridge") {
      assert(m.contains("params"));
      const auto& p = m["params"];
      assert(p["num_sets"] == 256);
      assert(p["tag_bits"] == 20);
      assert(p["idx_bits"] == 8);
      assert(p["line_data_bits"] == 512);
      return;
    }
  }
  assert(false && "L1CacheTLMBridge module not found");
}

}  // namespace

int main() {
  printf("=== soc/l1_cache_minimal.json Validation Tests (Phase 1.3b) ===\n");
  test_soc_json_has_required_top_level_fields();
  test_soc_json_modules_match_v2_topology();
  test_soc_json_connections_match_v2_pipeline();
  test_soc_json_l1_bridge_params_match_plugin_geometry();
  printf("=== All tests passed ===\n");
  return 0;
}