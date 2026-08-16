# Debug 事件记录

格式：现象 → 根因 → 解法 → 教训。新事件追加在最上面。

---

## 2026-08-16 PowerShell 测试脚本全员"compile error"

**现象**：目录重组后 `run_tests.ps1` 每个用例都报 compile error，
但命令行手动跑编译器完全正常。

**根因**：往 `.ps1` 里加了中文注释，而文件是无 BOM 的 UTF-8。
Windows PowerShell 5.1 对无 BOM 脚本按 ANSI（本机 GBK）解码，
中文注释变成乱码并吞掉了下一行的赋值，导致 `$asm` 变量为空，
编译器收到错误参数退出码非 0。

**解法**：脚本内的注释改为纯 ASCII。

**教训**：Windows PowerShell 5.1 的 .ps1 文件要么纯 ASCII，
要么存成带 BOM 的 UTF-8。脚本行为诡异时先怀疑编码。
（排查过程中还被 bash 抢先展开 `$LASTEXITCODE`、printf 把 `\t`
转义成 TAB 干扰了两次——复现脚本本身也可能是污染源。）

## 2026-08-15 e2e 驱动读出乱码：Lexer 悬垂引用

**现象**：端到端驱动读源文件，lexer 报一堆"unexpected character"。

**根因**：`Lexer lexer(oss.str());`——`oss.str()` 是临时 string，
`Lexer` 内部存 `const std::string&`，整行结束后临时对象析构，
lexer 读到已释放内存。

**解法**：源码绑定到命名变量再构造 lexer。

**教训**：以 const 引用存成员的类，传入临时对象编译器不会拦。
警惕"存引用"的构造函数。

## 2026-08-14 IR 程序退出时崩溃：析构顺序与悬空观察者

**现象**：IR dump 输出正确，程序退出阶段抛
`remove_use: didn't find`。

**根因**：`Module` 成员销毁顺序（声明逆序）导致常量池先于
functions 销毁；指令析构时对已死的 ConstantInt 调 remove_use。
while 回边还有块间交叉引用问题（前面的块先死，后面块的 JMP
再摘引用即悬空）。

**解法**：①调整成员声明顺序让 functions 先销毁；②给 Function
加析构，先遍历所有指令 `drop_operands()`（趁大家都活着先解除
全部交叉引用），再正常销毁。即 LLVM 的 dropAllReferences 模式。

**教训**：双向链/观察者结构销毁时必须"先解引用，后销毁"。
fail-fast 的 remove_use（找不到即抛）把潜在 UB 变成了明确异常。

## 2026-08-13 make_inst 全部指令抛 "unknown op"

**现象**：任何合法指令构建都抛异常。

**根因**：switch 的 case 分支全部漏写 `break`，控制流一路贯穿
到 default。

**解法**：逐分支补 `break`。

**教训**：`switch` 贯穿是 C 系经典坑；`-Wimplicit-fallthrough`
可以让编译器帮忙抓。
