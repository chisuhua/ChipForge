// tools/verilator_runner/cpu_verilator_sim.cpp
//
// Phase 6d.5 E8 standalone Verilator runner for the 5-stage CH_MEM CPU.
//
// Flow:
//   1. Load vendored ELF bytes (same PT_LOAD routing as the CppHDL Simulator
//      harness: exec segments -> imem, writable segments -> dmem).
//   2. Build the CPU via CpuFactoryChmem<ch_uint<32>> (preload_elf=true) and
//      emit the Verilog to --verilog (default /tmp/cpu.v). CppHDL codegen
//      now emits ch_mem arrays + read/write ports + literal assignments
//      (ADR-035 R8 memory backdoor), so the Verilog is self-contained.
//   3. Compile it with `verilator --cc --public-flat-rw` and link the model
//      together with sim_main.cpp into a dlopen-able libVtop.so.
//   4. dlopen libVtop.so, run cf_run_sim() (reset pulse + clock loop) and
//      report TOHOST/CYCLES. Exit 0 on tohost=1, 1 otherwise.

#ifdef CF_PLUGIN_USE_CH_MEM

#include <cstdint>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <string>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <core/uint.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/cpu_factory_chmem.h"
// P2 (2026-10-27): JSON 配置面。★ 只在本文件 include —— 工厂头不拖 nlohmann。
#include "ip/cpu/configs/cpu_config_chmem_json.h"

namespace {

constexpr std::uint32_t kElfBase = 0x80000000;
constexpr std::uint32_t kDefaultMaxCycles = 2000;

std::vector<std::uint8_t> load_elf_file(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) return {};
  f.seekg(0, std::ios::end);
  std::streamsize n = f.tellg();
  f.seekg(0, std::ios::beg);
  if (n <= 0) return {};
  std::vector<std::uint8_t> buf(static_cast<std::size_t>(n));
  f.read(reinterpret_cast<char*>(buf.data()), n);
  return buf;
}

bool run_shell(const std::string& cmd) {
  return std::system(cmd.c_str()) == 0;
}

std::string verilator_bin() {
  const char* env = std::getenv("VERILATOR_BIN");
  if (env && *env) return env;
#ifdef CF_VERILATOR_BIN
  {
    std::ifstream f(CF_VERILATOR_BIN);
    if (f.is_open()) return CF_VERILATOR_BIN;
  }
#endif
  const char* roots[] = {
      "/workspace/main/opt/verilator/bin/verilator",
      "/usr/local/bin/verilator",
  };
  for (const char* r : roots) {
    std::ifstream f(r);
    if (f.is_open()) return r;
  }
  return "verilator";
}

std::string verilator_include_dir(const std::string& bin) {
  std::string dir = bin;
  auto pos = dir.rfind('/');
  if (pos != std::string::npos) dir = dir.substr(0, pos);  // .../bin
  pos = dir.rfind('/');
  if (pos != std::string::npos) dir = dir.substr(0, pos);  // .../opt/verilator
  std::string candidate = dir + "/share/verilator/include";
  std::ifstream f(candidate + "/verilated.h");
  if (f.is_open()) return candidate;
  const char* env = std::getenv("VERILATOR_ROOT");
  if (env && *env) return std::string(env) + "/include";
  return "/workspace/main/opt/verilator/share/verilator/include";
}

struct Options {
  std::string elf_path;
  std::string verilog_path = "/tmp/cpu.v";
  std::string work_dir = "/tmp/cpu_vl_sim";
  std::uint32_t max_cycles = kDefaultMaxCycles;
  // Phase 6d P1/P2 plumbing-only hooks. See change
  // verilator-cpu-factory-extensible-params for scope. These flags forward
  // through to CpuFactoryChmem::build_cpu's new std::optional parameters.
  // --enable-mmu / --mmu-mode are no-op stubs at the CH_MEM MMU layer
  // (mmu_chmem.h not implemented); --enable-cache fail-fast throws because
  // L1Cache CH_MEM is unimplemented (refer to wave4 P2 placeholder change).
  bool enable_mmu = false;
  std::string mmu_mode = "bare";
  bool enable_cache = false;
  // chmem-branch-microbench-fix: per-cycle PC-value histogram + pc_changes.
  // ⚠️ NOT a fetch/retire count (the sampled PC stream contains values that
  //    are not on the architectural path; mechanism unconfirmed).
  // --stats            print PC_HIST lines (one per sampled PC value)
  // --stats-base <hex> histogram window base (default = ELF base)
  // --stats-len  <n>   number of 4-byte slots (default 256)
  bool stats = false;
  std::uint32_t stats_base = 0x80000000u;
  std::uint32_t stats_len = 256;
  // chmem-bp-btb-gshare-poc Task 1/4: BP 开关面
  //  - --no-bp: 关闭 BP（默认**开**）★ MUST NOT 保留 --enable-bp（与默认开冗余
  //    ⇒ 会产生「两者同给时谁优先」的歧义）
  //  - --btb-entries N: 逻辑条目数 M ∈ {16,32,64,128,256}（越界 fail-fast）
  bool enable_bp = true;
  std::uint32_t btb_entries = 256;
  std::string json_path;      // "" => no JSON fragment
  int reset_pulse = 1;        // 1 = legacy (reset for 1 cycle); 0 = parity mode
  // chmem-bp-btb-gshare ② 主体增量 1: `jal`/`j` 建 BTB 项（恒 taken 预测）
  //  - bp_uncond: 默认 true；--no-bp-uncond 关掉 ⇒ 用于「量收益」对比
  //    （② 主体 Task 2h 的「逐档独立测量」）
  bool bp_uncond = true;

  // ── P2 (2026-10-27) JSON 配置面 ──
  // --config <path> 与下列配置类 flag **互斥**（静默覆盖 = 假绿通道）：
  //   --enable-mmu / --mmu-mode / --enable-cache / --no-bp /
  //   --no-bp-uncond / --btb-entries
  std::string config_path;
  std::vector<std::string> config_flags_seen;  // 被显式设置的配置类 flag
};

bool parse_args(int argc, char** argv, Options& opts) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--config" && i + 1 < argc) {
      opts.config_path = argv[++i];
    } else if (a == "--elf" && i + 1 < argc) {
      opts.elf_path = argv[++i];
    } else if (a == "--verilog" && i + 1 < argc) {
      opts.verilog_path = argv[++i];
    } else if (a == "--work-dir" && i + 1 < argc) {
      opts.work_dir = argv[++i];
    } else if (a == "--cycles" && i + 1 < argc) {
      opts.max_cycles = static_cast<std::uint32_t>(std::atoi(argv[++i]));
    } else if (a == "--enable-mmu") {
      opts.enable_mmu = true;
      opts.config_flags_seen.push_back(a);
    } else if (a == "--mmu-mode" && i + 1 < argc) {
      opts.mmu_mode = argv[++i];
      opts.config_flags_seen.push_back(a);
    } else if (a == "--enable-cache") {
      opts.enable_cache = true;
      opts.config_flags_seen.push_back(a);
    } else if (a == "--json" && i + 1 < argc) {
      opts.json_path = argv[++i];
    } else if (a == "--reset-pulse" && i + 1 < argc) {
      const int v = std::atoi(argv[++i]);
      if (v != 0 && v != 1) {
        std::fprintf(stderr, "--reset-pulse expects 0 or 1 (got %d)\n", v);
        return false;
      }
      opts.reset_pulse = v;
    } else if (a == "--stats") {
      opts.stats = true;
    } else if (a == "--stats-base" && i + 1 < argc) {
      opts.stats_base = static_cast<std::uint32_t>(
          std::strtoul(argv[++i], nullptr, 0));
    } else if (a == "--stats-len" && i + 1 < argc) {
      opts.stats_len = static_cast<std::uint32_t>(std::atoi(argv[++i]));
    } else if (a == "--no-bp") {
      opts.enable_bp = false;
      opts.config_flags_seen.push_back(a);
    } else if (a == "--no-bp-uncond") {
      // ★ 增量 1 开关（② 主体 Task 2h）：关掉 `jal`/`j` 建项 ⇒ 回到增量 1 之前
      opts.bp_uncond = false;
      opts.config_flags_seen.push_back(a);
    } else if (a == "--btb-entries" && i + 1 < argc) {
      // ★ N2: `atoi` 容忍尾串（`16abc` ⇒ 静默取 16）⇒ MUST 校验完整解析
      const char* num = argv[++i];
      char* end = nullptr;
      const unsigned long v = std::strtoul(num, &end, 10);
      if (end == num || (end != nullptr && *end != '\0') || v > 0xFFFFFFFFul) {
        std::fprintf(stderr, "--btb-entries expects an integer (got '%s')\n", num);
        return false;
      }
      opts.btb_entries = static_cast<std::uint32_t>(v);
      opts.config_flags_seen.push_back(a);
    } else if (a == "--help" || a == "-h") {
      std::printf("usage: %s --elf <path> [flags]\n", argv[0]);
      std::printf("  --elf            vendored rv32ui-p-* ELF path (required)\n");
      std::printf("  --verilog        output Verilog path (default /tmp/cpu.v)\n");
      std::printf("  --work-dir       dir for verilator artifacts (default /tmp/cpu_vl_sim)\n");
      std::printf("  --cycles         max sim cycles (default %u)\n",
                  kDefaultMaxCycles);
      std::printf("  --enable-mmu     emit plumbing-only no-op MMU hook (CH_MEM stub)\n");
      std::printf("  --mmu-mode <s>   mmu mode string for the log only (default \"bare\")\n");
      std::printf("  --enable-cache   fail-fast: L1Cache CH_MEM not implemented\n");
      std::printf("  --json <path>    write a parity observation fragment (A2a inc-2)\n");
  std::printf("  --reset-pulse <0|1>  reset for 1 cycle before running (default 1;\n");
  std::printf("                   --reset-pulse=0 mirrors the Simulator baseline, contract S-3)\n");
  std::printf("  --stats          print PC_HIST lines (per-cycle PC value histogram,\n");
      std::printf("                   INCLUDING speculative fetches - NOT a fetch/retire count)\n");
      std::printf("  --stats-base <h> histogram window base (default 0x80000000)\n");
      std::printf("  --stats-len <n>  histogram slots of 4 bytes (default 256)\n");
      std::printf("  --no-bp          disable the branch predictor (default: enabled)\n");
      std::printf("  --no-bp-uncond  disable jal/j BTB entries (increment-1 switch)\n");
      std::printf("  --btb-entries <n> BP logical entries N in {16,32,64,128,256}\n");
  std::printf("  --config <path> load CPU config from JSON (CH_MEM schema;\n");
  std::printf("                  mutually exclusive with the config flags above;\n");
  std::printf("                  see ip/cpu/configs/cpu_chmem_default.json)\n");
      std::printf("\n");
      std::printf("Note: --enable-cache requires L1Cache CH_MEM (see change\n");
      std::printf("      verilator-l1cache-e2e-coverage in wave4 P2 placeholder).\n");
      std::printf("      --enable-mmu is a no-op log; real sv32 translation is not\n");
      std::printf("      delivered by this runner (mmu_chmem.h not yet implemented).\n");
      return false;
    } else {
      std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
      return false;
    }
  }

  // P2 (2026-10-27): --config 与配置类 flag 互斥。
  // ★ 静默择一 = 假绿通道（"配了 config 以为生效，其实被后面的 flag 覆盖"）。
  if (!opts.config_path.empty() && !opts.config_flags_seen.empty()) {
    std::fprintf(stderr,
                 "--config is mutually exclusive with config flags "
                 "(got --config +");
    for (const auto& f : opts.config_flags_seen) {
      std::fprintf(stderr, " %s", f.c_str());
    }
    std::fprintf(stderr,
                 ")\n  Move those settings into the JSON file instead.\n");
    return false;
  }
  return !opts.elf_path.empty();
}

// Determinism fingerprint of the elaboration (contract S-2): identical on the
// Simulator side (tools/parity_runner) because both drivers elaborate the same
// build_cpu configuration. Note this is taken from the DAG in THIS process —
// not from the emitted netlist — so the two sides are directly comparable.
struct Determinism {
  std::uint64_t node_count = 0;
  std::uint64_t names_hash = 0;
};

namespace {
constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
std::uint64_t det_hash_names(std::vector<std::string> names) {
  std::sort(names.begin(), names.end());
  std::uint64_t h = kFnvOffset;
  for (const auto& n : names) {
    for (char c : n) h = (h ^ static_cast<std::uint8_t>(c)) * kFnvPrime;
    h = (h ^ 0xFFu) * kFnvPrime;  // separator (same as parity_sim_runner)
  }
  return h;
}
}  // namespace

// Build the CH_MEM CPU with the vendored ELF preloaded and emit Verilog.
bool generate_verilog(const Options& opts, const cf::cpu::CpuConfigChmem& cfg,
                      const std::vector<std::uint8_t>& elf, Determinism* det) {
  ch::core::context ctx("cpu_verilator_sim");
  ch::core::ctx_swap guard(&ctx);
  // P2 (2026-10-27): 统一走 config 重载 —— 无论配置来自 JSON 还是 CLI,
  //   进入构建的唯一入口是 CpuConfigChmem（单一配置面, 不会出现
  //   "JSON 解析了但 build_cpu 用的还是 CLI 值" 的双轨）。
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(&ctx, cfg, elf);
  if (!pb) {
    std::fprintf(stderr, "build_cpu failed\n");
    return false;
  }
  // ch::toVerilog does not create parent dirs; build_libvtop's mkdir comes
  // after the read, so top.v would silently not exist on a fresh --work-dir.
  std::string mkdir_work = "mkdir -p " + opts.work_dir;
  if (!run_shell(mkdir_work)) return false;
  pb->elaborate(ctx);
  if (det) {
    std::vector<std::string> names;
    names.reserve(ctx.get_nodes().size());
    for (const auto& node : ctx.get_nodes()) {
      if (node) names.push_back(node->name());
    }
    det->node_count = static_cast<std::uint64_t>(names.size());
    det->names_hash = det_hash_names(std::move(names));
  }
  pb->to_verilog(opts.verilog_path);
  return true;
}

// Compile top.v + sim_main.cpp into work_dir/obj_dir + libVtop.so
// (CppHDL VerilatorBackend two-phase recipe: verilator --cc then g++ -shared).
bool build_libvtop(const Options& opts) {
  // ★ BP 统计信号的读取**必须条件编译**（sim_main.cpp 的 CF_BP_STATS_READ）:
  //   BP 未注册（--no-bp）⇒ 生成的 Verilog 无 bp_*_count 信号 ⇒ 引用即编译失败。
  const bool bp_stats = opts.enable_bp;
  std::string bin = verilator_bin();
  std::string inc = verilator_include_dir(bin);
  std::string work = opts.work_dir;
  std::string src = __FILE__;
  auto slash = src.rfind('/');
  std::string harness_dir =
      slash != std::string::npos ? src.substr(0, slash) : "tools/verilator_runner";
  std::string harness = harness_dir + "/sim_main.cpp";

  std::string mkdir = "mkdir -p " + work + "/obj_dir";
  if (!run_shell(mkdir)) return false;

  std::string cc =
      "cd " + work + " && " + bin +
      " --cc --public-flat-rw -Mdir obj_dir -j 0 -Wno-WIDTH -Wno-UNOPTFLAT "
      "--top-module top " + opts.verilog_path + " 2>&1";
  if (!run_shell(cc)) {
    std::fprintf(stderr, "verilator --cc failed\n");
    return false;
  }

  std::string link =
      "cd " + work + "/obj_dir && g++ -shared -fPIC -std=c++17 " +
      (bp_stats ? "-DCF_BP_STATS_READ " : "") + "-o libVtop.so " +
      harness + " ./*.cpp " + inc + "/verilated.cpp " + inc +
      "/verilated_threads.cpp -I. -I" + inc + " -I" + inc +
      "/vltstd -L" + inc + " -pthread -lz 2>&1";
  if (!run_shell(link)) {
    std::fprintf(stderr, "g++ -shared failed\n");
    return false;
  }
  return true;
}

}  // anonymous namespace

int main(int argc, char** argv) {
  Options opts;
  if (!parse_args(argc, argv, opts)) return 2;

  // ── BP 开关面的合法性检查 (fail-fast; 不得静默 clamp) ────────────────
  // ★ N3: `--no-bp` 时 `--btb-entries` 无意义 ⇒ 不校验（否则「明明关了还报错」）
  // ── P2 (2026-10-27): 配置来源 —— JSON 或 CLI, 二者互斥 ──
  cf::cpu::CpuConfigChmem cfg;
  if (!opts.config_path.empty()) {
    auto loaded = cf::cpu::load_cpu_config_chmem(opts.config_path);
    if (!loaded) {
      std::fprintf(stderr, "%s",
                   cf::cpu::format_cpu_config_errors(loaded.error()).c_str());
      return 2;
    }
    cfg = *loaded;
    std::fprintf(stderr, "CONFIG=%s name=%s bp=%d btb=%u uncond=%d\n",
                 opts.config_path.c_str(), cfg.name.c_str(),
                 static_cast<int>(cfg.enable_bp), cfg.btb_entries,
                 static_cast<int>(cfg.bp_uncond));
  } else {
    // CLI 路径: 与 P2 之前的行为**逐位等价**
    cfg.initial_pc   = kElfBase;
    cfg.enable_mmu   = opts.enable_mmu;
    cfg.mmu_mode     = opts.mmu_mode;
    cfg.enable_cache = opts.enable_cache;
    cfg.enable_bp    = opts.enable_bp;
    cfg.btb_entries  = opts.btb_entries;
    cfg.bp_uncond    = opts.bp_uncond;
  }

  // P2: 收敛后的配置统一校验（CLI 路径也需要 —— config 路径在 load 时已校验）。
  //   ★ 早于任何构建动作 ⇒ 干净返回 2，而不是在 build_cpu 里抛异常。
  {
    const std::string verr = cf::cpu::validate_cpu_config_chmem(cfg);
    if (!verr.empty()) {
      std::fprintf(stderr, "invalid CPU config: %s\n", verr.c_str());
      return 2;
    }
  }

  // ★ 生效值回显 (stderr) ⇒ 测量脚本据此断言「flag 确实生效」
  //   ★ P2 起回显的是 **cfg 的生效值**（JSON 或 CLI 收敛后的唯一真相），
  //     不再是 CLI 默认值 —— 否则走 --config 时会回显一串与事实无关的数字。
  // ★ N4: 关态**不**回显 entries（否则易被脚本误读为「生效值」）
  if (cfg.enable_bp) {
    std::fprintf(stderr, "BP=on btb_entries=%u bp_uncond=%s\n",
                 cfg.btb_entries, cfg.bp_uncond ? "on" : "off");
  } else {
    std::fprintf(stderr, "BP=off\n");
  }

  auto elf = load_elf_file(opts.elf_path);
  if (elf.size() < 52 || elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' ||
      elf[3] != 'F') {
    std::fprintf(stderr, "failed to load ELF: %s\n", opts.elf_path.c_str());
    return 2;
  }
  std::string elf_name = opts.elf_path;
  auto p = elf_name.rfind('/');
  if (p != std::string::npos) elf_name = elf_name.substr(p + 1);

  Determinism det;
  if (!generate_verilog(opts, cfg, elf, &det)) return 1;
  if (!build_libvtop(opts)) return 1;

  std::string so = opts.work_dir + "/obj_dir/libVtop.so";
  void* handle = dlopen(so.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    std::fprintf(stderr, "dlopen %s failed: %s\n", so.c_str(), dlerror());
    return 1;
  }
  auto* new_vtop = reinterpret_cast<void* (*)()>(dlsym(handle, "cf_new_vtop"));
  auto* run_sim = reinterpret_cast<int (*)(void*, std::uint32_t, const char*)>(
      dlsym(handle, "cf_run_sim"));
  auto* delete_vtop =
      reinterpret_cast<void (*)(void*)>(dlsym(handle, "cf_delete_vtop"));
  if (!new_vtop || !run_sim || !delete_vtop) {
    std::fprintf(stderr, "dlsym failed: %s\n", dlerror());
    dlclose(handle);
    return 1;
  }

  void* top = new_vtop();
  int rc;

  // ── Parity path (A2a inc-2): used when a JSON fragment is requested or when
  //    reset_mode must match the Simulator baseline (--reset-pulse=0).
  //    The legacy paths below stay byte-identical so the 48-ELF baseline is
  //    not perturbed.
  if (opts.reset_pulse == 0) {
    // ★ Measured 2026-10-26: the emitted Verilog loads ch_reg init values ONLY
    //   on the reset edge (`else pc <= 32'h80000000;`). Skipping the pulse
    //   therefore starts PC at 0 (x30 becomes 0x1000 instead of 0x80001000) --
    //   cycles can still "match" because the dmem window is base-agnostic.
    //   => reset_pulse=0 is NOT parity-compatible; keep it as a diagnostic.
    std::fprintf(stderr,
                 "WARNING: --reset-pulse 0 skips ch_reg initialisation (Verilog "
                 "loads init only on the reset edge) => NOT parity-compatible\n");
  }

  if (!opts.json_path.empty() || opts.reset_pulse == 0) {
    struct CfParityObs {
      std::uint64_t tohost;
      std::uint64_t cycles;
      std::uint64_t regs_hash;
      std::uint64_t pchash;
      std::uint64_t pc_samples;
      std::uint32_t timed_out;
      std::uint32_t pc_head[8];
      std::uint32_t regs_raw[31];
      std::uint64_t mem_sig;
      std::uint64_t mem_sig_updates;
    };
    auto* run_parity = reinterpret_cast<int (*)(void*, std::uint32_t, int,
                                                CfParityObs*)>(
        dlsym(handle, "cf_run_sim_parity"));
    if (!run_parity) {
      std::fprintf(stderr, "dlsym cf_run_sim_parity failed: %s\n", dlerror());
      delete_vtop(top);
      dlclose(handle);
      return 1;
    }
    CfParityObs obs{};
    rc = run_parity(top, opts.max_cycles, opts.reset_pulse, &obs);
    delete_vtop(top);
    dlclose(handle);

    char hex_regs[24], hex_pc[24], hex_det[24];
    std::snprintf(hex_regs, sizeof(hex_regs), "%016llx",
                  static_cast<unsigned long long>(obs.regs_hash));
    std::snprintf(hex_pc, sizeof(hex_pc), "%016llx",
                  static_cast<unsigned long long>(obs.pchash));
    std::snprintf(hex_det, sizeof(hex_det), "%016llx",
                  static_cast<unsigned long long>(det.names_hash));

    std::string json;
    json += "{\n";
    json += "  \"backend\": \"verilator\",\n";
    json += "  \"elf\": \"" + elf_name + "\",\n";
    json += "  \"reset_mode\": \"" +
            std::string(opts.reset_pulse ? "pulse1" : "none") + "\",\n";
    json += "  \"determinism\": { \"node_count\": " +
            std::to_string(det.node_count) + ", \"names_hash\": \"" + hex_det +
            "\" },\n";
    json += "  \"observations\": {\n";
    json += "    \"tohost\": " + std::to_string(obs.tohost) + ",\n";
    json += "    \"regs\": \"" + std::string(hex_regs) + "\",\n";
    json += "    \"cycles\": " + std::to_string(obs.cycles) + ",\n";
    json += "    \"pchash\": \"" + std::string(hex_pc) + "\",\n";
    json += "    \"pc_samples\": " + std::to_string(obs.pc_samples) + ",\n";
    char hex_sig[24];
    std::snprintf(hex_sig, sizeof(hex_sig), "%016llx",
                  static_cast<unsigned long long>(obs.mem_sig));
    json += "    \"memsig\": \"" + std::string(hex_sig) + "\",\n";
    json += "    \"memsig_updates\": " + std::to_string(obs.mem_sig_updates) + "\n";
    json += "  },\n";
    json += "  \"pc_head\": [";
    for (int i = 0; i < 8; ++i) {
      json += (i ? ", " : "") + std::to_string(obs.pc_head[i]);
    }
    json += "],\n";
    json += "  \"regs_raw\": [";
    for (int i = 0; i < 31; ++i) {
      json += (i ? ", " : "") + std::to_string(obs.regs_raw[i]);
    }
    json += "],\n";
    json += "  \"done\": " +
            std::string(obs.timed_out ? "false" : "true") + ",\n";
    json += "  \"timeout\": " +
            std::string(obs.timed_out ? "true" : "false") + "\n";
    json += "}\n";

    if (!opts.json_path.empty()) {
      std::ofstream out(opts.json_path);
      if (!out) {
        std::fprintf(stderr, "cannot write %s\n", opts.json_path.c_str());
        return 2;
      }
      out << json;
    }
    std::fputs(json.c_str(), stdout);
    return rc;
  }

  if (opts.stats) {
    auto* run_stats =
        reinterpret_cast<int (*)(void*, std::uint32_t, const char*,
                                 std::uint32_t*, std::uint32_t, std::uint32_t,
                                 std::uint32_t*, std::uint32_t*,
                                 std::uint32_t*, std::uint32_t*)>(
            dlsym(handle, "cf_run_sim_stats"));
    if (!run_stats) {
      std::fprintf(stderr, "dlsym cf_run_sim_stats failed: %s\n", dlerror());
      delete_vtop(top);
      dlclose(handle);
      return 1;
    }
    std::vector<std::uint32_t> hist(opts.stats_len, 0u);
    std::uint32_t cycles = 0, tohost = 0, dropped = 0, pc_changes = 0;
    rc = run_stats(top, opts.max_cycles, elf_name.c_str(), hist.data(),
                   opts.stats_len, opts.stats_base, &cycles, &tohost,
                   &dropped, &pc_changes);
    // ★ FLUSH 事件数（DUT 内部计数器; 1 拍延迟 ⇒ 差 1 拍）
    auto* read_flush = reinterpret_cast<std::uint32_t (*)(void*)>(
        dlsym(handle, "cf_read_flush_count"));
    const std::uint32_t flush_count = read_flush ? read_flush(top) : 0u;
    // ★ BP 事件计数（命中 / 未命中 / 预测 taken）
    //   ★ **BP 未注册时（--no-bp）Verilog 里没有这些信号** ⇒ 读它们会让
    //     Verilator 编译失败（`top__DOT__bp_hit_count` 不存在）⇒ MUST NOT 读。
    std::uint32_t bp_hit = 0, bp_miss = 0, bp_taken = 0;
    if (opts.enable_bp) {
      // ★ 与 harness 的 CF_BP_STATS_READ 条件一致（BP 注册 ⇒ 宏已定义）
      auto* read_bp_hit = reinterpret_cast<std::uint32_t (*)(void*)>(
          dlsym(handle, "cf_read_bp_hit_count"));
      auto* read_bp_miss = reinterpret_cast<std::uint32_t (*)(void*)>(
          dlsym(handle, "cf_read_bp_miss_count"));
      auto* read_bp_taken = reinterpret_cast<std::uint32_t (*)(void*)>(
          dlsym(handle, "cf_read_bp_taken_count"));
      bp_hit = read_bp_hit ? read_bp_hit(top) : 0u;
      bp_miss = read_bp_miss ? read_bp_miss(top) : 0u;
      bp_taken = read_bp_taken ? read_bp_taken(top) : 0u;
    }
    // One line per sampled PC value (dense form; the host tool aggregates).
    // ⚠️ NOT a fetch count — the sampled stream includes non-architectural
    //    PC values (mechanism unconfirmed); relative/cross-mode use only.
    std::uint32_t distinct = 0;
    for (std::uint32_t i = 0; i < opts.stats_len; ++i) {
      if (hist[i] == 0) continue;
      ++distinct;
      std::printf("PC_HIST pc=0x%08x count=%u\n", opts.stats_base + (i << 2),
                  hist[i]);
    }
    std::printf("PC_HIST_TOTAL distinct=%u dropped=%u window=0x%08x+%u\n",
                distinct, dropped, opts.stats_base, opts.stats_len);
    std::printf("PC_STATS cycles=%u pc_changes=%u\n", cycles, pc_changes);
    // ★ FLUSH 事件数（★ 语义：错路指令被抑制的次数 —— 由 DUT 内部计数器给出）
    std::printf("FLUSH_STATS count=%u\n", flush_count);
    // ★ BP 事件计数（★ 命中 > 0 ⇒ BTB 真的能命中 ⇒ AC-F2-14 自证）
    std::printf("BP_STATS hit=%u miss=%u pred_taken=%u\n", bp_hit, bp_miss,
                bp_taken);
  } else {
    rc = run_sim(top, opts.max_cycles, elf_name.c_str());
  }
  delete_vtop(top);
  dlclose(handle);
  return rc;
}

#else
#error "cpu_verilator_sim.cpp requires CF_PLUGIN_USE_CH_MEM"
#endif
