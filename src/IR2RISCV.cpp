#include"IR2RISCV.hpp"
#include<cstring>

namespace IR{

static uint32_t float_to_bits(float f){
    uint32_t bits;
    static_assert(sizeof(f) == sizeof(bits));
    std::memcpy(&bits,&f,sizeof(f));
    return bits;
}

void IR2RISCV::load_int_operand(const Value* v, const std::string& reg){
    if(auto* c = dynamic_cast<const ConstantInt*>(v))
        emit("li " + reg + ", " + std::to_string(c->i_val));
    else if(auto* a = dynamic_cast<const Argument*>(v))
        emit("addi " + reg + ", " + arg_reg_of_.at(a) + ", 0");
    else
        emit("lw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
}

void IR2RISCV::load_float_operand(const Value* v, const std::string& reg){
    if(auto* c = dynamic_cast<const ConstantFloat*>(v)){
        emit("li t0, " + std::to_string(static_cast<int32_t>(float_to_bits(c->f_val))));
        emit("fmv.w.x " + reg + ", t0");
    } else if(auto* a = dynamic_cast<const Argument*>(v)){
        emit("fmv.s " + reg + ", " + arg_reg_of_.at(a));
    } else {
        emit("flw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
    }
}

void IR2RISCV::store_int_result(const Value* v, const std::string& reg){
    emit("sw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
}

void IR2RISCV::store_float_result(const Value* v, const std::string& reg){
    emit("fsw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
}

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
    int int_arg_count = 0;
    int float_arg_count = 0;
    for(const auto& arg : func->args){
        if(arg->type == IntType::get()){
            arg_reg_of_[arg.get()] = "a" + std::to_string(int_arg_count++);
        } else if(arg->type == FloatType::get()){
            arg_reg_of_[arg.get()] = "fa" + std::to_string(float_arg_count++);
        } else {
            throw std::runtime_error("unsupported argument type");
        }
    }

    slot_of_.clear();
    next_offset_ = -12;
    for(const auto& bb : func->blocks){
        for(const auto& inst : bb->insts){
            if(inst->op == Opcode::ALLOCA || inst->type != VoidType::get()){
                slot_of_[inst.get()] = next_offset_;
                next_offset_ -= inst->type->size();
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
            if(inst->type == IntType::get()){
                load_int_operand(inst->operands[0],"t0");
                store_int_result(inst.get(),"t0");
            } else if(inst->type == FloatType::get()){
                load_float_operand(inst->operands[0],"ft0");
                store_float_result(inst.get(),"ft0");
            }
            break;
        case Opcode::STORE:
            if(inst->operands[0]->type == IntType::get()){
                load_int_operand(inst->operands[0],"t0");
                store_int_result(inst->operands[1],"t0");
            } else if(inst->operands[0]->type == FloatType::get()){
                load_float_operand(inst->operands[0],"ft0");
                store_float_result(inst->operands[1],"ft0");
            }
            break;
        case Opcode::ADD:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("add t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FADD:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("fadd.s ft0, ft0, ft1");
            store_float_result(inst.get(),"ft0");
            break;
        case Opcode::SUB:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FSUB:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("fsub.s ft0, ft0, ft1");
            store_float_result(inst.get(),"ft0");
            break;
        case Opcode::MUL:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("mul t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FMUL:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("fmul.s ft0, ft0, ft1");
            store_float_result(inst.get(),"ft0");
            break;
        case Opcode::DIV:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("div t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FDIV:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("fdiv.s ft0, ft0, ft1");
            store_float_result(inst.get(),"ft0");
            break;
        case Opcode::REM:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("rem t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::NEG:
            load_int_operand(inst->operands[0],"t0");
            emit("sub t0, zero, t0");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FNEG:
            load_float_operand(inst->operands[0],"ft0");
            emit("fneg.s ft0, ft0");
            store_float_result(inst.get(),"ft0");
            break;
        case Opcode::LT:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("slt t0, t0, t1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FLT:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("flt.s t0, ft0, ft1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::GT:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("slt t0, t1, t0");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FGT:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("flt.s t0, ft1, ft0");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::LE:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("slt t0, t1, t0");
            emit("xori t0, t0, 1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FLE:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("fle.s t0, ft0, ft1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::GE:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("slt t0, t0, t1");
            emit("xori t0, t0, 1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FGE:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("flt.s t0, ft0, ft1");
            emit("xori t0, t0, 1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::EQ:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            emit("sltiu t0, t0, 1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FEQ:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("feq.s t0, ft0, ft1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::NE:
            load_int_operand(inst->operands[0],"t0");
            load_int_operand(inst->operands[1],"t1");
            emit("sub t0, t0, t1");
            emit("sltu t0, zero, t0");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::FNE:
            load_float_operand(inst->operands[0],"ft0");
            load_float_operand(inst->operands[1],"ft1");
            emit("feq.s t0, ft0, ft1");
            emit("xori t0, t0, 1");
            store_int_result(inst.get(),"t0");
            break;
        case Opcode::BR:
            load_int_operand(inst->operands[0],"t0");
            emit("beq t0, zero, " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[2])));
            emit("j " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[1])));
        break;
        case Opcode::JMP: 
            emit("j " + label_of(func,dynamic_cast<BasicBlock*>(inst->operands[0])));
        break;
        case Opcode::RET:
            if(func->return_type == IntType::get())
                load_int_operand(inst->operands[0],"a0");
            else if(func->return_type == FloatType::get())
                load_float_operand(inst->operands[0],"fa0");
            emit("j " + current_epilogue_);
            break;
        case Opcode::CALL: {
            int temp_int_cnt = 0;
            int temp_float_cnt = 0;
            for(size_t i = 1;i < inst->operands.size();++i){
                if(inst->operands[i]->type == IntType::get())
                    load_int_operand(inst->operands[i],"a" + std::to_string(temp_int_cnt++));
                else
                    load_float_operand(inst->operands[i],"fa" + std::to_string(temp_float_cnt++));
            }
            emit("call " + inst->operands[0]->name);
            if(inst->type == IntType::get())
                store_int_result(inst.get(), "a0");
            else if(inst->type == FloatType::get())
                store_float_result(inst.get(),"fa0");
            break;
        }
        case Opcode::PHI:
        default:
            throw std::runtime_error("unknown opcode");
        }
    }
}

}//namespace IR

