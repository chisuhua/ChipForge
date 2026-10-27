// ip/cpu/configs/cpu_config_chmem_json.h
//
// 功能描述: CpuConfigChmem 的 JSON 加载/解析 (P2)
// 作者: ChipForge Plugin Team
// 最后修改日期: 2026-10-27
//
// ============================================================================
// 为什么单独一个头文件 (而不是塞进 cpu_config_chmem.h)
// ============================================================================
// nlohmann/json 是重量级头 (~25k 行)。cpu_factory_chmem.h 被 ~10 个目标 include
// (runner / 测试 / 未来的 SoC 装配), 把 nlohmann 拖进去会显著增加编译时间, 且
// 工厂本身**不需要** JSON。
// ⇒ 本文件只被「需要读配置文件」的调用方 include (runner / schema 测试)。
//
// ============================================================================
// ★ 核心纪律: CH_MEM 不支持的字段 ⇒ ERROR (MUST NOT 静默忽略)
// ============================================================================
// 这是 P2 的主要价值。历史教训 (cpu-pipeline-tick-rca F1): load_config() 静默
// 丢弃 8 个字段 ⇒ 配置看起来生效、实际没生效 ⇒ 排查成本极高。
//
// ⇒ 本解析器对 TLM-only 字段**逐字段给出拒绝原因** (不是笼统的 "unknown field"),
//   让「为什么不能配」在错误信息里自解释。
//
// ⇒ 一次报告**全部**问题 (不是首错即返) —— 否则用户要来回改 N 次。
// ============================================================================

#ifndef CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_JSON_H
#define CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_JSON_H

#include <expected>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "ip/cpu/configs/cpu_config_chmem.h"

namespace cf {
namespace cpu {

// ============================================================================
// CpuConfigError —— 加载失败的分类 (调用方据此决定退出码/提示)
// ============================================================================
struct CpuConfigError {
  enum class Kind {
    FileIO,            // 文件打不开
    ParseError,        // JSON 语法错
    UnsupportedField,  // 字段在 CH_MEM 路径上不存在 (TLM-only 或拼写错)
    BadType,           // 字段类型不符 (如 btb_entries 给了字符串)
    InvalidValue,      // 值域越界 (如 btb_entries=100)
  };
  Kind kind;
  std::string message;  // 人类可读, 含字段名与原因
  std::string field;    // 出错字段 (FileIO/ParseError 时为空)

  // ★ 统一走工厂函数 —— 位置初始化 {kind, message, field} 极易把
  //   message 和 field 写反 (2026-10-27 实现时确实写反过, 导致错误信息为空)。
  static CpuConfigError make(Kind k, std::string msg, std::string fld = {}) {
    return CpuConfigError{k, std::move(msg), std::move(fld)};
  }
};

// ============================================================================
// TLM-only 字段 → 拒绝原因
//
// ★ 这里的每一项都对应 cpu_params_schema.json 里 x-chmem-supported=false 的字段。
//   schema 是字段清单的 SSOT; 本表补充「为什么不支持」的人类可读说明。
//   tests/cpu/schema/test_cpu_config_chmem.cpp 有断言钉住两表一致。
// ============================================================================
inline const std::unordered_map<std::string, std::string>&
chmem_rejected_fields() {
  static const std::unordered_map<std::string, std::string> kMap = {
      // ── ISA 形态: 编译期定形, 无运行期旋钮 ──
      {"isa",
       "CH_MEM 固定 rv32im (RV32I + M 扩展编译期编入); RV64/其他扩展未实装 "
       "(ADR-087)"},
      {"xlen",
       "CH_MEM 固定 XLEN=32; RV64 数据通路未实装 (ADR-087 D2)"},
      {"ext_m",
       "M 扩展编译期编入 (固定启用), 无运行期开关"},
      {"ext_zicsr",
       "Zicsr 编译期编入 (固定启用), 无运行期开关"},
      {"ext_a", "A 扩展未实装"},
      {"ext_f", "F 扩展未实装"},
      {"ext_d", "D 扩展未实装"},
      {"ext_zifencei", "Zifencei 未实装"},

      // ── 流水线拓扑: CH_MEM 是组合数据通路, 固定 5 级 ──
      {"pipeline_stages",
       "CH_MEM 是组合数据通路 (无流水线寄存器), 级数固定 5 且不可配; "
       "TLM 侧才有多级展开"},
      {"split_if_id", "CH_MEM 固定 5 级拓扑 (IF/ID 未拆分)"},
      {"merge_ex_mem", "CH_MEM 固定 5 级拓扑 (EX/MEM 未合并)"},

      // ── superscalar: 未实装 ──
      {"n_lanes", "superscalar 未实装 (CH_MEM 固定单发射)"},
      {"dispatch_width", "superscalar 未实装 (CH_MEM 固定单发射)"},
      {"issue_queue_size", "乱序发射未实装 (CH_MEM 无发射队列)"},
      {"rob_size", "乱序提交未实装 (CH_MEM 无 ROB)"},
      {"lsq_size", "乱序访存未实装 (CH_MEM 无 LSQ)"},
      {"rename_table_size", "寄存器重命名未实装 (CH_MEM 无 rename table)"},
      {"retire_width", "superscalar 未实装 (CH_MEM 固定单提交)"},
      {"fetch_width", "superscalar 未实装 (CH_MEM 固定单取指)"},
      {"commit_width", "superscalar 未实装 (CH_MEM 固定单提交)"},
      {"n_threads", "SMT 未实装 (CH_MEM 固定单线程)"},
      {"use_strict_scoreboard", "scoreboard 未实装 (组合数据通路无乱序窗口)"},

      // ── BP: 字段名刻意不同 (能力集不同) ──
      {"branch_predictor",
       "CH_MEM BP 开关面是 enable_bp (bool) + bp_uncond (bool) + btb_entries; "
       "★ GShare (gshare) 未签批 ⇒ 不暴露对应开关 (见 cpu_config_chmem.h 文件头)"},
      {"enable_branch_predictor",
       "CH_MEM 改用 enable_bp (字段名统一到 CH_MEM 的命名面)"},
      {"ghr_bits",
       "GHR 宽度由 btb_entries 推导 (CH_MEM 内硬编码), 不可配"},

      // ── M 扩展实现形态 ──
      {"mul_latency",
       "CH_MEM M 扩展是组合式实现 (1 拍出结果), 无多周期延迟参数; "
       "TLM 侧的 radix-2 多周期语义不同构"},

      // ── Cache / PMP / MMU 细节: 未实装 ──
      {"icache_latency", "Cache CH_MEM 未实装 (enable_cache=true 会 fail-fast)"},
      {"dcache_latency", "Cache CH_MEM 未实装 (enable_cache=true 会 fail-fast)"},
      {"enable_pmp", "PMP 未实装"},
      {"satp_ppn",
       "MMU CH_MEM 尚未实装翻译 (enable_mmu=true 仅 plumbing-only no-op log)"},

      // ── 纯元数据 / TLM 运行参数 ──
      {"clock_freq_mhz",
       "无时钟域概念: elaboration 定形, 不产生时钟频率参数"},
      {"collect_stats",
       "统计信号 (bp_hit_count / bp_miss_count 等) 由 elaboration 期发射, "
       "非配置项"},
  };
  return kMap;
}

// 顶层允许的元字段 (不是 params)
inline bool is_chmem_meta_field(const std::string& k) {
  static const std::unordered_map<std::string, bool> kMeta = {
      {"name", true},      {"type", true},     {"description", true},
      {"$schema", true},   {"info", true},     {"tags", true},
      {"_comment", true},  {"$comment", true},
  };
  return kMeta.count(k) != 0;
}

namespace detail {

inline void append_error(std::vector<CpuConfigError>* errs,
                         CpuConfigError::Kind kind, const std::string& field,
                         const std::string& message) {
  errs->push_back(CpuConfigError::make(kind, message, field));
}

// 读布尔字段 (缺省 ⇒ 不改)
inline bool read_bool(const nlohmann::json& p, const char* key, bool* out,
                      const std::string& src, std::vector<CpuConfigError>* errs) {
  if (!p.contains(key)) return true;
  const auto& v = p.at(key);
  if (!v.is_boolean()) {
    append_error(errs, CpuConfigError::Kind::BadType, key,
                 src + ": field '" + key + "' must be a boolean (got " +
                     std::string(v.type_name()) + ")");
    return false;
  }
  *out = v.get<bool>();
  return true;
}

inline bool read_u32(const nlohmann::json& p, const char* key,
                     std::uint32_t* out, const std::string& src,
                     std::vector<CpuConfigError>* errs) {
  if (!p.contains(key)) return true;
  const auto& v = p.at(key);
  if (!v.is_number_unsigned() && !v.is_number_integer()) {
    append_error(errs, CpuConfigError::Kind::BadType, key,
                 src + ": field '" + key + "' must be an integer (got " +
                     std::string(v.type_name()) + ")");
    return false;
  }
  const auto n = v.get<std::int64_t>();
  if (n < 0 || n > 0xFFFFFFFFll) {
    append_error(errs, CpuConfigError::Kind::InvalidValue, key,
                 src + ": field '" + key + "' out of uint32 range (got " +
                     std::to_string(n) + ")");
    return false;
  }
  *out = static_cast<std::uint32_t>(n);
  return true;
}

inline bool read_string(const nlohmann::json& p, const char* key,
                        std::string* out, const std::string& src,
                        std::vector<CpuConfigError>* errs) {
  if (!p.contains(key)) return true;
  const auto& v = p.at(key);
  if (!v.is_string()) {
    append_error(errs, CpuConfigError::Kind::BadType, key,
                 src + ": field '" + key + "' must be a string (got " +
                     std::string(v.type_name()) + ")");
    return false;
  }
  *out = v.get<std::string>();
  return true;
}

// 遍历并拒绝 CH_MEM 不支持的字段 (一次报告全部)
inline void reject_unsupported(const nlohmann::json& p, const std::string& src,
                               std::vector<CpuConfigError>* errs) {
  const auto& rej = chmem_rejected_fields();
  for (auto it = p.begin(); it != p.end(); ++it) {
    const std::string key = it.key();
    auto r = rej.find(key);
    if (r != rej.end()) {
      append_error(errs, CpuConfigError::Kind::UnsupportedField, key,
                   src + ": field '" + key + "' is not supported on the CH_MEM "
                         "backend -- " + r->second);
      continue;
    }
    // 完全未知 (拼写错 / 新增字段未登记) ⇒ 也拒绝 (不静默忽略)
    // 已登记的 CH_MEM 字段由下方 read_* 消费, 此处只拦「既不是已知该拒的、
    // 也不是本解析器认识的」。
    static const std::unordered_map<std::string, bool> kKnown = {
        {"name", true},          {"initial_pc", true},
        {"enable_bp", true},     {"btb_entries", true},
        {"bp_uncond", true},
        {"enable_mmu", true},    {"mmu_mode", true},
        {"enable_cache", true},
    };
    if (kKnown.count(key) == 0) {
      append_error(errs, CpuConfigError::Kind::UnsupportedField, key,
                   src + ": unknown field '" + key +
                       "' (CH_MEM config supports: name, initial_pc, enable_bp, "
                       "btb_entries, bp_uncond, enable_mmu, "
                       "mmu_mode, enable_cache)");
    }
  }
}

}  // namespace detail

// ============================================================================
// parse_cpu_config_chmem_json —— 从 JSON 文本解析
//
// 支持两种形态:
//   ① {"name": "...", "params": {...}}      (与 ip/cpu/configs/*.json 一致)
//   ② {"name": "...", ...}                  (params 扁平化, 简写)
//
// ★ 失败时返回**全部**错误 (不是首错即返), 便于一次性修正。
// ============================================================================
inline std::expected<CpuConfigChmem, std::vector<CpuConfigError>>
parse_cpu_config_chmem_json(const std::string& text, const std::string& source) {
  std::vector<CpuConfigError> errs;

  nlohmann::json j;
  try {
    j = nlohmann::json::parse(text);
  } catch (const std::exception& e) {
    errs.push_back(CpuConfigError::make(CpuConfigError::Kind::ParseError,
                                        source + ": JSON parse error: " +
                                            e.what()));
    return std::unexpected(errs);
  }
  if (!j.is_object()) {
    errs.push_back(CpuConfigError::make(
        CpuConfigError::Kind::ParseError,
        source + ": top-level JSON must be an object"));
    return std::unexpected(errs);
  }

  CpuConfigChmem cfg;

  // 顶层元字段
  if (j.contains("name") && j.at("name").is_string()) {
    cfg.name = j.at("name").get<std::string>();
  }

  // params 段 (形态 ①) 或顶层 (形态 ②)
  const nlohmann::json* pj = &j;
  if (j.contains("params") && j.at("params").is_object()) {
    pj = &j.at("params");
  }

  // ★ 顶层未知字段 (排除元字段 + params 容器本身)
  for (auto it = j.begin(); it != j.end(); ++it) {
    const std::string k = it.key();
    if (k == "params" || is_chmem_meta_field(k)) continue;
    // 形态 ②: 顶层直接放 params ⇒ 交给下面 reject_unsupported 统一处理
    if (pj == &j) continue;
    detail::append_error(&errs, CpuConfigError::Kind::UnsupportedField, k,
                         source + ": unknown top-level field '" + k + "'");
  }

  // params 段: 先拒绝不支持的, 再读支持的
  detail::reject_unsupported(*pj, source, &errs);

  detail::read_u32(*pj, "initial_pc", &cfg.initial_pc, source, &errs);
  detail::read_bool(*pj, "enable_bp", &cfg.enable_bp, source, &errs);
  detail::read_u32(*pj, "btb_entries", &cfg.btb_entries, source, &errs);
  detail::read_bool(*pj, "bp_uncond", &cfg.bp_uncond, source, &errs);
  detail::read_bool(*pj, "enable_mmu", &cfg.enable_mmu, source, &errs);
  detail::read_string(*pj, "mmu_mode", &cfg.mmu_mode, source, &errs);
  detail::read_bool(*pj, "enable_cache", &cfg.enable_cache, source, &errs);

  if (!errs.empty()) return std::unexpected(errs);

  // 值域校验 (btb_entries 取值 / mmu_mode 枚举 / initial_pc 对齐)
  const std::string verr = validate_cpu_config_chmem(cfg);
  if (!verr.empty()) {
    errs.push_back(CpuConfigError::make(CpuConfigError::Kind::InvalidValue,
                                        source + ": " + verr));
    return std::unexpected(errs);
  }

  return cfg;
}

// ============================================================================
// load_cpu_config_chmem —— 从文件加载
// ============================================================================
inline std::expected<CpuConfigChmem, std::vector<CpuConfigError>>
load_cpu_config_chmem(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    return std::unexpected(std::vector<CpuConfigError>{
        CpuConfigError::make(CpuConfigError::Kind::FileIO,
                             "cannot open config file: " + path)});
  }
  std::ostringstream ss;
  ss << ifs.rdbuf();
  return parse_cpu_config_chmem_json(ss.str(), path);
}

// ============================================================================
// format_cpu_config_errors —— 把错误列表渲染成多行文本 (runner 直接用)
// ============================================================================
inline std::string format_cpu_config_errors(
    const std::vector<CpuConfigError>& errs) {
  std::ostringstream os;
  os << "config error" << (errs.size() > 1 ? "s" : "") << " ("
     << errs.size() << "):\n";
  for (const auto& e : errs) {
    os << "  - " << e.message << "\n";
  }
  os << "  hint: CH_MEM configs accept only: name, initial_pc, enable_bp,\n"
        "        btb_entries, bp_uncond, enable_mmu, mmu_mode,\n"
        "        enable_cache.  See ip/cpu/configs/cpu_chmem_default.json\n";
  return os.str();
}

}  // namespace cpu
}  // namespace cf

#endif  // CF_IP_CPU_CONFIGS_CPU_CONFIG_CHMEM_JSON_H
