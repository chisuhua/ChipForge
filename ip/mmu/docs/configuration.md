# MMU 配置参数

详见 [configs/params_schema.json](../configs/params_schema.json) — JSON Schema draft-07 完整定义。

## 顶层结构

```json
{
  "name": "mmu_default",
  "type": "mmu",
  "impl_mode": "TLM_ONLY",
  "params": {
    "topology": "unified",
    "asid_bits": 9,
    "sv_mode": "sv39",
    "supported_page_sizes": [4096, 2097152, 1073741824],
    "ptw_max_inflight": 2,
    "shadow_fill_from_next": true,
    "levels": [
      { "name": "L0", "entries": 8,  "associativity": 8, "num_lookup_ports": 1, "lookup_latency_cycles": 1, "replacement_policy": "FIFO" },
      { "name": "L1", "entries": 64, "associativity": 4, "num_lookup_ports": 1, "lookup_latency_cycles": 2, "replacement_policy": "LRU" }
    ]
  }
}
```

## 字段详表

### 顶层字段

| 字段 | 类型 | required | 默认 | 说明 |
|------|------|----------|------|------|
| `name` | string | ✓ | - | 实例名称（unique） |
| `type` | string | ✓ | - | 固定为 `"mmu"`（IP 类型标识） |
| `impl_mode` | string | ✗ | `"TLM_ONLY"` | `TLM_ONLY` / `RTL_ONLY` / `COMPARE` / `SHADOW` |
| `params` | object | ✓ | - | 见下 |

### params 字段

| 字段 | 类型 | required | 默认 | 范围/枚举 | 说明 |
|------|------|----------|------|----------|------|
| `topology` | string | ✗ | `"unified"` | `["unified", "split_id"]` | TLB 拓扑。`split_id` 推迟到 `mmu-tlb-ptw-impl`+ |
| `asid_bits` | int | ✗ | `9` | 0-16 | ASID/VMID 位宽。0=无 ASID, 9=RISC-V sv39, 12-16=GPU |
| `sv_mode` | string | ✗ | `"sv39"` | `["Bare", "Sv32", "Sv39", "Sv48"]` | RISC-V 虚拟内存模式 |
| `supported_page_sizes` | array[int] | ✗ | `[4096, 2097152, 1073741824]` | 4096-1073741824 | 支持的 page size (字节) |
| `ptw_max_inflight` | int | ✗ | `2` | 1-8 | PTW 状态机可同时处理的 walk 数 |
| `shadow_fill_from_next` | bool | ✗ | `true` | - | 浅层是否从深层 hit/PTW 回填 |
| `levels` | array | ✓ | required | 1-4 项 | TLB 层级定义 |

### levels[] 字段

| 字段 | 类型 | required | 默认 | 范围/枚举 | 说明 |
|------|------|----------|------|----------|------|
| `name` | string | ✓ | - | - | 层级名（L0/L1/L2） |
| `entries` | int | ✓ | - | 1-4096 | entry 总数 |
| `associativity` | int | ✓ | - | 1-64 | 关联度（路数） |
| `num_lookup_ports` | int | ✗ | `1` | 1-8 | 并行查找端口数（GPU 4-8, CPU 1-2） |
| `lookup_latency_cycles` | int | ✗ | `1` | 0-16 | 查找延迟（周期） |
| `replacement_policy` | string | ✗ | `"LRU"` | `["None", "FIFO", "LRU", "RRIP"]` | 替换策略 |

## 典型配置示例

### 1. 嵌入式 RV32 (无 MMU, Bare mode)
```json
{
  "name": "mmu_embedded_bare",
  "type": "mmu",
  "params": {
    "sv_mode": "Bare",
    "asid_bits": 0,
    "supported_page_sizes": [4096],
    "levels": [
      { "name": "L0", "entries": 4, "associativity": 1, "replacement_policy": "None" }
    ]
  }
}
```

### 2. 典型 RISC-V CPU (5 级流水线 + sv39)
```json
{
  "name": "mmu_rv64_sv39",
  "type": "mmu",
  "params": {
    "asid_bits": 9,
    "sv_mode": "sv39",
    "supported_page_sizes": [4096, 2097152, 1073741824],
    "levels": [
      { "name": "L0", "entries": 8,  "associativity": 8, "replacement_policy": "FIFO" },
      { "name": "L1", "entries": 64, "associativity": 4, "replacement_policy": "LRU" }
    ]
  }
}
```

### 3. GPU 风格 (unified + 高端口 + 多种 page size)
```json
{
  "name": "mmu_gpu_unified",
  "type": "mmu",
  "params": {
    "asid_bits": 16,
    "sv_mode": "sv39",
    "supported_page_sizes": [4096, 65536, 2097152, 1073741824],
    "ptw_max_inflight": 4,
    "shadow_fill_from_next": false,
    "levels": [
      { "name": "L0", "entries": 16,  "associativity": 16, "num_lookup_ports": 4, "replacement_policy": "FIFO" },
      { "name": "L1", "entries": 256, "associativity": 8,  "num_lookup_ports": 4, "replacement_policy": "LRU" }
    ]
  }
}
```

## DSE 扫描维度

```
topology:          [unified, split_id]              // 2
levels.count:      [1, 2, 3]                         // 3
levels[].entries:  [4, 8, 16, 32, 64, 128, 256]     // 7
levels[].ways:     [1, 2, 4, 8]                      // 4
levels[].policy:   [None, FIFO, LRU, RRIP]           // 4
levels[].ports:    [1, 2, 4]                         // 3
asid_bits:         [0, 9, 12, 16]                    // 4
sv_mode:           [Bare, Sv39]                      // 2
supported_pages:   [4KB-only, +2MB, +1GB, +64KB]    // 4
ptw_max_inflight:  [1, 2, 4, 8]                     // 4
shadow_fill:       [true, false]                     // 2
```

≈ 200+ 组合 (实际典型 ~50-100 受白名单约束)。

## 验证工具

```bash
# JSON Schema 验证
python3 -c "import json, jsonschema; json.load(open('ip/mmu/configs/params_schema.json'))"  # syntax check
python3 -c "import json, jsonschema; s=json.load(open('ip/mmu/configs/params_schema.json')); jsonschema.validate(json.load(open('soc/mmu_default.json')), s)"  # config 验证

# ctest
ctest -L mmu -R mmu_config
```
