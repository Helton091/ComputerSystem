#pragma once
#include "IR.hpp"

namespace IR{
class IR2RISCV {
public:
    void generate(const IRProgram& program, std::ostream& out);

private:
    // 输出缓冲区
    std::vector<std::string> asm_lines_;
    void emit(const std::string& line);

    // 当前函数状态
    const IRFunction* current_func_ = nullptr;
    std::unordered_map<std::string, int> var_offset_;  // 变量名 -> 相对 s0 偏移
    int frame_size_ = 0;
    int next_offset_ = -8;  // 参数/局部变量从 -8 开始（-4 是 s0，-8 是 ra）
    std::string current_epilogue_;
    int label_counter_ = 0;

    std::string new_label(const std::string& prefix);

    void gen_program(const IRProgram& program);
    void gen_function(const IRFunction& func);
    void gen_block(const BasicBlock& block);
    void gen_instruction(const Instruction& inst);

    // 变量访问
    void load_operand_to_t0(const Operand& op);
    void load_operand_to_register(const Operand& op, const std::string& reg);
    int lookup_var_offset(const std::string& name);

    // 栈操作
    void push_t0();
    void pop_to_register(const std::string& reg);
};
}