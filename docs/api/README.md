# ChipForge API 参考

cf::plugin 命名空间的公共 API 文档。

## 文档

- [`cf_plugin`](cf_plugin.md) — Phase 0 Plugin 脚手架完整 API 参考

## 生成方式

Phase 0 阶段由于环境限制 (doxygen 不可安装), API 文档手动维护。

未来迁移路径 (Phase 6 后):
1. 安装 doxygen: `apt install doxygen`
2. 在头文件添加 `@brief/@param/@return` 标签
3. `docs/Doxyfile` 自动生成 HTML + LaTeX
4. CI 集成: `make docs` 触发构建

## 设计原则

- **自解释优先**: 代码本身 + 文件头 (中文, AGENTS.md 强制) 应足以理解
- **示例驱动**: 每个公共 API 配 1-2 个最小示例
- **编译期保证**: 静态断言 + 模板约束 (在文档中显式列出)
- **运行时保证**: 测试覆盖 (引用 test_*.cpp 文件)
