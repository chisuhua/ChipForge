# tests/soc/ — SoC Topology Integration Tests

Catch2 test suite that structurally validates + simulates each SoC topology
declared as JSON config under `soc/`. One subdirectory per SoC topology, each
with `test_<soc>.cpp` + `README.md`.

## Subdirectories (mirror `soc/*.json` 1:1)

| Subdirectory | SoC JSON | Family tag | Contents |
|--------------|----------|-----------|----------|
| [`soc-cpu-l1-mmu-demo/`](soc-cpu-l1-mmu-demo/) | [`soc/cpu_l1_mmu_demo.json`](../../soc/cpu_l1_mmu_demo.json) | `[soc][cpu-l1-mmu-demo]` | CPU + MMU + Memory e2e + helper |
| [`mmu-minimal/`](mmu-minimal/) | [`soc/mmu_minimal.json`](../../soc/mmu_minimal.json) | `[soc]` | MMU standalone |
| [`l1-cache-minimal/`](l1-cache-minimal/) | [`soc/l1_cache_minimal.json`](../../soc/l1_cache_minimal.json), [`soc/l1_cache_adapter_e2e.json`](../../soc/l1_cache_adapter_e2e.json) | `[soc]` | L1Cache schema + minimal + bridge adapter |

## Adding a new SoC topology

1. Add `soc/<name>.json` under `soc/`
2. Add a new subdirectory `tests/soc/<soc-name>/` containing:
   - `test_<soc-name>.cpp` (Catch2 TEST_CASE with `[soc]` family tag)
   - `README.md` (cross-reference the SoC JSON + family tag + upstream spec)
3. **No CMake changes required**: `tests/CMakeLists.txt` uses `file(GLOB_RECURSE)`
   that auto-discovers `test_*.cpp` in all subdirectories.

## Backward compatibility

> **Archive note (2026-10-18)**: This restructure moved 6 `.cpp`/`.h` files
> from this flat layout into 3 subdirectories. Active openspec changes that
> referenced `tests/soc/<soc-name>/test_<soc-name>.cpp` paths have been updated in the same
> PR (see commit history). The `README.md.old` is kept for historical diff
> reference and can be removed in v0.12.0 cleanup.
