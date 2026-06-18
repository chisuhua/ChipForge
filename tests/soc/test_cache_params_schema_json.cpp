// tests/soc/test_cache_params_schema_json.cpp
//
// 功能描述: ip/cache/configs/params_schema.json 结构验证 (Phase 1.3c)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-10
//
// 验证 L1CachePlugin IP 的 JSON Schema:
//   - 4 核心 param 字段 (num_sets/tag_bits/idx_bits/line_data_bits) required
//   - type const = "l1_cache"
//   - impl_mode enum 包含 TLM_ONLY (Phase 1 仅此模式生效)
//   - params.additionalProperties = false (严格 schema)
//
// 详见:
//   - .omo/drafts/decision-phase-1.3-bridge-2026-06-10.md §3 (D1=C)
//   - ip/cpu/configs/cpu_params_schema.json (参考模板)

#include <cassert>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace {

const char* kSchemaPath = "ip/cache/configs/params_schema.json";

nlohmann::json load_schema() {
  std::ifstream f(kSchemaPath);
  if (!f.is_open()) {
    fprintf(stderr, "FAIL: cannot open %s\n", kSchemaPath);
    return {};
  }
  return nlohmann::json::parse(f);
}

void test_schema_has_required_top_level_fields() {
  auto j = load_schema();
  assert(!j.is_null());
  assert(j.contains("$schema"));
  assert(j["$schema"].get<std::string>() ==
         "http://json-schema.org/draft-07/schema#");
  assert(j.contains("title"));
  assert(j["title"].get<std::string>() == "L1CachePlugin IP Configuration Schema");
  assert(j.contains("type"));
  assert(j["type"] == "object");
  assert(j.contains("properties"));
  assert(j.contains("required"));

  printf("  [PASS] test_schema_has_required_top_level_fields\n");
}

void test_schema_type_const_is_l1_cache() {
  auto j = load_schema();
  const auto& props = j["properties"];
  assert(props.contains("type"));
  assert(props["type"].contains("const"));
  assert(props["type"]["const"] == "l1_cache");

  printf("  [PASS] test_schema_type_const_is_l1_cache\n");
}

void test_schema_impl_mode_enum_includes_tlm_only() {
  auto j = load_schema();
  const auto& props = j["properties"];
  assert(props.contains("impl_mode"));
  const auto& impl_mode = props["impl_mode"];
  assert(impl_mode.contains("enum"));
  const auto& values = impl_mode["enum"];
  bool found_tlm_only = false;
  for (const auto& v : values) {
    if (v == "TLM_ONLY") found_tlm_only = true;
  }
  assert(found_tlm_only);
  assert(impl_mode["default"] == "TLM_ONLY");

  printf("  [PASS] test_schema_impl_mode_enum_includes_tlm_only\n");
}

void test_schema_params_required_4_core_fields() {
  auto j = load_schema();
  const auto& params = j["properties"]["params"];
  assert(params.contains("required"));
  const auto& required = params["required"];
  assert(required.is_array());
  std::set<std::string> required_set(required.begin(), required.end());
  assert(required_set.count("num_sets") == 1);
  assert(required_set.count("tag_bits") == 1);
  assert(required_set.count("idx_bits") == 1);
  assert(required_set.count("line_data_bits") == 1);

  printf("  [PASS] test_schema_params_required_4_core_fields\n");
}

void test_schema_params_strict_no_additional() {
  auto j = load_schema();
  const auto& params = j["properties"]["params"];
  assert(params.contains("additionalProperties"));
  assert(params["additionalProperties"] == false);

  printf("  [PASS] test_schema_params_strict_no_additional\n");
}

void test_schema_param_defaults_match_l1_cache_geometry() {
  auto j = load_schema();
  const auto& params = j["properties"]["params"]["properties"];
  assert(params["num_sets"]["default"] == 256);
  assert(params["tag_bits"]["default"] == 20);
  assert(params["idx_bits"]["default"] == 8);
  assert(params["line_data_bits"]["default"] == 512);

  printf("  [PASS] test_schema_param_defaults_match_l1_cache_geometry\n");
}

}  // namespace

int main() {
  printf("=== ip/cache/configs/params_schema.json Validation Tests (Phase 1.3c) ===\n");
  test_schema_has_required_top_level_fields();
  test_schema_type_const_is_l1_cache();
  test_schema_impl_mode_enum_includes_tlm_only();
  test_schema_params_required_4_core_fields();
  test_schema_params_strict_no_additional();
  test_schema_param_defaults_match_l1_cache_geometry();
  printf("=== All tests passed ===\n");
  return 0;
}