#pragma once
#include "token.hpp"
#include "ast.hpp"
#include <vector>
#include <string>
#include <memory>
#include <unordered_set>
#include <stdexcept>

class Parser {
public:
    const std::vector<Token>& tokens;
    size_t pos = 0;

    const Token& peek() {
        if (pos < tokens.size()) return tokens[pos];
        throw std::runtime_error("Unexpected end of file");
    }
    const Token& advance() {
        if (pos < tokens.size()) return tokens[pos++];
        throw std::runtime_error("Unexpected end of file");
    }
    const Token& previous() {
        if (pos > 0) return tokens[pos - 1];
        throw std::runtime_error("No previous token");
    }
    const Token& expect(Tok type, const std::string& err_msg) {
        if (pos < tokens.size() && tokens[pos].type == type) {
            return tokens[pos++];
        }
        const Token& tok = peek();
        throw std::runtime_error(
            err_msg + " at line " + std::to_string(tok.line_no) +
            ", col " + std::to_string(tok.col_no)
        );
    }
    bool match(Tok type) {
        if (pos < tokens.size() && tokens[pos].type == type) {
            ++pos;
            return true;
        }
        return false;
    }

    std::unique_ptr<ProgramNode> parse_program();
    std::unique_ptr<FunctionNode> parse_function();
    std::vector<Param> parse_parameters();
    std::unique_ptr<BlockStatement> parse_block();
    std::unique_ptr<StatementNode> parse_statement();
    std::unique_ptr<ReturnStatement> parse_return_statement();
    std::unique_ptr<StatementNode> parse_declaration_statement();

    // Pratt 表达式解析
    std::unique_ptr<ExprNode> parse_expression(int min_bp = 0);
    std::unique_ptr<ExprNode> nud(const Token& token);
    std::unique_ptr<ExprNode> led(const Token& token, std::unique_ptr<ExprNode> left, std::unique_ptr<ExprNode> right);
    int left_binding_power(Tok type);
    int right_binding_power(Tok type);

public:
    explicit Parser(const std::vector<Token>& toks) : tokens(toks) {}
    std::unique_ptr<ProgramNode> parse();
};
