#include"IR2RISCV.hpp"
#include<cstring>
#include<sstream>

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

void IR2RISCV::load_pointer_operand(const Value* v, const std::string& reg){
    if(auto* c = dynamic_cast<const ConstantInt*>(v)){
        emit("li " + reg + ", " + std::to_string(c->i_val));
    } else if(dynamic_cast<const NULLPointer*>(v)){
        emit("li " + reg + ", 0");
    } else if(auto* c = dynamic_cast<const Argument*>(v)){
        emit("addi " + reg + ", " + arg_reg_of_.at(c) + ", 0");
    } else if(auto* g = dynamic_cast<const GlobalVariable*>(v)){
        emit("la " + reg + ", " + g->name);
    } else if(const Instruction* ins = dynamic_cast<const Instruction*>(v)){
        if(ins->op == Opcode::ALLOCA) emit("addi " + reg + ", " + "s0, " + std::to_string(slot_of_.at(v)));
        else emit("lw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
    } else {
        emit("lw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
    }
}

void IR2RISCV::load_float_operand(const Value* v, const std::string& reg){
    if(auto* c = dynamic_cast<const ConstantFloat*>(v)){
        emit("li t0, " + std::to_string(static_cast<int32_t>(float_to_bits(c->f_val))));
        emit("fmv.w.x " + reg + ", t0");
    } else if(auto* a = dynamic_cast<const Argument*>(v)){
        emit("fmv.s " + reg + ", " + arg_reg_of_.at(a));
    } else if(auto* g = dynamic_cast<const GlobalVariable*>(v)){
        emit("la t0, " + g->name);
        emit("flw " + reg + ", 0(t0)");
    }
    else {
        emit("flw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
    }
}

void IR2RISCV::store_int_result(const Value* v, const std::string& reg){
    if(auto* g = dynamic_cast<const GlobalVariable*>(v)){
        emit("la t1, " + g->name);
        emit("sw " + reg + ", 0(t1)");
    }
    else
    emit("sw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
}

void IR2RISCV::store_float_result(const Value* v, const std::string& reg){
    if(auto* g = dynamic_cast<const GlobalVariable*>(v)){
        emit("la t0, " + g->name);
        emit("fsw " + reg + ", 0(t0)");
    } 
    else
    emit("fsw " + reg + ", " + std::to_string(slot_of_.at(v)) + "(s0)");
}

void IR2RISCV::generate(const Module* mod, std::ostream& out){
    instructions_.clear();
    gen_program(mod);
    for(const auto& line : instructions_) out << line << "\n";
}

void IR2RISCV::gen_program(const Module* mod){

    if(!mod->get_globals().empty()){
        emit(".data");
        for(const auto& glob : mod->get_globals()){
            Type* elem_type = static_cast<PointerType*>(glob->type)->element_type;
            emit(glob->name + ":");
            if(elem_type == IntType::get()){
                auto* c = dynamic_cast<ConstantInt*>(glob->init_value);
                int val = 0;
                if(c) val = c->i_val;
                else throw std::runtime_error("[IR2RISCV] int global variable '" + glob->name + "' should only be initialized with an int literal");
                emit(".word " + std::to_string(val));
            } else if(elem_type == FloatType::get()){
                auto* c = dynamic_cast<ConstantFloat*>(glob->init_value);
                uint32_t bits = 0;
                if(c) bits = float_to_bits(c->f_val);
                else throw std::runtime_error("[IR2RISCV] float global variable '" + glob->name + "' should only be initialized with a float literal");
                std::stringstream ss;
                ss << "0x" << std::hex << bits;
                emit(".word " + ss.str());
            } else if(dynamic_cast<PointerType*>(elem_type)){
                auto* c = dynamic_cast<NULLPointer*>(glob->init_value);
                if(!c) throw std::runtime_error("[IR2RISCV] pointer global variable '" + glob->name + "' should only be initialized with nullptr");
                emit(".word 0");
            } else {
                throw std::runtime_error("[IR2RISCV] unsupported global variable type '" + elem_type->to_string() + "' for variable '" + glob->name + "'");
            }
        }
    }

    emit(".text");
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
        if(arg->type == IntType::get() || dynamic_cast<PointerType*>(arg->type)){
            arg_reg_of_[arg.get()] = "a" + std::to_string(int_arg_count++);
        } else if(arg->type == FloatType::get()){
            arg_reg_of_[arg.get()] = "fa" + std::to_string(float_arg_count++);
        } 
        else {
            throw std::runtime_error("[IR2RISCV] unsupported argument type '" + arg->type->to_string() + "' in function '" + func->name + "'");
        }
    }

    slot_of_.clear();
    next_offset_ = -8;
    for(const auto& bb : func->blocks){
        for(const auto& inst : bb->insts){
            if(inst->op == Opcode::ALLOCA){
                next_offset_ -= static_cast<PointerType*>(inst->type)->element_type->size();
                slot_of_[inst.get()] = next_offset_;
            } else if(inst->type != VoidType::get()){
                next_offset_ -= inst->type->size();
                slot_of_[inst.get()] = next_offset_;
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
            load_pointer_operand(inst->operands[0],"t0");
            if(inst->type == FloatType::get()){
                emit("flw ft0, 0(t0)");
                store_float_result(inst.get(),"ft0");
            } else {
                emit("lw t0, 0(t0)");
                store_int_result(inst.get(),"t0");
            }
            break;
        case Opcode::STORE:{
            Value* val = inst->operands[0];
            Value* addr = inst->operands[1];
            if(val->type == FloatType::get()){
                load_float_operand(val,"ft0");
            } else if(dynamic_cast<PointerType*>(val->type)){
                load_pointer_operand(val,"t0");
            } else {
                load_int_operand(val,"t0");
            }

            load_pointer_operand(addr,"t1");

            if(val->type == FloatType::get()){
                emit("fsw ft0, 0(t1)");
            } else {
                emit("sw t0, 0(t1)");
            }
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
            else if(dynamic_cast<PointerType*>(func->return_type))
                load_pointer_operand(inst->operands[0],"a0");
            emit("j " + current_epilogue_);
            break;
        case Opcode::CALL: {
            int temp_int_cnt = 0;
            int temp_float_cnt = 0;
            for(size_t i = 1;i < inst->operands.size();++i){
                if(inst->operands[i]->type == IntType::get())
                    load_int_operand(inst->operands[i],"a" + std::to_string(temp_int_cnt++));
                else if(inst->operands[i]->type == FloatType::get())
                    load_float_operand(inst->operands[i],"fa" + std::to_string(temp_float_cnt++));
                else if(dynamic_cast<PointerType*>(inst->operands[i]->type))
                    load_pointer_operand(inst->operands[i],"a" + std::to_string(temp_int_cnt++));
            }
            emit("call " + inst->operands[0]->name);
            if(inst->type == IntType::get() || dynamic_cast<PointerType*>(inst->type))
                store_int_result(inst.get(), "a0");
            else if(inst->type == FloatType::get())
                store_float_result(inst.get(),"fa0");
            break;
        }
        case Opcode::GETPTR:{
            load_int_operand(inst->operands[1],"t1");
            load_pointer_operand(inst->operands[0],"t0");
            auto pt = dynamic_cast<PointerType*>(inst->type);
            if(pt->element_type->size() == 4) emit("slli t1, t1, 2");
            else throw std::runtime_error("[IR2RISCV] getptr unsupported type");
            emit("add   t0, t0, t1");
            store_int_result(inst.get(),"t0");
        }
        break;
        case Opcode::PTRDIFF:{
            load_pointer_operand(inst->operands[0],"t0");
            load_pointer_operand(inst->operands[1],"t1");
            emit("sub t0,t0,t1");
            auto pt = dynamic_cast<PointerType*>(inst->operands[0]->type);
            if(pt->element_type->size() == 4) emit("srai t0, t0, 2");
            else throw std::runtime_error("[IR2RISCV] ptrdiff unsupported pointee size");
            store_int_result(inst.get(),"t0");
        }
        break;
        case Opcode::PHI:
        default:
            throw std::runtime_error("[IR2RISCV] unknown opcode " + std::to_string(static_cast<int>(inst->op)) + " in function '" + func->name + "'");
        }
    }
}

}//namespace IR

