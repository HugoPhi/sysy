# 如何测试

- 测试s0: 
```bash
python test.py s0 &> test0.txt && cat log.txt && grep "'retval': 1" test0.txt > error0.txt
```
- 测试s1: 
```bash
python test.py s1 &> test1.txt && cat log.txt && grep "'retval': 1" test1.txt > error1.txt
```

这段命令的作用是：
1. 执行 `test.py` 脚本，传入参数 `s0` 或 `s1`。
2. 将标准输出和标准错误输出重定向到 `test0.txt` 或 `test1.txt` 文件中。
3. 使用 `cat log.txt` 命令显示 `log.txt` 文件的内容。
4. 使用 `grep` 命令从 `test0.txt` 或 `test1.txt` 中筛选出包含 `'retval': 1` 的行，并将结果输出到 `error0.txt` 或 `error1.txt` 文件中。
