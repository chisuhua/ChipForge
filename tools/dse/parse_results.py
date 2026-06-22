#!/usr/bin/env python3
"""
parse_results.py — 重新解析 cpu_sim stdout 为统一 JSON

用法:
  python3 parse_results.py <sweep.json> > <out.json>

行为:
  - 主路径: sweep_driver.py 已直接解析 cpu_sim stdout 为 KEY=VALUE 字段,
    本脚本读取 sweep.json 后直接 dump 格式化输出 (indent=2)
  - 兜底路径: 若 sweep.json 中某 entry 包含 "raw_stdout" 字段,
    重新 parse_kv() 解析后 update 进 entry (兼容未来 sweep_driver 存储 raw stdout)

约束:
  - 仅依赖 Python 3 标准库 (无第三方依赖)
  - 输出统一格式 JSON (indent=2, UTF-8, ensure_ascii=False 不强求, 因 cfg name 全 ASCII)

设计: M5-DSE M5.15 DSE Sweep toolchain §6.6
"""
import json
import sys
from pathlib import Path


def parse_kv(stdout):
    """解析 'KEY=VALUE' 行为 dict, 类型自动推断 (int/float/str)"""
    result = {}
    for line in stdout.split("\n"):
        if "=" in line and not line.startswith("#"):
            k, _, v = line.partition("=")
            v_strip = v.strip()
            try:
                # 优先 int (无小数点), 然后 float (有小数点), 否则 str
                if "." in v_strip:
                    result[k.strip()] = float(v_strip)
                elif v_strip.isdigit() or (v_strip.startswith("-") and v_strip[1:].isdigit()):
                    result[k.strip()] = int(v_strip)
                else:
                    result[k.strip()] = v_strip
            except ValueError:
                result[k.strip()] = v_strip
    return result


def main():
    if len(sys.argv) != 2:
        print("Usage: parse_results.py <sweep.json>", file=sys.stderr)
        sys.exit(1)
    sweep_json = Path(sys.argv[1])
    if not sweep_json.is_file():
        print(f"FAIL: input not found: {sweep_json}", file=sys.stderr)
        sys.exit(1)

    data = json.loads(sweep_json.read_text())
    # 兜底: 若 entry 含 raw_stdout (未来 sweep_driver 升级后存储), 重新解析 KV
    for r in data:
        if "raw_stdout" in r:
            kv = parse_kv(r["raw_stdout"])
            r.update(kv)

    print(json.dumps(data, indent=2))


if __name__ == "__main__":
    main()