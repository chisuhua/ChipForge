# fixture e —— 代码围栏豁免

下面的命令输出是**代码**，不是正文断言：

```bash
$ python3 tools/parity/parity_gate.py --layer pr
verdict=PASS  # 当前实现已落地
已实现 16/16
```

★ 反引号内的**行内代码不属豁免范围**（避免把正文断言藏进反引号里逃检）。
