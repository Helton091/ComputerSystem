#include "compiler.hpp"

// 把 token type 转成可读名字（用于调试输出）
const char* tok_name(Tok t) {
    switch (t) {
        case Tok::IDENT:    return "IDENT";
        case Tok::NUMBER:   return "NUMBER";
        case Tok::EOF_TOK:  return "EOF";
        case Tok::KW_INT:   return "KW_INT";
        case Tok::KW_RETURN:return "KW_RETURN";
        case Tok::LPAREN:   return "LPAREN";
        case Tok::RPAREN:   return "RPAREN";
        case Tok::LCURLY:   return "LCURLY";
        case Tok::RCURLY:   return "RCURLY";
        case Tok::SEMICOLON:return "SEMI";
        case Tok::COMMA:    return "COMMA";
        case Tok::ADD:      return "ADD";
        case Tok::SUB:      return "SUB";
        case Tok::STAR:     return "STAR";
        case Tok::SLASH:    return "SLASH";
        case Tok::PERCENT:  return "PERCENT";
        default:            return "?";
    }
}

struct TestCase {
    const char* name;
    const char* source;
};

TestCase tests[] = {
    {"简单数字",
        "int main() {\n"
        "    return 42;\n"
        "}\n"},

    {"加法",
        "int main() {\n"
        "    return 1 + 2;\n"
        "}\n"},

    {"优先级: 1 + 2 * 3",
        "int main() {\n"
        "    return 1 + 2 * 3;\n"
        "}\n"},

    {"优先级: 1 * 2 + 3",
        "int main() {\n"
        "    return 1 * 2 + 3;\n"
        "}\n"},

    {"左结合: 1 - 2 - 3",
        "int main() {\n"
        "    return 1 - 2 - 3;\n"
        "}\n"},

    {"左结合: 1 - 2 + 3",
        "int main() {\n"
        "    return 1 - 2 + 3;\n"
        "}\n"},

    {"除法和取模: 10 / 3 + 10 % 3",
        "int main() {\n"
        "    return 10 / 3 + 10 % 3;\n"
        "}\n"},

    {"混合: 2 * 3 + 4 * 5",
        "int main() {\n"
        "    return 2 * 3 + 4 * 5;\n"
        "}\n"},

    {"连续加法: 1 + 2 + 3 + 4",
        "int main() {\n"
        "    return 1 + 2 + 3 + 4;\n"
        "}\n"},

    {"连续乘法: 2 * 3 * 4",
        "int main() {\n"
        "    return 2 * 3 * 4;\n"
        "}\n"},

    {"混合四则: 1 + 2 * 3 - 4 / 2",
        "int main() {\n"
        "    return 1 + 2 * 3 - 4 / 2;\n"
        "}\n"},

    {"带参数的函数",
        "int main(int a, int b) {\n"
        "    return 1;\n"
        "}\n"},

    {"多条语句",
        "int main() {\n"
        "    return 1;\n"
        "    return 2;\n"
        "}\n"},

    {"括号: (1+2)*3",
        "int main() {\n"
        "    return (1 + 2) * 3;\n"
        "}\n"},

    {"一元负号: -5",
        "int main() {\n"
        "    return -5;\n"
        "}\n"},

    {"一元负号: -5 + 3",
        "int main() {\n"
        "    return -5 + 3;\n"
        "}\n"},

    {"一元负号: -a * b → (-a)*b",
        "int main() {\n"
        "    return -5 * 2;\n"
        "}\n"},

    {"一元负号: -(1+2)",
        "int main() {\n"
        "    return -(1 + 2);\n"
        "}\n"},

    {"双重负号: --5",
        "int main() {\n"
        "    return - -5;\n"
        "}\n"},

    {"负号与括号: (-3) * 4",
        "int main() {\n"
        "    return (-3) * 4;\n"
        "}\n"},

    {"混合: -2 + 3 * 4",
        "int main() {\n"
        "    return -2 + 3 * 4;\n"
        "}\n"},
};

int main() {
    int passed = 0;
    int failed = 0;
    int total = sizeof(tests) / sizeof(tests[0]);

    for (int i = 0; i < total; i++) {
        std::cout << "========================================\n";
        std::cout << "Test " << (i + 1) << ": " << tests[i].name << "\n";
        std::cout << "----------------------------------------\n";
        std::cout << "Source:\n" << tests[i].source << "\n";

        // 写入临时文件
        const char* test_file = "test.cmm";
        std::ofstream ofs(test_file);
        ofs << tests[i].source;
        ofs.close();

        Compiler compiler(test_file, "out.s");
        compiler.read_source_file();
        if (compiler.has_error) {
            std::cerr << "  [FAIL] read_source_file error\n\n";
            failed++;
            continue;
        }

        compiler.Lexer();
        if (compiler.has_error) {
            std::cerr << "  [FAIL] lexer error\n\n";
            failed++;
            continue;
        }

        try {
            Parser parser(compiler.tokens);
            ASTNodePtr ast = parser.parse();

            std::cout << "  AST:\n";
            ast->dump(2);
            std::cout << "\n  [PASS]\n\n";
            passed++;
        } catch (const std::exception& e) {
            std::cerr << "  [FAIL] Parse error: " << e.what() << "\n\n";
            failed++;
        }
    }

    std::cout << "========================================\n";
    std::cout << "Result: " << passed << "/" << total << " passed";
    if (failed > 0) {
        std::cout << " (" << failed << " failed)";
    }
    std::cout << "\n";

    return failed > 0 ? 1 : 0;
}
