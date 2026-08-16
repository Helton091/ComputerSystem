#include"IR2RISCV.hpp"

namespace IR{
int IR2RISCV::calculate_frame_size(const Function* func){
    int frame_size = 8;
    const BasicBlock* entry = func->entry;
    const std::vector<std::unique_ptr<Instruction>>& insts = entry->insts;
    for(const auto& i : insts){
        if(i->op == Opcode::ALLOCA) frame_size += 4;
    }
    return frame_size;
}

void IR2RISCV::gen_program(const Module* mod){
    emit("_start:");
    emit("call main");
    emit("li a7, 10");
    emit("ecall");
    const auto& funcs = mod->get_functions();
    for(const auto& func : funcs){
        gen_function(func.get());
    }

}

void IR2RISCV::gen_function(const Function* func){
    next_offset_ = -12;
    frame_size_ = calculate_frame_size(func);
    current_epilogue_ = new_label(func->name + "_epilogue");
    emit(func->name + ":");
    emit("addi sp, sp, " + std::to_string(-frame_size_));
    emit("sw s0, " + std::to_string(frame_size_ - 4) + "(sp)");
    emit("sw ra, " + std::to_string(frame_size_ - 8) + "(sp)");
    emit("addi s0, sp, " + std::to_string(frame_size_));

    const BasicBlock* entry = func->entry;
    const std::vector<std::unique_ptr<Instruction>>& insts = entry->insts;
    for(const auto& i : insts){
        if(i->op == Opcode::ALLOCA){
            slot_of_[i.get()] = next_offset_;
            next_offset_ -= 4;
        }
    }

    for(const auto& bb : func->blocks) gen_bb(bb.get());

    emit(current_epilogue_ + ":");
    emit("lw ra, " + std::to_string(-8) + "(s0)");
    emit("lw s0, " + std::to_string(-4) + "(s0)");
    emit("addi sp, sp, " + std::to_string(frame_size_));
    emit("ret");

}

void IR2RISCV::gen_bb(const BasicBlock* bb){
    emit(new_label(bb->name) + ":");
    for(const auto& inst : bb->insts){
        switch(inst->op){
        case Opcode::ALLOCA: break;
        case Opcode::LOAD: break;
        case Opcode::STORE: break;
        case Opcode::ADD: break;
        case Opcode::SUB: break;
        case Opcode::MUL: break;
        case Opcode::DIV: break;
        case Opcode::REM: break;
        case Opcode::NEG: break;
        case Opcode::LT: break;
        case Opcode::GT: break;
        case Opcode::LE: break;
        case Opcode::GE: break;
        case Opcode::EQ: break;
        case Opcode::NE: break;
        case Opcode::BR: break;
        case Opcode::JMP: break;
        case Opcode::RET: break;
        case Opcode::CALL: break;
        case Opcode::PHI: break;
        }
    }
}

}//namespace IR

