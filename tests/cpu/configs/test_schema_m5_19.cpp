// tests/cpu/configs/test_schema_m5_19.cpp
//
// 功能描述: cpu_params_schema.json 扩展验证 (M5.19, Section 5)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-06-22
//
// 验证 M5.19 Schema 扩展:
//   - 9 个新 optional 字段 (n_lanes, dispatch_width, issue_queue_size, rob_size,
//                          lsq_size, rename_table_size, retire_width,
//                          fetch_width, commit_width)
//   - 3 字段重命名: icache_latency_cycles → icache_latency
//                   dcache_latency_cycles → dcache_latency
//                   mul_latency (保留, 已与 struct 对齐)
//   - 5 个 JSON 实例 (cpu_default/cpu_embedded/cpu_superscalar/cpu_deep_pipeline
//                    + 隐含 schema 自检) 全部使用新字段名
//
// 详见:
//   - .omo/plans/m5-dse-superscalar.md Task 1 (Section 5, M5.19)
//   - openspec/changes/m5-dse-superscalar/design.md Decision 5
//
// 测试策略:
//   - 验证 schema 结构本身 (含 defaults) + 所有 JSON 实例使用新字段名
//   - 与 ajv 校验互补: 本测试聚焦 schema 字段结构, ajv 验证 JSON 兼容性

#include "catch_amalgamated.hpp"
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace {

const char* kSchemaPath = "ip/cpu/configs/cpu_params_schema.json";
const char* kDefaultJsonPath = "ip/cpu/configs/cpu_default.json";
const char* kEmbeddedJsonPath = "ip/cpu/configs/cpu_embedded.json";
const char* kSuperscalarJsonPath = "ip/cpu/configs/cpu_superscalar.json";
const char* kDeepPipelineJsonPath = "ip/cpu/configs/cpu_deep_pipeline.json";

nlohmann::json load_file(const char* path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    fprintf(stderr, "FAIL: cannot open %s\n", path);
    return {};
  }
  return nlohmann::json::parse(f);
}

const nlohmann::json& params_props(const nlohmann::json& schema) {
  return schema["properties"]["params"]["properties"];
}

}  // namespace

// ----------------------------------------------------------------------------
// Schema 结构验证: 9 新字段存在
// ----------------------------------------------------------------------------
TEST_CASE("schema_has_9_new_optional_fields", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  REQUIRE(!schema.is_null());

  const auto& pp = params_props(schema);

  const std::set<std::string> new_fields = {
      "n_lanes",            // M5-DSE 2-wide superscalar (default 1, enum [1,2,4,8])
      "dispatch_width",     // M5-DSE dispatch lane 数 (default 1, enum [1,2,4])
      "issue_queue_size",   // M5-DSE IQ entries (default 0 = 关闭 OoO)
      "rob_size",           // M5-DSE ROB entries (default 0 = 关闭 OoO)
      "lsq_size",           // M5-DSE LSQ entries (default 0 = 关闭 OoO)
      "rename_table_size",  // M5-DSE 重命名表项数 (default 0 = 不 rename)
      "retire_width",       // M5-DSE retire lane 数 (default 1, enum [1,2,4])
      "fetch_width",        // M5-DSE fetch lane 数 (default 1, enum [1,2,4])
      "commit_width",       // M5-DSE commit lane 数 (default 1, enum [1,2,4])
  };

  for (const auto& field : new_fields) {
    REQUIRE(pp.contains(field));
    REQUIRE(pp[field].contains("type"));
    REQUIRE(pp[field]["type"] == "integer");
    REQUIRE(pp[field].contains("default"));
    const bool has_minimum = pp[field].contains("minimum");
    const bool has_enum = pp[field].contains("enum");
    REQUIRE((has_minimum || has_enum));
    if (has_minimum) {
      REQUIRE(pp[field]["minimum"] == 0);
    }
  }
}

// ----------------------------------------------------------------------------
// Schema 结构验证: 新字段默认值正确 (1 for width, 0 for size)
// ----------------------------------------------------------------------------
TEST_CASE("schema_new_field_defaults_correct", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  REQUIRE(pp["n_lanes"]["default"] == 1);
  REQUIRE(pp["dispatch_width"]["default"] == 1);
  REQUIRE(pp["retire_width"]["default"] == 1);
  REQUIRE(pp["fetch_width"]["default"] == 1);
  REQUIRE(pp["commit_width"]["default"] == 1);

  REQUIRE(pp["issue_queue_size"]["default"] == 0);
  REQUIRE(pp["rob_size"]["default"] == 0);
  REQUIRE(pp["lsq_size"]["default"] == 0);
  REQUIRE(pp["rename_table_size"]["default"] == 0);
}

// ----------------------------------------------------------------------------
// Schema 结构验证: 3 个宽度字段 enum 约束
// ----------------------------------------------------------------------------
TEST_CASE("schema_width_fields_have_enum_constraint", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  const auto& n_lanes_enum = pp["n_lanes"]["enum"];
  std::set<int> n_lanes_set(n_lanes_enum.begin(), n_lanes_enum.end());
  REQUIRE(n_lanes_set.count(1) == 1);
  REQUIRE(n_lanes_set.count(2) == 1);
  REQUIRE(n_lanes_set.count(4) == 1);
  REQUIRE(n_lanes_set.count(8) == 1);

  const std::set<std::string> width_fields = {
      "dispatch_width", "retire_width", "fetch_width", "commit_width"};
  for (const auto& f : width_fields) {
    const auto& e = pp[f]["enum"];
    std::set<int> es(e.begin(), e.end());
    REQUIRE(es.count(1) == 1);
    REQUIRE(es.count(2) == 1);
    REQUIRE(es.count(4) == 1);
  }
}

// ----------------------------------------------------------------------------
// Schema 重命名验证: 旧字段已删除, 新字段已就位
// ----------------------------------------------------------------------------
TEST_CASE("schema_renamed_latency_fields", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  REQUIRE(!pp.contains("icache_latency_cycles"));
  REQUIRE(!pp.contains("dcache_latency_cycles"));

  REQUIRE(pp.contains("icache_latency"));
  REQUIRE(pp.contains("dcache_latency"));
  REQUIRE(pp["icache_latency"]["type"] == "integer");
  REQUIRE(pp["icache_latency"]["default"] == 1);
  REQUIRE(pp["icache_latency"]["minimum"] == 0);
  REQUIRE(pp["dcache_latency"]["type"] == "integer");
  REQUIRE(pp["dcache_latency"]["default"] == 1);
  REQUIRE(pp["dcache_latency"]["minimum"] == 0);

  REQUIRE(pp.contains("mul_latency"));
  REQUIRE(pp["mul_latency"]["default"] == 1);
}

// ----------------------------------------------------------------------------
// JSON 实例验证: cpu_default.json 使用新字段名
// ----------------------------------------------------------------------------
TEST_CASE("cpu_default_json_uses_renamed_fields", "[cpu-configs]") {
  auto cfg = load_file(kDefaultJsonPath);
  REQUIRE(!cfg.is_null());

  const auto& p = cfg["params"];
  REQUIRE(!p.contains("icache_latency_cycles"));
  REQUIRE(!p.contains("dcache_latency_cycles"));
  REQUIRE(p["icache_latency"] == 1);
  REQUIRE(p["dcache_latency"] == 1);
}

TEST_CASE("cpu_embedded_json_uses_renamed_fields", "[cpu-configs]") {
  auto cfg = load_file(kEmbeddedJsonPath);
  REQUIRE(!cfg.is_null());

  const auto& p = cfg["params"];
  REQUIRE(!p.contains("icache_latency_cycles"));
  REQUIRE(!p.contains("dcache_latency_cycles"));
  REQUIRE(p["icache_latency"] == 1);
  REQUIRE(p["dcache_latency"] == 1);
}

TEST_CASE("cpu_superscalar_json_uses_renamed_fields", "[cpu-configs]") {
  auto cfg = load_file(kSuperscalarJsonPath);
  REQUIRE(!cfg.is_null());

  const auto& p = cfg["params"];
  REQUIRE(!p.contains("icache_latency_cycles"));
  REQUIRE(!p.contains("dcache_latency_cycles"));
  REQUIRE(p["icache_latency"] == 1);
  REQUIRE(p["dcache_latency"] == 2);

  REQUIRE(p["mul_latency"] == 3);
}

TEST_CASE("cpu_deep_pipeline_json_uses_renamed_fields", "[cpu-configs]") {
  auto cfg = load_file(kDeepPipelineJsonPath);
  REQUIRE(!cfg.is_null());

  const auto& p = cfg["params"];
  REQUIRE(!p.contains("icache_latency_cycles"));
  REQUIRE(!p.contains("dcache_latency_cycles"));
  REQUIRE(p["icache_latency"] == 2);
  REQUIRE(p["dcache_latency"] == 2);

  REQUIRE(p["mul_latency"] == 5);
}

// ----------------------------------------------------------------------------
// Schema 严谨性: new fields 都是 optional, 不破坏 baseline 5-stage JSON
// ----------------------------------------------------------------------------
TEST_CASE("schema_new_fields_do_not_break_5stage_baseline", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  auto cfg = load_file(kDefaultJsonPath);

  const auto& pp = params_props(schema);
  const std::set<std::string> new_fields = {
      "n_lanes", "dispatch_width", "issue_queue_size", "rob_size",
      "lsq_size", "rename_table_size", "retire_width",
      "fetch_width", "commit_width"};

  for (const auto& f : new_fields) {
    const auto& required = schema["properties"]["params"]["required"];
    std::set<std::string> required_set(required.begin(), required.end());
    REQUIRE(required_set.count(f) == 0);

    REQUIRE(!cfg["params"].contains(f));

    REQUIRE(pp[f].contains("default"));
  }
}
