## 1. Fix D4 #3 violations in `ip/mmu/lib/tlb.h`

- [ ] 1.1 Replace `const uint64_t tag = ...` with `const typename Entry::tag_type tag = ...` at line 73 (in `lookup()`)
- [ ] 1.2 Replace `const uint64_t tag = ...` with `const typename Entry::tag_type tag = ...` at line 98 (in `insert()`)
- [ ] 1.3 Replace `const uint64_t tag = ...` with `const typename Entry::tag_type tag = ...` at line 146 (in `insert_from()`)
- [ ] 1.4 Replace `const uint64_t tag = ...` with `const typename Entry::tag_type tag = ...` at line 189 (in `invalidate_vaddr()`)
- [ ] 1.5 Verify with `grep -nE "(ch_uint<\d+>|uint(8|16|32|64)_t) +(addr|data|tag|idx|valid|burst_len|is_write)" ip/mmu/lib/tlb.h` returns no matches

## 2. Fix ADR-040 #1 violations in `ip/mmu/tlm/MMUPlugin.cpp`

- [ ] 2.1 Wrap `tlb_lookup_ifetch` body in `if (n) { ... }` (eliminates line 47 `if (!n) return;`)
- [ ] 2.2 Wrap `tlb_lookup_loadstore` body in `if (n) { ... }` (eliminates line 73 `if (!n) return;`)
- [ ] 2.3 Wrap `ptw_l0` body in `if (n && (*n)(K8::PTW_ACTIVE)) { ... }` (eliminates line 93 + line 94)
- [ ] 2.4 Wrap `ptw_l1` body in `if (n && (*n)(K8::PTW_ACTIVE)) { ... }` (eliminates line 102 + line 103)
- [ ] 2.5 Wrap `ptw_l2` body in `if (n && (*n)(K8::PTW_ACTIVE)) { ... }` (eliminates line 111 + line 112)
- [ ] 2.6 Verify with `awk` detector: `tools/check_plugin_portability.sh` reports no MMUPlugin.cpp violations

## 3. Verify build + regression

- [ ] 3.1 `cmake --build build -j$(nproc)` exits 0 (no new compile errors)
- [ ] 3.2 `ctest --test-dir build --output-on-failure` shows 43/43 tests passed
- [ ] 3.3 `bash tools/verify_plugin_decision.sh` exits 0 and prints `=== D4 + ADR-040 检查全部通过 ===`

## 4. Archive change (post-implementation)

- [ ] 4.1 `openspec archive mmu-d4-adr040-compliance --yes` (run after all above checks pass; requires user authorization)