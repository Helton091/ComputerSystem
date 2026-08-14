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
    // ---- 负向用例：现在还不支持，预期抛异常（fail-fast 在工作）----
    // 支持之后把 expect_throw 去掉、补上预期输出即可转成正向用例。
    {
        "comparison not mapped yet",
        "int main() { return 1 < 2; }",
        "",
        true
    },
    {
        "unary minus not supported yet",
        "int main() { return -5; }",
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
