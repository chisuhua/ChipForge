# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 0.0.x   | :white_check_mark: |

## Reporting a Vulnerability

**Please do NOT file a public GitHub issue for security vulnerabilities.**

请通过以下方式私下报告安全问题:
- Email: security@chipforge.local
- GitHub Security Advisories: https://github.com/chisuhua/ChipForge/security/advisories/new

我们承诺:
- 24 小时内确认收到
- 72 小时内给出初步评估
- 严重漏洞 7 天内修复并发布补丁

## Scope

- 文档健康度检查工具中的代码执行风险
- CI workflow 中的权限升级
- 配置文件中的敏感信息泄露

不在范围:
- 已知依赖项(CppTLM/CppHDL)的漏洞(请向对应项目报告)