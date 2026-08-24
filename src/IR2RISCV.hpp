#pragma once
#include "IR.hpp"

namespace IR{

class IR2RISCV{
public:
    void generate(const Module* mod, std::ostream& out);
private:
    std::vector<std::string> instructions_;
    std::unordered_map<const Value*, int> slot_of_;// relative to s0, aka, fp
    int frame_size_ = 0;
    int next_offset_ = -12;
    std::string current_epilogue_;
    void emit(const std::string& line) { instructions_.push_back(line); }
    void gen_program(const Module* mod);
    void gen_function(const Function* func);
    void gen_bb(const Function* func, const BasicBlock* bb);
    int label_counter = 0;
    std::string new_label(const std::string& prefix) { return prefix + std::to_string(label_counter++); }
    std::string label_of(const Function* f, const BasicBlock* bb){
        return ".L" + f->name + "_" + bb->name;
    }
    std::unordered_map<const Argument*,std::string> arg_reg_of_;
    void load_int_operand(const Value* v, const std::string& reg);
    void load_pointer_operand(const Value* v, const std::string& reg);
    void load_float_operand(const Value* v, const std::string& reg);
    void store_int_result(const Value* v, const std::string& reg);
    void store_float_result(const Value* v, const std::string& reg);

};
}
