#pragma once
#include "ast.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <stdexcept>

class CodeGen {
public:
    void generate(ASTNode* ast, std::ostream& out);

private:
    std::vector<std::string> instructions;
    std::vector<std::unordered_map<std::string, int>> scope_stack; // relative to s0, aka, fp
    int count_decl_with_size(const BlockStatement* block);
    int count_params_with_size(const FunctionNode* funct);
    int calculate_frame_size(const FunctionNode* funct) {
        return count_params_with_size(funct) + count_decl_with_size(funct->body.get()) + 8;
    }
    // 8 means s0(4) + ra(4)
    int lookup_variable(const std::string& name);
    void enter_scope() { scope_stack.push_back(std::unordered_map<std::string, int>{}); }
    void exit_scope() { if (!scope_stack.empty()) scope_stack.pop_back(); }
    int frame_size = 0;
    int next_offset = -8;
    std::string current_epilogue;

    void emit(const std::string& line) { instructions.push_back(line); }

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
    std::string new_label(const std::string& prefix) { return prefix + std::to_string(label_counter++); }
};
