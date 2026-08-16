# C-- 编译器 IR 重构教学报告（新手版）

> 这份文档讲清楚一件事：我们为什么要把 IR 相关的 4 个文件
> （`IR.hpp`、`IR.cpp`、`IR2RISCV.hpp`、`IR2RISCV.cpp`）全部重写，
> 以及新的 IR 长什么样、怎么从 AST 翻译过来。
>
> 阅读前提：只要你看过 `ast.hpp` 和 `parser.cpp`，知道我们的编译器
> 能把 C-- 代码解析成一棵语法树就够了。其他概念本文都会从头解释。

---

## 第 0 章 先修小词典（不懂的术语先在这里查）

**IR（中间表示，Intermediate Representation）**
源代码和机器码之间的"中间语言"。我们的编译器流程是：

```
源代码 → AST（语法树）→ IR → RISC-V 汇编 → 机器码
```

为什么不直接从 AST 生成汇编？因为 AST 长得像"人写的代码"（有嵌套、
有括号、有优先级），而汇编是"一条一条的直线指令"。IR 是两者之间的
台阶：它把嵌套的表达式拆成一步一条的简单指令，但又不像汇编那样
关心寄存器、栈偏移这些硬件细节。

**三地址码**
IR 最常见的形式。每条指令最多三个"地址"：两个操作数 + 一个结果。
比如 `a + b * 2` 会被拆成：

```
t1 = b * 2
t2 = a + t1
```

**基本块（Basic Block）**
一段"只能从头进、从尾出"的指令序列：中间没有跳转进来，也没有
跳转出去，只有最后一条指令可以是跳转或 return。一个函数 = 若干
基本块 + 块之间的跳转关系（这张跳转关系图叫**控制流图 / CFG**）。

**SSA（静态单赋值，Static Single Assignment）**
一种 IR 风格：每个变量在整个程序里只被赋值一次。每次"重新赋值"
其实都改用一个新名字：

```
a = 1        →   a1 = 1
a = a + 2    →   a2 = a1 + 2
```

好处：看到 `a2` 就能立刻知道它是谁算出来的（`a1 + 2`），不用翻
上下文。优化算法几乎都假设 IR 是 SSA 形式。

**phi 节点（φ）**
SSA 的配套装置。问题：如果变量在 if 的两个分支里各被赋值一次，
汇合之后它是哪个版本？答案是在汇合点放一条假想的"选择指令"：

```
if (c) a = 1; else a = 2;
→
if (c) a1 = 1; else a2 = 2;
汇合点: a3 = phi(a1, a2)   // 从哪条路来的就取哪个值
```

phi 不是真实机器指令，后端生成汇编时会把它消解掉。

**use-def 链（使用-定义链）**
双向的"谁用我 / 我用谁"记录。比如指令 `t2 = a + t1`：
- 它的 def（定义）是 `a` 和 `t1` ——它*使用*了这两个值；
- 反过来看 `t1`，它的 use 列表里记着"我被 `t2 = a + t1` 用了"。

有了这张网，问"如果删掉 t1 的定义会有谁受影响"，看一眼列表就
知道，不用搜遍整个函数。

**支配（dominance）**
如果从函数入口出发，到达基本块 B 的*每一条*路都必须先经过 A，
就说 A 支配 B。这个概念只在将来做 SSA 转换时用到，现在只需
知道有这个词。

---

## 第 1 章 现在的 IR 为什么要废掉

现在的 IR（`IR.hpp` / `IR.cpp`）长这样：指令的操作数是**字符串
名字**，比如：

```
t3 = add t1, t2        // t1、t2、t3 都是字符串
```

变量 `a` 在 IR 里直接对应一个栈上的位置（变量即栈槽）。这个设计
能跑，但有三个会越来越疼的问题。

### 问题 1：查"谁用了我"要翻遍整个函数

操作数只是字符串，指令之间没有真实的连接。想做最简单的优化——
"这个 t3 后面没人用了，把算它的指令删掉"——都得从头扫描整个函数
看 `"t3"` 这个字符串还出现过没有。优化写不了一点。

### 问题 2：类型没地方放

现在类型挂在整条指令上。将来支持 float 时，`add` 到底是整数加
还是浮点加？只能新造一批 `FADD`、`FSUB` 指令，或者到处写特判。
根源是：**类型应该跟着"值"走，而不是跟着"指令"走**。`a + b` 是
整数加还是浮点加，看 a 和 b 的类型就知道，指令本身不该背这个锅。

### 问题 3：变量和栈槽焊死了，SSA 的门被关上

现在 IR 里没有"读内存 / 写内存"这样的真实指令（`LOAD`/`STORE`
是空壳预留），变量就是一个栈偏移量。这意味着 IR 层面根本没有
"内存"这个概念，将来想做 SSA 转换时没有可以分析和消除的对象。
（第 4 章会讲新方案怎么解开这个死结。）

### 结论

好消息是：把 IR 翻译成汇编的 `IR2RISCV.cpp` 目前只写了一半
（算术能翻，控制流和函数调用还是空的），现在推倒重来的代价最小。
所以决定：**4 个 IR 文件全部重写，不留兼容包袱**。

唯一保留的是旧的 `codegen.cpp`（AST 直接生成汇编的老路），它能
跑通所有测试。新路修好后，用它当"标准答案"逐题对答案，全部一致
再切换、再删旧路。

---

## 第 2 章 新 IR 的核心思想：一切皆是 Value

新设计一句话概括：**把每一个"计算结果"都做成一个真实的对象，
指令之间用指针直接相连，不再用字符串名字。**

这个对象叫 `Value`（值）。哪些东西是 Value？

| Value 的种类 | 例子 |
|---|---|
| 常量 | `10`、`42` |
| 一条指令的计算结果 | `a + b` 的结果 |
| 函数的形参 | `fib(int n)` 里的 `n` |
| 基本块 | 跳转指令的目标 |
| 函数 | 调用指令的目标 |

`Value` 身上带着三样东西：

```cpp
struct Value {
    Type* type;                 // 我是什么类型（目前只有 i32）
    std::string name;           // 我的名字（仅调试用）
    std::vector<User*> users;   // 谁在用我（use 链，裸指针=只观察，不拥有）
};
```

而"使用值的东西"叫 `User`，它带着操作数列表（def 链）：

```cpp
struct User : Value {
    std::vector<Value*> operands;  // 我用了哪些值（裸指针=只观察，不拥有）
};
```

**指令（Instruction）既是 Value 又是 User**：它是 Value，因为它
产出一个结果，可以被别的指令用；它是 User，因为它使用别的值当
操作数。`a + b` 这条指令，用指针指着 `a` 和 `b` 这两个 Value；
同时 `a` 的 `users` 列表里自动记着这条加法指令。

这就是第 0 章说的 use-def 链——构建指令的那一刻自动织好，不用
任何字符串查找。第 1 章的问题 1（查"谁用了我"）就此解决：看一眼
`users` 列表就行。

类型挂在每个 Value 上（问题 2 解决）：`add` 指令的两个操作数是
什么类型，结果就是什么类型，将来加 float 只是多一种 `Type`。

> **注意：IR 的 Type 是自己新定义的一套，不复用 `ast.hpp` 的
> Type。** 三个原因：
> 1. **依赖方向**：后端只许 include `IR.hpp`，不许碰 `ast.hpp`。
>    IR 的类型若来自 `ast.hpp`，后端就会传递依赖上前端，分层破掉。
> 2. **演化方向不同**：AST 的 Type 描述"程序员写了什么"，跟着
>    语法走；IR 的 Type 是机器视角，需要 `VoidType`（源代码里
>    没有 void 变量，但无结果指令需要它）、`PtrType` 要带 pointee
>    （指针算术的缩放依据）。现在两边碰巧都只有 int，但不会一直
>    这么巧。
> 3. **类型实例唯一化**：IR 的类型实例由 Module 统一持有，整个
>    程序里 i32 只有一份，"类型相等"就是一次指针比较，构建指令
>    时做类型检查（如 ADD 两操作数必须同型）零成本。
>
> 两边的翻译由 AST2IR 负责：查 AST 节点的类型，映射成 IR 类型。
> 现在就是一行 `IntType → i32`，将来加 float 也只改这一处。

### 顺便省掉一条指令

旧 IR 调用函数要两条指令配合：先 `PARAM 参数` 逐个数，再 `CALL f`。
新设计里，实参直接作为 CALL 指令的 operands 挂在它身上：

```
call @fib, %t5        // 一条指令搞定，参数就是它操作数列表里的成员
```

`PARAM` 指令整个取消。

### 谁拥有谁：所有权与裸指针的分工（重要）

细心的读者会注意到：上面的 `users`、`operands` 全是裸指针，这不
安全吗？这里有一条现代 C++ 的核心纪律，也是 LLVM 的真实做法：

**拥有关系是一棵树，用 `unique_ptr`；交叉引用是一张图，用裸指针
"观察"。** 裸指针只许看，绝不许 `delete`。

拥有关系长这样（父拥有子，逐层向下）：

```
Module
 ├── 拥有 Function 们       vector<unique_ptr<Function>>
 ├── 拥有 ConstantInt 常量池 vector<unique_ptr<ConstantInt>>
 └── 拥有 Type 实例
Function
 ├── 拥有 Argument 们        vector<unique_ptr<Argument>>
 └── 拥有 BasicBlock 们      vector<unique_ptr<BasicBlock>>
BasicBlock
 └── 拥有 Instruction 们     vector<unique_ptr<Instruction>>
```

而下面这些都是**观察者**（裸指针，不拥有任何东西）：

- 指令的 `operands` 和 `users`——它们是图上横着走的边；
- AST2IR 里的 `cur_func_` / `cur_block_`；
- `scope_stack_` 里存的 ALLOCA 指令指针；
- BR 的目标块、CALL 的目标函数。

四条规则：

1. **`unique_ptr` 只出现在所有权树的"父子边"上。** 谁把对象造
   出来，就交给对应的父容器收养；此后大家拿到的都是裸指针观察者。
2. **容器一律用 `vector<unique_ptr<T>>`，不用 `vector<T>`。** 原因
   很实际：`vector` 扩容时会整体搬家。如果里面装的是对象本身，
   搬家后所有指向它们的裸指针全部悬空；装 `unique_ptr` 则搬走的
   只是指针，对象在原地纹丝不动，观察者始终有效。
3. **删除一条指令前，先沿 use-def 链清场。** 顺序是：① 它的
   `users` 若不为空，先让使用者改指别的值（提供一个工具函数
   `replace_all_uses_with(新值)`）；② 把自己从每个 operand 的
   `users` 列表里摘掉；③ 从所属块的 `vector` 里移除，
   `unique_ptr` 自动释放内存。这套动作封装成 `erase_from_parent()`，
   任何地方想删指令都必须走它，不许徒手 `delete`。
4. **观察者的有效期靠所有权树保证：父活着，子就活着。** 比如
   翻译函数期间 `scope_stack_` 里的 ALLOCA 指针一直有效，因为
   entry 块归 Function 拥有，翻译中途不会删它。反过来，优化
   pass 如果要删指令，就不要跨删除操作长期捏着裸指针——要么
   删之前把要用的信息取完，要么删完重新查找。

附带的好处：构建中途抛异常（比如遇到 undefined variable）时，
`unique_ptr` 会自动沿路释放已建好的 IR，不会泄漏。这一点和
`ast.hpp` 现有的风格（全部 `unique_ptr`）也是一致的。

---

## 第 3 章 新 IR 的指令清单

一共就这些，先混个眼熟，第 5 章会逐条看到它们的用法：

```
内存：  ALLOCA  LOAD  STORE
算术：  ADD SUB MUL DIV REM  NEG（一元负号）
比较：  LT GT LE GE EQ NE      （结果是 0 或 1）
控制流：BR（条件跳转） JMP（无条件跳转） RET（返回）
函数：  CALL
预留：  PHI（现在不会出现，将来 SSA 转换时才产生）
```

几条说明：

- **BR** 统一了过去设想的 `JZ`/`JNZ` 两种条件跳转：
  `BR(条件, 真块, 假块)`。
- **STORE / BR / JMP / RET** 不产出任何值，它们的类型是"void"
  （新加的 `VoidType`），并且不允许被别的指令当操作数引用。
- 常量（`ConstantInt`）放在一个全局**常量池**里，整个程序里
  数字 `10` 只有一份，大家共用。

---

## 第 4 章 Alloca 模型：变量是"储物柜"，读写要走前台

这是本次重构**最重要的一个决定**，值得慢慢讲。

### 老办法的问题

老 IR 里，变量 `a` 直接等于"栈上偏移 -12 的格子"。IR 指令直接
读写这个格子，没有"内存操作"这个概念。简单直接，但前面说了，
这把将来 SSA 优化的门焊死了。

### 新办法：三条指令分工

新设计把"变量"拆成三个概念：

1. **ALLOCA**：在栈上给变量分一个格子。可以把它想成去储物柜
   前台**租一个柜子**，你拿到的是柜子的编号（地址）。
2. **STORE**：往柜子里**存**东西。`store 值, 柜子编号`
3. **LOAD**：从柜子里**取**东西。`t = load 柜子编号`

源代码里的每个局部变量 = 一条 ALLOCA（一个柜子）。对变量的
每一次读都是 LOAD，每一次写都是 STORE。

看一个完整例子，左边是源代码，右边是 IR：

```
int a = 10;      →   %a.addr = alloca i32        // 租柜子，叫 a.addr
                     store i32 10, i32* %a.addr  // 存入 10

a + 1            →   %t1 = load i32, i32* %a.addr // 取出 a
                     %t2 = add i32 %t1, 1          // 算 a+1

a = a + 1        →   %t3 = load i32, i32* %a.addr // 取出 a
                     %t4 = add i32 %t3, 1
                     store i32 %t4, i32* %a.addr  // 存回同一个柜子
```

注意最后这个例子：重新赋值**不需要新柜子**，就是往原柜子里再存
一次。这正是"变量是内存"的忠实表达，所以 AST→IR 翻译时再也不
需要旧代码里那套给变量改名（`all_ir_names_`）的机制了。

### 三条铁律（必须背下来）

这套模型将来能不能顺利升级成 SSA，全看这三条守不守得住：

1. **ALLOCA 只能出现在函数的 entry 块**（第一个基本块），一个
   源变量只租一个柜子，循环体里绝对不许租柜子。
   **进一步：entry 块专职放 ALLOCA。** entry 块里只有 ALLOCA 和
   一条末尾的 `JMP(start块)`，不放任何计算；函数一建成就同时
   建好 entry 和 start 两个块，JMP 立刻塞好。于是"插 alloca"
   永远只有一种操作——插到 JMP 之前，没有"块还没终结"的分支
   情况。形参的 store 等真实计算从 start 块开始。（这也是 LLVM
   mem2reg 只提升 entry alloca 的同款设计。）
2. **LOAD/STORE 的地址只能是 ALLOCA 给的柜子编号**（将来加指针
   后放宽为 ALLOCA 或 GEP），不许把地址计算揉进 LOAD/STORE。
3. **除了 LOAD，没有任何指令能直接"读变量"**。变量的唯一实体
   就是那个柜子；想要值，走 LOAD。

为什么强调这个？因为将来做 SSA 转换（mem2reg）时，算法要做的
第一件事就是筛选"哪些柜子是安分的"——只被 LOAD/STORE 碰过、
编号没被别人知道的柜子，才能被提升成 SSA 值。铁律保证了我们
的柜子全都安分，筛选这一步几乎不用写代码。

### 这套模型为指针留的后门

将来加指针时，`&a`（取 a 的地址）是什么？就是 `%a.addr` 本身——
租柜子时拿到的编号天然就是地址，取地址零成本。这就是 alloca
模型对指针"接受度高"的具体含义。

---

## 第 5 章 AST 怎么翻译成 IR（逐节点对照）

这一章是施工图纸。`parser.cpp` 实际会产出的每一种 AST 节点，
这里都有对应的翻译方法。先认识一下翻译器（AST2IR）身上的状态：

```cpp
Module*      module_;      // 正在构建的整个程序
Function*    cur_func_;    // 当前函数
BasicBlock*  cur_block_;   // 当前正在往里塞指令的基本块
// 作用域栈：变量名 → 它的柜子（ALLOCA 指令）
// 进入 { 压一层，离开 } 弹一层，内层可以遮住外层同名变量
std::vector<std::unordered_map<std::string, Instruction*>> scope_stack_;
```

### 语句节点

**ProgramNode（程序）** → 一个 Module，把每个函数翻译一遍装进去。

**FunctionNode（函数）** → 建一个 Function，**一次建两个块**：
entry（专职 alloca，见铁律 1）和 start（真实计算的起点），然后
立刻在 entry 末尾造 `JMP(start)`，`cur_block_` 切到 start 开始
翻译。每个形参做两件事：建一个 `Argument` 值；在 entry 里给它
租柜子，但**存参数的 STORE 发在 start 块**：

```
int fib(int n)  →  entry:
                     %n.addr = alloca i32
                     jmp label %start
                   start:
                     store i32 %arg0, i32* %n.addr
```

这样函数体内访问形参 `n` 和访问普通局部变量没有任何区别。

**BlockStatement（{ ... } 语句块）** → 压一层作用域，逐句翻译，
弹一层作用域。注意：语句块**不产生新的基本块**，它只是作用域
的边界（决定变量在哪里可见），不改变控制流。

**DeclStmt（int a = 表达式;）** → 分两步，位置不同，这是新手
最容易踩的坑：

1. ALLOCA 插到 **entry 块的 JMP 之前**（不管这条声明出现在多深
   的嵌套里）——铁律 1。entry 块从建成就带着 JMP、永远处于已
   终结状态，所以需要一个专门的 `add_alloca()` 方法插在
   terminator 之前，而不能用普通的 `add_inst`；
2. 初值的计算和 STORE 发在**当前块**——因为初值表达式可能依赖
   当时的控制流。

没有初值（`int a;`）就补一条 `store 0`，和老编译器行为一致，
防止读到垃圾值。

**ReturnStatement（return 表达式;）** → 翻译表达式，发 `RET(值)`。
RET 是"块终结者"：它之后如果源代码里还有语句，那些语句必须放进
一个新的、没有入口的块里（或者直接丢弃并给出警告），**绝不能往
已经结束的块里继续塞指令**。

**IfStmt（if / else）** → 条件值算出来后，发条件跳转，然后分别
生成两个分支的块，最后汇合：

```
       BR(条件, then块, else块/merge块)
then块: ...   JMP(merge块)
else块: ...   JMP(merge块)     // 没有 else 就省略这块
merge块: 后续代码
```

**WhileStmt（while）** → 三个块：

```
       JMP(cond块)
cond块: 算条件; BR(条件, body块, end块)
body块: 循环体; JMP(cond块)
end块:  后续代码
```

### 表达式节点

**NumberNode（数字 42）** → 从常量池取这个数。常量是 Value，
可以直接当操作数，不需要任何指令。

**IdentifierNode（变量名 a）** → 查作用域栈找到 a 的柜子，发一条
LOAD。查不到就报"undefined variable"。注意：**哪怕前一条指令刚
往这个柜子里 STORE 过，这里也老老实实发 LOAD**。消除这种冗余是
优化 pass 的事，翻译阶段只追求机械、正确。

**AssignmentExpr（a = 表达式）** → 先翻译右边得到值 v，找到 a 的
柜子，发 `STORE(v, 柜子)`。整个赋值表达式本身的值就是 v——所以
`a = b = 1` 能自然工作（b = 1 的值直接又存给 a，不用再 LOAD）。

**BinaryExpr（a + b 等）** → 翻译左右两边，发对应指令。运算符
到指令的对照：`+ - * / %` → `ADD SUB MUL DIV REM`，比较运算符
`<<= >>= == !=` → `LT LE GT GE EQ NE`。

**UnaryExpr（-a）** → 发 `NEG(操作数)`。目前语言里一元运算只有
负号这一种。

**CallExpr（fib(x)）** → 按名字在整个 Module 里找到被调函数
（找不到立刻报"undefined function"），实参逐个翻译，发
`CALL(函数, 实参1, 实参2, ...)`。

---

## 第 6 章 完整示例：fib 走一遍

源程序（P4 验收用例）：

```c
int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}
int main() {
    return fib(10);
}
```

新 IR 打印出来的样子：

```
define i32 @fib(i32 %arg0) {
entry:
  %n.addr = alloca i32
  jmp label %start
start:
  store i32 %arg0, i32* %n.addr
  %t1 = load i32, i32* %n.addr       // 取出 n
  %t2 = le i32 %t1, 1                // n <= 1 ?
  br i32 %t2, label %.L_if_then_0, label %.L_if_end_1
.L_if_then_0:
  %t3 = load i32, i32* %n.addr       // return n
  ret i32 %t3
.L_if_end_1:
  %t4 = load i32, i32* %n.addr
  %t5 = sub i32 %t4, 1               // n - 1
  %t6 = call i32 @fib(i32 %t5)       // fib(n-1)
  %t7 = load i32, i32* %n.addr
  %t8 = sub i32 %t7, 2               // n - 2
  %t9 = call i32 @fib(i32 %t8)       // fib(n-2)
  %t10 = add i32 %t6, %t9
  ret i32 %t10
}

define i32 @main() {
entry:
  jmp label %start
start:
  %t1 = call i32 @fib(i32 10)
  ret i32 %t1
}
```

对照源码逐行读一遍，你会看到第 5 章的每条规则都在里面：
形参租柜子（entry 里 alloca、start 里 store）、变量引用必走
LOAD、if 翻成 BR + 两个块、函数调用一条 CALL 搞定。entry 块
专职 alloca，哪怕 main 没有局部变量也保留 entry → start 的
结构（统一形态，后端不用特判）。这个文本格式同时也是将来
测试比对的"标准格式"。

---

## 第 7 章 后端（IR → 汇编）怎么消费新 IR

`IR2RISCV.cpp` 重写时遵守这些约定：

- **ALLOCA** → 在栈帧里给这个柜子分一个具体位置。因为所有
  ALLOCA 都集中在 entry 块且 entry 块只含 ALLOCA（铁律 1），
  函数一开始遍历 entry 块数一遍就能算清栈帧需要多大。
- **LOAD / STORE** → 一条 `lw` / `sw`。
- **每条算出值的指令**，结果先存到栈上属于它的格子，用的时候
  再 `lw` 出来。很笨但正确，和老编译器"算完压栈"一个水平——
  先把正确性跑通，寄存器分配是以后的优化项。
- **BR / JMP** → 此时才把基本块对应回汇编 label（`.L函数名_块名`），
  生成 `beq`/`bne`/`j`。
- **CALL** → 实参按顺序放进 `a0`-`a7`，执行 `call`，返回值在
  `a0`。沿用 plans.txt 里的 RV32IM 调用约定。
- **PHI** → 直接报错"not implemented"。现在没有任何环节会产生
  PHI，真看到了说明有未知的优化 pass 混进来了，必须响铃报警，
  不许静默吞掉。

还有一条架构纪律：**后端不许 include `ast.hpp`**。后端只认 IR，
这样前端（AST）将来怎么改都不会波及后端。

---

## 第 8 章 将来：float、指针、SSA 各自怎么落地

这份设计"可扩展"不是口号，这一章说清每样扩展具体挂在哪里。

**加 float**：新增一个 `FloatType`。因为类型挂在每个 Value 上，
`add` 看到两个 float 操作数自然就是浮点加，IR 层改动不到 50 行。
真正的工作在后端：RISC-V 的浮点有一套独立寄存器（`f0`-`f31`）
和独立的传参约定（`fa0`-`fa7`），那是后端的活。

**加指针**：新增 `PtrType` 和一条 `GEP` 指令（指针算术专用）。
`&a` 就是 alloca 的编号本身。LOAD/STORE 的合法地址从"只有
ALLOCA"放宽为"ALLOCA 或 GEP"。

**升级到 SSA（mem2reg）**：四步——① 筛选"安分"的柜子（只被
LOAD/STORE 碰过）；② 在需要汇合的位置插 PHI；③ 沿支配树把
LOAD 替换成当时的值、删掉 STORE；④ 删掉这些柜子的 ALLOCA。
这是一个**纯新增**的优化 pass，AST→IR 翻译和后端一行都不用改。
教学编译器圈还有一个更简单的替代算法（Braun 等人 2013 年的论文，
连支配树都不用算），到时候二选一即可。

---

## 第 9 章 施工顺序与验收标准

1. 重写 `IR.hpp` + `IR.cpp`：第 2、3 章的类 + 打印函数
   （输出格式以第 6 章为准）。
2. 重写 AST→IR 翻译：按第 5 章逐节点实现。每做完一种节点，拿
   `tests/` 里对应的 `.cmm` 用例打印出 IR 人工检查。
3. 重写 `IR2RISCV`：按第 7 章约定。
4. **双跑回归**：`tests/` 全部用例，新管线
   （compiler → assembler → simulator）的结果和旧 `codegen.cpp`
   老路的结果逐题对比，全部一致。
5. 把 `compiler.cpp` 的入口切到新管线，删除旧文件（包括根目录
   那个遗留的 `IR.o`）。

**验收**：P4 用例 `fib(10)` 在新管线上输出 55；`tests/` 全绿；
在 `plans.txt` 里补记本次重构的决策和日期。

---

## 附录：施工期间每日自查清单

- [ ] ALLOCA 只在 entry 块，一个源变量一条；entry 块只含 ALLOCA + 末尾一条 JMP
- [ ] LOAD/STORE 的地址只有 ALLOCA（将来可加 GEP）
- [ ] 除了 LOAD，没有指令直接读变量
- [ ] STORE/BR/JMP/RET 不产出值，也没有别的指令引用它们
- [ ] 常量从常量池取，不重复造
- [ ] RET/BR/JMP 之后立刻开新块，绝不往已终结的块里塞指令
- [ ] 后端没有 include `ast.hpp`
- [ ] 看到 PHI 必须报错，不许静默
- [ ] `unique_ptr` 只用于所有权树（Module→Function→BasicBlock→Instruction），交叉引用全是观察者裸指针
- [ ] 容器一律 `vector<unique_ptr<T>>`，不用 `vector<T>`
- [ ] 删指令一律走 `erase_from_parent()`（先清 use-def 两边再移除），没有徒手 `delete`
