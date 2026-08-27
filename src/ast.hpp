#pragma once
#include "token.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace AST {

class Type {
public:
    virtual ~Type() = default;
    virtual int size() const = 0;
    virtual std::string to_string() = 0;
};

class IntType : public Type {
public:
    int size() const override { return 4; }
    std::string to_string() override { return "int"; }
};

class FloatType : public Type{
public:
    int size() const override {return 4;}
    std::string to_string() override {return "float";}
};

class VoidType : public Type{
public:
    int size() const override {return 0;}
    std::string to_string() override{return "void";}
};

class PointerType : public Type{
public:
    int size() const override {return 4;}
    std::unique_ptr<Type> pointee;
    explicit PointerType(std::unique_ptr<Type> p) : pointee(std::move(p)){}
    std::string to_string() override{return pointee->to_string() + "*";}
};

class ArrayType : public Type{
public:
    int length;
    std::unique_ptr<Type> base_type;
    int size() const override {return base_type->size() * length;}
    std::string to_string() override{return base_type->to_string() + "[" + std::to_string(length) + "]";}
    explicit ArrayType(std::unique_ptr<Type> base, int len) : length(len),base_type(std::move(base)){}
};

struct TypedName{
    std::string name;
    std::unique_ptr<Type> type;
    TypedName(std::string n, std::unique_ptr<Type> t)
        : name(std::move(n)), type(std::move(t)) {}
};

using Param = TypedName;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void dump(int indent = 0) const = 0;
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

class ExprNode : public ASTNode {};

class NullPointerNode : public ExprNode {
public:
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "NullPointerNode\n";
    }
};

class CallExpr : public ExprNode {
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

class IntNumberNode : public ExprNode {
public:
    int value;
    IntNumberNode(int val) : value(val) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "NumberNode(" << value << ")\n";
    }
};

class FloatNumberNode : public ExprNode{
public:
    float value;
    FloatNumberNode(float val) : value(val){}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "NumberNode(" << value << ")\n";
    }
};

class IdentifierNode : public ExprNode {
public:
    std::string name;
    IdentifierNode(std::string n) : name(std::move(n)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Identifier(" << name << ")\n";
    }
};

class AssignmentExpr : public ExprNode {
public:
    std::unique_ptr<ExprNode> lhs;
    std::unique_ptr<ExprNode> rhs;

    AssignmentExpr(std::unique_ptr<ExprNode> l, std::unique_ptr<ExprNode> r)
        : lhs(std::move(l)), rhs(std::move(r)) {}

    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "AssignmentExpr\n";
        lhs->dump(indent + 2);
        rhs->dump(indent + 2);
    }
};

class IndexExpr : public ExprNode{
public:
    std::unique_ptr<ExprNode> base;
    std::unique_ptr<ExprNode> index;
    IndexExpr(std::unique_ptr<ExprNode> b,std::unique_ptr<ExprNode> idx) : base(std::move(b)),index(std::move(idx)){}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "IndexExpr\n";
        base->dump(indent + 2);
        index->dump(indent + 2);
    }
};

class StatementNode : public ASTNode {};

class ForStmt : public StatementNode{
public:
    std::unique_ptr<StatementNode> init; // may be nullptr
    std::unique_ptr<ExprNode> cond;
    std::unique_ptr<ExprNode> update;
    std::unique_ptr<StatementNode> body;

    ForStmt(std::unique_ptr<StatementNode> i,
            std::unique_ptr<ExprNode> c,
            std::unique_ptr<ExprNode> u,
            std::unique_ptr<StatementNode> b)
        : init(std::move(i)), cond(std::move(c)), update(std::move(u)), body(std::move(b)) {}

    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ForStmt\n";
        if (init) {
            std::cout << std::string(indent + 2, ' ') << "Init:\n";
            init->dump(indent + 4);
        }
        if (cond) {
            std::cout << std::string(indent + 2, ' ') << "Cond:\n";
            cond->dump(indent + 4);
        }
        if (update) {
            std::cout << std::string(indent + 2, ' ') << "Update:\n";
            update->dump(indent + 4);
        }
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->dump(indent + 4);
    }
};

class WhileStmt : public StatementNode {
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
    TypedName var;
    std::unique_ptr<ExprNode> init;

    DeclStmt(TypedName v, std::unique_ptr<ExprNode> i = nullptr)
        : var(std::move(v)), init(std::move(i)) {}

    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "DeclStmt(" << var.name << ")\n";
        if (init) init->dump(indent + 2);
    }
};

class UnaryExpr : public ExprNode {
public:
    Tok op;
    std::unique_ptr<ExprNode> operand;
    UnaryExpr(Tok oper, std::unique_ptr<ExprNode> opera)
        : op(std::move(oper)), operand(std::move(opera)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "UnaryExpr(" << static_cast<int>(op) << ")\n";
        operand->dump(indent + 2);
    }
};



class BinaryExpr : public ExprNode {
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

class ReturnStatement : public StatementNode {
public:
    std::unique_ptr<ExprNode> expr;
    ReturnStatement(std::unique_ptr<ExprNode> e) : expr(std::move(e)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ReturnStatement\n";
        expr->dump(indent + 2);
    }
};

class BlockStatement : public StatementNode {
public:
    std::vector<std::unique_ptr<StatementNode>> statements;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "BlockStatement\n";
        for (const auto& stmt : statements) {
            stmt->dump(indent + 2);
        }
    }
};

class FunctionNode : public ASTNode {
public:
    std::string name;
    std::unique_ptr<Type> return_type;
    std::vector<Param> params;
    std::unique_ptr<BlockStatement> body;
    FunctionNode(const std::string& n, std::unique_ptr<Type> ret_type,
                 std::vector<Param> p, std::unique_ptr<BlockStatement> b)
        : name(n), return_type(std::move(ret_type)), params(std::move(p)), body(std::move(b)) {}
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "FunctionNode(" << name << ")\n";
        std::cout << std::string(indent + 2, ' ') << "ReturnType: " << return_type->to_string() << "\n";
        std::cout << std::string(indent + 2, ' ') << "Parameters:\n";
        for (const auto& param : params) {
            std::cout << std::string(indent + 4, ' ') << "Param(" << param.name << ", " << param.type->to_string() << ")\n";
        }
        std::cout << std::string(indent + 2, ' ') << "Body:\n";
        body->dump(indent + 4);
    }
};

class ProgramNode : public ASTNode {
public:
    std::vector<std::unique_ptr<DeclStmt>> glob_vars;
    std::vector<std::unique_ptr<FunctionNode>> functions;
    void dump(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "ProgramNode\n";
        std::cout << std::string(indent + 2, ' ') << "Globals:\n";
        for (const auto& gv : glob_vars) {
            gv->dump(indent + 4);
        }
        std::cout << std::string(indent + 2, ' ') << "Functions:\n";
        for (const auto& func : functions) {
            func->dump(indent + 4);
        }
    }
};

} // namespace AST
