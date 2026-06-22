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
//   - 纯 main() + assert (项目惯例, 无 gtest 依赖)
//   - 验证 schema 结构本身 (含 defaults) + 所有 JSON 实例使用新字段名
//   - 与 ajv 校验互补: 本测试聚焦 schema 字段结构, ajv 验证 JSON 兼容性

#include <cassert>
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

// ----------------------------------------------------------------------------
// Schema 结构验证: 9 新字段存在
// ----------------------------------------------------------------------------
void test_schema_has_9_new_optional_fields() {
  auto schema = load_file(kSchemaPath);
  assert(!schema.is_null());

  const auto& pp = params_props(schema);

  // 9 个新 optional 字段必须全部定义在 params.properties
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
    assert(pp.contains(field));
    assert(pp[field].contains("type"));
    assert(pp[field]["type"] == "integer");
    assert(pp[field].contains("default"));
    // 约束方式: size 类用 minimum=0, width 类用 enum=[1,2,4(,8)]
    const bool has_minimum = pp[field].contains("minimum");
    const bool has_enum = pp[field].contains("enum");
    assert(has_minimum || has_enum);
    if (has_minimum) {
      assert(pp[field]["minimum"] == 0);
    }
  }

  printf("  [PASS] test_schema_has_9_new_optional_fields\n");
}

// ----------------------------------------------------------------------------
// Schema 结构验证: 新字段默认值正确 (1 for width, 0 for size)
// ----------------------------------------------------------------------------
void test_schema_new_field_defaults_correct() {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  // 宽度类字段: default 1 (byte-identical baseline)
  assert(pp["n_lanes"]["default"] == 1);
  assert(pp["dispatch_width"]["default"] == 1);
  assert(pp["retire_width"]["default"] == 1);
  assert(pp["fetch_width"]["default"] == 1);
  assert(pp["commit_width"]["default"] == 1);

  // 容量类字段: default 0 (关闭 OoO 路径, byte-identical baseline)
  assert(pp["issue_queue_size"]["default"] == 0);
  assert(pp["rob_size"]["default"] == 0);
  assert(pp["lsq_size"]["default"] == 0);
  assert(pp["rename_table_size"]["default"] == 0);

  printf("  [PASS] test_schema_new_field_defaults_correct\n");
}

// ----------------------------------------------------------------------------
// Schema 结构验证: 3 个宽度字段 enum 约束
// ----------------------------------------------------------------------------
void test_schema_width_fields_have_enum_constraint() {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  // n_lanes: [1, 2, 4, 8]
  const auto& n_lanes_enum = pp["n_lanes"]["enum"];
  std::set<int> n_lanes_set(n_lanes_enum.begin(), n_lanes_enum.end());
  assert(n_lanes_set.count(1) == 1);
  assert(n_lanes_set.count(2) == 1);
  assert(n_lanes_set.count(4) == 1);
  assert(n_lanes_set.count(8) == 1);

  // dispatch_width / retire_width / fetch_width / commit_width: [1, 2, 4]
  const std::set<std::string> width_fields = {
      "dispatch_width", "retire_width", "fetch_width", "commit_width"};
  for (const auto& f : width_fields) {
    const auto& e = pp[f]["enum"];
    std::set<int> es(e.begin(), e.end());
    assert(es.count(1) == 1);
    assert(es.count(2) == 1);
    assert(es.count(4) == 1);
  }

  printf("  [PASS] test_schema_width_fields_have_enum_constraint\n");
}

// ----------------------------------------------------------------------------
// Schema 重命名验证: 旧字段已删除, 新字段已就位
// ----------------------------------------------------------------------------
void test_schema_renamed_latency_fields() {
  auto schema = load_file(kSchemaPath);
  const auto& pp = params_props(schema);

  // 旧字段已移除 (Phase M4-DSE 引入的 _cycles 后缀)
  assert(!pp.contains("icache_latency_cycles"));
  assert(!pp.contains("dcache_latency_cycles"));

  // 新字段已添加
  assert(pp.contains("icache_latency"));
  assert(pp.contains("dcache_latency"));
  assert(pp["icache_latency"]["type"] == "integer");
  assert(pp["icache_latency"]["default"] == 1);
  assert(pp["icache_latency"]["minimum"] == 0);
  assert(pp["dcache_latency"]["type"] == "integer");
  assert(pp["dcache_latency"]["default"] == 1);
  assert(pp["dcache_latency"]["minimum"] == 0);

  // mul_latency 保持不变 (已与 CpuConfig struct 对齐)
  assert(pp.contains("mul_latency"));
  assert(pp["mul_latency"]["default"] == 1);

  printf("  [PASS] test_schema_renamed_latency_fields\n");
}

// ----------------------------------------------------------------------------
// JSON 实例验证: cpu_default.json 使用新字段名
// ----------------------------------------------------------------------------
void test_cpu_default_json_uses_renamed_fields() {
  auto cfg = load_file(kDefaultJsonPath);
  assert(!cfg.is_null());

  const auto& p = cfg["params"];
  assert(!p.contains("icache_latency_cycles"));
  assert(!p.contains("dcache_latency_cycles"));
  assert(p["icache_latency"] == 1);
  assert(p["dcache_latency"] == 1);

  printf("  [PASS] test_cpu_default_json_uses_renamed_fields\n");
}

void test_cpu_embedded_json_uses_renamed_fields() {
  auto cfg = load_file(kEmbeddedJsonPath);
  assert(!cfg.is_null());

  const auto& p = cfg["params"];
  assert(!p.contains("icache_latency_cycles"));
  assert(!p.contains("dcache_latency_cycles"));
  assert(p["icache_latency"] == 1);
  assert(p["dcache_latency"] == 1);

  printf("  [PASS] test_cpu_embedded_json_uses_renamed_fields\n");
}

void test_cpu_superscalar_json_uses_renamed_fields() {
  auto cfg = load_file(kSuperscalarJsonPath);
  assert(!cfg.is_null());

  const auto& p = cfg["params"];
  assert(!p.contains("icache_latency_cycles"));
  assert(!p.contains("dcache_latency_cycles"));
  assert(p["icache_latency"] == 1);
  assert(p["dcache_latency"] == 2);

  // mul_latency 保留 (M5.14 触发)
  assert(p["mul_latency"] == 3);

  printf("  [PASS] test_cpu_superscalar_json_uses_renamed_fields\n");
}

void test_cpu_deep_pipeline_json_uses_renamed_fields() {
  auto cfg = load_file(kDeepPipelineJsonPath);
  assert(!cfg.is_null());

  const auto& p = cfg["params"];
  assert(!p.contains("icache_latency_cycles"));
  assert(!p.contains("dcache_latency_cycles"));
  assert(p["icache_latency"] == 2);
  assert(p["dcache_latency"] == 2);

  // mul_latency 保留 (M5.14 deep pipeline)
  assert(p["mul_latency"] == 5);

  printf("  [PASS] test_cpu_deep_pipeline_json_uses_renamed_fields\n");
}

// ----------------------------------------------------------------------------
// Schema 严谨性: new fields 都是 optional, 不破坏 baseline 5-stage JSON
// ----------------------------------------------------------------------------
void test_schema_new_fields_do_not_break_5stage_baseline() {
  auto schema = load_file(kSchemaPath);
  auto cfg = load_file(kDefaultJsonPath);

  // 5-stage baseline JSON 不包含任何新字段, 必须仍然可与 schema 兼容
  // (因为所有新字段都是 optional 且有 defaults)
  const auto& pp = params_props(schema);
  const std::set<std::string> new_fields = {
      "n_lanes", "dispatch_width", "issue_queue_size", "rob_size",
      "lsq_size", "rename_table_size", "retire_width",
      "fetch_width", "commit_width"};

  for (const auto& f : new_fields) {
    // 新字段不在 params.required 中
    const auto& required = schema["properties"]["params"]["required"];
    std::set<std::string> required_set(required.begin(), required.end());
    assert(required_set.count(f) == 0);

    // 新字段不在 cpu_default.json 中 (baseline 字节级不变)
    assert(!cfg["params"].contains(f));

    // 新字段 schema 定义有 default
    assert(pp[f].contains("default"));
  }

  printf("  [PASS] test_schema_new_fields_do_not_break_5stage_baseline\n");
}

}  // namespace

int main() {
  printf("=== cpu_params_schema.json M5.19 Extension Tests ===\n");
  test_schema_has_9_new_optional_fields();
  test_schema_new_field_defaults_correct();
  test_schema_width_fields_have_enum_constraint();
  test_schema_renamed_latency_fields();
  test_cpu_default_json_uses_renamed_fields();
  test_cpu_embedded_json_uses_renamed_fields();
  test_cpu_superscalar_json_uses_renamed_fields();
  test_cpu_deep_pipeline_json_uses_renamed_fields();
  test_schema_new_fields_do_not_break_5stage_baseline();
  printf("=== All tests passed ===\n");
  return 0;
}