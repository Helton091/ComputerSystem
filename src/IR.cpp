#include "IR.hpp"

namespace IR {

void Value::remove_use(User* u){
    for(auto it = users.begin(); it != users.end(); ++it){
        if(*it == u){
            users.erase(it);
            return;
        }
    }
    throw std::runtime_error("[IR] when remove user " + u->name + " in " + name + ", didn't find " + u->name);
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
    if(!users.empty()) throw std::runtime_error("[IR] erase_from_parent: instruction '" + name + "' still has users");
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
    if(is_terminated()) throw std::runtime_error("[IR] cannot add a new instruction to terminated block '" + name + "'");
    inst->parent = this;
    insts.push_back(std::move(inst));
    return insts.back().get();
}

Argument* Function::add_arg(Type* typ){
    args.push_back(std::make_unique<Argument>(args.size(),typ));
    return args.back().get();
}

BasicBlock* Function::add_block(const std::string& name){
    std::unique_ptr<BasicBlock> new_block = std::make_unique<BasicBlock>(name);
    new_block->parent = this;
    if(blocks.empty()) entry = new_block.get();
    blocks.push_back(std::move(new_block));
    return blocks.back().get();
}

GlobalVariable* Module::find_global(const std::string& name) const{
    auto it = global_map_.find(name);
    if(it != global_map_.end()) return it->second;
    return nullptr;
}

GlobalVariable* Module::add_global(const std::string& name, Type* type, Value* init){
    if(find_global(name)) throw std::runtime_error("[IR] duplicated global variable '" + name + "'");
    globals.push_back(std::make_unique<GlobalVariable>(type,name,init));
    global_map_[name] = globals.back().get();
    return globals.back().get();
}

Function* Module::add_function(const std::string& name, Type* typ){
    if(find_function(name)) throw std::runtime_error("[IR] duplicated function '" + name + "'");
    functions.push_back(std::make_unique<Function>(name,typ));
    function_map_[name] = functions.back().get();
    return functions.back().get();
}

Function* Module::find_function(const std::string& name) const{
    auto it = function_map_.find(name);
    if(it != function_map_.end()) return it->second;
    return nullptr;
}

ConstantInt* Module::get_const(int v){
    auto it = int_const_map_.find(v);
    if(it == int_const_map_.end()){
        int_const_pool_.push_back(std::make_unique<ConstantInt>(v));
        int_const_map_[v] = int_const_pool_.back().get();
    }
    return int_const_map_[v];
}

ConstantFloat* Module::get_const(float f){
    auto it = float_const_map_.find(f);
    if(it == float_const_map_.end()){
        float_const_pool_.push_back(std::make_unique<ConstantFloat>(f));
        float_const_map_[f] = float_const_pool_.back().get();
    }
    return float_const_map_[f];
}

NULLPointer* Module::get_nullptr(Type* pointee_type){
    auto it = NULLPointer_map_.find(pointee_type);
    if(it == NULLPointer_map_.end()){
        NULLPointer_pool_.push_back(std::make_unique<NULLPointer>(pointee_type));
        NULLPointer_map_[pointee_type] = NULLPointer_pool_.back().get();
    }
    return NULLPointer_map_[pointee_type];
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
    case Opcode::FADD:   return "fadd";
    case Opcode::FSUB:   return "fsub";
    case Opcode::FMUL:   return "fmul";
    case Opcode::FDIV:   return "fdiv";
    case Opcode::FNEG:   return "fneg";
    case Opcode::LT:     return "lt";
    case Opcode::GT:     return "gt";
    case Opcode::LE:     return "le";
    case Opcode::GE:     return "ge";
    case Opcode::EQ:     return "eq";
    case Opcode::NE:     return "ne";
    case Opcode::FLT:    return "flt";
    case Opcode::FGT:    return "fgt";
    case Opcode::FLE:    return "fle";
    case Opcode::FGE:    return "fge";
    case Opcode::FEQ:    return "feq";
    case Opcode::FNE:    return "fne";
    case Opcode::BR:     return "br";
    case Opcode::JMP:    return "jmp";
    case Opcode::RET:    return "ret";
    case Opcode::CALL:   return "call";
    case Opcode::GETPTR: return "getptr";
    case Opcode::PHI:    return "phi";
    }
    return "?";
}

// 操作数在指令里的样子：常量印数字，函数印 @名，其余印 %名
static std::string operand_str(const Value* v){
    if(auto* c = dynamic_cast<const ConstantInt*>(v))
        return std::to_string(c->i_val);
    if(auto* f = dynamic_cast<const ConstantFloat*>(v))
        return std::to_string(f->f_val);
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
        if(inst.operands.empty()){
            out << "ret void";
        } else {
            out << "ret " << inst.operands[0]->type->to_string() << " "
                << operand_str(inst.operands[0]);
        }
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
    for(const auto& glob : globals){
        out << "@" << glob->name << " = global " << glob->type->to_string();
        if(glob->init_value){
            out << " " << operand_str(glob->init_value);
        } else {
            out << " 0";
        }
        out << "\n";
    }
    if(!globals.empty()) out << "\n";

    for(const auto& func : functions){
        out << "define " << func->return_type->to_string() << " @" << func->name << "(";
        for(size_t i = 0; i < func->args.size(); ++i){
            if(i > 0) out << ", ";
            out << func->args[i]->type->to_string() << " %" << func->args[i]->name;
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

Instruction* Function::add_alloca(const std::string& var_name, Type* typ){
        std::unique_ptr<Instruction> inst = std::make_unique<Instruction>(Opcode::ALLOCA,typ,var_name);
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
        if(operands.size() != 2) throw std::runtime_error("[IR] for ADD/SUB/MUL/DIV/REM, there should be exactly 2 operands");
        if(operands[0]->type != IntType::get() || operands[1]->type != IntType::get())
            throw std::runtime_error("[IR] for ADD/SUB/MUL/DIV/REM, operands must be i32");
        if(operands[0]->type != operands[1]->type) throw std::runtime_error("[IR] for ADD/SUB/MUL/DIV/REM, operands' type must be equal");
        if(type != IntType::get()) throw std::runtime_error("[IR] for ADD/SUB/MUL/DIV/REM, the result's type must be i32");
    }
    break;
    case Opcode::LT:
    case Opcode::GT:
    case Opcode::LE:
    case Opcode::GE:
    case Opcode::EQ:
    case Opcode::NE:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("[IR] for LT/GT/LE/GE/EQ/NE, there should be exactly 2 operands");
        if(operands[0]->type != IntType::get() || operands[1]->type != IntType::get())
            throw std::runtime_error("[IR] for LT/GT/LE/GE/EQ/NE, operands must be i32");
        if(operands[0]->type != operands[1]->type) throw std::runtime_error("[IR] for LT/GT/LE/GE/EQ/NE, operands' type must be equal");
        if(type != IntType::get()) throw std::runtime_error("[IR] for LT/GT/LE/GE/EQ/NE, the result's type must be i32");
    }
    break;
    case Opcode::NEG:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("[IR] negation should have only one operand");
        if(new_inst->operands[0]->type != IntType::get()) throw std::runtime_error("[IR] negation operand must be i32");
        if(type != IntType::get()) throw std::runtime_error("[IR] negation result must be i32");
    }
    break;
    case Opcode::FADD:
    case Opcode::FSUB:
    case Opcode::FMUL:
    case Opcode::FDIV:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("[IR] for FADD/FSUB/FMUL/FDIV, there should be exactly 2 operands");
        if(operands[0]->type != FloatType::get() || operands[1]->type != FloatType::get())
            throw std::runtime_error("[IR] for FADD/FSUB/FMUL/FDIV, operands must be float");
        if(type != FloatType::get()) throw std::runtime_error("[IR] for FADD/FSUB/FMUL/FDIV, the result's type must be float");
    }
    break;
    case Opcode::FLT:
    case Opcode::FGT:
    case Opcode::FLE:
    case Opcode::FGE:
    case Opcode::FEQ:
    case Opcode::FNE:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("[IR] for FLT/FGT/FLE/FGE/FEQ/FNE, there should be exactly 2 operands");
        if(operands[0]->type != FloatType::get() || operands[1]->type != FloatType::get())
            throw std::runtime_error("[IR] for FLT/FGT/FLE/FGE/FEQ/FNE, operands must be float");
        if(type != IntType::get()) throw std::runtime_error("[IR] for FLT/FGT/FLE/FGE/FEQ/FNE, the result's type must be i32");
    }
    break;
    case Opcode::FNEG:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("[IR] float negation should have only one operand");
        if(new_inst->operands[0]->type != FloatType::get()) throw std::runtime_error("[IR] float negation operand must be float");
        if(type != FloatType::get()) throw std::runtime_error("[IR] float negation result must be float");
    }
    break;
    case Opcode::LOAD: {
        if(operands.size() != 1)
            throw std::runtime_error("[IR] load instruction requires exactly one operand");
        
        auto operand_type = operands[0]->type;
        auto pt = dynamic_cast<PointerType*>(operand_type);
        if(!pt)
            throw std::runtime_error("[IR] the operand of load must have pointer type");
        
        if(pt->element_type != type)
            throw std::runtime_error("[IR] load result type does not match operand's element type");
    }
    break;
    case Opcode::STORE:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("[IR] store should have 2 operands");
        auto type1 = dynamic_cast<PointerType*>(operands[1]->type);
        if(!type1)
            throw std::runtime_error("[IR] the destination of store must be pointertype");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("[IR] store's type must be void");
        if(type1 != PointerType::get(operands[0]->type) && (!dynamic_cast<NULLPointer*>(operands[0]))) throw std::runtime_error("[IR] store's second operand should be the pointer type of the first one");
    }
    break;

    case Opcode::GETPTR:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 2) throw std::runtime_error("[IR] get pointer should have 2 operands");
        auto type1 = dynamic_cast<PointerType*>(operands[0]->type);
        if(!type1) throw std::runtime_error("[IR] base type of get pointer should have pointer type");
        if(!dynamic_cast<IntType*>(operands[1]->type)) throw std::runtime_error("[IR] the second operand of GETPTR should be int type");
        if(auto ar_tp = dynamic_cast<ArrayType*>(type1->element_type)){
            if(new_inst->type != PointerType::get(ar_tp->element_type)) throw std::runtime_error("[IR] get pointer's return type should be equal to the base type");
        }
        else if(new_inst->type != PointerType::get(type1->element_type)) throw std::runtime_error("[IR] get pointer's return type should be equal to the base type");
    }
    break;
    case Opcode::BR:{
        std::vector<Value*> operands = new_inst->operands;
        if(operands.size() != 3) throw std::runtime_error("[IR] there should be 3 operands of BR");
        if(!dynamic_cast<BasicBlock*>(operands[1]) || !dynamic_cast<BasicBlock*>(operands[2])) throw std::runtime_error("[IR] last two operands of BR must be BasicBlock");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("[IR] BR's type must be void");
    }
    break;
    case Opcode::JMP:{
        if(new_inst->operands.size() != 1) throw std::runtime_error("[IR] jmp needs exactly 1 operand");
        if(!dynamic_cast<BasicBlock*>(new_inst->operands[0])) throw std::runtime_error("[IR] jmp target must be a BasicBlock");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("[IR] jmp's type must be void");
    }
    break;
    case Opcode::RET:{
        if(new_inst->operands.size() > 1) throw std::runtime_error("[IR] ret needs at most 1 operand");
        if(new_inst->type != VoidType::get()) throw std::runtime_error("[IR] ret's type must be void");
    }
    break;
    case Opcode::CALL:{
        if(new_inst->operands.empty()) throw std::runtime_error("[IR] call needs at least the callee");
        Function* callee = dynamic_cast<Function*>(new_inst->operands[0]);
        if(!callee) throw std::runtime_error("[IR] call's first operand must be a Function");
        if(new_inst->operands.size() - 1 != callee->args.size())
            throw std::runtime_error("[IR] call argument count mismatch with function " + callee->name);
        if(new_inst->type != callee->return_type)
            throw std::runtime_error("[IR] call result type must match return type of function '" + callee->name + "'");
        for(size_t i = 0; i < callee->args.size(); ++i){
            if(new_inst->operands[i+1]->type != callee->args[i]->type)
                throw std::runtime_error("[IR] call argument type mismatch with function " + callee->name);
        }
    }
    break;
    case Opcode::PHI:
        throw std::runtime_error("[IR] PHI is not implemented (mem2reg not in place yet)");
    default:
        throw std::runtime_error("[IR] unknown op");
    }

    return bb->add_inst(std::move(new_inst));
    
}


} // namespace IR
