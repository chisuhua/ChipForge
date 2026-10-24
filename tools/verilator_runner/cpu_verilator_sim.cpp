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
  // chmem-branch-microbench-fix: resident per-static-PC fetch histogram.
  // --stats            print PC_HIST lines (one per fetched static PC)
  // --stats-base <hex> histogram window base (default = ELF base)
  // --stats-len  <n>   number of 4-byte slots (default 256)
  bool stats = false;
  std::uint32_t stats_base = 0x80000000u;
  std::uint32_t stats_len = 256;
};

bool parse_args(int argc, char** argv, Options& opts) {
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--elf" && i + 1 < argc) {
      opts.elf_path = argv[++i];
    } else if (a == "--verilog" && i + 1 < argc) {
      opts.verilog_path = argv[++i];
    } else if (a == "--work-dir" && i + 1 < argc) {
      opts.work_dir = argv[++i];
    } else if (a == "--cycles" && i + 1 < argc) {
      opts.max_cycles = static_cast<std::uint32_t>(std::atoi(argv[++i]));
    } else if (a == "--enable-mmu") {
      opts.enable_mmu = true;
    } else if (a == "--mmu-mode" && i + 1 < argc) {
      opts.mmu_mode = argv[++i];
    } else if (a == "--enable-cache") {
      opts.enable_cache = true;
    } else if (a == "--stats") {
      opts.stats = true;
    } else if (a == "--stats-base" && i + 1 < argc) {
      opts.stats_base = static_cast<std::uint32_t>(
          std::strtoul(argv[++i], nullptr, 0));
    } else if (a == "--stats-len" && i + 1 < argc) {
      opts.stats_len = static_cast<std::uint32_t>(std::atoi(argv[++i]));
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
      std::printf("  --stats          print PC_HIST lines (per-cycle PC value histogram,\n");
      std::printf("                   INCLUDING speculative fetches - NOT a fetch/retire count)\n");
      std::printf("  --stats-base <h> histogram window base (default 0x80000000)\n");
      std::printf("  --stats-len <n>  histogram slots of 4 bytes (default 256)\n");
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
  return !opts.elf_path.empty();
}

// Build the CH_MEM CPU with the vendored ELF preloaded and emit Verilog.
bool generate_verilog(const Options& opts, const std::vector<std::uint8_t>& elf) {
  ch::core::context ctx("cpu_verilator_sim");
  ch::core::ctx_swap guard(&ctx);
  auto pb = cf::cpu::CpuFactoryChmem<ch_uint<32>>::build_cpu(
      &ctx, nullptr, ch_uint<32>(kElfBase), true, elf,
      opts.enable_mmu,
      opts.mmu_mode,
      opts.enable_cache);
  if (!pb) {
    std::fprintf(stderr, "build_cpu failed\n");
    return false;
  }
  // ch::toVerilog does not create parent dirs; build_libvtop's mkdir comes
  // after the read, so top.v would silently not exist on a fresh --work-dir.
  std::string mkdir_work = "mkdir -p " + opts.work_dir;
  if (!run_shell(mkdir_work)) return false;
  pb->elaborate(ctx);
  pb->to_verilog(opts.verilog_path);
  return true;
}

// Compile top.v + sim_main.cpp into work_dir/obj_dir + libVtop.so
// (CppHDL VerilatorBackend two-phase recipe: verilator --cc then g++ -shared).
bool build_libvtop(const Options& opts) {
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
      "cd " + work + "/obj_dir && g++ -shared -fPIC -std=c++17 -o libVtop.so " +
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

  auto elf = load_elf_file(opts.elf_path);
  if (elf.size() < 52 || elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' ||
      elf[3] != 'F') {
    std::fprintf(stderr, "failed to load ELF: %s\n", opts.elf_path.c_str());
    return 2;
  }
  std::string elf_name = opts.elf_path;
  auto p = elf_name.rfind('/');
  if (p != std::string::npos) elf_name = elf_name.substr(p + 1);

  if (!generate_verilog(opts, elf)) return 1;
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
    // One line per fetched static PC (dense form; the host tool aggregates).
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
