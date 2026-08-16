#include"IR2RISCV.hpp"

namespace IR{

void IR2RISCV::generate(const Module* mod, std::ostream& out){
    instructions_.clear();
    gen_program(mod);
    for(const auto& line : instructions_) out << line << "\n";
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
    slot_of_.clear();
    next_offset_ = -12;
    for(const auto& bb : func->blocks){
        for(const auto& inst : bb->insts){
            if(inst->op == Opcode::ALLOCA || inst->type != VoidType::get()){
                slot_of_[inst.get()] = next_offset_;
                next_offset_ -= 4;
            }
        }
    }
    frame_size_ = ((-next_offset_ - 4 + 15) / 16) * 16;

    current_epilogue_ = ".L" + func->name + "_epilogue";
    emit(func->name + ":");
    emit("addi sp, sp, " + std::to_string(-frame_size_));
    emit("sw s0, " + std::to_string(frame_size_ - 4) + "(sp)");
    emit("sw ra, " + std::to_string(frame_size_ - 8) + "(sp)");
    emit("addi s0, sp, " + std::to_string(frame_size_));

    for(const auto& bb : func->blocks) gen_bb(func, bb.get());

    emit(current_epilogue_ + ":");
    emit("lw ra, " + std::to_string(-8) + "(s0)");
    emit("lw s0, " + std::to_string(-4) + "(s0)");
    emit("addi sp, sp, " + std::to_string(frame_size_));
    emit("ret");

}

void IR2RISCV::gen_bb(const Function* func, const BasicBlock* bb){
    emit(label_of(func, bb) + ":");
    for(const auto& inst : bb->insts){
        switch(inst->op){
        case Opcode::ALLOCA: break;
        case Opcode::LOAD:
            load_operand(inst->operands[0],"t0");
            store_result(inst.get(),"t0");
            break;
        case Opcode::STORE:
            load_operand(inst->operands[0],"t0");
            store_result(inst->operands[1],"t0");
            break;
        case Opcode::ADD:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("add t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::SUB:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::MUL:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("mul t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::DIV:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("div t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::REM:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("rem t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::NEG:
            load_operand(inst->operands[0],"t0");
            emit("sub t0, zero, t0");
            store_result(inst.get(),"t0");
            break;
        case Opcode::LT:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("slt t0, t0, t1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::GT:
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("slt t0, t1, t0");
            store_result(inst.get(),"t0");
            break;
        case Opcode::LE:  
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("slt t0, t1, t0");
            emit("xori t0, t0, 1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::GE:    
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("slt t0, t0, t1");
            emit("xori t0, t0, 1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::EQ:    
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            emit("sltiu t0, t0, 1");
            store_result(inst.get(),"t0");
            break;
        case Opcode::NE:    
            load_operand(inst->operands[0],"t0");
            load_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            emit("sltu t0, zero, t0");
            store_result(inst.get(),"t0");
            break;
        case Opcode::BR:
            load_operand(inst->operands[0],"t0");
            emit("beq t0, zero, " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[2])));
            emit("j " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[1])));
        break;
        case Opcode::JMP: 
            emit("j " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[0])));
        break;
        case Opcode::RET: 
            load_operand(inst->operands[0],"a0");
            emit("j " + current_epilogue_);
        break;
        case Opcode::CALL: 
            for(size_t i = 1;i < inst->operands.size();++i){
                load_operand(inst->operands[i],"a" + std::to_string(i-1));
            }
            emit("call " + inst->operands[0]->name);
            store_result(inst.get(), "a0");
        break;
        case Opcode::PHI:
        default:
            throw std::runtime_error("unknown opcode");
        }
    }
}

}//namespace IR

