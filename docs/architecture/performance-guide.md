# 性能优化指南

## 1. 概述

本文档指导如何优化 ChipForge 平台的仿真性能，覆盖 TLM 加速、RTL 仿真优化、以及混合模式下的性能调优。

## 2. TLM 仿真加速

### 2.1 DMI（Direct Memory Interface）
DMI 允许跳过时序模型直接访问内存区域，适用于非性能关键路径。

**使用场景：**
- ROM/Flash 程序加载
- 大块数据初始化
- 非时序敏感的 DMA 传输

**启用方式：**
```json
{
  "memory": {
    "dmi_enabled": true,
    "dmi_regions": [
      {"start": "0x80000000", "end": "0x80100000", "access": "read"}
    ]
  }
}
```

**加速效果：** 内存加载速度提升 10-100x

### 2.2 时间量子（Temporal Decoupling）
允许组件在本地时间超前运行，减少同步开销。

**配置：**
```json
{
  "simulation": {
    "quantum_ns": 1000,
    "sync_policy": "quantum"
  }
}
```

**权衡：** 量子越大速度越快，但时序精度降低。

### 2.3 事件合并
将多个相邻事件合并为单次处理，减少 EventQueue 调度开销。

**适用条件：**
- 连续地址的内存访问
- 同一周期内的多个寄存器写入
- 无副作用的重复操作

### 2.4 组件级优化
| 组件 | 优化手段 | 加速比 |
|------|---------|--------|
| CPU ISS | 基本块缓存（解码结果复用） | 2-5x |
| Cache | 快速路径（命中无需完整流水线） | 1.5-3x |
| Memory | DMI + 批量访问 | 10-100x |
| Interconnect | 直连模式（单主单从跳过仲裁） | 1.2-2x |

## 3. RTL 仿真优化

### 3.1 Verilator 编译优化
```bash
# 推荐编译选项
verilator --cc --exe \
  -O3 \
  --threads 4 \
  --trace-fst \
  --assert \
  top.v
```

### 3.2 CppHDL 优化
- **信号合并**：减少不必要的中间信号
- **模块剪裁**：未使用端口的模块不参与仿真
- **增量编译**：仅重新编译修改的模块

### 3.3 仿真速度基准
| 配置 | 预期速度 | 说明 |
|------|---------|------|
| TLM_ONLY (功能) | 10-100 MIPS | 无时序、DMI 全开 |
| TLM_ONLY (时序) | 1-10 MIPS | 周期精确 |
| RTL_ONLY | 0.01-0.1 MIPS | Verilator 仿真 |
| COMPARE | 0.005-0.05 MIPS | TLM+RTL 同步对比 |

## 4. 混合模式性能调优

### 4.1 SHADOW 模式优化
SHADOW 模式中 RTL 为主，TLM 为观察者：
- TLM 可降低精度以减少开销
- 仅在关注点附近启用 TLM 详细跟踪
- 使用采样而非全量对比

### 4.2 ImplMode 切换策略
```json
{
  "impl_mode_config": {
    "cpu": "TLM_ONLY",
    "cache": "RTL_ONLY",
    "memory": "TLM_ONLY",
    "comment": "仅对关注组件启用 RTL，其余用 TLM 加速"
  }
}
```

## 5. 性能分析工具

### 5.1 内置 Profiler
```json
{
  "profiler": {
    "enabled": true,
    "output_format": "json",
    "metrics": ["sim_time_ratio", "event_count", "component_load"]
  }
}
```

### 5.2 性能指标
| 指标 | 说明 | 目标 |
|------|------|------|
| sim_time_ratio | 仿真时间/实际时间 | < 10x |
| event_throughput | 每秒处理事件数 | > 1M |
| memory_usage | 仿真内存占用 | < 4 GB |

### 5.3 瓶颈定位
1. 启用 profiler 收集各组件耗时
2. 识别热点组件（占比 > 30% 的组件）
3. 对热点组件应用上述优化手段
4. 重新测量确认效果

## 6. 最佳实践

### 6.1 开发阶段推荐配置
```
功能开发 → TLM_ONLY + DMI + 大量子 → 最快速度
算法验证 → TLM_ONLY + 时序精确 → 中等速度
RTL 开发 → COMPARE 局部组件 → 较慢但精确
最终验证 → COMPARE 全系统 → 最慢但最可靠
```

### 6.2 DSE 场景优化
- 参数扫描时优先使用 TLM_ONLY
- 仅对 Pareto 前沿附近的配置启用 RTL 验证
- 利用增量仿真（仅重新运行受参数影响的组件）

## 7. CppTLM 统计收集框架

基于 CppTLM 实际代码（`CppTLM/include/metrics/`），框架提供以下性能收集基础设施：

### 7.1 Stats 基类与核心类型

基于 `metrics/stats.hh`（namespace `tlm_stats`），提供四种基础统计类型：

| 类型 | 说明 | 线程安全 |
|------|------|----------|
| `Scalar` | 简单计数器，支持 `++` / `+=` | 原子操作（`std::atomic`） |
| `Average` | 时间加权平均，`Σ(value×cycles)/Σ(cycles)` | 原子操作 |
| `Distribution` | 分布统计（min/max/mean/stddev） | CAS + Kahan 求和 |
| `Formula` | 计算型指标（`std::function<Result()>`） | 只读，不存储状态 |

**抽象基类：**
```cpp
class StatBase {
public:
    virtual void reset() = 0;
    virtual void dump(std::ostream& os, const std::string& path, int width) const = 0;
    virtual std::string unit() const = 0;
    virtual std::string description() const = 0;
};
```

**使用示例：**
```cpp
using namespace tlm_stats;

// Scalar：请求计数
Scalar req_count("Total read requests", "count");
++req_count;
req_count += 10;
uint64_t val = req_count.value();  // 读取当前值

// Average：缓存占用率
Average cache_occupancy("Cache line occupancy", "lines");
cache_occupancy.sample(32, 100);   // 32条线持续100周期
cache_occupancy.sample(48, 200);   // 48条线持续200周期
double avg = cache_occupancy.result();  // 时间加权平均

// Distribution：访存延迟分布
Distribution mem_latency("Memory access latency", "cycle");
mem_latency.sample(5);   // 记录一次 5 周期延迟
mem_latency.sample(120); // 记录一次 120 周期延迟
// 查询：min(), max(), mean(), stddev(), count()

// Formula：缓存命中率（计算型）
Scalar hits("Cache hits", "count");
Scalar misses("Cache misses", "count");
// Formula 通过 StatGroup::addFormula() 注册
```

### 7.2 StatGroup 层次化统计组

`StatGroup` 支持层次化组织统计指标：

```cpp
// 创建统计组
StatGroup cache_stats("cache");

// 添加统计指标
auto& hits = cache_stats.addScalar("hits", "Cache hits", "count");
auto& misses = cache_stats.addScalar("misses", "Cache misses", "count");
auto& latency = cache_stats.addDistribution("latency", "Access latency", "cycle");

// 添加计算型指标
auto& hit_rate = cache_stats.addFormula(
    "hit_rate", "Cache hit rate", "ratio",
    [&]() -> double {
        uint64_t total = hits.value() + misses.value();
        return total > 0 ? (double)hits.value() / total : 0.0;
    }
);

// 添加子组
auto& l2_stats = cache_stats.addSubgroup("l2");
l2_stats.addScalar("evictions", "L2 eviction count", "count");

// 查找（支持路径语法）
StatGroup* found = cache_stats.findSubgroup("l2");
StatBase* stat = cache_stats.findStat("hits");
```

### 7.3 StatsManager 全局统计管理

基于 `metrics/stats_manager.hh`，`StatsManager` 为单例模式的全局管理中枢：

```cpp
auto& mgr = tlm_stats::StatsManager::instance();

// 注册统计组（按层次路径）
mgr.register_group(&cache_stats, "system.cache");
mgr.register_group(&mem_stats, "system.memory");
mgr.register_group(&cpu_stats, "system.cpu");

// 查找
mgr.find_group("system.cache");  // 返回 StatGroup*

// 输出所有统计
mgr.dump_all(std::cout, 50);
std::string text = mgr.dump_text(50);

// 重置所有统计
mgr.reset_all();
```

**输出格式示例（gem5 风格）：**
```
---------- Begin Simulation Statistics ----------
system.cache.hits                                       12345  # Cache hits (count)
system.cache.misses                                      1234  # Cache misses (count)
system.cache.hit_rate                                0.909091  # Cache hit rate (ratio)
system.cache.latency.count                              13579  # Access latency sample count (cycle)
system.cache.latency.min                                    2  # Access latency minimum (cycle)
system.cache.latency.avg                               12.500  # Access latency average (cycle)
system.cache.latency.max                                  200  # Access latency maximum (cycle)
---------- End Simulation Statistics ----------
```

### 7.4 PercentileHistogram 百分位直方图

基于 `metrics/histogram.hh`，提供指数桶百分位统计：

```cpp
// 通过 StatGroup 注册
auto& hist = cache_stats.addPercentileHistogram(
    "read_latency", "Read latency distribution", "cycle"
);

// 记录样本
hist.record(5);    // 5 cycles
hist.record(12);   // 12 cycles
hist.record(150);  // 150 cycles (tail latency)

// 查询百分位
int64_t median = hist.p50();      // 50th percentile
int64_t tail   = hist.p95();      // 95th percentile
int64_t worst  = hist.p99();      // 99th percentile
int64_t ultra  = hist.p99_9();    // 99.9th percentile
int64_t custom = hist.percentile(99.5);  // 任意百分位
```

**实现特点：**
- **32 个指数桶**：桶边界为 2ⁿ，覆盖 1 到 2³² 的范围
- **内存开销固定**：每个直方图仅占 32×8 = 256 字节（计数器）
- **线程安全**：所有操作使用 `std::atomic` + CAS
- **线性插值**：桶内使用线性插值提高精度

### 7.5 MetricsReporter 多格式报告

基于 `metrics/metrics_reporter.hh`，支持多种导出格式：

| Reporter | 格式 | 适用场景 |
|----------|------|----------|
| `TextReporter` | gem5 对齐列格式 | 终端查看、CI 日志 |
| `JSONReporter` | 嵌套 JSON | 程序化分析、DSE 数据源 |
| `MarkdownReporter` | Markdown 表格 | 文档/报告生成 |
| `MultiReporter` | 同时多格式 | 生产环境全量导出 |

```cpp
using namespace tlm_stats;

// 单格式导出
TextReporter text_rpt(50);
text_rpt.generate("output/metrics.txt");

JSONReporter json_rpt;
std::string json_str = json_rpt.generateToString();

// 多格式同时导出（自动创建目录）
MultiReporter multi;
multi.generate_all("output/stats");
// 生成：output/stats/metrics.txt
//       output/stats/metrics.json
//       output/stats/metrics.md
```

### 7.6 端口统计（PortStats）

基于 `core/port_stats.hh`，每个 ch_stream 端口自动收集传输统计：

```cpp
struct PortStats {
    uint64_t req_count;       // 请求包数量
    uint64_t resp_count;      // 响应包数量
    uint64_t byte_count;      // 总字节数
    uint64_t total_delay;     // 累计延迟（cycles）
    uint64_t min_delay;       // 最小延迟
    uint64_t max_delay;       // 最大延迟
    // 信用流控专用
    uint64_t credit_sent;     // 发出的信用包数
    uint64_t credit_received; // 接收的信用包数
    uint64_t credit_value;    // 实际传递的信用额度

    void reset();
    void merge(const PortStats& other);
    std::string toString() const;
};
```

### 7.7 事务追踪与性能分析

基于 `framework/transaction_tracker.hh`，支持端到端延迟测量：

```cpp
auto& tracker = TransactionTracker::instance();

// 创建事务并记录路径
uint64_t tid = tracker.create_transaction(payload, "CPU", "READ");
tracker.record_hop(tid, "L1Cache", 2, "MISS");
tracker.record_hop(tid, "Interconnect", 3, "ROUTE");
tracker.record_hop(tid, "L2Cache", 8, "HIT");
tracker.complete_transaction(tid);

// 查询端到端延迟
const auto* record = tracker.get_transaction(tid);
uint64_t e2e_latency = record->complete_timestamp - record->create_timestamp;

// 检测潜在瓶颈：查找活跃时间最长的事务
auto active = tracker.get_active_transactions();
// 活跃事务数 > 阈值可能表示瓶颈

// 结合 PercentileHistogram 统计延迟分布
PercentileHistogram latency_hist("E2E read latency", "cycle");
latency_hist.record(e2e_latency);
// 分析 p99 tail latency 定位性能问题
```

### 7.8 完整使用示例

```cpp
#include "metrics/stats.hh"
#include "metrics/histogram.hh"
#include "metrics/stats_manager.hh"
#include "metrics/metrics_reporter.hh"

using namespace tlm_stats;

// 模块初始化时注册统计
class MyCache {
    StatGroup stats_{"cache"};
    Scalar& read_hits_  = stats_.addScalar("read_hits", "Read hit count");
    Scalar& read_miss_  = stats_.addScalar("read_miss", "Read miss count");
    Distribution& lat_  = stats_.addDistribution("latency", "Access latency", "cycle");
    PercentileHistogram& pct_ = stats_.addPercentileHistogram(
        "latency_pct", "Latency percentiles", "cycle");
    Formula& hit_rate_  = stats_.addFormula(
        "hit_rate", "Read hit rate", "ratio",
        [this]() -> double {
            uint64_t total = read_hits_.value() + read_miss_.value();
            return total > 0 ? (double)read_hits_.value() / total : 0.0;
        });

public:
    MyCache() {
        // 注册到全局管理器
        StatsManager::instance().register_group(&stats_, "system.cache");
    }

    void handle_read(uint64_t addr) {
        uint64_t start = get_sim_time();
        bool hit = lookup(addr);
        uint64_t latency = get_sim_time() - start;

        if (hit) ++read_hits_; else ++read_miss_;
        lat_.sample(latency);
        pct_.record(latency);
    }
};

// 仿真结束时导出
void on_simulation_end() {
    MultiReporter reporter;
    reporter.generate_all("output/stats");

    // 或者输出到终端
    StatsManager::instance().dump_all(std::cout, 50);
}
```

## 8. 相关文档
- [项目架构总览](overview.md)
- [测试与 DSE 框架](testing-and-dse.md)
- [错误处理机制](error-handling.md)
