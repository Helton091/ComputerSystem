// AST2IR 回归测试：每个用例 = 源码 + 预期 dump 文本，逐字比对。
// 编译：g++ -std=c++17 -Wall -Wextra AST2IRTEST.cpp IR.cpp AST2IR.cpp lexer.cpp parser.cpp -o ast2ir_test
#include "lexer.hpp"
#include "parser.hpp"
#include "AST2IR.hpp"
#include <iostream>
#include <sstream>
#include <vector>

struct Case {
    std::string name;
    std::string src;
    std::string expect;      // 预期的 dump 输出（expect_throw 为 true 时忽略）
    bool expect_throw = false;  // true = 预期翻译期抛异常（fail-fast 路径）
};

static const std::vector<Case> cases = {
    {
        "return const",
        "int main() { return 0; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  ret i32 0\n"
        "}\n"
        "\n"
    },
    {
        "precedence: 1 + 2 * 3",
        "int main() { return 1 + 2 * 3; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = mul i32 2, 3\n"
        "  %t2 = add i32 1, %t1\n"
        "  ret i32 %t2\n"
        "}\n"
        "\n"
    },
    {
        "parens: (10 - 2) / 4 + 5",
        "int main() { return (10 - 2) / 4 + 5; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = sub i32 10, 2\n"
        "  %t2 = div i32 %t1, 4\n"
        "  %t3 = add i32 %t2, 5\n"
        "  ret i32 %t3\n"
        "}\n"
        "\n"
    },
    {
        "left assoc: 10 - 3 - 2",
        "int main() { return 10 - 3 - 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = sub i32 10, 3\n"
        "  %t2 = sub i32 %t1, 2\n"
        "  ret i32 %t2\n"
        "}\n"
        "\n"
    },
    {
        "comparison: lt",
        "int main() { return 1 < 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = lt i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "comparison: gt",
        "int main() { return 1 > 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = gt i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "comparison: le",
        "int main() { return 1 <= 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = le i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "comparison: ge",
        "int main() { return 1 >= 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = ge i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "comparison: eq",
        "int main() { return 1 == 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = eq i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "comparison: ne",
        "int main() { return 1 != 2; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = ne i32 1, 2\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "unary minus",
        "int main() { return -5; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = neg i32 5\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "unary minus on expr",
        "int main() { return -(1 + 2); }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = add i32 1, 2\n"
        "  %t2 = neg i32 %t1\n"
        "  ret i32 %t2\n"
        "}\n"
        "\n"
    },
    // ---- 负向用例：现在还不支持，预期抛异常（fail-fast 在工作）----
    // 支持之后把 expect_throw 去掉、补上预期输出即可转成正向用例。
    {
        "decl with init",
        "int main() { int a = 1; return 1; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 1, i32* %a\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
    },
    {
        "decl auto init to 0",
        "int main() { int a; return 1; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 0, i32* %a\n"
        "  ret i32 1\n"
        "}\n"
        "\n"
    },
    {
        "decl and use",
        "int main() { int a; return a + 1; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 0, i32* %a\n"
        "  %t1 = load i32, i32* %a\n"
        "  %t2 = add i32 %t1, 1\n"
        "  ret i32 %t2\n"
        "}\n"
        "\n"
    },
    {
        "assignment chain",
        "int main() { int a = 1; a = a + 2; return a; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 1, i32* %a\n"
        "  %t1 = load i32, i32* %a\n"
        "  %t2 = add i32 %t1, 2\n"
        "  store i32 %t2, i32* %a\n"
        "  %t3 = load i32, i32* %a\n"
        "  ret i32 %t3\n"
        "}\n"
        "\n"
    },
    {
        "expr statement",
        "int main() { int a = 1; a + 2; return a; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 1, i32* %a\n"
        "  %t1 = load i32, i32* %a\n"
        "  %t2 = add i32 %t1, 2\n"
        "  %t3 = load i32, i32* %a\n"
        "  ret i32 %t3\n"
        "}\n"
        "\n"
    },
    {
        "nested scope: inner decl does not leak",
        "int main() { int a = 1; { int b = 2; b = 3; } return a; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  %b = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 1, i32* %a\n"
        "  store i32 2, i32* %b\n"
        "  store i32 3, i32* %b\n"
        "  %t1 = load i32, i32* %a\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "if-else both return",
        "int main() { if (1 < 2) return 1; else return 2; return 0; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = lt i32 1, 2\n"
        "  br i32 %t1, label %_if_then_0, label %_if_else_1\n"
        "_if_then_0:\n"
        "  ret i32 1\n"
        "_if_else_1:\n"
        "  ret i32 2\n"
        "_if_end_2:\n"
        "  ret i32 0\n"
        "}\n"
        "\n"
    },
    {
        "if without else",
        "int main() { int a = 0; if (a) a = 1; return a; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %a = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 0, i32* %a\n"
        "  %t1 = load i32, i32* %a\n"
        "  br i32 %t1, label %_if_then_0, label %_if_end_1\n"
        "_if_then_0:\n"
        "  store i32 1, i32* %a\n"
        "  jmp label %_if_end_1\n"
        "_if_end_1:\n"
        "  %t2 = load i32, i32* %a\n"
        "  ret i32 %t2\n"
        "}\n"
        "\n"
    },
    {
        "if as last stmt: fallback ret 0",
        "int main() { if (1) return 1; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  br i32 1, label %_if_then_0, label %_if_end_1\n"
        "_if_then_0:\n"
        "  ret i32 1\n"
        "_if_end_1:\n"
        "  ret i32 0\n"
        "}\n"
        "\n"
    },
    {
        "while: sum loop",
        "int main() { int i = 0; int s = 0; while (i < 3) { s = s + i; i = i + 1; } return s; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %i = alloca i32\n"
        "  %s = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 0, i32* %i\n"
        "  store i32 0, i32* %s\n"
        "  jmp label %_while_cond_0\n"
        "_while_cond_0:\n"
        "  %t1 = load i32, i32* %i\n"
        "  %t2 = lt i32 %t1, 3\n"
        "  br i32 %t2, label %_while_body_1, label %_while_end_2\n"
        "_while_body_1:\n"
        "  %t3 = load i32, i32* %s\n"
        "  %t4 = load i32, i32* %i\n"
        "  %t5 = add i32 %t3, %t4\n"
        "  store i32 %t5, i32* %s\n"
        "  %t6 = load i32, i32* %i\n"
        "  %t7 = add i32 %t6, 1\n"
        "  store i32 %t7, i32* %i\n"
        "  jmp label %_while_cond_0\n"
        "_while_end_2:\n"
        "  %t8 = load i32, i32* %s\n"
        "  ret i32 %t8\n"
        "}\n"
        "\n"
    },
    {
        "while: nested",
        "int main() { int i = 0; while (i < 2) { int j = 0; while (j < 2) j = j + 1; i = i + 1; } return i; }",
        "define i32 @main() {\n"
        "entry:\n"
        "  %i = alloca i32\n"
        "  %j = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 0, i32* %i\n"
        "  jmp label %_while_cond_0\n"
        "_while_cond_0:\n"
        "  %t1 = load i32, i32* %i\n"
        "  %t2 = lt i32 %t1, 2\n"
        "  br i32 %t2, label %_while_body_1, label %_while_end_2\n"
        "_while_body_1:\n"
        "  store i32 0, i32* %j\n"
        "  jmp label %_while_cond_3\n"
        "_while_end_2:\n"
        "  %t9 = load i32, i32* %i\n"
        "  ret i32 %t9\n"
        "_while_cond_3:\n"
        "  %t3 = load i32, i32* %j\n"
        "  %t4 = lt i32 %t3, 2\n"
        "  br i32 %t4, label %_while_body_4, label %_while_end_5\n"
        "_while_body_4:\n"
        "  %t5 = load i32, i32* %j\n"
        "  %t6 = add i32 %t5, 1\n"
        "  store i32 %t6, i32* %j\n"
        "  jmp label %_while_cond_3\n"
        "_while_end_5:\n"
        "  %t7 = load i32, i32* %i\n"
        "  %t8 = add i32 %t7, 1\n"
        "  store i32 %t8, i32* %i\n"
        "  jmp label %_while_cond_0\n"
        "}\n"
        "\n"
    },
    {
        "fib: full pipeline",
        "int fib(int n) { if (n <= 1) return n; return fib(n-1) + fib(n-2); } int main() { return fib(10); }",
        "define i32 @fib(i32 %arg0) {\n"
        "entry:\n"
        "  %n = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 %arg0, i32* %n\n"
        "  %t1 = load i32, i32* %n\n"
        "  %t2 = le i32 %t1, 1\n"
        "  br i32 %t2, label %_if_then_0, label %_if_end_1\n"
        "_if_then_0:\n"
        "  %t3 = load i32, i32* %n\n"
        "  ret i32 %t3\n"
        "_if_end_1:\n"
        "  %t4 = load i32, i32* %n\n"
        "  %t5 = sub i32 %t4, 1\n"
        "  %t6 = call i32 @fib(i32 %t5)\n"
        "  %t7 = load i32, i32* %n\n"
        "  %t8 = sub i32 %t7, 2\n"
        "  %t9 = call i32 @fib(i32 %t8)\n"
        "  %t10 = add i32 %t6, %t9\n"
        "  ret i32 %t10\n"
        "}\n"
        "\n"
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = call i32 @fib(i32 10)\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
    },
    {
        "mutual recursion, main defined first",
        "int main() { return is_even(4); }"
        "int is_even(int n) { if (n == 0) return 1; return is_odd(n - 1); }"
        "int is_odd(int n) { if (n == 0) return 0; return is_even(n - 1); }",
        "define i32 @main() {\n"
        "entry:\n"
        "  jmp label %start\n"
        "start:\n"
        "  %t1 = call i32 @is_even(i32 4)\n"
        "  ret i32 %t1\n"
        "}\n"
        "\n"
        "define i32 @is_even(i32 %arg0) {\n"
        "entry:\n"
        "  %n = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 %arg0, i32* %n\n"
        "  %t1 = load i32, i32* %n\n"
        "  %t2 = eq i32 %t1, 0\n"
        "  br i32 %t2, label %_if_then_0, label %_if_end_1\n"
        "_if_then_0:\n"
        "  ret i32 1\n"
        "_if_end_1:\n"
        "  %t3 = load i32, i32* %n\n"
        "  %t4 = sub i32 %t3, 1\n"
        "  %t5 = call i32 @is_odd(i32 %t4)\n"
        "  ret i32 %t5\n"
        "}\n"
        "\n"
        "define i32 @is_odd(i32 %arg0) {\n"
        "entry:\n"
        "  %n = alloca i32\n"
        "  jmp label %start\n"
        "start:\n"
        "  store i32 %arg0, i32* %n\n"
        "  %t1 = load i32, i32* %n\n"
        "  %t2 = eq i32 %t1, 0\n"
        "  br i32 %t2, label %_if_then_2, label %_if_end_3\n"
        "_if_then_2:\n"
        "  ret i32 0\n"
        "_if_end_3:\n"
        "  %t3 = load i32, i32* %n\n"
        "  %t4 = sub i32 %t3, 1\n"
        "  %t5 = call i32 @is_even(i32 %t4)\n"
        "  ret i32 %t5\n"
        "}\n"
        "\n"
    },
    // ---- 负向用例：预期抛异常（fail-fast 在工作）----
    {
        "same-scope redefinition is rejected",
        "int main() { int a = 1; int a = 2; return a; }",
        "",
        true
    },
    {
        "undeclared variable is rejected",
        "int main() { return x; }",
        "",
        true
    },
};

int main() {
    int passed = 0, failed = 0;
    for (const auto& c : cases) {
        std::string got;
        bool threw = false;
        try {
            Lexer lexer(c.src);
            auto tokens = lexer.tokenize();
            Parser parser(tokens);
            auto ast = parser.parse();
            IR::AST2IR translator;
            auto module = translator.translate(ast.get());
            std::ostringstream oss;
            module->dump(oss);
            got = oss.str();
        } catch (const std::exception& e) {
            threw = true;
            got = std::string("threw: ") + e.what();
        }

        bool ok = c.expect_throw ? threw : (!threw && got == c.expect);
        if (ok) {
            ++passed;
            std::cout << "PASS  " << c.name << "\n";
        } else {
            ++failed;
            std::cout << "FAIL  " << c.name << "\n";
            if (c.expect_throw) {
                std::cout << "  expected: throw\n  got: no throw\n";
            } else {
                std::cout << "  ---- expected ----\n" << c.expect
                          << "  ---- got ----\n" << got << "\n";
            }
        }
    }
    std::cout << "----------------------------------------\n"
              << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
