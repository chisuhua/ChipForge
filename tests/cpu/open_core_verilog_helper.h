// tests/cpu/open_core_verilog_helper.h
//
// 功能描述: 开放核（`ip/cpu/core/`）测试的**共享 Verilog 产物断言 helper**
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-29
//
// ---------------------------------------------------------------------------
// ★★ 为什么要共享（★ A2.5 收尾，ADR-091 v1.11 §D3a.5 / V-15）：
//
// ★ 原本 `top_port_declared()` 在 `test_cpu_open_core_ibus.cpp` 与
//   `test_cpu_open_core_dmem.cpp` 各有一份**完全相同**的实现。
// ★ ⇒ 若只改一处 ⇒ 两边的断言强度**不一致** ⇒ ★ 漂移风险。
// ★ ⇒ 抽到本文件 ⇒ **精确匹配逻辑只有一份** ⇒ 天然不会漂移。
//
// ---------------------------------------------------------------------------
// ★★★ 核心纪律：顶层端口断言 MUST 用**精确名匹配**（ADR-091 v1.11 V-15）
//
// ★★ **MUST NOT 用子串匹配** —— 原因（★ 实测，见 plan §K21.4）：
//
//     output [31:0] open_pc_1      ← 端口**实际**名（被 CppHDL 改名）
//     REQUIRE(top_port_declared(vpath, "open_pc"));   ← ★ 子串匹配 ⇒ **通过**！
//
// ★ CppHDL codegen 按 `eval_list` 顺序建名表（`codegen_verilog.cpp:64-76`）
//   ⇒ **内部 node 先占名** ⇒ 重名者加 `_N` 后缀。
// ★ ⇒ 当内部 `ch_reg` 与顶层端口**同名**（ADR-091 v1.11 **G-1o** 禁止），
//   端口会被静默改名 ⇒ ★ **子串匹配会掩盖这件事** ⇒ ★ 假绿。
//
// ★ 精确匹配的写法：★ 取端口声明行的**最后一个标识符 token**（= 端口名）
//   ⇒ ★ 逐位比较 ⇒ ★ `open_pc` ≠ `open_pc_1` ⇒ ★ 改名**必被捕获**。

#ifndef CF_TESTS_CPU_OPEN_CORE_VERILOG_HELPER_H
#define CF_TESTS_CPU_OPEN_CORE_VERILOG_HELPER_H

#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

namespace cf_test {
namespace open_core {

/// ★ 在生成的网表里查**顶层端口声明**（★ **精确名匹配** —— 见文件头纪律）
///
/// ★ 端口声明行形态（★ 由 CppHDL codegen 产出）：
///     input default_clock,
///     input  imem_req_ready,
///     input [31:0] imem_rsp_data,
///     output [31:0] imem_req_addr,
///     output  open_pc_1
///
/// ★ 取该行**最后一个标识符 token**（跳过方向关键字与位宽 `[31:0]`），
///   去掉尾逗号 ⇒ ★ 即端口名 ⇒ ★ 逐位比较。
inline bool top_port_declared(const std::string& verilog_path,
                              const std::string& name) {
  std::ifstream f(verilog_path);
  if (!f) return false;
  std::string line;
  while (std::getline(f, line)) {
    if (line.find("input") == std::string::npos &&
        line.find("output") == std::string::npos) {
      continue;
    }
    // ★ 取最后一个标识符 token（★ `[31:0]` 以 `[` 开头 ⇒ 不是标识符 ⇒ 跳过）
    std::istringstream iss(line);
    std::string tok;
    std::string last;
    while (iss >> tok) {
      if (!tok.empty() &&
          (std::isalnum(static_cast<unsigned char>(tok[0])) || tok[0] == '_')) {
        last = tok;
      }
    }
    if (!last.empty() && last.back() == ',') last.pop_back();
    if (last == name) return true;   // ★★ 精确匹配（★ NOT 子串）
  }
  return false;
}

// ★ 注：★ 本 helper **零 CppHDL 依赖**（★ 纯文本处理）⇒ ★ 可被任意 TU
//   include 而无 G-1b（`<ch.hpp>` 多 TU 链接失败）风险。
// ★ ⇒ `read_out`（依赖 `ch::Simulator`）**故意不放这里** ⇒ 留在各测试文件本地。

}  // namespace open_core
}  // namespace cf_test

#endif  // CF_TESTS_CPU_OPEN_CORE_VERILOG_HELPER_H
