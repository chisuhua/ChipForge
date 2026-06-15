# SoC 集成指南

> 本文档面向**创建/修改/调试 SoC 配置**的工程师。
> 阅读本文前建议先浏览 `soc/README.md` 了解 .json 文件清单。

## 1. 配置文件结构

每个 SoC 配置文件 (`*.json`) 描述一个 SoC 的完整实例化清单。CMake 构建系统读取它产出可执行文件。

最小结构示例:

```json
{
  "name": "riscv_virt",
  "description": "baseline RISC-V virtual platform",
  "ips": [
    { "name": "cpu", "instance": "cpu0", "params": { "isa": "rv64imac" } },
    { "name": "cache", "instance": "l1", "params": { "size_kb": 32 } },
    { "name": "memory", "instance": "ram", "params": { "size_mb": 256 } }
  ],
  "memory_map": [
    { "base": "0x80000000", "size": "0x10000000", "device": "ram" }
  ],
  "build": {
    "target": "tlm_sim",
    "trace": true
  }
}
```

### 字段语义

| 字段 | 必填 | 说明 |
| --- | --- | --- |
| `name` | ✓ | SoC 唯一标识; CMake target 名从此派生 |
| `description` | ✗ | 给人看的一行描述,出现在 build log 和 help 中 |
| `ips[]` | ✓ | 实例化的 IP 列表; 顺序无关,构建系统按依赖图拓扑排序 |
| `ips[].name` | ✓ | 对应 `ip/<name>/` 目录的 IP 名称 |
| `ips[].instance` | ✓ | 在该 SoC 中的实例 ID, 跨 SoC 必须唯一 |
| `ips[].params` | ✗ | 运行时参数, 由对应 IP 的 `configuration.md` 文档定义 |
| `memory_map[]` | ✓ | 地址空间划分; `device` 字段对应 `ips[].instance` |
| `build.target` | ✗ | `tlm_sim` / `rtl_sim` / `cosim` 之一, 默认 `tlm_sim` |
| `build.trace` | ✗ | 是否开启波形/事务级 trace, 默认 false |

## 2. 添加新 IP 到 SoC

1. 确认 IP 已存在于 `ip/<your_ip>/` 且有 `README.md` 描述
2. 在该 IP 的 `docs/configuration.md` 中声明可调参数(可选, 但强烈建议)
3. 在目标 SoC 的 `.json` 中 `ips[]` 添加一项
4. 如有新增地址空间, 在 `memory_map[]` 添加对应项
5. 跑 `ctest` 验证仿真启动, 跑相关集成测试

## 3. 创建一个新 SoC

1. **复制最近邻的 SoC**: `cp soc/<closest>.json soc/<your_soc>.json`
2. 修改 `name`、`description`、IP 清单和地址映射
3. 跑 `cmake --build build` 触发新 SoC target 生成
4. 跑 `ctest -R <your_soc_target>` 验证
5. 在 `soc/README.md` 的速查表中添加新行

## 4. 常见错误

| 现象 | 原因 | 修复 |
| --- | --- | --- |
| CMake 报 "unknown IP: xxx" | `ips[].name` 拼错或对应 `ip/<xxx>/` 不存在 | 对照 `ip/` 目录核对大小写 |
| 启动时 SIGSEGV | `memory_map[]` 区域重叠 | 用 `tools/verify_memory_map.py` (TODO) 检查 |
| IP 启动后不响应 | 缺少跨 IP 桥接(bus adapter) | 确认 `interconnect` IP 已包含此 instance |
| TLM 仿真死锁 | bus timeout 触发 | 调高 SoC 级 `bus_timeout_cycles` 参数 |

## 5. 进一步阅读

- IP 索引: [`docs/architecture/ip-catalog.md`](../../docs/architecture/ip-catalog.md)
- 跨 IP 接口约定: [`docs/architecture/interfaces.md`](../../docs/architecture/interfaces.md)
- 各 IP 可配参数: `ip/<name>/docs/configuration.md`
