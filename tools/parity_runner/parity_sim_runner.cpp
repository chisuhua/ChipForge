// tools/parity_runner/parity_sim_runner.cpp
//
// Double-backend parity gate — **CppHDL Simulator side** runner.
//
// Why this file exists: the only Simulator-side ELF entry point today is a
// hard-coded Catch2 table (tests/cpu/test_cpu_chmem_vendored_elf.cpp, 47
// entries) with no CLI. The Verilator side already has a CLI
// (tools/verilator_runner/cpu_verilator_sim.cpp). Parity requires running the
// *same* ELF through *both* backends with *identical* knobs — so this runner
// mirrors the Verilator CLI surface 1:1.
//
// Contract: docs/architecture/parity-gate-contract.md (S-3/S-4/S-5/S-7/S-8/
// S-14). Observations collected here:
//   tohost   — node "tohost_probe_data_proxy"          (S-4)
//   regs     — nodes "reg_1".."reg_31", sha256-ish hash (S-5)
//   cycles   — tick count to done                        (S-7)
//   pchash   — per-cycle ordered FNV-1a over node "pc"   (S-8)
//   memsig   — NOT collected yet (needs the DUT-side probe; A2a increment 2)
//
// Collection discipline (contract S-2 §2.1): resolve node ids ONCE by
// sweeping ctx.get_nodes(), then read data_map() per cycle. NEVER call
// get_value_by_name() in the loop (O(nodes) per call) and NEVER use
// get_signal_id_by_name() (it only knows about trace-registered signals,
// and tracing is off by default).
//
// reset mode: NONE — matches the Simulator-side baseline behaviour
// (contract S-3; see test_cpu_chmem_vendored_elf.cpp: reset() would clobber
// the PC init value 0x80000000).

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <ch.hpp>
#include <core/context.h>
#include <core/uint.h>

#include "cf/plugin/pipe_builder.h"
#include "cf/plugin/result_macros.h"
#include "cf/plugin/uint_t.h"
#include "ip/cpu/cpu_factory_chmem.h"

namespace {

constexpr std::uint32_t kElfBase = 0x80000000;
constexpr std::uint32_t kDefaultMaxCycles = 2000000;

// ── FNV-1a 64 ────────────────────────────────────────────────────────────
constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;

std::uint64_t fnv_u8(std::uint64_t h, std::uint8_t b) {
  return (h ^ b) * kFnvPrime;
}

std::uint64_t fnv_u32le(std::uint64_t h, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) {
    h = fnv_u8(h, static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu));
  }
  return h;
}

std::uint64_t fnv_u64le(std::uint64_t h, std::uint64_t v) {
  for (int i = 0; i < 8; ++i) {
    h = fnv_u8(h, static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFull));
  }
  return h;
}

std::uint64_t fnv_str(std::uint64_t h, const std::string& s) {
  for (char c : s) h = fnv_u8(h, static_cast<std::uint8_t>(c));
  return h;
}

std::string hex64(std::uint64_t v) {
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
  return std::string(buf);
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
      out.push_back(c);
    } else if (static_cast<unsigned char>(c) < 0x20) {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out.push_back(c);
    }
  }
  return out;
}

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

struct Options {
  std::string elf_path;
  std::string json_path;          // "" ⇒ stdout only
  std::uint32_t max_cycles = kDefaultMaxCycles;
  std::string reset_mode = "none";  // contract S-3: only "none" is supported
  bool enable_mmu = false;
  std::string mmu_mode = "bare";
  bool enable_cache = false;
  bool enable_bp = true;
  std::uint32_t btb_entries = 256;  // mirror cpu_verilator_sim default
  bool bp_uncond = true;
  bool verbose = false;
  // Increment-1 scaffolding: memsig needs the DUT-side probe (increment 2).
  // Without this flag a missing observation is a hard ERROR (contract S-1/S-14).
  bool allow_incomplete = false;
  std::string dump_nodes;   // diagnostic: substring filter for DAG node listing
};

void usage(const char* argv0) {
  std::printf("usage: %s --elf <path> [flags]\n", argv0);
  std::printf("  --elf <path>        ELF to run (required)\n");
  std::printf("  --cycles <n>        per-ELF cycle cap (default %u)\n", kDefaultMaxCycles);
  std::printf("  --reset <mode>      reset mode; only \"none\" is supported (contract S-3)\n");
  std::printf("  --enable-mmu        forward to CpuFactoryChmem (plumbing only)\n");
  std::printf("  --mmu-mode <s>      mmu mode string (log/ff only, default \"bare\")\n");
  std::printf("  --enable-cache      forward to CpuFactoryChmem (fail-fast if unimplemented)\n");
  std::printf("  --no-bp             disable branch prediction\n");
  std::printf("  --btb-entries <n>   logical BTB entries (one of 16/32/64/128/256)\n");
  std::printf("  --no-bp-uncond      disable unconditional-branch prediction\n");
  std::printf("  --json <path>       write the report fragment to <path>\n");
  std::printf("  --verbose           per-cycle trace to stderr\n");
  std::printf("  --allow-incomplete  permit not-yet-collected observations (memsig)\n");
  std::printf("  --dump-nodes <sub> list DAG nodes whose name contains <sub> (diagnostic)\n");
  std::printf("  --help / -h         this message\n");
}

bool parse_args(int argc, char** argv, Options* o) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto need = [&](const char* what) -> std::string {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "%s expects a value\n", what);
        return {};
      }
      return argv[++i];
    };
    if (a == "--elf") {
      o->elf_path = need("--elf");
    } else if (a == "--cycles") {
      auto v = need("--cycles");
      if (v.empty()) return false;
      o->max_cycles = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
    } else if (a == "--reset") {
      o->reset_mode = need("--reset");
    } else if (a == "--enable-mmu") {
      o->enable_mmu = true;
    } else if (a == "--mmu-mode") {
      o->mmu_mode = need("--mmu-mode");
    } else if (a == "--enable-cache") {
      o->enable_cache = true;
    } else if (a == "--no-bp") {
      o->enable_bp = false;
    } else if (a == "--btb-entries") {
      auto v = need("--btb-entries");
      if (v.empty()) return false;
      o->btb_entries = static_cast<std::uint32_t>(std::strtoul(v.c_str(), nullptr, 10));
    } else if (a == "--no-bp-uncond") {
      o->bp_uncond = false;
    } else if (a == "--json") {
      o->json_path = need("--json");
    } else if (a == "--verbose") {
      o->verbose = true;
    } else if (a == "--allow-incomplete") {
      o->allow_incomplete = true;
    } else if (a == "--dump-nodes" && i + 1 < argc) {
      o->dump_nodes = need("--dump-nodes");
    } else if (a == "--help" || a == "-h") {
      usage(argv[0]);
      std::exit(0);
    } else {
      std::fprintf(stderr, "unknown arg: %s\n", a.c_str());
      return false;
    }
  }
  if (o->elf_path.empty()) {
    std::fprintf(stderr, "--elf is required\n");
    return false;
  }
  if (o->reset_mode != "none") {
    // Contract S-3: the Simulator side baseline does NOT reset (reset() would
    // clobber the PC init value). Any other mode would silently diverge from
    // the reference behaviour, so fail fast instead of pretending.
    std::fprintf(stderr,
                 "--reset %s is not supported: the Simulator side runs with "
                 "reset_mode=none (contract S-3)\n",
                 o->reset_mode.c_str());
    return false;
  }
  return true;
}

// Resolve a node id by NAME once (contract S-2 §2.1: one sweep, then direct
// data_map() reads). Returns std::nullopt when the node does not exist.
std::optional<std::uint32_t> resolve_node_id(ch::core::context& ctx,
                                             const std::string& name) {
  for (const auto& node : ctx.get_nodes()) {
    if (node && node->name() == name) return node->id();
  }
  return std::nullopt;
}

std::optional<std::uint64_t> read_slot(const ch::data_map_t& dm,
                                      std::uint32_t id) {
  auto it = dm.find(id);
  if (it == dm.end()) return std::nullopt;
  return static_cast<std::uint64_t>(it->second);
}

}  // namespace

int main(int argc, char** argv) {
  Options opts;
  if (!parse_args(argc, argv, &opts)) {
    usage(argv[0]);
    return 2;  // contract S-10: ERROR (gate/environment fault, not divergence)
  }

  auto elf = load_elf_file(opts.elf_path);
  if (elf.size() < 52 || elf[0] != 0x7F || elf[1] != 'E') {
    std::fprintf(stderr, "not an ELF: %s\n", opts.elf_path.c_str());
    return 2;
  }

  // CppHDL logs every node creation through CHINFO with no runtime level
  // control (include/utils/logger.h has no threshold). Keep stdout clean for
  // the report by pointing fd 1 at fd 2 while the DUT builds/runs; restore
  // before emitting JSON.
  const int saved_stdout = dup(1);
  if (saved_stdout >= 0) {
    fflush(stdout);
    dup2(2, 1);
  }
  struct StdoutRestore {
    int saved;
    ~StdoutRestore() {
      if (saved >= 0) {
        fflush(stdout);
        dup2(saved, 1);
        close(saved);
      }
    }
  } stdout_restore{saved_stdout};

  ch::core::context ctx("parity_sim_runner");
  ch::core::ctx_swap guard(&ctx);

  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), /*preload_elf=*/true, elf,
      opts.enable_mmu, opts.mmu_mode, opts.enable_cache,
      opts.enable_bp, opts.btb_entries, opts.bp_uncond);
  if (!pb) {
    std::fprintf(stderr, "build_cpu failed\n");
    return 2;
  }

  if (!pb->elaborate(ctx).has_value()) {
    std::fprintf(stderr, "elaborate failed\n");
    return 2;
  }

  // Diagnostic: list DAG nodes whose name contains a substring, together with
  // their membership in the evaluation list. Used to answer "is my probe
  // actually scheduled?" / "is it emitted into the Verilog?" (the codegen
  // only emits nodes in the eval list).
  if (!opts.dump_nodes.empty()) {
    for (const auto& node : ctx.get_nodes()) {
      if (!node) continue;
      const auto nm = node->name();
      if (nm.find(opts.dump_nodes) == std::string::npos) continue;
      bool in_eval = false;
      for (const auto& e : ctx.get_eval_list()) {
        if (e && e->id() == node->id()) { in_eval = true; break; }
      }
      std::fprintf(stderr, "[nodes] id=%u name=%s type=%d eval=%s\n", node->id(),
                   nm.c_str(), (int)node->type(), in_eval ? "yes" : "no");
    }
  }

  // ── determinism preconditions (contract S-2) ───────────────────────────
  std::vector<std::string> names;
  names.reserve(ctx.get_nodes().size());
  for (const auto& node : ctx.get_nodes()) {
    if (node) names.push_back(node->name());
  }
  const std::uint64_t node_count = static_cast<std::uint64_t>(names.size());
  std::vector<std::string> sorted_names = names;
  std::sort(sorted_names.begin(), sorted_names.end());
  std::uint64_t names_hash = kFnvOffset;
  for (const auto& n : sorted_names) {
    names_hash = fnv_str(names_hash, n);
    names_hash = fnv_u8(names_hash, 0xFFu);  // separator
  }

  auto sim_res = pb->create_simulator();
  if (!sim_res) {
    std::fprintf(stderr, "create_simulator failed\n");
    return 2;
  }
  std::unique_ptr<ch::Simulator> sim = std::move(sim_res.value());

  // ── resolve node ids once ──────────────────────────────────────────────
  auto tohost_id = resolve_node_id(ctx, "tohost_probe_data_proxy");
  auto pc_id = resolve_node_id(ctx, "pc");
  // ③ memsig (contract S-6): DUT-side probe registers, named identically on
  // both backends so the two sides can be compared directly.
  auto mem_sig_id = resolve_node_id(ctx, "mem_sig");
  auto mem_sig_upd_id = resolve_node_id(ctx, "mem_sig_updates");

  constexpr int kNumX = 32;
  std::vector<std::optional<std::uint32_t>> reg_ids(kNumX);
  int regs_missing = 0;
  for (int i = 1; i < kNumX; ++i) {
    reg_ids[i] = resolve_node_id(ctx, "reg_" + std::to_string(i));
    if (!reg_ids[i]) ++regs_missing;
  }

  if (!tohost_id) {
    std::fprintf(stderr, "missing observation 'tohost': node "
                         "tohost_probe_data_proxy not found\n");
    return 2;
  }
  if (!pc_id) {
    std::fprintf(stderr, "missing observation 'pchash': node pc not found\n");
    return 2;
  }
  if (regs_missing != 0) {
    std::fprintf(stderr, "missing observation 'regs': %d of 31 registers not "
                         "found\n", regs_missing);
    return 2;
  }

  // ── run (contract S-7: identical loop shape on both backends) ───────────
  const auto t0 = std::chrono::steady_clock::now();
  std::uint64_t pchash = kFnvOffset;
  std::uint64_t pc_samples = 0;
  std::uint32_t pc_head[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // first 8 samples
  std::uint64_t tohost_val = 0;
  std::uint64_t cycles = 0;
  bool timed_out = false;
  std::string obs_error;

  for (std::uint32_t i = 0; i < opts.max_cycles; ++i) {
    sim->tick();
    cycles = static_cast<std::uint64_t>(i) + 1;

    const auto& dm = sim->data_map();
    auto pc_v = read_slot(dm, *pc_id);
    auto th_v = read_slot(dm, *tohost_id);
    if (!pc_v || !th_v) {
      obs_error = !pc_v ? "pc slot missing" : "tohost slot missing";
      break;
    }
    const auto pc_u32 = static_cast<std::uint32_t>(*pc_v);
    if (pc_samples < 8) pc_head[pc_samples] = pc_u32;
    pchash = fnv_u32le(pchash, pc_u32);
    ++pc_samples;
    tohost_val = *th_v;

    if (opts.verbose) {
      std::fprintf(stderr, "[%llu] pc=0x%08x tohost=%llu\n",
                   static_cast<unsigned long long>(cycles),
                   static_cast<unsigned>(*pc_v),
                   static_cast<unsigned long long>(tohost_val));
    }
    if (tohost_val & 0x1ull) break;
    if (cycles == opts.max_cycles) timed_out = true;
  }
  const auto t1 = std::chrono::steady_clock::now();
  const auto sim_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

  if (!obs_error.empty()) {
    std::fprintf(stderr, "observation error: %s\n", obs_error.c_str());
    return 2;
  }

  // ── final regs hash (contract S-5: x1..x31, 32-bit LE, ascending) ───────
  std::uint64_t regs_hash = kFnvOffset;
  std::uint32_t regs_raw[31] = {0};
  {
    const auto& dm = sim->data_map();
    for (int i = 1; i < kNumX; ++i) {
      auto v = read_slot(dm, *reg_ids[i]);
      if (!v) {
        std::fprintf(stderr, "observation error: reg_%d slot missing\n", i);
        return 2;
      }
      regs_raw[i - 1] = static_cast<std::uint32_t>(*v);
      regs_hash = fnv_u32le(regs_hash, regs_raw[i - 1]);
    }
  }

  // Contract S-1 / S-14: a declared observation that is not collected is an
  // ERROR, not a "skip". memsig lands in increment 3, so it is reported as
  // pending and only tolerated with --allow-incomplete.
  const bool memsig_available = mem_sig_id.has_value() && mem_sig_upd_id.has_value();
  if (!memsig_available && !opts.allow_incomplete) {
    std::fprintf(stderr,
                 "missing observation 'memsig': DUT-side probe not implemented "
                 "yet (A2a increment 2); pass --allow-incomplete to run without "
                 "it\n");
    return 2;
  }

  // ③ memsig + self-check (contract S-6): "the guest wrote tohost but the
  // store counter says zero" => the signature is a false green => ERROR.
  std::uint64_t mem_sig_val = 0;
  std::uint64_t mem_sig_upd_val = 0;
  if (memsig_available) {
    const auto& dm = sim->data_map();
    auto sv = read_slot(dm, *mem_sig_id);
    auto uv = read_slot(dm, *mem_sig_upd_id);
    if (!sv || !uv) {
      std::fprintf(stderr, "observation error: mem_sig slot missing\n");
      return 2;
    }
    mem_sig_val = *sv;
    mem_sig_upd_val = *uv;
    if (tohost_val != 0 && mem_sig_upd_val == 0) {
      std::fprintf(stderr,
                   "observation error: mem_sig_updates == 0 although tohost=%llu "
                   "was written => the memory signature is a false green\n",
                   static_cast<unsigned long long>(tohost_val));
      return 2;
    }
  }

  const bool done = (tohost_val & 0x1ull) != 0;
  const bool pass = (tohost_val == 1);
  const std::string elf_name = [&] {
    auto slash = opts.elf_path.rfind('/');
    return slash == std::string::npos ? opts.elf_path : opts.elf_path.substr(slash + 1);
  }();

  std::string json;
  json.reserve(768);
  json += "{\n";
  json += "  \"backend\": \"simulator\",\n";
  json += "  \"elf\": \"" + json_escape(elf_name) + "\",\n";
  json += "  \"reset_mode\": \"" + json_escape(opts.reset_mode) + "\",\n";
  json += "  \"determinism\": { \"node_count\": " + std::to_string(node_count) +
          ", \"names_hash\": \"" + hex64(names_hash) + "\" },\n";
  json += "  \"observations\": {\n";
  json += "    \"tohost\": " + std::to_string(tohost_val) + ",\n";
  json += "    \"regs\": \"" + hex64(regs_hash) + "\",\n";
  json += "    \"cycles\": " + std::to_string(cycles) + ",\n";
  json += "    \"pchash\": \"" + hex64(pchash) + "\",\n";
  json += "    \"pc_samples\": " + std::to_string(pc_samples) + ",\n";
  if (memsig_available) {
    json += "    \"memsig\": \"" + hex64(mem_sig_val) + "\",\n";
    json += "    \"memsig_updates\": " + std::to_string(mem_sig_upd_val) + "\n";
  } else {
    json += "    \"memsig\": null,\n";
    json += "    \"memsig_updates\": null\n";
  }
  json += "  },\n";
  json += "  \"pc_head\": [";
  for (int i = 0; i < 8; ++i) {
    json += (i ? ", " : "") + std::to_string(pc_head[i]);
  }
  json += "],\n";
  json += "  \"regs_raw\": [";
  for (int i = 0; i < 31; ++i) {
    json += (i ? ", " : "") + std::to_string(regs_raw[i]);
  }
  json += "],\n";
  json += "  \"done\": " + std::string(done ? "true" : "false") + ",\n";
  json += "  \"timeout\": " + std::string(timed_out ? "true" : "false") + ",\n";
  json += "  \"timings\": { \"sim_ms\": " + std::to_string(sim_ms) + " }\n";
  json += "}\n";

  if (!opts.json_path.empty()) {
    std::ofstream out(opts.json_path);
    if (!out) {
      std::fprintf(stderr, "cannot write %s\n", opts.json_path.c_str());
      return 2;
    }
    out << json;
  }
  if (saved_stdout >= 0) {
    fflush(stdout);
    dup2(saved_stdout, 1);
    close(saved_stdout);
    stdout_restore.saved = -1;  // already restored
  }
  std::fputs(json.c_str(), stdout);

  if (!done) {
    std::fprintf(stderr, "TIMEOUT: %s did not write tohost within %u cycles\n",
                 elf_name.c_str(), opts.max_cycles);
    return 2;  // contract S-10: TIMEOUT is ERROR, not FAIL
  }
  return pass ? 0 : 1;  // 1 = harness wrote a fail value (corpus-level failure)
}
