// tests/cpu/schema/test_cpu_config_chmem.cpp
//
// 功能描述: CpuConfigChmem (CH_MEM 配置面, P2) 的契约测试
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-27
//
// 覆盖:
//   - ★ 默认值 == cpu_factory_chmem.h::build_cpu 的默认参数 (防漂移钉)
//   - JSON 解析 (params 嵌套 / 扁平两种形态)
//   - 元字段允许 (name / type / description / _comment ...)
//   - ★ CH_MEM 不支持的字段 ⇒ 拒绝 + 给出原因 (不是一个笼统的 unknown)
//   - 未知字段 (拼写错) ⇒ 拒绝
//   - 类型错 / 值域越界 ⇒ 拒绝
//   - ★ schema 一致性: cpu_params_schema.json 的 x-chmem-supported=false
//     必须在 chmem_rejected_fields() 里有对应原因
//   - cpu_chmem_default.json 可被解析且与默认值一致
//
// 变异对照:
//   - 把 cpu_chmem_default.json 里任一字段改成 TLM-only 名字 ⇒ 本文件 FAIL
//   - 把 chmem_rejected_fields() 里某项删掉 (同时 schema 仍标 false) ⇒ FAIL
//   - 把 CpuConfigChmem 的某个默认值改掉 (与 build_cpu 默认参数脱钩) ⇒ FAIL

#include "catch_amalgamated.hpp"

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "ip/cpu/configs/cpu_config_chmem.h"
#include "ip/cpu/configs/cpu_config_chmem_json.h"

namespace {

const char* kSchemaPath = "ip/cpu/configs/cpu_params_schema.json";
const char* kDefaultConfigPath = "ip/cpu/configs/cpu_chmem_default.json";

std::string read_file(const std::string& path) {
  std::ifstream f(path);
  REQUIRE(f.is_open());
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

}  // namespace

// ============================================================================
// 1. ★ 默认值必须与 build_cpu 的默认参数一致 (防漂移)
//
//    build_cpu 签名默认:  initial_pc=T{0x80000000}, enable_mmu=nullopt(≡false),
//                        mmu_mode=nullopt(≡"bare"), enable_cache=nullopt(≡false),
//                        enable_bp=true, btb_entries=nullopt(≡256),
//                        bp_uncond=true
//    ★ 若 build_cpu 的默认参数变更, MUST 同步本测试与 CpuConfigChmem。
//    ★ **不含 bp_gshare** —— GShare 增量 3 未签批 ⇒ 配置面不暴露其开关。
// ============================================================================
TEST_CASE("chmem config defaults match build_cpu default arguments",
          "[cpu-configs][chmem-config]") {
  const cf::cpu::CpuConfigChmem d;
  CHECK(d.initial_pc == 0x80000000u);
  CHECK(d.enable_mmu == false);
  CHECK(d.mmu_mode == "bare");
  CHECK(d.enable_cache == false);
  CHECK(d.enable_bp == true);
  CHECK(d.btb_entries == 256u);  // btb_entries = nullopt ⇒ value_or(256)
  CHECK(d.bp_uncond == true);
}

// ============================================================================
// 2. 合法 JSON 的两种形态
// ============================================================================
TEST_CASE("chmem config parses nested params form",
          "[cpu-configs][chmem-config]") {
  const std::string text = R"({
    "name": "my_cpu",
    "type": "cpu",
    "params": { "btb_entries": 64, "enable_bp": true }
  })";
  auto r = cf::cpu::parse_cpu_config_chmem_json(text, "<test>");
  REQUIRE(r.has_value());
  CHECK(r->name == "my_cpu");
  CHECK(r->btb_entries == 64u);
  CHECK(r->enable_bp == true);
  // 未提供的字段保持默认
  CHECK(r->bp_uncond == true);
  CHECK(r->initial_pc == 0x80000000u);
}

TEST_CASE("chmem config parses flat form", "[cpu-configs][chmem-config]") {
  const std::string text = R"({"name":"flat","btb_entries":16,"bp_uncond":false})";
  auto r = cf::cpu::parse_cpu_config_chmem_json(text, "<test>");
  REQUIRE(r.has_value());
  CHECK(r->name == "flat");
  CHECK(r->btb_entries == 16u);
  CHECK(r->bp_uncond == false);
}

TEST_CASE("chmem config allows meta fields", "[cpu-configs][chmem-config]") {
  const std::string text = R"({
    "name": "m", "type": "cpu", "description": "d", "_comment": "note",
    "params": {"btb_entries": 32}
  })";
  auto r = cf::cpu::parse_cpu_config_chmem_json(text, "<test>");
  REQUIRE(r.has_value());
  CHECK(r->btb_entries == 32u);
}

// ============================================================================
// 3. ★ TLM-only 字段必须被拒绝, 且原因为非空 (P2 的核心价值)
// ============================================================================
TEST_CASE("chmem config rejects every registered TLM-only field",
          "[cpu-configs][chmem-config]") {
  const auto& rejected = cf::cpu::chmem_rejected_fields();
  REQUIRE(rejected.size() > 20);  // 回归: 表不应被缩水

  for (const auto& [field, reason] : rejected) {
    INFO("field = " << field);
    CHECK_FALSE(reason.empty());  // 每个拒绝都要有原因, 不能是空字符串

    // 该字段单独出现就必须触发拒绝
    const std::string text = R"({"params":{"__F__": 1}})" ;
    std::string t = text;
    t.replace(t.find("__F__"), 5, field);

    auto r = cf::cpu::parse_cpu_config_chmem_json(t, "<test>");
    if (field == "branch_predictor" || field == "mmu_mode" ||
        field == "isa" || field == "mul_impl") {
      // 字符串型字段给 1 会先撞 BadType；只要**不是**成功即算拒绝
      CHECK_FALSE(r.has_value());
      continue;
    }
    CHECK_FALSE(r.has_value());
    if (!r.has_value()) {
      bool found = false;
      for (const auto& e : r.error()) {
        if (e.field == field) {
          found = true;
          CHECK(e.kind == cf::cpu::CpuConfigError::Kind::UnsupportedField);
        }
      }
      CHECK(found);
    }
  }
}

TEST_CASE("chmem config reports all problems at once",
          "[cpu-configs][chmem-config]") {
  // ★ 一次报告全部 (不是首错即返) —— 否则用户要来回改 N 次
  const std::string text = R"({"params":{
      "pipeline_stages": 7, "mul_latency": 5, "icache_latency": 3
  }})";
  auto r = cf::cpu::parse_cpu_config_chmem_json(text, "<test>");
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().size() == 3);
}

TEST_CASE("chmem config rejects unknown field (typo)",
          "[cpu-configs][chmem-config]") {
  auto r = cf::cpu::parse_cpu_config_chmem_json(
      R"({"params":{"btb_entry":64}})", "<test>");  // 少了 s
  REQUIRE_FALSE(r.has_value());
  CHECK(r.error().front().kind ==
        cf::cpu::CpuConfigError::Kind::UnsupportedField);
  CHECK(r.error().front().field == "btb_entry");
}

// ============================================================================
// 4. 类型 / 值域
// ============================================================================
TEST_CASE("chmem config rejects bad types", "[cpu-configs][chmem-config]") {
  SECTION("btb_entries as string") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"btb_entries":"64"}})", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind == cf::cpu::CpuConfigError::Kind::BadType);
  }
  SECTION("enable_bp as number") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"enable_bp":1}})", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind == cf::cpu::CpuConfigError::Kind::BadType);
  }
  SECTION("mmu_mode as boolean") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"mmu_mode":true}})", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind == cf::cpu::CpuConfigError::Kind::BadType);
  }
}

TEST_CASE("chmem config rejects out-of-range values",
          "[cpu-configs][chmem-config]") {
  SECTION("btb_entries not in the legal set") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"btb_entries":100}})", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind ==
          cf::cpu::CpuConfigError::Kind::InvalidValue);
  }
  SECTION("unknown mmu_mode") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"mmu_mode":"sv99"}})", "<test>");
    REQUIRE_FALSE(r.has_value());
  }
  SECTION("misaligned initial_pc") {
    auto r = cf::cpu::parse_cpu_config_chmem_json(
        R"({"params":{"initial_pc":2147483649}})", "<test>");
    REQUIRE_FALSE(r.has_value());
  }
}

TEST_CASE("chmem config rejects malformed json and missing file",
          "[cpu-configs][chmem-config]") {
  SECTION("syntax error") {
    auto r = cf::cpu::parse_cpu_config_chmem_json("{not json", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind ==
          cf::cpu::CpuConfigError::Kind::ParseError);
  }
  SECTION("top-level not an object") {
    auto r = cf::cpu::parse_cpu_config_chmem_json("[1,2,3]", "<test>");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind ==
          cf::cpu::CpuConfigError::Kind::ParseError);
  }
  SECTION("missing file") {
    auto r = cf::cpu::load_cpu_config_chmem("/nonexistent/path.json");
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().front().kind == cf::cpu::CpuConfigError::Kind::FileIO);
  }
}

// ============================================================================
// 5. ★ schema 一致性: schema 的 x-chmem-supported=false ⊆ rejected_fields
//
//    方向性是刻意的: rejected_fields 可以有 schema 未收录的字段 (如 n_threads
//    / satp_ppn 存在于 CPUConfig struct 但 schema 未列), 反之则不允许 ——
//    schema 标了不支持、解析器却接受 = 假绿通道。
// ============================================================================
TEST_CASE("schema x-chmem-supported=false all have a rejection reason",
          "[cpu-configs][chmem-config]") {
  const auto schema = nlohmann::json::parse(read_file(kSchemaPath));
  const auto& props = schema.at("properties").at("params").at("properties");
  const auto& rejected = cf::cpu::chmem_rejected_fields();

  size_t checked = 0;
  for (auto it = props.begin(); it != props.end(); ++it) {
    const std::string field = it.key();
    INFO("field = " << field);
    REQUIRE(it.value().contains("x-chmem-supported"));  // 每个字段都要有标记
    const bool supported = it.value().at("x-chmem-supported").get<bool>();
    if (!supported) {
      ++checked;
      CHECK(rejected.count(field) == 1);
    } else {
      // 支持的字段 MUST NOT 在拒绝表里
      CHECK(rejected.count(field) == 0);
    }
  }
  CHECK(checked == 30);  // 33 字段 − 3 支持
}

TEST_CASE("schema registers the CH_MEM-only fields",
          "[cpu-configs][chmem-config]") {
  const auto schema = nlohmann::json::parse(read_file(kSchemaPath));
  REQUIRE(schema.contains("x-chmem-extra-fields"));
  const auto& extra = schema.at("x-chmem-extra-fields");
  for (const char* f : {"initial_pc", "enable_bp", "bp_uncond"}) {
    INFO("field = " << f);
    CHECK(extra.contains(f));
  }
}

// ============================================================================
// 6. 仓库内示例配置本身必须合法
// ============================================================================
TEST_CASE("cpu_chmem_default.json parses and equals defaults",
          "[cpu-configs][chmem-config]") {
  auto r = cf::cpu::load_cpu_config_chmem(kDefaultConfigPath);
  REQUIRE(r.has_value());
  const cf::cpu::CpuConfigChmem d;
  CHECK(r->initial_pc == d.initial_pc);
  CHECK(r->enable_bp == d.enable_bp);
  CHECK(r->btb_entries == d.btb_entries);
  CHECK(r->bp_uncond == d.bp_uncond);
  CHECK(r->enable_mmu == d.enable_mmu);
  CHECK(r->mmu_mode == d.mmu_mode);
  CHECK(r->enable_cache == d.enable_cache);
}

// ============================================================================
// 7. TLM 的配置实例在 CH_MEM 上 MUST 被拒 (防"直接拿 TLM json 当 CH_MEM 用")
// ============================================================================
TEST_CASE("TLM config instances are rejected on the CH_MEM path",
          "[cpu-configs][chmem-config]") {
  for (const char* p : {"ip/cpu/configs/cpu_default.json",
                        "ip/cpu/configs/cpu_embedded.json",
                        "ip/cpu/configs/cpu_superscalar.json",
                        "ip/cpu/configs/cpu_deep_pipeline.json"}) {
    INFO("file = " << p);
    auto r = cf::cpu::load_cpu_config_chmem(p);
    REQUIRE_FALSE(r.has_value());
    // 至少要有 pipeline_stages 或 branch_predictor 的拒绝记录
    bool saw_reason = false;
    for (const auto& e : r.error()) {
      if (e.kind == cf::cpu::CpuConfigError::Kind::UnsupportedField) {
        saw_reason = true;
        break;
      }
    }
    CHECK(saw_reason);
  }
}
