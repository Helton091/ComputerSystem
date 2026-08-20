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
        throw std::runtime_error("[Parser] unexpected end of file");
    }
    const Token& advance() {
        if (pos < tokens.size()) return tokens[pos++];
        throw std::runtime_error("[Parser] unexpected end of file");
    }
    const Token& previous() {
        if (pos > 0) return tokens[pos - 1];
        throw std::runtime_error("[Parser] no previous token");
    }
    const Token& expect(Tok type, const std::string& err_msg) {
        if (pos < tokens.size() && tokens[pos].type == type) {
            return tokens[pos++];
        }
        const Token& tok = peek();
        throw std::runtime_error(
            "[Parser] " + err_msg + " at line " + std::to_string(tok.line_no) +
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

    std::unique_ptr<AST::Type> parse_type_tok(const std::string& err_msg = "expected data type (like int, float)"){
        const Token& token = advance();
        switch(token.type){
        case Tok::KW_INT:
            return std::make_unique<AST::IntType>();
        case Tok::KW_FLOAT:
            return std::make_unique<AST::FloatType>();
        default:
            throw std::runtime_error(
                "[Parser] " + err_msg + " at line " + std::to_string(token.line_no) +
                ", col " + std::to_string(token.col_no)
            );
        } 

    }

    std::unique_ptr<AST::ProgramNode> parse_program();
    std::unique_ptr<AST::FunctionNode> parse_function();
    std::vector<AST::Param> parse_parameters();
    std::unique_ptr<AST::BlockStatement> parse_block();
    std::unique_ptr<AST::StatementNode> parse_statement();
    std::unique_ptr<AST::ReturnStatement> parse_return_statement();
    std::unique_ptr<AST::DeclStmt> parse_declaration_statement();

    // Pratt 表达式解析
    std::unique_ptr<AST::ExprNode> parse_expression(int min_bp = 0);
    std::unique_ptr<AST::ExprNode> nud(const Token& token);
    std::unique_ptr<AST::ExprNode> led(const Token& token, std::unique_ptr<AST::ExprNode> left, std::unique_ptr<AST::ExprNode> right);
    int left_binding_power(Tok type);
    int right_binding_power(Tok type);

public:
    explicit Parser(const std::vector<Token>& toks) : tokens(toks) {}
    std::unique_ptr<AST::ProgramNode> parse();
};
