# ChipForge 工具集

## doc_checker.py — 文档健康度检查

自动检查项目文档的完整性、链接有效性和规范合规性。

### 使用方式

```bash
# 运行所有检查（文本格式）
python tools/doc_checker.py

# 详细输出
python tools/doc_checker.py --verbose

# JSON 格式输出（供 CI 使用）
python tools/doc_checker.py --format json

# 仅运行特定检查
python tools/doc_checker.py --check links
python tools/doc_checker.py --check ip
python tools/doc_checker.py --check json
python tools/doc_checker.py --check deprecated
python tools/doc_checker.py --check framework
```

### 检查项

| 检查 | 说明 | CI 阻断 |
|------|------|---------|
| 文件完整性 | 验证必需文档是否存在 | ✓ |
| 链接有效性 | 检查 Markdown 相对链接目标 | ✓ |
| IP 结构合规 | 验证 IP 目录遵循模板 | ✓ |
| JSON Schema | 校验配置文件格式 | ✓ |
| 旧引用残留 | 检测已弃用的文件名 | ✓ |
| 框架路径 | 验证代码映射文档路径 | ⚠️ (需要 CppTLM/CppHDL) |

### CI 集成

文档检查已集成到 GitHub Actions，在以下情况自动触发：
- push 到 main/develop 分支且修改了 docs/ 或 ip/
- PR 修改了 docs/ 或 ip/

检查失败时 PR 无法合并，并会在 PR 中添加详细报告评论。

### 添加新的检查规则

在 `doc_checker.py` 的 `DocChecker` 类中添加新方法，并在 `run_all_checks()` 中注册即可。
