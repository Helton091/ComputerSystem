# Debug 事件记录

格式：现象 → 根因 → 解法 → 教训。新事件追加在最上面。

---

## 2026-08-20 综合测试触发 Module 析构崩溃

**现象**：新增 `integration/counter.cmm`（全局变量 + 多函数调用）后，
`compiler.exe` 崩溃：`[IR] when remove user t1 in , didn't find t1`。
奇怪的是只调用两次 `inc()` 时不崩溃，三次才崩溃。

**根因**：[IR.hpp](file:///c:/Users/helton/Desktop/%E5%AD%A6%E4%B9%A0%E6%96%87%E4%BB%B6/compiler/src/IR.hpp#L138-L149)
中 `Module` 的析构顺序是：先按 `functions` vector 顺序析构 `Function`，
再析构 `GlobalVariable`。`CALL` 指令把被调函数 `Function*` 作为第一个操作数，
因此 `main()` 中的 `call @inc` 引用着 `inc()` 对应的 `Function`。
当 `inc()` 先于 `main()` 析构时，`main()` 的 `CALL` 变成了悬空指针；
后续 `main()` 析构，其 `CALL` 尝试从已释放的 `Function` 中移除 use，
导致 use 链损坏，最终在另一个临时指令上爆出 `didn't find`。

**解法**：给 `Module` 添加显式析构函数，在 `Function` 对象被释放之前，
先遍历所有基本块并调用 `Instruction::drop_operands()`，
切断跨函数引用（尤其是 `CALL` → `Function`），避免访问已释放对象。

**教训**：
1. 拥有图/引用关系的对象（`Module` → `Function` → `Instruction`）
   析构顺序必须显式控制，不能依赖 `unique_ptr` 默认顺序。
2. 单元测试往往只覆盖单个函数或简单场景，综合测试更容易暴露
   对象生命周期和跨组件引用问题。
3. “偶发”崩溃（调用两次不崩、三次崩）通常是 use-after-free 或内存损坏，
   值得优先怀疑对象所有权和析构顺序。

---

## 2026-08-20 全局变量写入测试报 compile error

**现象**：新增 `global/write.cmm` 和 `global/float_write.cmm` 后，
`run_tests.ps1` 显示 `[FAIL] compile error`，但错误细节被脚本吞掉；
手动单步编译才看到 `the init value of global variable must be literal number`。

**根因**：[AST2IR.cpp](file:///c:/Users/helton/Desktop/%E5%AD%A6%E4%B9%A0%E6%96%87%E4%BB%B6/compiler/src/AST2IR.cpp#L54-L68)
在翻译全局变量时，只接受字面量初始化，对未初始化的全局变量
（如 `int g;`、`float g;`）直接抛异常。但零初始化全局变量是合法语法。

**解法**：当全局变量没有显式初始化时，按类型补默认值：
`int` 补常量 `0`，`float` 补常量 `0.0f`。

**教训**：
1. 测试脚本用 `2>&1 | Out-Null` 吞掉 stderr 是这次调试费劲的主要原因；
   编译失败时应把错误信息打印出来，否则只能盲人摸象。
2. 错误消息只说了"must be literal number"，没报变量名、文件、行号，
   定位慢。异常信息应带上下文（标识符、当前函数/阶段）。
3. 新增语法覆盖时，别忘了给"默认/缺省"分支写测试。

---

## 2026-08-19 float 测试死循环：li 伪指令长度导致标签地址错位

**现象**：新增 `float/literal` 测试时，simulator 在 `j .Lmain_epilogue`
处无限循环，`fa0` 已经正确赋值为 `2.500000` 但程序不退出。

**根因**：`li t0, 1075838976` 的立即数超出 12 位，assembler 实际展开为
`lui + addi`，占 **8 字节**。但 [assembler.hpp](file:///c:/Users/helton/Desktop/学习文件/compiler/src/assembler.hpp#L27-L30) 中的
`get_instruction_size` 对所有指令都返回 **4**，导致 Pass1 给后续标签计算
的地址比实际小了 4。于是 `j .Lmain_epilogue` 跳转到了自己，形成死循环。

**解法**：让 `get_instruction_size` 识别 `li`：当立即数在 `[-2048, 2047]`
范围内时返回 4，否则返回 8。

**教训**：伪指令在 Pass1（算标签地址）和 Pass2（真正 emit）时的长度必须
一致；标签地址错位会让跳转/调用出现非常隐蔽的死循环。

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
