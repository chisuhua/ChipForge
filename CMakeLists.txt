# ChipForge root CMakeLists.txt — STUB
# Phase 1 will populate with real targets. For now, this file exists only to
# satisfy documentation references and to provide a minimal build foundation.

# ════════════════════════════════════════════════════════════════════
# ⚠️  This is a STUB. Phase 1 will replace with real implementation.
# ════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.20)

project(ChipForge
    VERSION 0.0.0
    DESCRIPTION "RISC-V Virtual Verification Platform"
    LANGUAGES CXX
)

# ─────────────────────────────────────────────────────────────────────
# Future layout (commented out until Phase 1):
#
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)
#
# # CppTLM / CppHDL via symlinks
# add_subdirectory(${CMAKE_SOURCE_DIR}/CppTLM cpptlm_build)
# add_subdirectory(${CMAKE_SOURCE_DIR}/CppHDL cpphdl_build)
#
# # IP library
# add_subdirectory(ip)
#
# # SoC
# add_subdirectory(soc)
#
# # Tools
# add_subdirectory(tools)
# ─────────────────────────────────────────────────────────────────────

message(STATUS "ChipForge ${PROJECT_VERSION} — STUB build configuration")
message(STATUS "Phase 1 will replace this with real build targets")