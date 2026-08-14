#pragma once
#include "ast.hpp"
#include "IR.hpp"
namespace IR{
class AST2IR{
public:
    std::unique_ptr<Module> translate(ProgramNode* program);
private:
    std::unique_ptr<Module> module_;
    Function* curr_func_ = nullptr;
    BasicBlock* curr_bb_ = nullptr;
    std::vector<std::unordered_map<std::string, Instruction*>> scope_stack_;
    void enter_scope(){scope_stack_.push_back(std::unordered_map<std::string, Instruction*>{});}
    void exit_scope(){if(!scope_stack_.empty()) scope_stack_.pop_back();}
    int block_counter_ = 0;
    int temp_counter_ = 0;
    std::string new_block_name(){return "_block" + std::to_string(block_counter_++);}
    std::string new_temp_name(){return "t" + std::to_string(++temp_counter_);}
    void gen_function(FunctionNode* func);
    void gen_stmt(StatementNode* stmt);
    Value* gen_expr(ExprNode* expr);
};


}