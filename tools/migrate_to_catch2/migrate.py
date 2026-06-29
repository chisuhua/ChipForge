#!/usr/bin/env python3
"""
migrate.py: 统一调度四个转换器，把单个测试文件从「纯 main+assert」或「GTest」模式迁移到 Catch2。

用法:
    python3 migrate.py cache/test_replacement_policy.cpp --family cache
    python3 migrate.py --all    # 遍历 tests/ 下所有 test_*.cpp

执行顺序（重要）:
    1. transform_assert.py    (先转 assert)
    2. transform_check_macro.py (移除手写 CHECK 宏)
    3. transform_gtest.py     (GTest → Catch2 宏, 仅 mmu/ 用)
    4. transform_main.py      (最后转 main+static void 为 TEST_CASE)

输出:
    - 原地修改文件
    - 打印每个文件的转换计数
    - 失败时退出码 1
"""
import argparse
import subprocess
import sys
from pathlib import Path

FAMILY_BY_DIR = {
    'tests/framework': 'framework',
    'tests/cache': 'cache',
    'tests/cpu': 'cpu',
    'tests/cpu/integration': 'cpu-integration',
    'tests/cpu/configs': 'cpu-configs',
    'tests/soc': 'soc',
    'tests/bundles': 'bundles',
    'tests/mmu': 'mmu',
}

def detect_family(path: Path) -> str:
    p = str(path)
    for prefix, family in FAMILY_BY_DIR.items():
        if p.startswith(prefix):
            return family
    return 'unknown'

def migrate_file(path: Path) -> bool:
    family = detect_family(path)
    print(f"\n=== Migrating {path} (family={family}) ===")
    
    # Step 1: transform_assert (assert 阵营)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_assert.py', str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: assert transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 2: transform_check_macro (手写 CHECK 宏)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_check_macro.py', str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: check_macro transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 3: transform_gtest (GTest → Catch2, 仅 mmu/ 用)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_gtest.py', family, str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: gtest transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    # Step 4: transform_main (最后转 main+static void)
    r = subprocess.run(['python3', 'tools/migrate_to_catch2/transform_main.py', family, str(path)],
                        capture_output=True)
    if r.returncode != 0:
        print(f"  FAIL: main transform: {r.stderr.decode()}")
        return False
    if r.stdout:
        print(f"  {r.stdout.decode().strip()}")
    
    return True

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('files', nargs='*', help='Specific test files to migrate')
    ap.add_argument('--all', action='store_true', help='Migrate all test_*.cpp under tests/')
    args = ap.parse_args()
    
    if args.all:
        files = sorted(Path('tests').rglob('test_*.cpp'))
    else:
        files = [Path(f) for f in args.files]
    
    if not files:
        print("No files to migrate")
        sys.exit(1)
    
    failed = []
    for f in files:
        if not migrate_file(f):
            failed.append(f)
    
    print(f"\n=== Migration complete ===")
    print(f"  Total: {len(files)}")
    print(f"  Failed: {len(failed)}")
    if failed:
        for f in failed:
            print(f"    - {f}")
        sys.exit(1)

if __name__ == '__main__':
    main()