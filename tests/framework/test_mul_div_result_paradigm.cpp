// tests/framework/test_mul_div_result_paradigm.cpp
//
// 功能描述: MulDivFsmPlugin Result 范式 fail-fast 单元测试 (mfc Phase B.3 + B.4)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-09-30
//
// 测试覆盖 (B.3 RED + B.4 GREEN 共 4 用例):
//   1. mul_div_result_type_alias          — MulDivResult<T> 类型别名存在 (= std::expected<T, PluginError>)
//   2. mul_div_result_ok_static_factory   — ok(value) 工厂返回 ok 状态
//   3. mul_div_result_err_static_factory  — err(PluginError) 工厂返回 err 状态
//   4. mul_div_fsm_build_failfast_passes  — 合法 T = uint32 时, build() 顶部 fail-fast 不抛
//
// 设计:
//   - ADR-047 v0.6 静态配置期错误处理 Result 范式应用到 MulDivFsmPlugin (Phase B.4)
//   - MulDivResult<T> = cf::plugin::Result<T> 类型别名 (避免重复发明 std::expected 包装)
//   - mul_div_fsm_result::ok<T>(value) / err<T>(PluginError) 静态工厂简化调用
//   - build() 顶部 validate_build_preconditions() fail-fast, 失败 throw PluginException
//
// 约束:
//   - 框架已有 cf::plugin::Result<T> (plugin_error.h:56) + PB_TRY/PB_EXPECT/auto_throw 宏 (result_macros.h)
//   - MulDivResult 是 framework Result 的命名空间别名, 不增加新类型
//   - ADR-047 例外清单允许 build() 内部 throw PluginException (build() 业务回调代码自由选择)
//
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <expected>
#include <type_traits>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/plugin_error.h"
#include "cf/plugin/plugin_exception.h"
#include "ip/cpu/arch/riscv/mul_div_fsm.h"

using cf::plugin::PipeBuilder;
using cf::plugin::Phase;
using cf::plugin::PluginError;
using cf::plugin::PluginException;
using cf::plugin::Result;
using cf::cpu::arch::riscv::MulDivFsmPlugin;
using cf::cpu::arch::riscv::mul_div_fsm_result::MulDivResult;
using cf::cpu::arch::riscv::mul_div_fsm_result::ok;
using cf::cpu::arch::riscv::mul_div_fsm_result::err;
using T = std::uint32_t;

// ----------------------------------------------------------------------------
// Test 1: MulDivResult<T> 类型别名存在 (= std::expected<T, PluginError>)
// 编译期 static_assert + 运行时断言
// ----------------------------------------------------------------------------
TEST_CASE("mul_div_result_type_alias", "[framework][result-paradigm]") {
  static_assert(std::is_same_v<MulDivResult<T>, Result<T>>,
                "MulDivResult<T> must be Result<T> alias (= std::expected<T, PluginError>)");
  static_assert(std::is_same_v<MulDivResult<T>, std::expected<T, PluginError>>,
                "MulDivResult<T> must equal std::expected<T, PluginError>");
  REQUIRE(true);  // static_assert 已确保编译期通过
}

// ----------------------------------------------------------------------------
// Test 2: ok(value) 静态工厂返回 ok 状态
// ----------------------------------------------------------------------------
TEST_CASE("mul_div_result_ok_static_factory", "[framework][result-paradigm]") {
  auto r = ok<T>(42u);
  static_assert(std::is_same_v<decltype(r), MulDivResult<T>>);
  REQUIRE(r.has_value());
  REQUIRE(r.value() == 42u);

  // 默认构造 ok (空值) 也应 work
  auto r2 = ok<std::uint8_t>(0xFFu);
  REQUIRE(r2.has_value());
  REQUIRE(r2.value() == 0xFFu);
}

// ----------------------------------------------------------------------------
// Test 3: err(PluginError) 静态工厂返回 err 状态
// ----------------------------------------------------------------------------
TEST_CASE("mul_div_result_err_static_factory", "[framework][result-paradigm]") {
  auto r = err<T>(PluginError::BuildFailed);
  static_assert(std::is_same_v<decltype(r), MulDivResult<T>>);
  REQUIRE_FALSE(r.has_value());
  REQUIRE(r.error() == PluginError::BuildFailed);

  // err<void> 特化
  auto rv = err<void>(PluginError::StageNotFound);
  REQUIRE_FALSE(rv.has_value());
  REQUIRE(rv.error() == PluginError::StageNotFound);
}

// ----------------------------------------------------------------------------
// Test 4: 合法 T = uint32 时, build() 顶部 fail-fast 不抛
// 验证 validate_build_preconditions() 返回 ok(), build() 走完正常路径
// ----------------------------------------------------------------------------
TEST_CASE("mul_div_fsm_build_failfast_passes", "[framework][result-paradigm]") {
  PipeBuilder pb;
  pb.at_stage("execute", Phase::NORMAL, []() {});
  MulDivFsmPlugin<T> mul;

  // 合法 T = uint32 (RV32), build() 顶部 fail-fast 应通过
  REQUIRE_NOTHROW(mul.build(pb));

  // validate_build_preconditions() 通过 friend 或 direct call 验证
  // 由于 validate_build_preconditions() 是 public, 可直接调
  auto vr = mul.validate_build_preconditions();
  REQUIRE(vr.has_value());
  REQUIRE(vr.error() == PluginError::NullPlugin);  // ok 状态, error 字段保留 initial value
}
