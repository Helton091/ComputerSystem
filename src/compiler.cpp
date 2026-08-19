#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "IR.hpp"
#include "AST2IR.hpp"
#include "IR2RISCV.hpp"

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
    std::unique_ptr<AST::ProgramNode> ast = parser.parse();

    IR::AST2IR ast2ir;
    std::unique_ptr<IR::Module> mod = ast2ir.translate(ast.get());

    IR::IR2RISCV ir2riscv;
    
    std::ofstream out(output_path);
    if (!out) {
        std::cerr << "Error: cannot open output file '" << output_path << "'\n";
        has_error = true;
        return;
    }

    ir2riscv.generate(mod.get(),out);

    
}
