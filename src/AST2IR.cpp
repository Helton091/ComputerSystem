#include"AST2IR.hpp"
#include<cassert>
namespace IR{

static bool is_float_type(Type* t){ return t == FloatType::get(); }

static bool is_comparison_opcode(Opcode op){
    return op == Opcode::LT  || op == Opcode::GT  || op == Opcode::LE  ||
           op == Opcode::GE  || op == Opcode::EQ  || op == Opcode::NE  ||
           op == Opcode::FLT || op == Opcode::FGT || op == Opcode::FLE ||
           op == Opcode::FGE || op == Opcode::FEQ || op == Opcode::FNE;
}

static void expect_type(Type* expected, Type* actual, const std::string& context){
    if(expected != actual){
        throw std::runtime_error(
            "type mismatch " + context +
            ": expected '" + expected->to_string() +
            "', found '" + actual->to_string() + "'"
        );
    }
}

static Opcode binop_opcode(Tok op, Type* operand_type){
    bool flt = is_float_type(operand_type);
    switch(op){
    case Tok::ADD:    return flt ? Opcode::FADD : Opcode::ADD;
    case Tok::SUB:    return flt ? Opcode::FSUB : Opcode::SUB;
    case Tok::STAR:   return flt ? Opcode::FMUL : Opcode::MUL;
    case Tok::SLASH:  return flt ? Opcode::FDIV : Opcode::DIV;
    case Tok::PERCENT:
        if(flt) throw std::runtime_error("float type does not support modulo operation");
        return Opcode::REM;
    case Tok::LT:     return flt ? Opcode::FLT : Opcode::LT;
    case Tok::LE:     return flt ? Opcode::FLE : Opcode::LE;
    case Tok::GT:     return flt ? Opcode::FGT : Opcode::GT;
    case Tok::GE:     return flt ? Opcode::FGE : Opcode::GE;
    case Tok::EQ:     return flt ? Opcode::FEQ : Opcode::EQ;
    case Tok::NE:     return flt ? Opcode::FNE : Opcode::NE;
    default: throw std::runtime_error("cannot translate binary token to opcode");
    }
}

static Opcode unop_opcode(Tok op, Type* operand_type){
    bool flt = is_float_type(operand_type);
    switch(op){
    case Tok::SUB: return flt ? Opcode::FNEG : Opcode::NEG;
    default: throw std::runtime_error("cannot translate unary token to opcode");
    }
}

std::unique_ptr<Module> AST2IR::translate(AST::ProgramNode* program){
    module_ = std::make_unique<Module>();
    for(const std::unique_ptr<AST::DeclStmt>& glob_var : program->glob_vars){
        Value* init = nullptr;
        Type* type = to_ir_type(glob_var->var.type.get());
        if(auto c = dynamic_cast<AST::IntNumberNode*>(glob_var->init.get())){
            init = module_->get_const(c->value);
        } else if(auto c = dynamic_cast<AST::FloatNumberNode*>(glob_var->init.get())){
            init = module_->get_const(c->value);
        } else if(!glob_var->init){
            if(type == IntType::get()) init = module_->get_const(0);
            else if(type == FloatType::get()) init = module_->get_const(0.0f);
            else throw std::runtime_error("unsupported type for default initialization of global variable '" + glob_var->var.name + "'");
        } else {
            throw std::runtime_error("the init value of global variable must be literal number");
        }
        module_->add_global(glob_var->var.name,type,init);
    }
    for(const std::unique_ptr<AST::FunctionNode>& funcnode : program->functions){
        Function* func = module_->add_function(funcnode->name,to_ir_type(funcnode->return_type.get()));
        for (size_t i = 0; i < funcnode->params.size(); ++i)
            func->add_arg(to_ir_type(funcnode->params[i].type.get()));

    }
    for(const std::unique_ptr<AST::FunctionNode>& funcnode : program->functions){
        gen_function(funcnode.get());
        
    }
    return std::move(module_);
}

void AST2IR::gen_function(AST::FunctionNode* funcnode){
    temp_counter_ = 0;
    curr_func_ = module_->find_function(funcnode->name);
    curr_func_->add_block("entry");
    curr_bb_ = curr_func_->add_block("start");
    make_inst(curr_func_->entry,Opcode::JMP,VoidType::get(),"jmp",{curr_bb_});

    enter_scope();
    int temp_cnt = 0;
    for(const AST::Param& arg : funcnode->params){
        Argument* new_arg =  curr_func_->args[temp_cnt].get();
        Instruction* addr = curr_func_->add_alloca(arg.name, to_ir_type(arg.type.get()));
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{new_arg,addr});
        if(scope_stack_.back().find(arg.name) != scope_stack_.back().end()) throw std::runtime_error("same name " + arg.name + " in function " + curr_func_->name + "'s arguments list");
        scope_stack_.back()[arg.name] = addr;
        ++temp_cnt;
    }

    gen_stmt(funcnode->body.get());
    if(!curr_bb_->is_terminated()) {
        if(curr_func_->return_type == IntType::get()){
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_const(0)});
        } else {
            make_inst(curr_bb_,Opcode::RET,VoidType::get(),"",{module_->get_const(0.0f)});
        }
    }
    exit_scope();
}

void AST2IR::gen_stmt(AST::StatementNode* stmt){
    if(auto bs = dynamic_cast<AST::BlockStatement*>(stmt)){
        enter_scope();
        for(const auto& bbs : bs->statements){
            gen_stmt(bbs.get());
        }
        exit_scope();
    } else if(auto rs = dynamic_cast<AST::ReturnStatement*>(stmt)){
        Value* ret_val = gen_expr(rs->expr.get());
        expect_type(curr_func_->return_type, ret_val->type,
                    "in return statement of function '" + curr_func_->name + "'");
        make_inst(curr_bb_, Opcode::RET, VoidType::get(),"",{ret_val});
    } else if(auto ds = dynamic_cast<AST::DeclStmt*>(stmt)){
        std::unordered_map<std::string,Instruction*>& curr_stack =  scope_stack_.back();
        if(curr_stack.find(ds->var.name) != curr_stack.end()) throw std::runtime_error("redefined variable " + ds->var.name);
        Instruction* di = curr_func_->add_alloca(ds->var.name, to_ir_type(ds->var.type.get()));
        curr_stack[ds->var.name] = di;
        if(ds->init){
            Value* stored_value = gen_expr(ds->init.get());
            expect_type(di->type, stored_value->type,
                        "in initialization of variable '" + ds->var.name + "'");
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{stored_value,di});
        } else {
            Value* v = nullptr;
            if(dynamic_cast<AST::IntType*>(ds->var.type.get())){
                v = module_->get_const(0);
            } else if(dynamic_cast<AST::FloatType*>(ds->var.type.get())){
                v = module_->get_const(0.0f);
            } else {
                throw std::runtime_error("unsupported type for default initialization of variable '" + ds->var.name + "'");
            }
            make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{v,di});
        }
    } else if(auto es = dynamic_cast<AST::ExprStmt*>(stmt)){
        gen_expr(es->expr.get());
    } else if(auto is = dynamic_cast<AST::IfStmt*>(stmt)){
        Value* cond_v = gen_expr(is->cond.get());
        expect_type(IntType::get(), cond_v->type,
                    "in condition of 'if' statement");
        std::string bb1_name = new_block_name("_if_then_");
        BasicBlock* bb1 = curr_func_->add_block(bb1_name);
        if(is->else_branch){
            std::string bb2_name = new_block_name("_if_else_");
            BasicBlock* bb2 = curr_func_->add_block(bb2_name);
            std::string bbm_name = new_block_name("_if_end_");
            BasicBlock* bbm = curr_func_->add_block(bbm_name);
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,bb1,bb2});
            
            curr_bb_ = bb1;
            gen_stmt(is->then_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            
            curr_bb_ = bb2;
            gen_stmt(is->else_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            
            curr_bb_ = bbm;
        } else {
            std::string bbm_name = new_block_name("_if_end_");
            BasicBlock* bbm = curr_func_->add_block(bbm_name);
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,bb1,bbm});
            curr_bb_ = bb1;
            gen_stmt(is->then_branch.get());
            if(!curr_bb_->is_terminated()) make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{bbm});
            curr_bb_ = bbm;
        }

        
    } else if(auto ws = dynamic_cast<AST::WhileStmt*>(stmt)){
        BasicBlock* cond_bb = curr_func_->add_block(new_block_name("_while_cond_"));
        BasicBlock* body_bb = curr_func_->add_block(new_block_name("_while_body_"));
        BasicBlock* end_bb  = curr_func_->add_block(new_block_name("_while_end_"));

        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = cond_bb;
        Value* cond_v = gen_expr(ws->cond.get());
        expect_type(IntType::get(), cond_v->type,
                    "in condition of 'while' statement");
        make_inst(curr_bb_,Opcode::BR,VoidType::get(),"",{cond_v,body_bb,end_bb});

        curr_bb_ = body_bb;
        gen_stmt(ws->body.get());
        if(!curr_bb_->is_terminated())
            make_inst(curr_bb_,Opcode::JMP,VoidType::get(),"",{cond_bb});

        curr_bb_ = end_bb;
    }
}

Value* AST2IR::gen_expr(AST::ExprNode* expr){
    if(auto nn = dynamic_cast<AST::IntNumberNode*>(expr)){
        return module_->get_const(nn->value);
    } else if(auto nn = dynamic_cast<AST::FloatNumberNode*>(expr)){
        return module_->get_const(nn->value);
    } else if(auto bn = dynamic_cast<AST::BinaryExpr*>(expr)){
        Value* o1 = gen_expr(bn->left.get());
        Value* o2 = gen_expr(bn->right.get());
        if(o1->type != o2->type) throw std::runtime_error("for binary expression, their operands'type shoud be equal");
        Opcode op = binop_opcode(bn->op,o1->type);
        Type* result_type = is_comparison_opcode(op) ? IntType::get() : o1->type;
        return make_inst(curr_bb_,op,result_type,new_temp_name(),{o1,o2});
    } else if(auto un = dynamic_cast<AST::UnaryExpr*>(expr)){
        Value* o = gen_expr(un->operand.get());
        return make_inst(curr_bb_,unop_opcode(un->op,o->type),o->type,new_temp_name(),{o});
    } else if(auto is = dynamic_cast<AST::IdentifierNode*>(expr)){
        Value* alloca = find_variable(is->name);
        if(!alloca) throw std::runtime_error("undefined yet used variable " + is->name);
        return make_inst(curr_bb_, Opcode::LOAD, alloca->type, new_temp_name(), {alloca});
    } else if(auto ae = dynamic_cast<AST::AssignmentExpr*>(expr)){
        Value* rhs_inst = gen_expr(ae->rhs.get());
        Value* alloca_left = find_variable(ae->lhs->name);
        if(!alloca_left) throw std::runtime_error("undefined yet used variable " + ae->lhs->name);
        expect_type(alloca_left->type, rhs_inst->type,
                    "in assignment to variable '" + ae->lhs->name + "'");
        make_inst(curr_bb_,Opcode::STORE,VoidType::get(),"",{rhs_inst,alloca_left});
        return rhs_inst;
    } else if(auto ce = dynamic_cast<AST::CallExpr*>(expr)){
        Function* func = module_->find_function(ce->name);
        if(!func) throw std::runtime_error("undefined function " + ce->name);
        if(func->args.size() != ce->args.size()) throw std::runtime_error("function " + ce->name + "should have " + std::to_string(func->args.size()) + "argument, but got " + std::to_string(ce->args.size()) + " arguments");
        std::vector<Value*> args;
        args.push_back(func);
        for(size_t i = 0; i < ce->args.size(); ++i){
            Value* arg_val = gen_expr(ce->args[i].get());
            Type* param_type = func->args[i]->type;
            expect_type(param_type, arg_val->type,
                        "in argument " + std::to_string(i + 1) +
                        " of call to function '" + ce->name + "'");
            args.push_back(arg_val);
        }
        return make_inst(curr_bb_,Opcode::CALL,func->return_type,new_temp_name(),args);

    }
    else{
        throw std::runtime_error("unknown node type");
    }

}

Instruction* AST2IR::find_alloc(const std::string& name){
    for(auto it = scope_stack_.rbegin(); it != scope_stack_.rend(); ++it){
        if(it->find(name) != it->end()){
            return (*it)[name];
        }
    }
    return nullptr;
}

Value* AST2IR::find_variable(const std::string& name){
    Instruction* local = find_alloc(name);
    if(local) return local;

    GlobalVariable* glob = module_->find_global(name);
    return glob;
}

Type* AST2IR::to_ir_type(AST::Type* ast_type){
    if(dynamic_cast<AST::IntType*>(ast_type)) return IntType::get();
    if(dynamic_cast<AST::FloatType*>(ast_type)) return FloatType::get();
    return VoidType::get();
}


}