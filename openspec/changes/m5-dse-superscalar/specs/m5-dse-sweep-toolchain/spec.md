## MODIFIED Requirements

### Requirement: sweep_driver.py dimension migration from 288 to 576 configs

`tools/dse/sweep_driver.py` MUST migrate its default sweep matrix from the existing 6-dimension 288-config space (`4 pipeline_stages × 3 branch_predictor × 3 btb_entries × 2 ext_m × 2 mul_latency × 2 isa`) to a new 7-dimension 576-config space (`4 pipeline_stages × 3 branch_predictor × 3 btb_entries × 2 xlen × 2 mul_latency × 2 icache_latency × 2 dcache_latency`).

The existing 6 dimensions (pipeline_stages, branch_predictor, btb_entries, mul_latency, ext_m, isa) SHALL remain supported via the `--space` JSON override path, but the new default MUST be the 7-dimension matrix.

#### Scenario: Migration from 288 to 576 preserves coverage via --space override

- **WHEN** `sweep_driver.py` is invoked with the existing 6-dimension `--space '{"pipeline_stages":[3,5,7,10],"btb_entries":[16,64,256],"branch_predictor":["static","gshare","tournament"],"ext_m":[false,true],"mul_latency":[1,3],"isa":["rv32i","rv64i"]}'`
- **THEN** the driver SHALL generate exactly 288 configs (regression parity with aedb9ce)
- **AND** running with no `--space` override and `--seed 0` SHALL produce the new 576-config matrix (per the ADDED Requirement below)

## ADDED Requirements

### Requirement: sweep_driver.py generates config matrix

`tools/dse/sweep_driver.py` SHALL generate a deterministic config matrix of `4 (pipeline_stages) × 3 (branch_predictor) × 3 (btb_entries) × 2 (xlen) × 2 (mul_latency) × 2 (icache_latency) × 2 (dcache_latency) = 576` configurations and invoke `cpu_sim` for each via `multiprocessing.Pool` with default worker count = `os.cpu_count()`.

The driver SHALL support `--limit N` to cap total runs, `--output PATH` to write `results/sweep.json`, and `--seed S` for reproducible run ordering.

#### Scenario: 576-config matrix generated deterministically

- **WHEN** `sweep_driver.py` is invoked with default args and `--seed 0`
- **THEN** the generated config matrix SHALL contain exactly 576 entries
- **AND** the matrix SHALL be ordered identically across re-runs with the same seed
- **AND** the matrix SHALL cover all 4 pipeline depths × 3 branch predictor types × 3 BTB sizes × 2 xlen × 2 mul_latency × 2 cache latencies

#### Scenario: --limit N caps total runs

- **WHEN** `sweep_driver.py --limit 100 --seed 0` is invoked
- **THEN** the driver SHALL execute exactly 100 `cpu_sim` invocations
- **AND** the remaining 476 configs SHALL be skipped (not queued, not run)

### Requirement: pareto_analyzer.py computes frontier

`tools/dse/pareto_analyzer.py` SHALL read `results/sweep.json` and compute the Pareto-optimal frontier over (`cycles`, `ipc`) or (`cycles`, `area_estimate`) metrics, writing the frontier to `results/pareto.json` and rendering an ASCII chart to stdout.

#### Scenario: 100-result sweep yields valid Pareto front

- **WHEN** `pareto_analyzer.py` is invoked on a 100-row `results/sweep.json` with mixed-quality entries
- **THEN** the output Pareto front SHALL be a strict subset of the 100 input rows
- **AND** every non-frontier row SHALL be dominated by at least one frontier row
- **AND** the ASCII chart SHALL render at least 2 distinct (cycles, ipc) coordinate pairs
