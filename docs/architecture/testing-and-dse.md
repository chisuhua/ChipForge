# 测试框架与设计空间探索

---

## 测试框架设计

### 三级测试分层

```
Level C: 系统级
    Linux Boot -> Shell 交互 -> 应用程序运行
    Zephyr / FreeRTOS -> RTOS 系统调用

Level B: 功能级
    riscv-tests ISA 单元测试
    riscv-arch-test 合规测试
    CSR / PMP / 虚拟内存专项

Level A: 组件级
    各组件独立单元测试（Google Test / Catch2）
    L1CacheTlm vs L1CacheRtl 对比
```

### HTIF 通信协议

bare-metal 测试通过 HTIF 接口与平台通信：

```
tohost 写入值    含义
--------------------------------
1                测试 PASS，仿真结束
奇数 N           测试 FAIL，N>>1 为失败测试编号
偶数（非0）      系统调用请求（console 输出等）
```

### 自动化测试流程

```
scripts/run_tests.py
    |
    +-- compile_sw()        # 编译 ELF（riscv-unknown-elf-gcc）
    +-- load_elf()          # 加载到 TLM 平台
    +-- run_simulation()    # 启动仿真，设置超时
    +-- check_exit()        # 解析 tohost 退出码
    +-- generate_report()   # JSON / HTML 报告
```

### 覆盖率收集

```cpp
// verification/CoverageCollector.h
class CoverageCollector {
public:
    // 指令覆盖率
    void record_instruction(uint32_t opcode);

    // 功能覆盖率（自定义关注点）
    void record_event(const std::string& event_name);

    // 导出覆盖率报告
    void export_ucdb(const std::string& path);
    void export_json(const std::string& path);
};
```

---

## 设计空间探索框架

### 设计理念

参考 gem5 的 Python SimObject 配置系统、SST 的组件化参数热更新、Chipyard 的配置生成器，本项目利用 CppTLM 已有的 **JSON v4.0 配置系统**和 **ModuleFactory** 实现设计空间探索（DSE）：

```
设计空间探索工作流
===============================================================
    定义参数空间              执行仿真矩阵            分析结果
+------------------+    +------------------+    +------------------+
| sweep_config.json|--->| Python 驱动脚本  |--->| Pareto 前沿      |
| - 参数范围       |    | - 生成配置组合   |    | 敏感性分析       |
| - 基准测试       |    | - 并行执行仿真   |    | 热力图可视化     |
| - 收集指标       |    | - 收集统计数据   |    | 最优配置推荐     |
+------------------+    +------------------+    +------------------+
```

核心优势：
- **无需重编译**：通过 JSON 配置切换组件参数和策略
- **并行探索**：Python 驱动多进程同时运行不同配置
- **可复现**：每次仿真的完整配置和结果都持久化存储
- **渐进式**：从单参数扫描到多维 Pareto 优化

### JSON 配置驱动的参数化体系

#### SoC 配置文件

利用 CppTLM 的 ModuleFactory + JSON v4.0 配置系统组装 SoC：

```json
{
  "name": "RiscvVirtSoC",
  "description": "RV64GC Virtual Platform - Default Configuration",
  "impl_mode": "TLM_ONLY",
  "modules": [
    {
      "name": "cpu_0",
      "type": "RiscvIssTlm",
      "params": {
        "isa": "rv64gc",
        "clock_freq_mhz": 50,
        "enable_sv39": true
      }
    },
    {
      "name": "l1i_cache",
      "type": "L1CacheTlm",
      "params": {
        "size_kb": 32,
        "assoc": 8,
        "line_size": 64,
        "replacement_policy": "PLRU",
        "prefetcher": {"type": "stride", "degree": 2}
      }
    },
    {
      "name": "l1d_cache",
      "type": "L1CacheTlm",
      "params": {
        "size_kb": 32,
        "assoc": 8,
        "line_size": 64,
        "replacement_policy": "LRU",
        "write_policy": "WriteBack",
        "prefetcher": {"type": "none"}
      }
    },
    {
      "name": "l2_cache",
      "type": "L2CacheTlm",
      "params": {
        "size_kb": 512,
        "assoc": 16,
        "inclusion": "inclusive",
        "replacement_policy": "PLRU"
      }
    },
    {
      "name": "dram",
      "type": "DramTlm",
      "params": {"size_mb": 512, "latency_ns": 50, "enable_dmi": true}
    },
    {
      "name": "bus",
      "type": "BusMatrixTlm",
      "params": {"arbitration": "RoundRobin"}
    }
  ],
  "connections": [
    {"src": "cpu_0.ibus", "dst": "l1i_cache.cpu_port", "latency": 0},
    {"src": "cpu_0.dbus", "dst": "l1d_cache.cpu_port", "latency": 0},
    {"src": "l1i_cache.mem_port", "dst": "l2_cache.cpu_port.0", "latency": 1},
    {"src": "l1d_cache.mem_port", "dst": "l2_cache.cpu_port.1", "latency": 1},
    {"src": "l2_cache.mem_port", "dst": "bus.port.0", "latency": 2},
    {"src": "bus.port.1", "dst": "dram.port", "latency": 1}
  ]
}
```

#### 配置继承与覆盖

```json
{
  "extends": "configs/soc/riscv_virt.json",
  "description": "Small embedded variant",
  "modules": [
    {"name": "l1i_cache", "params": {"size_kb": 8, "assoc": 2}},
    {"name": "l1d_cache", "params": {"size_kb": 8, "assoc": 2}},
    {"name": "dram", "params": {"size_mb": 64}}
  ]
}
```

### 统计收集框架

参考 gem5 的 Stats 系统，每个 TLM 组件内置层次化统计收集：

```cpp
// metrics/statistics.h
namespace stats {

class Scalar {
public:
    void operator++();
    void operator+=(uint64_t v);
    uint64_t value() const;
};

class Distribution {
public:
    Distribution(uint64_t bucket_width, uint64_t num_buckets);
    void sample(uint64_t value);       // 记录一次采样
    double mean() const;
    double percentile(double p) const; // p50, p95, p99
};

class Vector {
public:
    Vector(size_t size);               // 每核/每线程独立计数
    Scalar& operator[](size_t idx);
};

class Formula {
public:
    Formula(std::function<double()> calc);  // 衍生指标
    double value() const;
};

class StatGroup {
public:
    void add(const std::string& name, Scalar& stat);
    void add(const std::string& name, Distribution& stat);
    void add(const std::string& name, Formula& stat);

    void export_json(const std::string& path) const;
    void export_csv(const std::string& path) const;
    void reset_all();
};

}  // namespace stats
```

#### 组件统计注

```cpp
class L1CacheTlm : public ChStreamModuleBase {
    stats::Scalar hits_, misses_, evictions_, writebacks_;
    stats::Distribution miss_latency_;
    stats::Formula hit_ratio_;

    void register_stats(stats::StatGroup& group) {
        group.add("hits", hits_);
        group.add("misses", misses_);
        group.add("evictions", evictions_);
        group.add("miss_latency", miss_latency_);
        group.add("hit_ratio", Formula([&](){
            auto total = hits_.value() + misses_.value();
            return total > 0 ? double(hits_.value()) / total : 0.0;
        }));
    }
};
```

#### 统计输出示例

```json
{
  "simulation": {"total_cycles": 1000000, "instructions": 850000},
  "cpu_0": {"ipc": 0.85, "branch_mispredicts": 12340},
  "l1i_cache": {"hits": 920000, "misses": 30000, "hit_ratio": 0.968},
  "l1d_cache": {
    "hits": 780000, "misses": 70000, "hit_ratio": 0.918,
    "miss_latency": {"mean": 45.2, "p50": 40, "p95": 120, "p99": 200}
  },
  "l2_cache": {"hits": 85000, "misses": 15000, "hit_ratio": 0.850},
  "dram": {"read_accesses": 15000, "write_accesses": 8000, "bandwidth_gbps": 2.4}
}
```

### 参数扫描工作流

#### 扫描配置定义

```json
{
  "name": "cache_design_exploration",
  "base_config": "configs/soc/riscv_virt.json",
  "sweep_space": {
    "l1d_cache.size_kb": [8, 16, 32, 64],
    "l1d_cache.assoc": [2, 4, 8],
    "l1d_cache.replacement_policy": ["LRU", "PLRU", "Random", "FIFO", "RRIP"],
    "l1d_cache.prefetcher.type": ["none", "stride", "next_line"]
  },
  "benchmarks": [
    "sw/baremetal/custom/coremark",
    "sw/baremetal/custom/dhrystone",
    "sw/baremetal/riscv-tests/rv64ui"
  ],
  "metrics": [
    "l1d_cache.hit_ratio",
    "l1d_cache.miss_latency.mean",
    "simulation.total_cycles",
    "cpu_0.ipc"
  ],
  "execution": {
    "parallelism": 8,
    "timeout_seconds": 300,
    "output_dir": "results/cache_dse"
  }
}
```

#### Python 驱动脚本

```python
# tools/dse/sweep_driver.py
class SweepDriver:
    def __init__(self, sweep_config_path):
        self.config = json.load(open(sweep_config_path))

    def generate_combinations(self):
        """生成参数笛卡尔积"""
        keys = list(self.config["sweep_space"].keys())
        values = list(self.config["sweep_space"].values())
        return [dict(zip(keys, combo))
                for combo in itertools.product(*values)]

    def run_single(self, param_dict, benchmark, output_dir):
        """执行单次仿真"""
        # 生成 SoC 配置（基于 base_config + 参数覆盖）
        soc_config = self.apply_overrides(param_dict)
        # 调用仿真器
        subprocess.run([
            "./build/chipforge_sim",
            "--config", f"{output_dir}/config.json",
            "--elf", benchmark,
            "--stats", f"{output_dir}/stats.json"
        ])

    def run_sweep(self):
        """并行执行所有参数组合 x 所有基准测试"""
        with ProcessPoolExecutor(max_workers=self.config["execution"]["parallelism"]) as pool:
            futures = []
            for combo in self.generate_combinations():
                for bench in self.config["benchmarks"]:
                    futures.append(pool.submit(self.run_single, combo, bench, ...))
            # 收集结果
            results = [f.result() for f in as_completed(futures)]
        return results
```

### 结果分析与可视化

#### Pareto 前沿分析

```python
# tools/dse/pareto_analyzer.py
class ParetoAnalyzer:
    def compute_frontier(self, results, objectives):
        """
        objectives: [("ipc", "maximize"), ("total_cycles", "minimize")]
        返回非支配解集合
        """
        ...

    def sensitivity_analysis(self, results, baseline, metric):
        """单参数变化对指标的影响百分比"""
        ...

    def generate_heatmap(self, results, x_param, y_param, metric):
        """双参数热力图"""
        ...

    def export_report(self, output_dir):
        """生成 HTML 报告（含交互式图表）"""
        ...
```

#### 典型 DSE 使用场景

| 场景 | 扫描参数 | 目标指标 | 预期结论 |
|------|---------|---------|---------|
| L1 Cache 大小 vs 性能 | size_kb: 8-64 | IPC, hit_ratio | 性价比拐点 |
| 替换策略对比 | LRU/PLRU/Random/RRIP | hit_ratio, miss_latency | 最优策略 |
| 预取激进度 | degree: 0-4 | bandwidth, pollution | 最佳预取度 |
| L2 容量 vs 面积 | size_kb: 128-2048 | L2_hit_ratio, area_est | 面积效率 |
| NoC 拓扑探索 | mesh_x/y: 2-8 | avg_latency, throughput | 最优拓扑 |

