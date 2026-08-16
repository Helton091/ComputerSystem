#include "IR.hpp"

namespace IR {

void Value::remove_use(User* u){
    for(auto it = users.begin(); it != users.end(); ++it){
        if(*it == u){
            users.erase(it);
            return;
        }
    }
    throw std::runtime_error("when remove user" + u->name + " in " + name + ", didn't find " + u->name);
}

void Value::replace_all_uses_with(Value* v){
    for(User* u : users){
        for(Value*& op : u->operands){
            if(op == this){
                op = v;
            }
        }
    }
    v->users.insert(v->users.end(),users.begin(),users.end());
    users.clear();
}

void User::add_operand(Value* v){
    operands.push_back(v);
    if(v) v->add_use(this);
}

void User::drop_operands(){
    for(Value* v : operands){
        if(v) v->remove_use(this);
    }
    operands.clear();
}

void Instruction::erase_from_parent(){
    if(!parent) return;
    if(!users.empty()) throw std::runtime_error("erase_from_parent: instruction still has users: " + name);
    std::vector<std::unique_ptr<Instruction>>& insts = parent->insts;
    for(auto it = insts.begin();it != insts.end(); ++it){
        if(it->get() == this){
            insts.erase(it);
            return;
        }
    }
}

bool BasicBlock::is_terminated() const{
    if(insts.empty()) return false;
    switch(insts.back()->op){
    case Opcode::BR:
    case Opcode::JMP:
    case Opcode::RET:
        return true;
    default:
        return false;
    }
}

Instruction* BasicBlock::add_inst(std::unique_ptr<Instruction> inst){
    if(is_terminated()) throw std::runtime_error("cannot add a new instruction to a terminated block");
    inst->parent = this;
    insts.push_back(std::move(inst));
    return insts.back().get();
}

Argument* Function::add_arg(){
    args.push_back(std::make_unique<Argument>(args.size()));
    return args.back().get();
}

BasicBlock* Function::add_block(const std::string& name){
    std::unique_ptr<BasicBlock> new_block = std::make_unique<BasicBlock>(name);
    new_block->parent = this;
    if(blocks.empty()) entry = new_block.get();
    blocks.push_back(std::move(new_block));
    return blocks.back().get();
}

Function* Module::add_function(const std::string& name){
    if(find_function(name)) throw std::runtime_error("duplicated function " + name);
    functions.push_back(std::make_unique<Function>(name));
    return functions.back().get();
}

Function* Module::find_function(const std::string& name) const{
    for(auto it = functions.begin(); it != functions.end();++it){
        if(name == it->get()->name) return it->get();
    }
    return nullptr;
}

ConstantInt* Module::get_const(int v){
    auto it = const_map_.find(v);
    if(it == const_map_.end()){
        const_pool_.push_back(std::make_unique<ConstantInt>(v));
        const_map_[v] = const_pool_.back().get();
    }
    return const_map_[v];
}

// ============================================================
// 打印（文本格式即测试比对基准，见报告第 6 章）
// ============================================================

static std::string op_str(Opcode op){
    switch(op){
    case Opcode::ALLOCA: return "alloca";
    case Opcode::LOAD:   return "load";
    case Opcode::STORE:  return "store";
    case Opcode::ADD:    return "add";
    case Opcode::SUB:    return "sub";
    case Opcode::MUL:    return "mul";
    case Opcode::DIV:    return "div";
    case Opcode::REM:    return "rem";
    case Opcode::NEG:    return "neg";
    case Opcode::LT:     return "lt";
    case Opcode::GT:     return "gt";
    case Opcode::LE:     return "le";
    case Opcode::GE:     return "ge";
    case Opcode::EQ:     return "eq";
    case Opcode::NE:     return "ne";
    case Opcode::BR:     return "br";
    case Opcode::JMP:    return "jmp";
    case Opcode::RET:    return "ret";
    case Opcode::CALL:   return "call";
    case Opcode::PHI:    return "phi";
    }
    return "?";
}

// 操作数在指令里的样子：常量印数字，函数印 @名，其余印 %名
static std::string operand_str(const Value* v){
    if(auto* c = dynamic_cast<const ConstantInt*>(v))
        return std::to_string(c->i_val);
    if(auto* f = dynamic_cast<const Function*>(v))
        return "@" + f->name;
    return "%" + v->name;
}

static void dump_inst(const Instruction& inst, std::ostream& out){
    out << "  ";
    if(inst.type != VoidType::get())
        out << "%" << inst.name << " = ";

    switch(inst.op){
    case Opcode::ALLOCA:
        out << "alloca " << inst.type->to_string();
        break;
    case Opcode::LOAD:
        out << "load " << inst.type->to_string() << ", "
            << inst.type->to_string() << "* " << operand_str(inst.operands[0]);
        break;
    case Opcode::STORE:
        out << "store " << inst.operands[0]->type->to_string() << " "
            << operand_str(inst.operands[0]) << ", "
            << inst.operands[0]->type->to_string() << "* "
            << operand_str(inst.operands[1]);
        break;
    case Opcode::BR:
        out << "br i32 " << operand_str(inst.operands[0])
            << ", label %" << inst.operands[1]->name
            << ", label %" << inst.operands[2]->name;
        break;
    case Opcode::JMP:
        out << "jmp label %" << inst.operands[0]->name;
        break;
    case Opcode::RET:
        out << "ret " << inst.operands[0]->type->to_string() << " "
            << operand_str(inst.operands[0]);
        break;
    case Opcode::CALL:
        out << "call " << inst.type->to_string() << " "
            << operand_str(inst.operands[0]) << "(";
        for(size_t i = 1; i < inst.operands.size(); ++i){
            if(i > 1) out << ", ";
            out << inst.operands[i]->type->to_string() << " "
                << operand_str(inst.operands[i]);
        }
        out << ")";
        break;
    default:
        // 算术 / 比较：op type lhs, rhs；NEG 只有一个操作数
        out << op_str(inst.op) << " " << inst.type->to_string()
            << " " << operand_str(inst.operands[0]);
        if(inst.operands.size() == 2)
            out << ", " << operand_str(inst.operands[1]);
        break;
    }
    out << "\n";
}

void Module::dump(std::ostream& out) const{
    for(const auto& func : functions){
        out << "define i32 @" << func->name << "(";
        for(size_t i = 0; i < func->args.size(); ++i){
            if(i > 0) out << ", ";
            out << "i32 %" << func->args[i]->name;
        }
        out << ") {\n";
        for(const auto& block : func->blocks){
            out << block->name << ":\n";
            for(const auto& inst : block->insts)
                dump_inst(*inst, out);
        }
        out << "}\n\n";
    }
}

Instruction* Function::add_alloca(const std::string& var_name){
        std::unique_ptr<Instruction> inst = std::make_unique<Instruction>(Opcode::ALLOCA,IntType::get(),var_name);
        Instruction* inst_spec = inst.get();
        BasicBlock* target = blocks[0].get();
        target->insts.insert(target->insts.end()-1,std::move(inst));
        return inst_spec;
}

Instruction* make_inst(BasicBlock* bb, Opcode op, Type* type,
                       const std::string& name,
                       const std::vector<Value*>& operands){
    std::unique_ptr<Instruction> new_inst = std::make_unique<Instruction>(op,type,name);
    for(Value* operand : operands) new_inst->add_operand(operand);
    switch(op){
    case Opcode::ADD:
    case Opcode::SUB:
    case Opcode::MUL:
    case Opcode::DIV:
    case Opcode::REM:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("for ADD/SUB/MUL/DIV/REM, there should be exactly 2 operand");
        if(operands[0]->type != operands[1]->type) throw std::runtime_error("for ADD/SUB/MUL/DIV/REM, there operands' type must be equal");
        if(type != operands[0]->type) throw std::runtime_error("for ADD/SUB/MUL/DIV/REM， the result's type must be equal to operands' type");
    }
    break;
    case Opcode::LT:
    case Opcode::GT:
    case Opcode::LE:
    case Opcode::GE:
    case Opcode::EQ:
    case Opcode::NE:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("for LT/GT/LE/GE/EQ/NE, there should be exactly 2 operand");
        if(operands[0]->type != operands[1]->type) throw std::runtime_error("for LT/GT/LE/GE/EQ/NE, there operands' type must be equal");
        if(type != IntType::get()) throw std::runtime_error("for LT/GT/LE/GE/EQ/NE， the result's type must be I32");
    }
    break;
    case Opcode::NEG:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("negation should have only one operand");
        if(type != new_inst->operands[0]->type) throw std::runtime_error("negation should have same type with its operand");
    }
    break;
    case Opcode::LOAD:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 1) throw std::runtime_error("for load, there should be exactly 1 operand");
        Value* val = operands[0];
        const Instruction* ins = nullptr;
        if(!(ins = dynamic_cast<const Instruction*>(val)) || (ins->op != Opcode::ALLOCA))
            throw std::runtime_error("the operand of load must be the result of alloca");
        if(ins->type != type)
            throw std::runtime_error("type of load and it's operand must be the same");
        
    }
    break;
    case Opcode::STORE:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("store should have 2 operands");
        const Instruction* ins = dynamic_cast<const Instruction*>(operands[1]);
        if(!ins || ins->op != Opcode::ALLOCA) 
            throw std::runtime_error("the second operand of store must be alloca instruction");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("store's type must be void");
        if(operands[0]->type != ins->type) throw std::runtime_error("store's two operands' type must be the same");
    }
    break;
    case Opcode::BR:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 3) throw std::runtime_error("there should be 3 operands of BR");
        if(!dynamic_cast<BasicBlock*>(operands[1]) || !dynamic_cast<BasicBlock*>(operands[2])) throw std::runtime_error("last two operands of BR must be BasicBlock");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("BR's type must be void");
    }
    break;
    case Opcode::JMP:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("jmp needs exactly 1 operand");
        if(!dynamic_cast<BasicBlock*>(new_inst->operands[0])) throw std::runtime_error("jmp target must be a BasicBlock");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("jmp's type must be void");
    }
    break;
    case Opcode::RET:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("ret needs exactly 1 operand");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("ret's type must be void");
    }
    break;
    case Opcode::CALL:{
        if(new_inst->operands.empty()) throw std::runtime_error("call needs at least the callee");
        Function* callee = dynamic_cast<Function*>(new_inst->operands[0]);
        if(!callee) throw std::runtime_error("call's first operand must be a Function");
        if(new_inst->operands.size() - 1 != callee->args.size())
            throw std::runtime_error("call argument count mismatch with function " + callee->name);
        if(new_inst->type != IntType::get())
            throw std::runtime_error("call result type must be i32 (all functions return i32 for now)");
    }
    break;
    case Opcode::PHI:
        throw std::runtime_error("PHI is not implemented (mem2reg not in place yet)");
    default:
        throw std::runtime_error("unknown op");
    }

    return bb->add_inst(std::move(new_inst));
    
}


} // namespace IR
