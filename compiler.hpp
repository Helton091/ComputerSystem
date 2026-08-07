#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>
enum class Tok {
    
    IDENT, NUMBER, EOF_TOK,
    
    KW_INT, KW_RETURN,         // P1
    
    LPAREN, RPAREN, LCURLY, RCURLY, SEMICOLON, COMMA,
    
    ADD, SUB, STAR, SLASH, PERCENT,
    ASSIGN,                    // P2
    EQ, NE, LT, GT, LE, GE,    // P3
};

inline std::unordered_map<std::string, Tok> keywords = {
    {"int", Tok::KW_INT},
    {"return", Tok::KW_RETURN},
};

struct Token{
    Tok type;
    std::string text;
    int line_no;
    int col_no;
};

#include<memory>
#include<iostream>
class ASTNode{
public:
    virtual ~ASTNode() = default;
    virtual void dump(int indent = 0) const = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class ExprNode : public ASTNode{};

class NumberNode : public ExprNode{
public:
    int value;
    NumberNode(int val) : value(val) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "NumberNode(" << value << ")\n";
    }
};

class BinaryExpr : public ExprNode{
public:
    Tok op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryExpr(Tok oper, ASTNodePtr lhs, ASTNodePtr rhs)
        : op(oper), left(std::move(lhs)), right(std::move(rhs)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BinaryExpr(" << static_cast<int>(op) << ")\n";
        left->dump(indent + 2);
        right->dump(indent + 2);
    }
};

class StatementNode : public ASTNode{};

class ReturnStatement : public StatementNode{
public:
    ASTNodePtr expr;
    ReturnStatement(ASTNodePtr e) : expr(std::move(e)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ReturnStatement\n";
        expr->dump(indent + 2);
    }
};

class BlockStatement : public StatementNode{
public:
    std::vector<ASTNodePtr> statements;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BlockStatement\n";
        for(const auto& stmt : statements){
            stmt->dump(indent + 2);
        }
    }
};

enum class TypeKind {
    INT, VOID
};

struct Param{
    std::string name;
    TypeKind type;
};

class FunctionNode : public ASTNode{
public:
    std::string name;
    TypeKind return_type;
    std::vector<Param> params;
    ASTNodePtr body;
    FunctionNode(const std::string& n, TypeKind ret_type, std::vector<Param> p, ASTNodePtr b)
        : name(n), return_type(ret_type), params(std::move(p)), body(std::move(b)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "FunctionNode(" << name << ")\n";
        std::cout << std::string(indent + 2, ' ') << "ReturnType: " << static_cast<int>(return_type) << "\n";
        std::cout << std::string(indent + 2, ' ') << "Parameters:\n";
        for(const auto& param : params){
            std::cout << std::string(indent + 4, ' ') << "Param(" << param.name << ", " << static_cast<int>(param.type) << ")\n";
        }
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->dump(indent + 4);
    }
};

class ProgramNode : public ASTNode{
public:
    std::vector<ASTNodePtr> functions;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ProgramNode\n";
        for(const auto& func : functions){
            func->dump(indent + 2);
        }
    }
};


class Parser {
private:
    const std::vector<Token>& tokens;
    size_t pos = 0;
    
    const Token& peek(){if(pos < tokens.size()) return tokens[pos]; throw std::runtime_error("Unexpected end of file"); }
    const Token& advance(){if(pos < tokens.size()) return tokens[pos++]; throw std::runtime_error("Unexpected end of file"); }
    const Token& previous(){if(pos > 0) return tokens[pos - 1]; throw std::runtime_error("No previous token"); }
    const Token& expect(Tok type, const std::string& err_msg)
    {
        if(pos < tokens.size() && tokens[pos].type == type){
            return tokens[pos++];
        }
        const Token& tok = peek();
        throw std::runtime_error(
            err_msg + " at line " + std::to_string(tok.line_no) +
            ", col " + std::to_string(tok.col_no)
        );
    }
    bool match(Tok type){
        if(pos < tokens.size() && tokens[pos].type == type){
            ++pos;
            return true;
        }
        return false;
    }
    
    ASTNodePtr parse_program();
    ASTNodePtr parse_function();
    std::vector<Param> parse_parameters();
    ASTNodePtr parse_block();
    ASTNodePtr parse_statement();
    ASTNodePtr parse_return_statement();
    
    // Pratt 表达式解析
    ASTNodePtr parse_expression(int min_bp = 0);
    ASTNodePtr nud(const Token& token);
    ASTNodePtr led(const Token& token, ASTNodePtr left, ASTNodePtr right);
    int left_binding_power(Tok type);
    int right_binding_power(Tok type);
    
public:
    explicit Parser(const std::vector<Token>& toks) : tokens(toks) {}
    ASTNodePtr parse();
};


class Compiler {
private:
    std::string source_path;
    std::string output_path;
    std::string source_code;
    void read_source_file();
    bool has_error = false;
    std::vector<Token> tokens;
    void Lexer();

public:
    Compiler(const std::string& src_path, const std::string& out_path)
        : source_path(src_path), output_path(out_path) {}
    void compile();
};

