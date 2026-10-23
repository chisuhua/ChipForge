// tests/cpu/test_cpu_factory_isa_xlen.cpp
// Family: [cpu] — ADR-087 §D1/D2 isa/xlen 契约 (rv64-isa-declaration-correction)
// 覆盖: 三层 fail-fast + declared-reserved 单发 warn (design.md Decision 1/2)
// 变异对照: 删 warn / cerr 改 cout / 删 throw → 本文件至少 1 用例 FAIL
#include "catch_amalgamated.hpp"
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ip/cpu/cpu_factory.h"

using namespace cf::cpu;  // CPUConfig / CpuFactory

namespace {

// 捕获 std::cerr (design D2: declared-reserved warn 走 cerr)
class CerrCapture {
 public:
  CerrCapture() : old_(std::cerr.rdbuf(buf_.rdbuf())) {}
  ~CerrCapture() { std::cerr.rdbuf(old_); }
  std::string str() const { return buf_.str(); }

 private:
  std::ostringstream buf_;
  std::streambuf* old_ = nullptr;
};

}  // namespace

TEST_CASE("CPUConfig default isa/xlen contract", "[cpu][isa-xlen]") {
  CPUConfig cfg;
  REQUIRE(cfg.isa == "rv32im");
  REQUIRE(cfg.xlen == 32);
}

TEST_CASE("unsupported isa fail-fast", "[cpu][isa-xlen]") {
  CPUConfig cfg;
  cfg.isa = "rv128i";
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("implemented isa xlen mismatch fail-fast", "[cpu][isa-xlen]") {
  CPUConfig cfg;  // isa=rv32im (implemented), 显式 xlen=64 冲突
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
}

TEST_CASE("kernel T vs xlen cross-check fail-fast (reserved path)",
          "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv64gc (declared-reserved) + 显式 xlen=64 + T=uint32
  cfg.isa = "rv64gc";
  cfg.xlen = 64;
  REQUIRE_THROWS_AS(CpuFactory<std::uint32_t>::build_cpu(cfg),
                    std::invalid_argument);
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
}

TEST_CASE("declared-reserved accepted with single warning", "[cpu][isa-xlen]") {
  CerrCapture cap;
  CPUConfig cfg;  // isa=rv32imac (declared-reserved), xlen 默认 32 一致
  cfg.isa = "rv32imac";
  REQUIRE_NOTHROW(CpuFactory<std::uint32_t>::build_cpu(cfg));
  REQUIRE(cap.str().find("[declared-reserved]") != std::string::npos);
  // 单发语义: 本用例仅一次装载, warn 只应出现一次
  REQUIRE(cap.str().find("[declared-reserved]") ==
          cap.str().rfind("[declared-reserved]"));
}
