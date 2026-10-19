# Contributing to ChipForge

> 感谢您考虑为 ChipForge 做贡献!

## 1. 项目简介

ChipForge 是基于 CppTLM + CppHDL 的 RISC-V 虚拟验证平台。
详细信息请阅读 [README.md](README.md) 和 [docs/architecture/overview.md](docs/architecture/overview.md)。

## 2. 开发环境

- C++17/20 编译器(GCC 12+ 或 Clang 14+)
- CMake 3.20+
- Python 3.12+(用于文档检查和测试)
- 外部依赖通过符号链接 `CppTLM` 和 `CppHDL` 提供(详见 [docs/DEVELOPMENT_SETUP.md](docs/DEVELOPMENT_SETUP.md))

## 3. Commit 规范

本项目使用 [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>
```

**Type 取值**: `feat`, `fix`, `docs`, `chore`, `test`, `refactor`
**Scope 示例**: `ci`, `schema`, `tools`, `arch`, `ip/cpu`, `ip/cache`

### 3.1 文档改动按文件粒度 (2026-10-19)

为避免多人协作 PR 冲突, 涉及以下目录的改动应**按文件粒度分 commit**:

- `.rddf/roadmap/{strategy,phases,features,objectives}/**` (路线图 SSOT)
- `AGENTS.md` 顶部 auto-gen block
- `CHANGELOG.md`
- `docs/architecture/**` (架构演进)

**反例**: 一个 PR 同时改 6 个 `feat-*.md` = 6 个 reviewer 必冲突.
**正例**: 1 PR 含 6 commit, 每个改 1 个 `feat-*.md` (auto-gen block 重生单独 commit) = git 自动 merge.

## 4. PR 流程

1. 从 `main` 创建 feature branch:`git checkout -b fix/<description>`
2. 提交前运行 `python3 tools/doc_checker.py` 确认通过
3. 运行 `python3 -m unittest tools.test_doc_checker -v` 确认测试通过
4. 提交 PR,使用提供的 PR 模板填写变更说明
5. 等待 review (SLA: 3 个工作日)

## 5. Review SLA

- 首次 review: 3 个工作日内
- 后续 review: 1 个工作日内
- 紧急修复(hotfix): 24 小时内

如有问题,请通过 GitHub Issues 联系我们。
