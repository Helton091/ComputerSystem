// 端到端验证：源码 → AST2IR → IR2RISCV → .s 文件
#include "lexer.hpp"
#include "parser.hpp"
#include "AST2IR.hpp"
#include "IR2RISCV.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "usage: e2e <src.cmm> <out.s>\n"; return 1; }
    std::ifstream in(argv[1]);
    std::ostringstream oss;
    oss << in.rdbuf();
    std::string src = oss.str();   // 命名变量：活得比 lexer 久

    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto ast = parser.parse();
    IR::AST2IR translator;
    auto module = translator.translate(ast.get());

    std::ofstream out(argv[2]);
    IR::IR2RISCV backend;
    backend.generate(module.get(), out);
}
