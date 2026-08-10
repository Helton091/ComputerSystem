#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

void Compiler::read_source_file() {
    std::ifstream file(source_path);
    if (!file) {
        std::cerr << "Error: cannot open source file '" << source_path << "'\n";
        has_error = true;
        return;
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    source_code = oss.str();
}

void Compiler::compile() {
    read_source_file();
    if (has_error) return;

    Lexer lexer(source_code);
    tokens = lexer.tokenize();
    if (lexer.has_error()) {
        has_error = true;
        return;
    }

    Parser parser(tokens);
    std::unique_ptr<ProgramNode> ast = parser.parse();

    std::ofstream out(output_path);
    if (!out) {
        std::cerr << "Error: cannot open output file '" << output_path << "'\n";
        has_error = true;
        return;
    }

    CodeGen codegen;
    codegen.generate(ast.get(), out);
}
