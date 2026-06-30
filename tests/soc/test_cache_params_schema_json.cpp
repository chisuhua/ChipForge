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

#include "catch_amalgamated.hpp"
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

const char* kSchemaPath = "ip/cache/configs/params_schema.json";

nlohmann::json load_schema() {
  std::ifstream f(kSchemaPath);
  REQUIRE(f.is_open());
  return nlohmann::json::parse(f);
}

TEST_CASE("schema_has_required_top_level_fields", "[soc]") {
  auto j = load_schema();
  REQUIRE(j.contains("$schema"));
  REQUIRE(j["$schema"].get<std::string>() ==
         "http://json-schema.org/draft-07/schema#");
  REQUIRE(j.contains("title"));
  REQUIRE(j["title"].get<std::string>() == "L1CachePlugin IP Configuration Schema");
  REQUIRE(j.contains("type"));
  REQUIRE(j["type"] == "object");
  REQUIRE(j.contains("properties"));
  REQUIRE(j.contains("required"));
}

TEST_CASE("schema_type_const_is_l1_cache", "[soc]") {
  auto j = load_schema();
  const auto& props = j["properties"];
  REQUIRE(props.contains("type"));
  REQUIRE(props["type"].contains("const"));
  REQUIRE(props["type"]["const"] == "l1_cache");
}

TEST_CASE("schema_impl_mode_enum_includes_tlm_only", "[soc]") {
  auto j = load_schema();
  const auto& props = j["properties"];
  REQUIRE(props.contains("impl_mode"));
  const auto& impl_mode = props["impl_mode"];
  REQUIRE(impl_mode.contains("enum"));
  const auto& values = impl_mode["enum"];
  bool found_tlm_only = false;
  for (const auto& v : values) {
    if (v == "TLM_ONLY") found_tlm_only = true;
  }
  REQUIRE(found_tlm_only);
  REQUIRE(impl_mode["default"] == "TLM_ONLY");
}

TEST_CASE("schema_params_required_4_core_fields", "[soc]") {
  auto j = load_schema();
  const auto& params = j["properties"]["params"];
  REQUIRE(params.contains("required"));
  const auto& required = params["required"];
  REQUIRE(required.is_array());
  std::set<std::string> required_set(required.begin(), required.end());
  REQUIRE(required_set.count("num_sets") == 1);
  REQUIRE(required_set.count("tag_bits") == 1);
  REQUIRE(required_set.count("idx_bits") == 1);
  REQUIRE(required_set.count("line_data_bits") == 1);
}

TEST_CASE("schema_params_strict_no_additional", "[soc]") {
  auto j = load_schema();
  const auto& params = j["properties"]["params"];
  REQUIRE(params.contains("additionalProperties"));
  REQUIRE(params["additionalProperties"] == false);
}

TEST_CASE("schema_param_defaults_match_l1_cache_geometry", "[soc]") {
  auto j = load_schema();
  const auto& params = j["properties"]["params"]["properties"];
  REQUIRE(params["num_sets"]["default"] == 256);
  REQUIRE(params["tag_bits"]["default"] == 20);
  REQUIRE(params["idx_bits"]["default"] == 8);
  REQUIRE(params["line_data_bits"]["default"] == 512);
}