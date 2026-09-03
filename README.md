# C-- 编译器

一个教学用的 C 语言子集（C--）编译器，把 `.cmm` 源文件编译为 RV32IMF
汇编，再经汇编器生成二进制，最终在自研模拟器上运行。

## 工具链流程

```
源代码 (.cmm)
   │  compiler.exe   Lexer → Parser → AST2IR → IR2RISCV
   ▼
RISC-V 汇编 (.s)
   │  assembler.exe
   ▼
二进制 (.bin)
   │  simulator.exe
   ▼
运行结果（a0 即 main 返回值，--trace 可查看）
```

## 目录结构

```
src/       现役代码：compiler / lexer / parser / IR / AST2IR / IR2RISCV
           / assembler / simulator
archive/   老代码存档（旧 AST 直译 codegen、参考实现）
doc/       文档：开发计划、IR 设计报告、语言版本记录、debug 日志
tests/     测试用例（.cmm）+ IR 层回归测试 + 全链路测试脚本
temp/      测试中间产物（.s / .bin），不进版本库
```

## 构建

```bash
make            # 构建 compiler.exe / assembler.exe / simulator.exe
# 没有 make 时，直接用 Makefile 里的 g++ 命令逐条执行即可
```

## 测试

```bash
# 全链路回归（120+ 个用例，从仓库根目录运行）
powershell tests/run_tests.ps1

# IR 层回归（AST → IR dump 逐字比对）
g++ -std=c++17 -Wall -Wextra -Isrc tests/AST2IRTEST.cpp \
    src/IR.cpp src/AST2IR.cpp src/lexer.cpp src/parser.cpp -o temp/ast2ir_test
./temp/ast2ir_test
```

## 当前状态

语言特性见 `doc/language_version.md`（当前 v11：变量与作用域、
控制流、函数与递归、`int`/`float`、指针与指针算术、一维数组、
`for` 循环、复合赋值、`break`/`continue`）。IR 为 value-based + alloca 内存模型，
设计细节见 `doc/ir_design_report.md`。

下一步路线（v11 剩余）：① 数组-指针退化 →
② 带跳转表的 switch；`&&`/`||`/`!` 顺延至通用 PHI 就绪后（v12+）。
