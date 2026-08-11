#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>
#include "ast.hpp"

namespace IR {

enum class IRType {
    INT,
    // 预留：FLOAT, VOID, PTR
};

enum class Opcode {
    ADD, SUB, MUL, DIV, REM,
    LT, GT, LE, GE, EQ, NE,
    NEG,          // 一元负
    ASSIGN,       // 复制：result = lhs
    LOAD, STORE,  // 内存操作（预留）
    PARAM,        // 函数参数准备：param lhs
    CALL,         // result = call lhs(, rhs.i_val 为参数个数)
    RET,          // return lhs
    LABEL,        // 块内标签（预留，基本块 label 已够用）
    JMP,          // 无条件跳转：jmp jump_label
    JZ,           // 条件为 0 跳转：jz lhs, jump_label
    JNZ,          // 条件非 0 跳转：jnz lhs, jump_label
    PHI,          // 预留，P4 不实现
};

// ---------- Operand ----------

struct Operand {
    enum Kind { IMM, VAR, LABEL } kind;
    IRType type = IRType::INT;

    std::string name;   // VAR / LABEL 使用
    int i_val = 0;      // IMM 使用

    Operand() = default;

    static Operand imm(int v, IRType t = IRType::INT);
    static Operand var(const std::string& n, IRType t = IRType::INT);
    static Operand label(const std::string& n);

    bool is_imm() const;
    bool is_var() const;
    bool is_label() const;
};

// ---------- Param ----------

struct Param {
    std::string name;
    IRType type = IRType::INT;
};

// ---------- Instruction ----------

struct Instruction {
    Opcode op;
    IRType type = IRType::INT;

    Operand result;      // 结果（可能为空）
    Operand lhs;         // 左操作数 / 主要操作数
    Operand rhs;         // 右操作数 / 辅助操作数（可能为空）

    std::string jump_label;  // JMP / JZ / JNZ 的跳转目标
    std::string comment;     // 调试用注释

    explicit Instruction(Opcode o = Opcode::ADD);
};

// ---------- BasicBlock ----------

struct BasicBlock {
    std::string label;
    std::vector<Instruction> insts;

    // 跳转目标统一用 label 字符串，不受 block 增删/重排影响。
    // 实际跳转目标由最后一条 terminator 指令的 jump_label 决定，
    // 这里保留两个字段作为辅助缓存，方便 CFG 分析。
    std::string next_label;   // 顺序跳转 / fall-through
    std::string branch_label; // 条件分支

    void dump(std::ostream& out, int indent = 0) const;
};

// ---------- IRFunction ----------

struct IRFunction {
    std::string name;
    std::vector<Param> params;
    std::vector<Param> locals;

    std::vector<std::unique_ptr<BasicBlock>> blocks;
    std::unordered_map<std::string, size_t> label_to_idx;
    size_t entry_idx = 0;

    // 根据 blocks 重建 label -> index 映射
    void build_label_map();

    BasicBlock* find_block(const std::string& label);
    const BasicBlock* find_block(const std::string& label) const;

    void dump(std::ostream& out) const;
};

// ---------- IRProgram ----------

struct IRProgram {
    std::vector<std::unique_ptr<IRFunction>> functions;

    void dump(std::ostream& out) const;
};

class AST2IR{
public:
    std::unique_ptr<IRProgram> translate(ProgramNode* node);

private:
    std::unique_ptr<IRProgram> program_;
    IRFunction* current_func_ = nullptr;
    BasicBlock* current_block_ = nullptr;
    int tmp_counter_ = 0;
    int label_counter_ = 0;
    std::vector<std::unordered_map<std::string,std::string>> scope_stack_;

    // ===== 顶层结构 =====
    void gen_program(const ProgramNode* node);
    std::unique_ptr<IRFunction> gen_function(const FunctionNode* node);

    // ===== 语句翻译 =====
    void gen_block(const BlockStatement* node);
    void gen_statement(const StatementNode* node);
    void gen_decl(const DeclStmt* node);
    void gen_expr_stmt(const ExprStmt* node);
    void gen_return(const ReturnStatement* node);
    void gen_if(const IfStmt* node);
    void gen_while(const WhileStmt* node);

    // ===== 表达式翻译 =====
    Operand gen_expr(const ExprNode* node);
    Operand gen_binary(const BinaryExpr* node);
    Operand gen_unary(const UnaryExpr* node);
    Operand gen_call(const CallExpr* node);
    Operand gen_identifier(const IdentifierNode* node);
    Operand gen_assignment(const AssignmentExpr* node);
    Operand gen_number(const NumberNode* node);

    // ===== 辅助函数 =====
    void enter_scope();
    void exit_scope();
    std::string lookup_var(const std::string& name) const;
    std::string declare_var(const std::string& name);
    std::string new_temp();
    std::string new_label(const std::string& prefix);
    BasicBlock* new_block(const std::string& prefix);
    void emit(const Instruction& inst);
    Opcode map_op(Tok op) const;
};

} // namespace IR
