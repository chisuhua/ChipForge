# tools/verilator_runner/CMakeLists.txt
#
# Phase 6d.5 E8: standalone Verilator runner for the 5-stage CH_MEM CPU.
#
# cpu_verilator_sim is a normal ChipForge executable (links CppHDL + cf_plugin,
# compiled with CF_PLUGIN_USE_CH_MEM). At runtime it:
#   1. loads a vendored ELF, builds the CPU via CpuFactoryChmem and emits
#      Verilog (CppHDL codegen now emits ch_mem arrays — ADR-035 R8 backdoor)
#   2. compiles it with `verilator --cc --public-flat-rw` and links the model
#      with sim_main.cpp into a dlopen-able libVtop.so
#   3. dlopens libVtop.so and drives clock/reset until dmem[tohost] == 1
#
# sim_main.cpp is NOT compiled into this target — it is compiled at runtime
# by the two-phase verilator + g++ -shared recipe inside cpu_verilator_sim.

# Find verilator binary (used at runtime, but the CMake target must exist even
# when verilator is absent — the runner reports a clear error in that case).
find_program(VERILATOR_BIN
    NAMES verilator
    HINTS
        $ENV{VERILATOR_BIN}
        /workspace/main/opt/verilator/bin
        /usr/local/bin
        /usr/bin
)
if(NOT VERILATOR_BIN)
    message(STATUS "verilator_runner: verilator not found — cpu_verilator_sim will error at runtime (set VERILATOR_BIN)")
endif()

add_executable(cpu_verilator_sim
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_verilator_sim.cpp
)
target_compile_features(cpu_verilator_sim PRIVATE cxx_std_23)
target_compile_definitions(cpu_verilator_sim PRIVATE
    CF_PLUGIN_USE_CH_MEM
    CHIPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    TEST_RV32UI_ELF_DIR="${CMAKE_SOURCE_DIR}/tests/cpu/riscv_tests/elf"
)
target_include_directories(cpu_verilator_sim PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/bundles
    ${CMAKE_SOURCE_DIR}/ip
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(cpu_verilator_sim PRIVATE cf_plugin)
cf_plugin_link_cpptlm(cpu_verilator_sim)
cf_plugin_link_cpphdl(cpu_verilator_sim)
find_package(Threads REQUIRED)
target_link_libraries(cpu_verilator_sim PRIVATE ${CMAKE_DL_LIBS} Threads::Threads)

set_target_properties(cpu_verilator_sim PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

if(VERILATOR_BIN)
    target_compile_definitions(cpu_verilator_sim PRIVATE
        CF_VERILATOR_BIN="${VERILATOR_BIN}"
    )
endif()
