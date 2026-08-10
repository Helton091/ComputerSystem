#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>
#include <unordered_set>
enum class Tok {
    
    IDENT, NUMBER, EOF_TOK,
    
    KW_INT, KW_RETURN,         
    
    LPAREN, RPAREN, LCURLY, RCURLY, SEMICOLON, COMMA,
    
    ADD, SUB, STAR, SLASH, PERCENT,
    ASSIGN,                    
    EQ, NE, LT, GT, LE, GE,  
    KW_IF, KW_ELSE, KW_WHILE,
};

inline std::unordered_map<std::string, Tok> keywords = {
    {"int", Tok::KW_INT},
    {"return", Tok::KW_RETURN},
    {"if", Tok::KW_IF},
    {"else", Tok::KW_ELSE},
    {"while", Tok::KW_WHILE},
};

struct Token{
    Tok type;
    std::string text;
    int line_no;
    int col_no;
};

class Type{
public:
    virtual ~Type() = default;
    virtual int size() const = 0;
    virtual std::string to_string() = 0;
};

class IntType : public Type{
public:
    int size() const override{return 4;}
    std::string to_string() override{return "int";}
};

struct Param{
    std::string name;
    std::unique_ptr<Type> type;
};

class ASTNode{
public:
    virtual ~ASTNode() = default;
    virtual void dump(int indent = 0) const = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class ExprNode : public ASTNode{};
class CallExpr : public ExprNode{
public:
    std::string name;
    std::vector<std::unique_ptr<ExprNode>> args;
    CallExpr(std::string n, std::vector<std::unique_ptr<ExprNode>> a)
        : name(std::move(n)), args(std::move(a)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "CallExpr(" << name << ")\n";
        for (auto& arg : args) {
            arg->dump(indent + 2);
        }
    }
};

class NumberNode : public ExprNode{
public:
    int value;
    NumberNode(int val) : value(val) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "NumberNode(" << value << ")\n";
    }
};

class IdentifierNode : public ExprNode{
public:
    std::string name;
    IdentifierNode(std::string n) : name(std::move(n)) {}
    
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Identifier(" << name << ")\n";
    }
};

class AssignmentExpr : public ExprNode {
public:
    std::unique_ptr<IdentifierNode> lhs;
    std::unique_ptr<ExprNode> rhs;
    
    AssignmentExpr(std::unique_ptr<IdentifierNode> l, std::unique_ptr<ExprNode> r)
        : lhs(std::move(l)), rhs(std::move(r)) {}
    
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "AssignmentExpr\n";
        lhs->dump(indent + 2);
        rhs->dump(indent + 2);
    }
};

class StatementNode : public ASTNode{};
class WhileStmt: public StatementNode{
public:
    std::unique_ptr<ExprNode> cond;
    std::unique_ptr<StatementNode> body;

    WhileStmt(std::unique_ptr<ExprNode> c, std::unique_ptr<StatementNode> b)
        : cond(std::move(c)), body(std::move(b)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "WhileStmt\n";
        std::cout << std::string(indent + 2, ' ') << "Cond:\n";
        cond->dump(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->dump(indent + 4);
    }
};

class IfStmt : public StatementNode {
public:
    std::unique_ptr<ExprNode> cond;    
    std::unique_ptr<StatementNode> then_branch;
    std::unique_ptr<StatementNode> else_branch;

    IfStmt(std::unique_ptr<ExprNode> c, std::unique_ptr<StatementNode> t,
           std::unique_ptr<StatementNode> e = nullptr)
        : cond(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "IfStmt\n";
        std::cout << std::string(indent + 2, ' ') << "Cond:\n";
        cond->dump(indent + 4);
        std::cout << std::string(indent + 2, ' ') << "Then:\n";
        then_branch->dump(indent + 4);
        if (else_branch) {
            std::cout << std::string(indent + 2, ' ') << "Else:\n";
            else_branch->dump(indent + 4);
        }
    }
};
class DeclStmt : public StatementNode {
public:
    std::string name;
    std::unique_ptr<ExprNode> init;  // 可能为空
    
    DeclStmt(std::string n, std::unique_ptr<ExprNode> i = nullptr)
        : name(std::move(n)), init(std::move(i)) {}
    
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "DeclStmt(" << name << ")\n";
        if (init) init->dump(indent + 2);
    }
};





class UnaryExpr : public ExprNode{
public:
    Tok op;
    std::unique_ptr<ExprNode> operand;
    UnaryExpr(Tok oper, std::unique_ptr<ExprNode> opera) : op(std::move(oper)), operand(std::move(opera)){}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "UnaryExpr(" << static_cast<int>(op) << ")\n";
        operand->dump(indent + 2);
    }

};

class BinaryExpr : public ExprNode{
public:
    Tok op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
    BinaryExpr(Tok oper, std::unique_ptr<ExprNode> lhs, std::unique_ptr<ExprNode> rhs)
        : op(oper), left(std::move(lhs)), right(std::move(rhs)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BinaryExpr(" << static_cast<int>(op) << ")\n";
        left->dump(indent + 2);
        right->dump(indent + 2);
    }
};

class ExprStmt : public StatementNode {
public:
    std::unique_ptr<ExprNode> expr;
    
    explicit ExprStmt(std::unique_ptr<ExprNode> e) : expr(std::move(e)) {}
    
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ExprStmt\n";
        expr->dump(indent + 2);
    }
};

class ReturnStatement : public StatementNode{
public:
    std::unique_ptr<ExprNode> expr;
    ReturnStatement(std::unique_ptr<ExprNode> e) : expr(std::move(e)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ReturnStatement\n";
        expr->dump(indent + 2);
    }
};

class BlockStatement : public StatementNode{
public:
    std::vector<std::unique_ptr<StatementNode>> statements;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BlockStatement\n";
        for(const auto& stmt : statements){
            stmt->dump(indent + 2);
        }
    }
};


class FunctionNode : public ASTNode{
public:
    std::string name;
    std::unique_ptr<Type> return_type;
    std::vector<Param> params;
    std::unique_ptr<BlockStatement> body;
    FunctionNode(const std::string& n, std::unique_ptr<Type> ret_type, std::vector<Param> p, std::unique_ptr<BlockStatement> b)
        : name(n), return_type(std::move(ret_type)), params(std::move(p)), body(std::move(b)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "FunctionNode(" << name << ")\n";
        std::cout << std::string(indent + 2, ' ') << "ReturnType: " << return_type->to_string() << "\n";
        std::cout << std::string(indent + 2, ' ') << "Parameters:\n";
        for(const auto& param : params){
            std::cout << std::string(indent + 4, ' ') << "Param(" << param.name << ", " << param.type->to_string() << ")\n";
        }
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->dump(indent + 4);
    }
};

class ProgramNode : public ASTNode{
public:
    std::vector<std::unique_ptr<FunctionNode>> functions;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ProgramNode\n";
        for(const auto& func : functions){
            func->dump(indent + 2);
        }
    }
};



class Parser {
public:
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

class CodeGen {
public:
    void generate(ASTNode* ast, std::ostream& out);
    
private:
    std::vector<std::string> instructions;
    std::vector<std::unordered_map<std::string, int>> scope_stack; //ralative to s0, aka, fp
    int count_decl_with_size(const BlockStatement* block);
    int count_params_with_size(const FunctionNode* funct);
    int calculate_frame_size(const FunctionNode* funct){return count_params_with_size(funct) + count_decl_with_size(funct->body.get()) + 8;}
    //8 means s0(4) + ra(4)
    int lookup_variable(const std::string& name);
    void enter_scope(){scope_stack.push_back(std::unordered_map<std::string, int>{});}
    void exit_scope(){if(!scope_stack.empty()) scope_stack.pop_back();}
    int frame_size = 0;
    int next_offset = -8;
    std::string current_epilogue;

    void emit(const std::string& line){instructions.push_back(line);}
    
    
    void gen_program(const ProgramNode* node);
    void gen_function(const FunctionNode* node);
    void gen_block(const BlockStatement* node);
    void gen_statement(const StatementNode* node);
    void gen_decl(const DeclStmt* node);
    void gen_identifier(const IdentifierNode* node);
    void gen_assignment(const AssignmentExpr* node);
    void gen_expr_stmt(const ExprStmt* node);
    void gen_expr(const ExprNode* node);
    void gen_return(const ReturnStatement* node);
    void gen_if(const IfStmt* node);
    void gen_while(const WhileStmt* node);
    int label_counter = 0;
    std::string new_label(const std::string& prefix){return prefix + std::to_string(label_counter++);}
};



class Compiler {
public:
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



