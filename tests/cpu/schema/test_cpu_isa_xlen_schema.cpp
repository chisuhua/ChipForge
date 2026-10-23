// tests/cpu/schema/test_cpu_isa_xlen_schema.cpp
//
// 功能描述: cpu_params_schema.json 的 isa/xlen 契约 + 4 份配置实例的声明值验证
//   (rv64-isa-declaration-correction, ADR-087 §D1/D2)
//
// 覆盖:
//   - schema.isa enum 收窄为 [rv32i, rv32im, rv32imac] (rv64* 移入 description)
//   - schema.isa default = rv32im
//   - schema.xlen 属性存在 (integer / enum [32,64] / default 32)
//   - 4 份配置实例的 isa 均为 rv32im (声明与实现水平一致)
//   - 3 份配置实例的 mmu_mode 均为 sv32 (XLEN=32 下只有 Sv32 合法,
//     且满足 schema 的 isa ∈ {rv32i,rv32im,rv32imac} ⇒ mmu_mode const sv32)
//   - schema 的 isa 条件约束仍然存在 (防止后人误删导致 32-bit 内核声明 sv39)
//
// 变异对照:
//   - 把任一 JSON 的 isa 改回 rv64gc / mmu_mode 改回 sv39 ⇒ 本文件 FAIL
//   - 把 schema.isa enum 加回 rv64i/rv64gc 或删掉 xlen ⇒ 本文件 FAIL

#include "catch_amalgamated.hpp"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

namespace {

const char* kSchemaPath = "ip/cpu/configs/cpu_params_schema.json";

nlohmann::json load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open()) {
    fprintf(stderr, "FAIL: cannot open %s\n", path.c_str());
    return {};
  }
  return nlohmann::json::parse(f);
}

const nlohmann::json& params_props(const nlohmann::json& schema) {
  return schema["properties"]["params"]["properties"];
}

}  // namespace

TEST_CASE("schema isa enum narrowed to implemented set", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  REQUIRE(!schema.is_null());
  const auto& pp = params_props(schema);

  REQUIRE(pp.contains("isa"));
  const auto& isa = pp["isa"];
  std::set<std::string> values;
  for (const auto& v : isa["enum"]) values.insert(v.get<std::string>());
  // 恰好 3 个 implemented + declared-reserved(rv32imac) 值, 不含 rv64*
  REQUIRE(values.size() == 3);
  REQUIRE(values.count("rv32i") == 1);
  REQUIRE(values.count("rv32im") == 1);
  REQUIRE(values.count("rv32imac") == 1);
  REQUIRE(values.count("rv64i") == 0);
  REQUIRE(values.count("rv64gc") == 0);
  REQUIRE(isa["default"] == "rv32im");
  // reserved 值必须仍被文档化 (description), 不得静默消失
  REQUIRE(isa["description"].get<std::string>().find("rv64i") != std::string::npos);
}

TEST_CASE("schema has xlen property", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  REQUIRE(!schema.is_null());
  const auto& pp = params_props(schema);

  REQUIRE(pp.contains("xlen"));
  const auto& xlen = pp["xlen"];
  REQUIRE(xlen["type"] == "integer");
  std::set<int> values;
  for (const auto& v : xlen["enum"]) values.insert(v.get<int>());
  REQUIRE(values.size() == 2);
  REQUIRE(values.count(32) == 1);
  REQUIRE(values.count(64) == 1);
  REQUIRE(xlen["default"] == 32);
}

TEST_CASE("schema keeps isa-conditioned mmu_mode constraint", "[cpu-configs]") {
  auto schema = load_file(kSchemaPath);
  REQUIRE(!schema.is_null());
  // 条件约束位于 properties.params.allOf (不是顶层 allOf)
  const auto& params = schema["properties"]["params"];
  REQUIRE(params.contains("allOf"));
  bool found = false;
  for (const auto& rule : params["allOf"]) {
    if (!rule.contains("if") || !rule["if"].contains("properties")) continue;
    const auto& props = rule["if"]["properties"];
    if (!props.contains("isa")) continue;
    std::set<std::string> isa_values;
    for (const auto& v : props["isa"]["enum"]) {
      isa_values.insert(v.get<std::string>());
    }
    if (isa_values.size() != 3) continue;
    REQUIRE(isa_values.count("rv32i") == 1);
    REQUIRE(isa_values.count("rv32im") == 1);
    REQUIRE(isa_values.count("rv32imac") == 1);
    if (rule.contains("then") && rule["then"].contains("properties")) {
      const auto& then_props = rule["then"]["properties"];
      if (then_props.contains("mmu_mode")) {
        REQUIRE(then_props["mmu_mode"]["const"] == "sv32");
        found = true;
      }
    }
  }
  REQUIRE(found);
}

TEST_CASE("config instances declare rv32im and sv32", "[cpu-configs]") {
  const char* files[] = {"cpu_default", "cpu_deep_pipeline", "cpu_superscalar",
                          "cpu_embedded"};
  for (const char* f : files) {
    const std::string path = std::string("ip/cpu/configs/") + f + ".json";
    auto cfg = load_file(path);
    REQUIRE(!cfg.is_null());
    const auto& params = cfg["params"];
    REQUIRE(params.contains("isa"));
    REQUIRE(params["isa"] == "rv32im");
    // cpu_embedded 无 mmu_mode (无 MMU), 其余 3 份必须是 sv32
    if (params.contains("mmu_mode")) {
      REQUIRE(params["mmu_mode"] == "sv32");
    }
  }
}
